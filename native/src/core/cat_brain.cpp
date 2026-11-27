#include "cat_brain.hpp"

#include <algorithm>
#include <cmath>
#include <map>

namespace gato {
namespace {

const std::map<int, std::string> LABEL_OF = {{29, "Ctrl"}, {97, "Ctrl"}, {56, "Alt"}, {100, "AltGr"}, {125, "❖"}};
const std::set<int> MODIFIERS = {29, 97, 56, 100, 125};  // Ctrl, Alt, AltGr, Super
const std::set<int> ENTER = {28, 96};
constexpr int KEY_CAPS_LOCK = 58, KEY_NUM_LOCK = 69;
constexpr double YAWN_MS = 3000;
// gaze is a direction scaled down only when the pointer is near the head
constexpr double NEAR = 0.95;
constexpr double DART_SPEED = 2.5;  // gaze units per second
constexpr size_t DARTS_TO_HUNT = 3;
constexpr double HUNT_MS = 1600, SWAT_MS = 200, SWAT_COOLDOWN_MS = 1400;
constexpr double TAP_MS = 140, MOUSE_HOLD_MS = 1100;
constexpr double SLEEPY_MS = 45000, SLEEP_MS = 120000;
constexpr size_t PET_STROKES = 8;
constexpr double PET_WINDOW_MS = 1000;
constexpr size_t EXCITED_KEYS = 6;
constexpr double EXCITED_WINDOW_MS = 1500, SCROLL_MS = 220;

/** Keeps the moments of the last `window` ms, then adds `t` */
void keepRecent(std::vector<double>& moments, double t, double window) {
  moments.erase(std::remove_if(moments.begin(), moments.end(), [&](double m) { return t - m >= window; }), moments.end());
}

const char* buttonLabel(MouseButton b) {
  switch (b) {
    case MouseButton::Left: return "Esq";
    case MouseButton::Right: return "Dir";
    default: return "Meio";
  }
}

}  // namespace

CatBrain::CatBrain(Clock now, Random random) : now_(std::move(now)), random_(std::move(random)) {
  lastInput_ = now_();
  nextBlink_ = lastInput_ + 3000;
}

std::optional<Bubble> CatBrain::handle(const CatInput& input) {
  const double t = now_();
  lastInput_ = t;
  switch (input.type) {
    case CatInput::Type::Key: {
      if (MODIFIERS.count(input.code)) {
        if (input.down) modifiers_.insert(input.code);
        else modifiers_.erase(input.code);
      }
      if (!input.down) return std::nullopt;
      if (input.code == KEY_CAPS_LOCK) capsLock_ = !capsLock_;
      if (input.code == KEY_NUM_LOCK) numLock_ = !numLock_;
      keepRecent(recentKeys_, t, EXCITED_WINDOW_MS);
      recentKeys_.push_back(t);
      Paw paw;
      if (auto hand = handForKey(input.code)) paw = *hand;
      else paw = alternate_ = (alternate_ == Paw::Left ? Paw::Right : Paw::Left);
      if (paw == Paw::Left) leftUntil_ = t + TAP_MS;
      else { rightUntil_ = t + TAP_MS; mouseUntil_ = 0; }
      pressed_ = {{input.code, t + TAP_MS}};
      // a shortcut takes both paws: one holds the modifier, the other taps the key
      std::optional<int> modifier;
      if (!MODIFIERS.count(input.code) && !modifiers_.empty()) modifier = *modifiers_.begin();
      if (modifier) {
        chord_ = {{*modifier, t + TAP_MS * 2}};
        leftUntil_ = rightUntil_ = t + TAP_MS * 2;
        mouseUntil_ = 0;
      }
      if (ENTER.count(input.code)) slamUntil_ = t + 260;
      std::string label = modifier ? LABEL_OF.at(*modifier) + "+" + input.label : input.label;
      return Bubble{label, paw, false};
    }
    case CatInput::Type::Button:
      if (!input.down) {
        if (held_ == input.which) held_.reset();
        return std::nullopt;
      }
      held_ = input.which;
      clickUntil_ = t + TAP_MS;
      mouseUntil_ = t + MOUSE_HOLD_MS;
      return Bubble{buttonLabel(input.which), Paw::Right, true};
    case CatInput::Type::Wheel: {
      // one bubble per scroll gesture, not one per wheel notch
      const bool fresh = !scroll_ || t > scroll_->second || scroll_->first != input.direction;
      scroll_ = {{input.direction, t + SCROLL_MS}};
      mouseUntil_ = t + MOUSE_HOLD_MS;
      if (!fresh) return std::nullopt;
      return Bubble{input.direction > 0 ? "↑" : "↓", Paw::Right, true};
    }
    case CatInput::Type::Gaze:
      noteDart({input.x, input.y}, t);
      gaze_ = {input.x, input.y};
      mouseUntil_ = std::max(mouseUntil_, t + MOUSE_HOLD_MS);
      return std::nullopt;
  }
  return std::nullopt;
}

void CatBrain::noteDart(const Gaze& gaze, double t) {
  const double dt = std::max(t - lastGaze_.t, 1.0);
  const double speed = std::hypot(gaze.x - lastGaze_.x, gaze.y - lastGaze_.y) / dt * 1000;
  lastGaze_ = {gaze.x, gaze.y, t};
  const bool near = std::hypot(gaze.x, gaze.y) < NEAR;
  // strokes over the head are petting, not prey
  const bool petted = std::any_of(strokes_.begin(), strokes_.end(), [&](double s) { return t - s < PET_WINDOW_MS; });
  if (!near || speed < DART_SPEED || petted) return;
  keepRecent(darts_, t, 500);
  darts_.push_back(t);
  if (darts_.size() < DARTS_TO_HUNT) return;
  huntUntil_ = t + HUNT_MS;
  if (t >= nextSwat_) {
    swatUntil_ = t + SWAT_MS;
    nextSwat_ = t + SWAT_COOLDOWN_MS;
  }
}

void CatBrain::stroke() {
  const double t = now_();
  keepRecent(strokes_, t, PET_WINDOW_MS);
  strokes_.push_back(t);
  if (strokes_.size() >= PET_STROKES) {
    petUntil_ = t + 1500;
    huntUntil_ = swatUntil_ = 0;
    lastInput_ = t;
  }
}

Pose CatBrain::pose(bool privacy) {
  const double t = now_();
  if (t >= nextBlink_) {
    const double r = random_();
    // mostly single blinks, some doubles, now and then a slow wink
    wink_ = r < 0.08 ? (random_() < 0.5 ? Wink::L : Wink::R) : Wink::Off;
    blinkUntil_ = t + (wink_ != Wink::Off ? 320 : 140);
    nextBlink_ = r > 0.75 ? t + 280 : t + 2500 + random_() * 4000;
  }
  const double idle = t - lastInput_;
  const bool sleeping = idle > SLEEP_MS;
  const bool yawning = !privacy && idle > SLEEP_MS - YAWN_MS && !sleeping;
  const bool petting = t < petUntil_;
  const bool onMouse = t < mouseUntil_;

  Pose p;
  p.eyes = petting ? Eyes::Happy
           : privacy || sleeping || yawning ? Eyes::Closed
           : idle > SLEEPY_MS ? Eyes::Sleepy
           : t < blinkUntil_ && wink_ == Wink::Off ? Eyes::Closed
           : Eyes::Open;
  p.left = t < leftUntil_ ? LeftPaw::Key : LeftPaw::Rest;
  p.right = t < rightUntil_ ? RightPaw::Key : t < clickUntil_ ? RightPaw::Click : onMouse ? RightPaw::Mouse : RightPaw::Rest;
  if (p.eyes == Eyes::Open || p.eyes == Eyes::Sleepy) p.gaze = gaze_;
  if (!privacy && pressed_ && t < pressed_->second) p.pressed = pressed_->first;
  p.sleeping = sleeping;
  p.petting = petting;
  const auto typedFast = std::count_if(recentKeys_.begin(), recentKeys_.end(), [&](double k) { return t - k < EXCITED_WINDOW_MS; });
  p.excited = petting || t < huntUntil_ || static_cast<size_t>(typedFast) >= EXCITED_KEYS;
  p.button = held_;
  p.scroll = scroll_ && t < scroll_->second ? scroll_->first : 0;
  p.capsLock = capsLock_;
  p.numLock = numLock_;
  if (chord_ && t < chord_->second) p.chord = chord_->first;
  p.slam = t < slamUntil_;
  p.yawning = yawning;
  p.wink = p.eyes == Eyes::Open && t < blinkUntil_ ? wink_ : Wink::Off;
  p.hunting = !privacy && !sleeping && !petting && t < huntUntil_;
  p.swat = !privacy && !sleeping && !petting && t < swatUntil_;
  return p;
}

Gaze gazeToward(double px, double py, double headX, double headY) {
  const double dx = px - headX, dy = py - headY;
  const double scale = 1 / std::max(std::hypot(dx, dy), 250.0);
  return {dx * scale, dy * scale};
}

}  // namespace gato
