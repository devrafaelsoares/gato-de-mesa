#include "cat_renderer.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <set>

#include "../core/catalog.hpp"

namespace gato {
namespace {

constexpr double PI = 3.14159265358979323846;

// The cat faces the viewer, so its left paw is on the viewer's right
// and its right paw (the mouse paw) on the viewer's left.
constexpr Pt SHOULDER[2] = {{117, 108}, {83, 108}};
constexpr Pt REST[2] = {{122, 166}, {78, 166}};
constexpr Pt MOUSE{30, 194};
constexpr Pt PIVOT{100, 172};
constexpr double LEAN_DEG = -10;
/** Longest a foreleg gets: beyond it the paw stops short instead of stretching */
constexpr double MAX_REACH = 92;

// Keyboard in perspective, turned towards the cat: nearest row at the top,
// mirrored left/right, centred on the cat.
constexpr double KB_TOP = 175, KB_BOTTOM = 204, UNIT = 4.1, BACK_SCALE = 0.82;
const std::set<int> ACCENT_KEYS = {1, 28, 96};

// The head is drawn at translate(8 0) scale(.92) inside its group
constexpr double HEAD_SCALE = 0.92, HEAD_SHIFT = 8;
constexpr Pt HEAD_ORIGIN{100, 104};
constexpr Pt EAR_ORIGIN[2] = {{HEAD_SHIFT + HEAD_SCALE * 76, HEAD_SCALE * 52}, {HEAD_SHIFT + HEAD_SCALE * 124, HEAD_SCALE * 52}};
constexpr Pt WHISKER_ORIGIN[2] = {{HEAD_SHIFT + HEAD_SCALE * 88, HEAD_SCALE * 90}, {HEAD_SHIFT + HEAD_SCALE * 112, HEAD_SCALE * 90}};
constexpr Pt TAIL_ORIGIN{136, 162};

constexpr uint32_t PUPIL = 0x111111;

int idx(Paw p) { return p == Paw::Left ? 0 : 1; }
double lerp(double a, double b, double t) { return a + (b - a) * t; }
Pt approach(Pt from, Pt to, double k) { return {lerp(from.x, to.x, k), lerp(from.y, to.y, k)}; }
double dist(Pt a, Pt b) { return std::hypot(a.x - b.x, a.y - b.y); }
/** A spring factor tuned for 60 fps, corrected for the real frame time */
double perFrame(double k, double dtMs) { return 1 - std::pow(1 - k, dtMs / 16.667); }
/** Eases towards a target like a CSS transition with time constant tau */
void ease(double& v, double target, double dtMs, double tauMs) { v += (target - v) * (1 - std::exp(-dtMs / tauMs)); }
/** 0..1..0 over a period, smooth like ease-in-out keyframes at 0/50/100% */
double wave(double tMs, double periodMs) { return (1 - std::cos(2 * PI * std::fmod(tMs, periodMs) / periodMs)) / 2; }
double smooth(double t) { t = std::clamp(t, 0.0, 1.0); return t * t * (3 - 2 * t); }

/** Pulls a paw target back so the leg never stretches past MAX_REACH */
Pt withinReach(Pt s, Pt target) {
  const double dx = target.x - s.x, dy = target.y - s.y, len = std::hypot(dx, dy);
  if (len <= MAX_REACH) return target;
  return {s.x + dx / len * MAX_REACH, s.y + dy / len * MAX_REACH};
}

/** A point of the body after the body leans by `deg` around its base */
Pt leaned(Pt p, double deg) {
  const double a = deg * PI / 180, dx = p.x - PIVOT.x, dy = p.y - PIVOT.y;
  return {PIVOT.x + dx * std::cos(a) - dy * std::sin(a), PIVOT.y + dx * std::sin(a) + dy * std::cos(a)};
}

/** Keyframes (time 0..1 -> value), eased between stops */
struct Key { double at, a, b, c; };
void keyframes(const std::vector<Key>& keys, double t, double& a, double& b, double& c) {
  t = std::clamp(t, 0.0, 1.0);
  for (size_t i = 0; i + 1 < keys.size(); i++) {
    if (t > keys[i + 1].at) continue;
    const double span = keys[i + 1].at - keys[i].at;
    const double k = span <= 0 ? 1 : smooth((t - keys[i].at) / span);
    a = lerp(keys[i].a, keys[i + 1].a, k);
    b = lerp(keys[i].b, keys[i + 1].b, k);
    c = lerp(keys[i].c, keys[i + 1].c, k);
    return;
  }
  a = keys.back().a; b = keys.back().b; c = keys.back().c;
}

double random01() {
  static std::mt19937 rng{std::random_device{}()};
  return std::uniform_real_distribution<double>(0, 1)(rng);
}

void fillUnits(Image& frame, const Path& path, const Mat& m, uint32_t color, const Mask* mask = nullptr, const Shader* shader = nullptr) {
  fill(frame, path.transformed(m), color, mask, shader);
}

}  // namespace

CatRenderer::CatRenderer(std::string assetDir) : assetDir_(std::move(assetDir)) {
  bubbleFont_.load(assetDir_, "glyphs-bubble");
  paws_[0] = REST[0];
  paws_[1] = REST[1];
  nextTwitch_ = 4000;
}

bool CatRenderer::setup(const Skin& skin, int widthPx) {
  if (skin.id == coat_ && widthPx == width_) return true;
  width_ = widthPx;
  height_ = static_cast<int>(std::lround(widthPx * ASPECT));
  scale_ = widthPx / VIEW_W;
  view_ = Mat::scale(scale_) * Mat::translate(-VIEW_X, 0);
  skin_ = skin;
  coat_ = skin.id;
  keyboardDirty_ = true;
  return sprites_.load(assetDir_, skin.id, scale_);
}

void CatRenderer::setDesk(uint32_t keyboardColor, uint32_t mouseColor, uint32_t background) {
  if (keyboardColor != keyboardColor_ || background != background_) keyboardDirty_ = true;
  keyboardColor_ = keyboardColor;
  mouseColor_ = mouseColor;
  background_ = background;
}

void CatRenderer::showDevices(const Devices& devices) {
  if (devices.keyboard.features != features_) keyboardDirty_ = true;
  features_ = devices.keyboard.features;
  laptop_ = devices.keyboard.laptop;
  touchpad_ = devices.pointer.touchpad;
}

void CatRenderer::setDragging(bool on, double nowMs) {
  if (dragging_ == on) return;
  dragging_ = on;
  if (!on) landingAt_ = nowMs;  // put down: a little landing bounce
}

void CatRenderer::twitchEars(double nowMs) {
  twitchAt_ = nowMs;
  twitchLeft_ = random01() < 0.5;
}

void CatRenderer::bubble(const Bubble& b, double nowMs) {
  const double x = b.atMouse ? mouseX_ + mouseOffset_.x : REST[idx(b.paw)].x + (b.paw == Paw::Left ? 14 : -14);
  const double y = b.atMouse ? 167 : 146;
  // rapid clicks replace the mouse label instead of stacking copies
  if (b.atMouse) bubbles_.erase(std::remove_if(bubbles_.begin(), bubbles_.end(), [](const LiveBubble& l) { return l.atMouse; }), bubbles_.end());
  bubbles_.push_back({b.label, x, y, nowMs, b.atMouse});
  if (bubbles_.size() > 4) bubbles_.erase(bubbles_.begin());
}

bool CatRenderer::overHead(double x, double y) const {
  const Pt u = headMat_.inverse().apply({x, y});
  const double cx = HEAD_SHIFT + HEAD_SCALE * 100, cy = HEAD_SCALE * 62, rx = HEAD_SCALE * 46, ry = HEAD_SCALE * 40;
  const double dx = (u.x - cx) / rx, dy = (u.y - cy) / ry;
  return dx * dx + dy * dy <= 1;
}

void CatRenderer::sprite(Image& frame, const std::string& name, const Mat& m, float opacity, const Mask* mask) {
  if (const Sprite* s = sprites_.get(name)) blit(frame, s->img, m * s->toUnits(), opacity, mask);
}

// ---------------------------------------------------------------- keyboard

void CatRenderer::buildKeyboardCache() {
  keyboardDirty_ = false;
  keyboardCache_.resize(width_, height_);
  caps_.clear();
  keyCenters_.clear();
  const KeyboardLayout layout = buildLayout(features_);
  const double cx = PIVOT.x;
  const double step = (KB_BOTTOM - KB_TOP - 3) / layout.rows, keyH = step * 0.78;
  auto scaleAt = [](double y) { return lerp(BACK_SCALE, 1, (y - KB_TOP) / (KB_BOTTOM - KB_TOP)); };
  for (const auto& k : layout.keys) {
    const double y = KB_TOP + 1.5 + (layout.rows - 1 - k.row) * step;
    const double s = scaleAt(y + keyH / 2);
    const double x = cx + (layout.width - k.x - k.w - layout.width / 2) * UNIT * s + 0.45 * s;
    const double w = k.w * UNIT * s - 0.9 * s;
    if (!keyCenters_.count(k.code)) keyCenters_[k.code] = {x + w / 2, y + keyH / 2};
    caps_.push_back({k.code, x, y, w, keyH, ACCENT_KEYS.count(k.code) > 0});
  }

  auto half = [&](double s) { return layout.width / 2 * UNIT * s + 3; };
  const double tl = cx - half(BACK_SCALE), tr = cx + half(BACK_SCALE), bl = cx - half(1), br = cx + half(1);
  const double B = KB_BOTTOM, T = KB_TOP;
  Path front;
  front.moveTo(bl, B - 2).lineTo(br, B - 2).lineTo(br, B + 6).quadTo(br, B + 9, br - 3, B + 9).lineTo(bl + 3, B + 9).quadTo(bl, B + 9, bl, B + 6);
  Path deck;
  deck.moveTo(tl + 2, T - 1.5).lineTo(tr - 2, T - 1.5).quadTo(tr, T - 1.5, tr + 1, T + 1).lineTo(br, B - 1).quadTo(br, B + 1, br - 3, B + 1)
      .lineTo(bl + 3, B + 1).quadTo(bl, B + 1, bl, B - 1).lineTo(tl - 1, T + 1).quadTo(tl, T - 1.5, tl + 2, T - 1.5);

  Image& k = keyboardCache_;
  fillUnits(k, Path().ellipse(cx, B + 9, (br - bl) / 2 + 2, 4), view_, premul(0, 0.2));
  fillUnits(k, front, view_, premul(keyboardColor_));
  fillUnits(k, front, view_, premul(0, 0.3));
  fillUnits(k, deck, view_, premul(keyboardColor_));
  std::vector<Pt> rim = deck.contours().front();
  rim.push_back(rim.front());
  fillUnits(k, strokePath(rim, 0.8), view_, premul(0, 0.25));
  for (const auto& c : caps_) {
    fillUnits(k, Path().rect(c.x, c.y + 1.5, c.w, c.h), view_, premul(c.accent ? 0xc99543 : 0xc9c0ae));
    fillUnits(k, Path().rect(c.x, c.y, c.w, c.h), view_, premul(c.accent ? 0xf3c77a : 0xf6f2ea));
  }
  // lock lights
  const uint32_t off = premul(0x6b6b70), on = premul(0x7dffa0);
  fillUnits(k, Path().ellipse(bl + 7, B + 3.6, 0.9, 0.9), view_, numLock_ ? on : off);
  fillUnits(k, Path().ellipse(bl + 10.5, B + 3.6, 0.9, 0.9), view_, capsLock_ ? on : off);
  fillUnits(k, Path().ellipse(bl + 14, B + 3.6, 0.9, 0.9), view_, off);
  // the mouse sits beside the keyboard, on the cat's right
  mouseX_ = bl - 12;
  mouseKey_ = -1;
}

void CatRenderer::drawKeyboard(Image& frame, const Pose& pose, double nowMs) {
  if (pose.numLock != numLock_ || pose.capsLock != capsLock_) {
    numLock_ = pose.numLock;
    capsLock_ = pose.capsLock;
    keyboardDirty_ = true;
  }
  if (keyboardDirty_) buildKeyboardCache();
  if (pose.slam && !wasSlam_) slamAt_ = nowMs;
  wasSlam_ = pose.slam;
  // Enter hit hard: the keyboard thumps
  double thump = 0, unused1, unused2;
  if (nowMs - slamAt_ < 260) keyframes({{0, 0, 0, 0}, {0.3, 1.6, 0, 0}, {1, 0, 0, 0}}, (nowMs - slamAt_) / 260, thump, unused1, unused2);
  const Mat m = view_ * Mat::translate(0, thump);
  blit(frame, keyboardCache_, Mat::translate(0, thump * scale_));

  for (auto code : {pose.pressed, pose.chord}) {
    if (!code) continue;
    for (const auto& c : caps_) {
      if (c.code != *code) continue;
      fillUnits(frame, Path().rect(c.x, c.y, c.w, 1.1), m, premul(keyboardColor_));
      fillUnits(frame, Path().rect(c.x, c.y + 1.5, c.w, c.h), m, premul(c.accent ? 0xc99543 : 0xc9c0ae));
      fillUnits(frame, Path().rect(c.x, c.y + 1.1, c.w, c.h), m, premul(0xf29b3b));
    }
  }
}

// ---------------------------------------------------------------- mouse

void CatRenderer::drawMouse(Image& frame, const Pose& pose, double nowMs) {
  const Mat place = view_ * Mat::translate(touchpad_ ? mouseX_ : mouseX_ + mouseOffset_.x, touchpad_ ? 0 : mouseOffset_.y);
  // a rolling wheel animates; everything else is drawn once and reused
  if (pose.scroll != 0 && !touchpad_) return drawMouseShape(frame, place, pose, nowMs);
  const int key = (touchpad_ ? 1 : 0) | (pose.button == MouseButton::Left ? 2 : 0) | (pose.button == MouseButton::Right ? 4 : 0) |
                  (pose.button == MouseButton::Middle ? 8 : 0);
  // the cache holds local units x -15..15, y M-15..M+20 at window scale
  const Mat local = Mat::translate(2, 2) * Mat::scale(scale_) * Mat::translate(15, -(MOUSE.y - 15));
  if (key != mouseKey_ || mouseColorCached_ != mouseColor_) {
    mouseKey_ = key;
    mouseColorCached_ = mouseColor_;
    mouseCache_.resize(static_cast<int>(30 * scale_) + 4, static_cast<int>(35 * scale_) + 4);
    drawMouseShape(mouseCache_, local, pose, nowMs);
  }
  blit(frame, mouseCache_, place * local.inverse());
}

void CatRenderer::drawMouseShape(Image& frame, const Mat& place, const Pose& pose, double nowMs) {
  const double M = MOUSE.y;
  if (touchpad_) {
    // laptop users get a touchpad where the mouse would be
    const Mat& m = place;
    fillUnits(frame, Path().roundRect(-13, M - 8, 26, 20, 4), m, premul(0, 0.22));
    fillUnits(frame, Path().roundRect(-13, M - 10, 26, 20, 4), m, premul(mouseColor_));
    fillUnits(frame, Path().roundRect(-13.75, M - 10.75, 27.5, 21.5, 4.75).roundRect(-12.25, M - 9.25, 24.5, 18.5, 3.25, true), m, premul(0, 0.45));
    fillUnits(frame, strokePath({{-13, M + 5}, {13, M + 5}}, 1), m, premul(0, 0.3));
    return;
  }
  // buttons point away from the cat, so its left button is on the viewer's right
  const Mat& m = place;
  fillUnits(frame, Path().ellipse(1.5, M + 5, 11.5, 13.5), m, premul(0, 0.2));
  fillUnits(frame, Path().ellipse(0, M + 3, 10, 13), m, premul(mouseColor_));
  fillUnits(frame, Path().ellipse(0, M + 3, 10, 13), m, premul(0, 0.32));
  fillUnits(frame, Path().ellipse(0, M, 10, 13), m, premul(mouseColor_));
  const Mat inv = m.inverse();
  const Mask inside = [&](double x, double y) {
    const Pt u = inv.apply({x, y});
    const double d = std::hypot(u.x / 10, (u.y - M) / 13);
    return static_cast<float>(std::clamp((1 - d) * 10 * scale_ + 0.5, 0.0, 1.0));
  };
  if (pose.button == MouseButton::Left) fillUnits(frame, Path().rect(0, M + 2, 10, 12), m, premul(0xf29b3b, 0.85), &inside);
  if (pose.button == MouseButton::Right) fillUnits(frame, Path().rect(-10, M + 2, 10, 12), m, premul(0xf29b3b, 0.85), &inside);
  fillUnits(frame, Path().ellipse(-3, M - 5, 4, 5), m, premul(0xffffff, 0.18), &inside);
  std::vector<Pt> seam;
  for (int i = 0; i <= 10; i++) {
    const double t = i / 10.0, u = 1 - t;
    seam.push_back({u * u * -9.6 + t * t * 9.6, u * u * (M + 2.4) + 2 * u * t * M + t * t * (M + 2.4)});
  }
  fillUnits(frame, strokePath(seam, 0.9), m, premul(0, 0.4));
  fillUnits(frame, strokePath({{0, M + 1.2}, {0, M + 13}}, 0.9), m, premul(0, 0.4));
  const bool wheelLit = pose.button == MouseButton::Middle || pose.scroll != 0;
  fillUnits(frame, Path().roundRect(-1.6, M + 3.5, 3.2, 6, 1.6), m, premul(wheelLit ? 0xf29b3b : 0x55565c));
  const double roll = pose.scroll == 0 ? 0 : std::fmod(nowMs, 180) / 180 * 1.5 * -pose.scroll;
  for (double ry : {5.0, 6.5, 8.0}) {
    const double y = M + ry + roll;
    if (y < M + 3.8 || y > M + 9.2) continue;
    fillUnits(frame, strokePath({{-1.1, y}, {1.1, y}}, 0.5), m, premul(0xffffff, 0.55));
  }
  // outline: a ring, so the top stays the mouse's own color
  fillUnits(frame, Path().ellipse(0, M, 10.7, 13.7).ellipse(0, M, 9.3, 12.3, true), m, premul(0, 0.45));
}

// ---------------------------------------------------------------- legs and eyes

Pt CatRenderer::pawTarget(Paw paw, const Pose& pose, double nowMs) const {
  const int i = idx(paw);
  if (dragging_) {
    const Pt s = leaned(SHOULDER[i], lean_);
    const double sway = std::sin(nowMs / 160 + (paw == Paw::Left ? 0 : 1.4)) * 2.5;
    return {s.x + (paw == Paw::Left ? 4 : -4) + sway, s.y + 50};
  }
  if (paw == Paw::Right && (pose.right == RightPaw::Mouse || pose.right == RightPaw::Click)) {
    // fingers move onto the button being pressed; the wheel gets a rolling nudge
    Pt press{0, 0};
    if (pose.button == MouseButton::Left) press = {3.5, 3};
    else if (pose.button == MouseButton::Right) press = {-3.5, 3};
    else if (pose.button == MouseButton::Middle || pose.scroll != 0) press = {0, 3.5};
    const double roll = pose.scroll != 0 ? std::sin(nowMs / 40) * 1.2 : 0;
    return {mouseX_ + mouseOffset_.x + press.x, MOUSE.y - 5 + mouseOffset_.y + press.y + roll + (pose.right == RightPaw::Click ? 1.5 : 0)};
  }
  if (pose.swat && paw == Paw::Right) {
    const Pt s = leaned(SHOULDER[1], lean_);
    return {s.x + pose.gaze.x * MAX_REACH, s.y + 20 + pose.gaze.y * 30};
  }
  if (pose.yawning) {
    // stretch: both paws reach out over the keyboard
    return {REST[i].x + (paw == Paw::Left ? 10 : -10), REST[i].y + 22};
  }
  const bool onKey = paw == Paw::Left ? pose.left == LeftPaw::Key : pose.right == RightPaw::Key;
  if (onKey) {
    // in a shortcut the cat's left paw (outer side of the keyboard) holds the modifier
    const std::optional<int> code = pose.chord && paw == Paw::Left ? pose.chord : pose.pressed;
    if (code && keyCenters_.count(*code)) {
      const Pt k = keyCenters_.at(*code);
      return {k.x, k.y - 1};
    }
    return {REST[i].x, 187};
  }
  return leaned(REST[i], lean_);
}

/**
 * A foreleg as a filled shape: thick at the shoulder, slimmer at the wrist,
 * bowed a little at the elbow. The outline only runs from the elbow down,
 * so the top blends into the chest.
 */
void CatRenderer::drawArm(Image& frame, Paw paw, const Mat& body, const Pose& pose, Pt p) {
  const Pt s = SHOULDER[idx(paw)];
  const Pt c{(s.x + p.x) / 2 + (paw == Paw::Left ? 3 : -3), (s.y + p.y) / 2};
  // the leg's outline, `grow` units wider all round
  auto leg = [&](double grow) {
    std::vector<Pt> a, b;
    for (int i = 0; i <= 8; i++) {
      const double t = i / 8.0, u = 1 - t;
      const double x = u * u * s.x + 2 * u * t * c.x + t * t * p.x, y = u * u * s.y + 2 * u * t * c.y + t * t * p.y;
      const double tx = 2 * u * (c.x - s.x) + 2 * t * (p.x - c.x), ty = 2 * u * (c.y - s.y) + 2 * t * (p.y - c.y);
      const double len = std::max(std::hypot(tx, ty), 1e-9);
      const double w = lerp(9.5, 6.5, t) + std::sin(t * PI) * 0.8 + grow;  // forearm muscle
      a.push_back({x - ty / len * w, y + tx / len * w});
      b.push_back({x + ty / len * w, y - tx / len * w});
    }
    a.insert(a.end(), b.rbegin(), b.rend());
    // a rounded shoulder on top
    return Path().polygon(a).ellipse(s.x, s.y + 3, 9.5 + grow, 11 + grow);
  };
  // the leg grows out of the body: up top it takes the color of the fur right
  // behind it, then shades into the paw color along the forearm. No outline,
  // no shoulder patch, nothing looks glued on.
  const uint32_t pawColor = premul(gato::pawColor(skin_)), fur = premul(skin_.body);
  const Mat inv = body.inverse();
  const Sprite* torso = sprites_.get("torso");
  const Mat toTorso = torso ? (body * torso->toUnits()).inverse() : Mat();
  const double dx = p.x - s.x, dy = p.y - s.y, len2 = std::max(dx * dx + dy * dy, 1.0);
  // white paws end in a crisp sock; Siamese points darken gradually
  const bool pointed = skin_.pattern == Pattern::Pointed;
  const double from = pointed ? 0.35 : 0.5, span = pointed ? 0.4 : 0.12;
  const Shader color = [&](double x, double y) {
    const Pt u = inv.apply({x, y});
    const double t = smooth((((u.x - s.x) * dx + (u.y - s.y) * dy) / len2 - from) / span);
    if (t >= 1) return pawColor;
    uint32_t behind = fur;
    if (torso) {
      const Pt q = toTorso.apply({x, y});
      const uint32_t c = sampleColor(torso->img, q.x, q.y);
      behind = mixColor(fur, c | 0xff000000u, (c >> 24) / 255.0);  // the torso's edge blends into fur
    }
    return mixColor(behind, pawColor, t);
  };
  fillUnits(frame, leg(0), body, 0, nullptr, &color);

  // the paw squashes while it presses, more on a hard Enter
  const bool pressing = paw == Paw::Left ? pose.left == LeftPaw::Key : (pose.right == RightPaw::Key || pose.right == RightPaw::Click);
  const double sx = pressing && pose.slam ? 1.14 : pressing ? 1 : 0.86;
  const double sy = pressing && pose.slam ? 0.7 : pressing ? 0.83 : 1;
  sprite(frame, "paw", body * Mat::translate(p.x, p.y) * Mat::scale(sx, sy));
}

void CatRenderer::drawEye(Image& frame, const Mat& face, bool left, const Pose& pose) {
  const char* side = left ? "L" : "R";
  const bool winked = pose.wink == (left ? Wink::L : Wink::R);
  if (pose.eyes == Eyes::Happy) return sprite(frame, std::string("eye-happy-") + side, face);
  if (pose.eyes == Eyes::Closed || winked) return sprite(frame, std::string("eye-line-") + side, face);

  // the eye itself, in its own units: a circle of radius 9 at translate(x 70) scale(1.15)
  const Mat inner = Mat::translate(HEAD_SHIFT, 0) * Mat::scale(HEAD_SCALE);
  const double osx = dragging_ ? 1.12 : 1, osy = dragging_ ? 1.12 : pose.eyes == Eyes::Sleepy ? 0.45 : 1;
  const Mat eye = face * inner * Mat::translate(left ? 78 : 122, 70) * Mat::scale(1.15) * Mat::scale(osx, osy);
  fillUnits(frame, Path().ellipse(0, 0, 9, 9), eye, premul(left ? skin_.eyeLeft : skin_.eyeRight));

  // pupils follow the pointer; picked up, they shrink to dots
  const double px = dragging_ ? 0 : pose.gaze.x * 3.2, py = dragging_ ? 0 : pose.gaze.y * 2.6, r = dragging_ ? 5.6 * 0.6 : 5.6;
  const Mat inv = eye.inverse();
  const double pxPerUnit = scale_ * HEAD_SCALE * 1.15 * std::min(osx, osy);
  const Mask iris = [&](double x, double y) {
    const Pt u = inv.apply({x, y});
    return static_cast<float>(std::clamp((9 - std::hypot(u.x, u.y)) * pxPerUnit + 0.5, 0.0, 1.0));
  };
  fillUnits(frame, Path().ellipse(px, py, r, r), eye, premul(PUPIL), &iris);
}

// ---------------------------------------------------------------- the frame

void CatRenderer::render(const Pose& pose, double nowMs, Image& frame) {
  const double dt = lastFrame_ < 0 ? 16.667 : std::clamp(nowMs - lastFrame_, 1.0, 200.0);
  lastFrame_ = nowMs;
  if (frame.w != width_ || frame.h != height_) frame.resize(width_, height_);
  frame.clear(background_);

  if (pose.yawning && !wasYawning_) yawnAt_ = nowMs;
  wasYawning_ = pose.yawning;
  if (nowMs >= nextTwitch_) {
    // now and then one ear flicks, twice, like a real cat listening
    twitchEars(nowMs);
    nextTwitch_ = nowMs + 4000 + random01() * 6000;
  }

  // springs: the body leans to the mouse, the mouse follows the pointer
  const bool onMouse = pose.right == RightPaw::Mouse || pose.right == RightPaw::Click;
  lean_ = lerp(lean_, onMouse ? LEAN_DEG : 0, perFrame(0.18, dt));
  mouseOffset_ = approach(mouseOffset_, onMouse ? Pt{pose.gaze.x * 6, pose.gaze.y * 3} : Pt{0, 0}, perFrame(0.3, dt));

  drawKeyboard(frame, pose, nowMs);
  drawMouse(frame, pose, nowMs);

  // body: breathes, stretches in a yawn, crouches to hunt, lands when put down
  ease(crouch_, pose.hunting ? 1 : 0, dt, 60);
  const double breathT = wave(nowMs, pose.sleeping ? 5500 : 3600);
  double sx = 1 + 0.012 * breathT, sy = 1 + 0.024 * breathT, ty = 0;
  if (nowMs - yawnAt_ < 3000) keyframes({{0, 1, 1, 0}, {0.4, 1.03, 1.06, 0}, {0.7, 1.03, 1.06, 0}, {1, 1, 1, 0}}, (nowMs - yawnAt_) / 3000, sx, sy, ty);
  // hunting: a slight lean forward, never squashed
  ty = lerp(ty, 1, crouch_);
  const bool landing = nowMs - landingAt_ < 380;
  if (landing) {
    keyframes({{0, 1, 1, -4}, {0.45, 1.04, 0.94, 0}, {0.75, 0.98, 1.02, 0}, {1, 1, 1, 0}}, (nowMs - landingAt_) / 380, sx, sy, ty);
  }
  const Mat lean = view_ * Mat::around(PIVOT.x, PIVOT.y, Mat::rotate(lean_));
  const Mat body = lean * Mat::around(PIVOT.x, PIVOT.y, Mat::translate(0, ty) * Mat::scale(sx, sy));

  // tail: lazy sway, quick wag when excited, hanging when picked up
  const double sway = dragging_ ? 28 : 9 * wave(nowMs, 3200);
  const double wag = pose.excited ? -8 * wave(nowMs, 350) : 0;
  sprite(frame, "tail", lean * Mat::around(TAIL_ORIGIN.x, TAIL_ORIGIN.y, Mat::rotate(sway) * Mat::rotate(wag)));

  sprite(frame, "torso", body);

  // head follows the pointer, droops asleep, leans into petting
  double hx = pose.gaze.x * 2.5, hy = pose.gaze.y * 1.6, hr = pose.gaze.x * 5;
  if (pose.sleeping) { hx = 0; hy = 5; hr = -6; }
  if (pose.petting) { hx = 0; hy = -2; hr = 5; }
  ease(headX_, hx, dt, 90);
  ease(headY_, hy, dt, 90);
  ease(headRot_, hr, dt, 90);
  ease(faceX_, 2.5 + pose.gaze.x * 5, dt, 90);
  ease(faceY_, pose.gaze.y * 2.5, dt, 90);
  const Mat head = body * Mat::around(HEAD_ORIGIN.x, HEAD_ORIGIN.y, Mat::translate(headX_, headY_) * Mat::rotate(headRot_));
  headMat_ = head;

  // ears: flick, flatten to hunt, fold back when picked up or petted
  const double sinceTwitch = nowMs - twitchAt_;
  const bool twitching = (sinceTwitch >= 0 && sinceTwitch < 130) || (sinceTwitch >= 260 && sinceTwitch < 390);
  for (int i = 0; i < 2; i++) {
    const double dir = i == 0 ? -1 : 1;
    double rot = 0, y = 0;
    if (twitching && twitchLeft_ == (i == 0)) rot = 12 * dir;
    // ears only tilt a little: turned far out they flatten the head
    if (pose.petting) { rot = 7 * dir; y = 1; }
    if (pose.hunting) { rot = -4 * dir; y = 0; }  // pricked forward, alert
    if (dragging_) { rot = 10 * dir; y = 0; }
    ease(earRot_[i], rot, dt, 40);
    ease(earY_[i], y, dt, 40);
    sprite(frame, i == 0 ? "ear-left" : "ear-right",
           head * Mat::around(EAR_ORIGIN[i].x, EAR_ORIGIN[i].y, Mat::rotate(earRot_[i]) * Mat::translate(0, earY_[i] * HEAD_SCALE)));
  }
  sprite(frame, "head-base", head);

  // the face slides a little towards the pointer, inside the head's outline
  const Mat face = head * Mat::translate(faceX_ * HEAD_SCALE, faceY_ * HEAD_SCALE);
  if (const Sprite* shape = sprites_.get("head-shape")) {
    const Mat inv = (head * shape->toUnits()).inverse();
    const Mask clip = [&](double x, double y) {
      const Pt p = inv.apply({x, y});
      return sampleAlpha(shape->img, p.x, p.y);
    };
    sprite(frame, "face", face, 1.f, &clip);
  }
  drawEye(frame, face, true, pose);
  drawEye(frame, face, false, pose);
  sprite(frame, "nose", face);
  if (pose.yawning) {
    // the mouth opens wide, from its top edge
    if (const Sprite* y = sprites_.get("yawn")) {
      double ysx = 1, ysy = 0.2, unused;
      keyframes({{0, 1, 0.2, 0}, {0.4, 1.1, 1.3, 0}, {0.7, 1.1, 1.3, 0}, {1, 1, 0.2, 0}}, (nowMs - yawnAt_) / 3000, ysx, ysy, unused);
      const double ox = y->ux + y->img.w / y->sx / 2, oy = y->uy;
      sprite(frame, "yawn", face * Mat::around(ox, oy, Mat::scale(ysx, ysy)));
    }
  } else {
    sprite(frame, "mouth", face);
  }
  // whiskers flick now and then, spread when excited
  for (int i = 0; i < 2; i++) {
    const double dir = i == 0 ? 1 : -1;
    double angle;
    if (pose.excited) {
      angle = -5 * dir;
    } else {
      const double phase = std::fmod(nowMs - (i == 0 ? 0 : 1300) + 7000 * 10, 7000) / 7000;
      angle = phase < 0.9 ? 0 : phase < 0.94 ? 6 * dir * smooth((phase - 0.9) / 0.04) : 6 * dir * smooth((1 - phase) / 0.06);
    }
    sprite(frame, i == 0 ? "whiskers-l" : "whiskers-r", face * Mat::around(WHISKER_ORIGIN[i].x, WHISKER_ORIGIN[i].y, Mat::rotate(angle)));
  }

  // feet hang when picked up
  ease(feetY_, dragging_ ? 5 : 0, dt, 60);
  sprite(frame, "feet", body * Mat::translate(0, feetY_));

  // forelegs: springs towards the keys, the mouse or the rest spot
  bool pawsMoving = false;
  for (Paw paw : {Paw::Left, Paw::Right}) {
    const int i = idx(paw);
    const bool pressing = paw == Paw::Left ? pose.left == LeftPaw::Key : (pose.right == RightPaw::Key || pose.right == RightPaw::Click);
    const Pt target = withinReach(SHOULDER[i], leaned(pawTarget(paw, pose, nowMs), -lean_));
    paws_[i] = approach(paws_[i], target, perFrame(pressing ? 0.55 : 0.22, dt));
    // the paw lifts while travelling and lands on the target: an arc, not a slide
    const double gap = dist(target, paws_[i]);
    pawsMoving = pawsMoving || gap > 0.2;
    const double lift = std::min(6.0, gap * 0.35);
    drawArm(frame, paw, body, pose, {paws_[i].x, paws_[i].y - lift});
  }

  // asleep: z z; petted: a heart
  if (pose.sleeping) {
    const double w = wave(nowMs, 2400);
    sprite(frame, "zzz", view_ * Mat::translate(0, -5 * w), static_cast<float>(1 - 0.4 * w));
  }
  if (pose.petting) {
    const double w = wave(nowMs, 1000);
    sprite(frame, "heart", view_ * Mat::translate(0, -5 * w), static_cast<float>(1 - 0.4 * w));
  }

  // keycap bubbles rise and fade
  bubbles_.erase(std::remove_if(bubbles_.begin(), bubbles_.end(), [&](const LiveBubble& b) { return nowMs - b.born >= 650; }), bubbles_.end());
  for (const auto& b : bubbles_) {
    const double t = (nowMs - b.born) / 650, e = 1 - (1 - t) * (1 - t);
    const float opacity = static_cast<float>(1 - e);
    const double width = 8 + codepoints(b.label).size() * 5.2;
    const Mat m = view_ * Mat::translate(0, -16 * e);
    const double x = b.x - width / 2, y = b.y;
    fillUnits(frame, Path().roundRect(x - 0.5, y - 0.5, width + 1, 13, 3.5).roundRect(x + 0.5, y + 0.5, width - 1, 11, 2.5, true), m, premul(0x222222, opacity));
    fillUnits(frame, Path().roundRect(x + 0.5, y + 0.5, width - 1, 11, 2.5), m, premul(0xffffff, opacity));
    const double textScale = scale_ / 2;  // the bubble font was drawn at 2 px per unit
    const Pt at = m.apply({b.x, y + 9});
    bubbleFont_.draw(frame, b.label, at.x - bubbleFont_.measure(b.label) * textScale / 2, at.y, premul(0x222222, opacity), textScale);
  }

  // how soon the next frame is needed
  const bool settled = std::abs(lean_ - (onMouse ? LEAN_DEG : 0)) < 0.05 && std::abs(headX_ - hx) < 0.05 && std::abs(headY_ - hy) < 0.05 &&
                       std::abs(feetY_ - (dragging_ ? 5 : 0)) < 0.05 && std::abs(earRot_[0]) + std::abs(earRot_[1]) < 0.1;
  const bool busy = dragging_ || landing || !bubbles_.empty() || pawsMoving || !settled || pose.excited || pose.hunting || pose.petting ||
                    pose.yawning || pose.slam || pose.scroll != 0 || pose.left == LeftPaw::Key || pose.right == RightPaw::Key ||
                    pose.right == RightPaw::Click;
  pace_ = busy ? Pace::Busy : pose.sleeping ? Pace::Asleep : Pace::Idle;
}

}  // namespace gato
