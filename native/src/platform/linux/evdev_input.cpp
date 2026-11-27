#include "evdev_input.hpp"

#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>

#include <cerrno>
#include <cmath>
#include <fstream>
#include <regex>
#include <sstream>

#include "../../app/cat_app.hpp"

namespace gato {
namespace {

constexpr double TOUCHPAD_SCALE = 0.5;
// A resting palm or a nudged mouse is not a switch: the new pointer must
// really move (or click) before it takes over the drawing
constexpr double SWITCH_TRAVEL = 120, SWITCH_WINDOW_MS = 600;
// Palms brush the touchpad while typing: ignore it for a moment after each key
// (what libinput calls disable-while-typing), and ask for a longer, deliberate move
constexpr double TYPING_GRACE_MS = 1200, TOUCHPAD_TRAVEL = 600;
constexpr int KEY_F1_CODE = 59, KEY_KP0_CODE = 82, KEY_FN_CODE = 464;

// Laptop parts, recognised by the names the kernel gives them
const std::regex BUILT_IN_KEYBOARD("AT Translated|AT Raw Set|i8042", std::regex::icase);
const std::regex BUILT_IN_POINTER("touchpad|trackpad|trackpoint|MSFT[0-9]|ELAN|SYNA|Synaptics", std::regex::icase);

bool matches(const std::string& name, const std::regex& re) { return std::regex_search(name, re); }

struct Listed { std::string name, handlers, keys; };

/** Keyboards and pointers, from the kernel's device list */
std::vector<Listed> listDevices() {
  std::ifstream in("/proc/bus/input/devices");
  std::vector<Listed> list;
  Listed cur;
  std::string line;
  auto flush = [&] {
    if (!cur.handlers.empty()) list.push_back(cur);
    cur = {};
  };
  while (std::getline(in, line)) {
    if (line.empty()) { flush(); continue; }
    if (line.rfind("N: Name=\"", 0) == 0) cur.name = line.substr(9, line.size() - 10);
    else if (line.rfind("H: Handlers=", 0) == 0) cur.handlers = line.substr(12);
    else if (line.rfind("B: KEY=", 0) == 0) cur.keys = line.substr(7);
  }
  flush();
  return list;
}

}  // namespace

void EvdevInput::start(Emit emit) {
  emit_ = std::move(emit);
  static const std::regex kinds("\\b(kbd|mouse[0-9]+)\\b"), event("event[0-9]+");
  for (const auto& d : listDevices()) {
    std::smatch m;
    if (!std::regex_search(d.handlers, kinds) || !std::regex_search(d.handlers, m, event)) continue;
    Device dev;
    dev.path = "/dev/input/" + m.str();
    dev.name = d.name;
    dev.builtInKeyboard = matches(d.name, BUILT_IN_KEYBOARD);
    dev.builtInPointer = matches(d.name, BUILT_IN_POINTER);
    // "B: KEY=" lists hex words, most significant first
    std::istringstream words(d.keys);
    std::string w;
    while (words >> w) dev.keyWords.insert(dev.keyWords.begin(), std::stoull(w, nullptr, 16));
    dev.fd = open(dev.path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (dev.fd < 0) {
      if (errno == EACCES || errno == EPERM) onDenied_(dev.path);
      continue;
    }
    devices_[dev.fd] = dev;
  }
}

void EvdevInput::stop() {
  for (auto& [fd, d] : devices_) close(fd);
  devices_.clear();
}

std::vector<int> EvdevInput::fds() const {
  std::vector<int> list;
  for (const auto& [fd, d] : devices_) list.push_back(fd);
  return list;
}

bool EvdevInput::hasKey(const Device& d, int code) const {
  const size_t word = code / 64, bit = code % 64;
  return word < d.keyWords.size() && ((d.keyWords[word] >> bit) & 1ULL);
}

bool EvdevInput::travelled(const std::string& path, double step, double needed) {
  const double now = monotonicMs();
  if (travel_.path != path || now - travel_.since > SWITCH_WINDOW_MS) travel_ = {path, 0, now};
  travel_.distance += step;
  return travel_.distance >= needed;
}

void EvdevInput::noteDevice(Device& d, const RawInput& input) {
  const bool keyboard = input.kind == RawInput::Kind::Key;
  std::string& active = keyboard ? activeKeyboard_ : activePointer_;
  if (active == d.path) return;
  const double needed = d.builtInPointer ? TOUCHPAD_TRAVEL : SWITCH_TRAVEL;
  if (input.kind == RawInput::Kind::Move && !travelled(d.path, std::abs(input.dx) + std::abs(input.dy), needed)) return;
  active = d.path;
  RawInput dev{RawInput::Kind::Device};
  dev.keyboard = keyboard;
  dev.name = d.name;
  dev.builtIn = keyboard ? d.builtInKeyboard : d.builtInPointer;
  if (keyboard) dev.features = KeyboardFeatures{hasKey(d, KEY_F1_CODE), hasKey(d, KEY_KP0_CODE), dev.builtIn || hasKey(d, KEY_FN_CODE)};
  emit_(dev);
}

void EvdevInput::readable(int fd) {
  auto it = devices_.find(fd);
  if (it == devices_.end()) return;
  Device& d = it->second;
  input_event events[64];
  for (;;) {
    const ssize_t n = read(fd, events, sizeof events);
    if (n <= 0) {
      if (n == 0 || (errno != EAGAIN && errno != EINTR)) {
        // unplugged
        close(fd);
        devices_.erase(it);
      }
      return;
    }
    for (size_t i = 0; i < static_cast<size_t>(n) / sizeof(input_event); i++) {
      const input_event& e = events[i];
      RawInput in{RawInput::Kind::Key};
      bool have = false;
      if (e.type == EV_KEY) {
        if (e.value == 2) continue;  // auto-repeat
        if (e.code == BTN_TOUCH) {
          d.touch.active = e.value == 1;
          d.touch.hasX = d.touch.hasY = false;
          continue;
        }
        if (e.code >= BTN_LEFT && e.code <= BTN_MIDDLE) {
          in.kind = RawInput::Kind::Button;
          in.which = e.code == BTN_LEFT ? MouseButton::Left : e.code == BTN_RIGHT ? MouseButton::Right : MouseButton::Middle;
          in.down = e.value == 1;
          have = true;
        } else if (e.code < 0x100) {
          in.code = e.code;
          in.down = e.value == 1;
          have = true;
        }
      } else if (e.type == EV_REL) {
        if (e.code == REL_X || e.code == REL_Y) {
          in.kind = RawInput::Kind::Move;
          (e.code == REL_X ? in.dx : in.dy) = e.value;
          have = true;
        } else if (e.code == REL_WHEEL || e.code == REL_HWHEEL) {
          in.kind = RawInput::Kind::Wheel;
          in.direction = e.value > 0 ? 1 : -1;
          have = true;
        }
      } else if (e.type == EV_ABS && d.touch.active && (e.code == ABS_X || e.code == ABS_Y)) {
        const bool x = e.code == ABS_X;
        bool& known = x ? d.touch.hasX : d.touch.hasY;
        int& last = x ? d.touch.x : d.touch.y;
        if (known) {
          in.kind = RawInput::Kind::Move;
          (x ? in.dx : in.dy) = (e.value - last) * TOUCHPAD_SCALE;
          have = true;
        }
        known = true;
        last = e.value;
      }
      if (!have) continue;
      const double now = monotonicMs();
      if (in.kind == RawInput::Kind::Key) lastKeyAt_ = now;
      const bool palm = d.builtInPointer && in.kind != RawInput::Kind::Key && now - lastKeyAt_ < TYPING_GRACE_MS;
      if (palm) continue;
      noteDevice(d, in);
      emit_(in);
    }
  }
}

}  // namespace gato
