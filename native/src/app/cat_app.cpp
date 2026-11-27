#include "cat_app.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>

#include "../core/catalog.hpp"

namespace gato {
namespace {

// A click is not a drag: the cat is only "picked up" once the pointer really moves
constexpr double PICK_UP_PX = 5;
constexpr double CURSOR_POLL_MS = 120;
// Frames per second the cat needs: smooth while active, slow while resting
constexpr double BUSY_MS = 1000.0 / 60, IDLE_MS = 1000.0 / 10, ASLEEP_MS = 1000.0 / 6;
constexpr uint32_t GREEN_SCREEN = 0xff00b140;

double random01() {
  static std::mt19937 rng{std::random_device{}()};
  return std::uniform_real_distribution<double>(0, 1)(rng);
}

}  // namespace

double monotonicMs() {
  using namespace std::chrono;
  return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
}

CatApp::CatApp(std::string assetDir, SettingsStore& store, InputSource& input, CatWindow& window)
    : assetDir_(assetDir),
      store_(store),
      window_(window),
      brain_(monotonicMs, random01),
      renderer_(assetDir),
      relay_(
          input,
          [this] {
            const Point p = window_.position();
            const auto [w, h] = size();
            // the head sits at x = 100 of a drawing spanning -24..200
            return Point{p.x + w * (124.0 / 224.0), p.y + h / 3.0};
          },
          [this] { return window_.desktop(); }, [this](const CatInput& in) { onInput(in); },
          [this](const Devices& d) {
            renderer_.showDevices(d);
            wake();
          }) {}

std::pair<int, int> CatApp::size() const {
  const int w = widthFor(settings_.size);
  return {w, static_cast<int>(std::lround(w * ASPECT))};
}

void CatApp::start(double nowMs) {
  now_ = nowMs;
  settings_ = store_.load();
  const auto [w, h] = size();
  renderer_.setup(resolveSkin(settings_), w);
  renderer_.setDesk(settings_.keyboardColor, settings_.mouseColor, settings_.greenScreen ? GREEN_SCREEN : 0);
  window_.resize(w, h);
  if (settings_.position) {
    window_.move(settings_.position->first, settings_.position->second);
  } else {
    // born in the bottom-right corner of the monitor the user is looking at
    const Rect work = window_.workArea(window_.cursor().value_or(Point{0, 0}));
    window_.move(static_cast<int>(work.x + work.width - w - 40), static_cast<int>(work.y + work.height - h - 20));
  }
  relay_.start(nowMs);
}

void CatApp::stop() { relay_.stop(); }

void CatApp::apply(const Settings& next) {
  const bool resized = next.size != settings_.size;
  settings_ = next;
  store_.save(settings_);
  const auto [w, h] = size();
  renderer_.setup(resolveSkin(settings_), w);
  renderer_.setDesk(settings_.keyboardColor, settings_.mouseColor, settings_.greenScreen ? GREEN_SCREEN : 0);
  if (resized) window_.resize(w, h);
  wake();
}

void CatApp::onInput(const CatInput& input) {
  if (auto bubble = brain_.handle(input); bubble && !settings_.privacy) renderer_.bubble(*bubble, now_);
  wake();
}

void CatApp::pointerDown(MouseButton button, double, double, double screenX, double screenY, double nowMs) {
  now_ = nowMs;
  if (button == MouseButton::Left) {
    drag_ = Drag{screenX, screenY, window_.position(), false};
    return;
  }
  if (button == MouseButton::Right) {
    MenuActions actions{[this](const Settings& s) { apply(s); }, [this] { window_.quit(); }};
    window_.popupMenu(buildMenu(settings_, relay_.devices(), actions), static_cast<int>(screenX), static_cast<int>(screenY));
  }
}

void CatApp::pointerMove(double x, double y, double screenX, double screenY, double nowMs) {
  now_ = nowMs;
  if (drag_) {
    const double dx = screenX - drag_->sx, dy = screenY - drag_->sy;
    if (!drag_->moved && std::hypot(dx, dy) < PICK_UP_PX) return;
    drag_->moved = true;
    renderer_.setDragging(true, nowMs);
    window_.move(static_cast<int>(std::lround(drag_->origin.x + dx)), static_cast<int>(std::lround(drag_->origin.y + dy)));
    wake();
    return;
  }
  // strokes over the head: enough of them in a row is petting
  if (renderer_.overHead(x, y)) {
    brain_.stroke();
    wake();
  }
}

void CatApp::pointerUp(MouseButton button, double nowMs) {
  now_ = nowMs;
  if (button != MouseButton::Left || !drag_) return;
  if (drag_->moved) {
    renderer_.setDragging(false, nowMs);
    const Point p = window_.position();
    settings_.position = {{static_cast<int>(p.x), static_cast<int>(p.y)}};
    store_.save(settings_);
  } else {
    renderer_.twitchEars(nowMs);  // a tap on the cat: it flicks its ears
  }
  drag_.reset();
  wake();
}

double CatApp::tick(double nowMs) {
  now_ = nowMs;
  relay_.tick(nowMs);

  // where the platform reports the real pointer, use it to correct the estimate
  if (nowMs >= nextCursorPoll_) {
    nextCursorPoll_ = nowMs + CURSOR_POLL_MS;
    const auto c = window_.cursor();
    if (c && (!lastCursor_ || c->x != lastCursor_->x || c->y != lastCursor_->y)) relay_.syncPointer(*c, nowMs);
    lastCursor_ = c;
  }

  if (nowMs >= nextFrame_) {
    const Pose pose = brain_.pose(settings_.privacy);
    renderer_.render(pose, nowMs, frame_);
    window_.present(frame_);
    const Pace pace = renderer_.pace();
    // GATO_FPS=1: print how many frames are drawn, to check the cat rests when idle
    static const bool countFrames = std::getenv("GATO_FPS") != nullptr;
    static int frames = 0;
    static double since = nowMs;
    if (countFrames && ++frames && nowMs - since >= 5000) {
      std::fprintf(stderr, "%.1f fps (%s)\n", frames * 1000 / (nowMs - since), pace == Pace::Busy ? "busy" : pace == Pace::Idle ? "idle" : "asleep");
      frames = 0;
      since = nowMs;
    }
    nextFrame_ = nowMs + (pace == Pace::Busy ? BUSY_MS : pace == Pace::Idle ? IDLE_MS : ASLEEP_MS);
  }

  double wait = std::min(nextFrame_, nextCursorPoll_) - nowMs;
  const double gaze = relay_.nextDue(nowMs);
  if (gaze >= 0) wait = std::min(wait, gaze);
  return std::max(0.0, wait);
}

}  // namespace gato
