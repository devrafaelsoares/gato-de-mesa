#include "input_relay.hpp"

#include <algorithm>

#include "../core/cat_brain.hpp"
#include "../core/keyboard.hpp"

namespace gato {
namespace {
constexpr double GAZE_INTERVAL_MS = 33;
}

InputRelay::InputRelay(InputSource& source, std::function<Point()> head, std::function<Rect()> desktop, Send send, OnDevices onDevices)
    : source_(source), head_(std::move(head)), desktop_(std::move(desktop)), send_(std::move(send)), onDevices_(std::move(onDevices)) {}

void InputRelay::start(double nowMs) {
  now_ = nowMs;
  const Rect d = desktop_();
  pointer_ = {d.x + d.width / 2, d.y + d.height / 2};
  source_.start([this](const RawInput& raw) { onRaw(raw); });
}

void InputRelay::stop() { source_.stop(); }

void InputRelay::syncPointer(Point p, double nowMs) {
  now_ = nowMs;
  pointer_ = p;
  scheduleGaze(nowMs);
}

void InputRelay::scheduleGaze(double nowMs) {
  if (gazeDue_ < 0) gazeDue_ = nowMs + GAZE_INTERVAL_MS;
}

void InputRelay::tick(double nowMs) {
  now_ = nowMs;
  if (gazeDue_ < 0 || nowMs < gazeDue_) return;
  gazeDue_ = -1;
  const Point h = head_();
  const Gaze g = gazeToward(pointer_.x, pointer_.y, h.x, h.y);
  CatInput in{CatInput::Type::Gaze};
  in.x = g.x;
  in.y = g.y;
  send_(in);
}

double InputRelay::nextDue(double nowMs) const { return gazeDue_ < 0 ? -1 : std::max(0.0, gazeDue_ - nowMs); }

void InputRelay::onRaw(const RawInput& raw) {
  switch (raw.kind) {
    case RawInput::Kind::Key: {
      CatInput in{CatInput::Type::Key};
      in.code = raw.code;
      in.label = labelForScanCode(raw.code);
      in.down = raw.down;
      return send_(in);
    }
    case RawInput::Kind::Button: {
      CatInput in{CatInput::Type::Button};
      in.which = raw.which;
      in.down = raw.down;
      return send_(in);
    }
    case RawInput::Kind::Wheel: {
      CatInput in{CatInput::Type::Wheel};
      in.direction = raw.direction;
      return send_(in);
    }
    case RawInput::Kind::Move: {
      const Rect d = desktop_();
      pointer_.x = std::clamp(pointer_.x + raw.dx, d.x, d.x + d.width);
      pointer_.y = std::clamp(pointer_.y + raw.dy, d.y, d.y + d.height);
      return scheduleGaze(now_);
    }
    case RawInput::Kind::Point: return syncPointer({raw.x, raw.y}, now_);
    case RawInput::Kind::Device:
      if (raw.keyboard) {
        devices_.keyboard.name = raw.name;
        devices_.keyboard.laptop = raw.builtIn;
        devices_.keyboard.features = raw.features.value_or(KeyboardFeatures{});
      } else {
        devices_.pointer.name = raw.name;
        devices_.pointer.touchpad = raw.builtIn;
      }
      return onDevices_(devices_);
  }
}

}  // namespace gato
