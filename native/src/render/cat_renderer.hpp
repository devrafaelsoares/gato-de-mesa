#pragma once
#include <map>
#include <string>
#include <vector>

#include "../core/cat_brain.hpp"
#include "../core/keyboard.hpp"
#include "../core/types.hpp"
#include "assets.hpp"
#include "canvas.hpp"

namespace gato {

/** How often the cat wants to be redrawn right now */
enum class Pace { Busy, Idle, Asleep };

/**
 * Draws the cat and the desk from sprites plus a few live shapes (legs,
 * eyes, keyboard, mouse). Knows nothing about input or timing rules.
 */
class CatRenderer {
 public:
  explicit CatRenderer(std::string assetDir);

  /** Window width in pixels; reloads the coat at that scale when it changed */
  bool setup(const Skin& skin, int widthPx);
  void setDesk(uint32_t keyboardColor, uint32_t mouseColor, uint32_t background);
  void showDevices(const Devices& devices);
  void setDragging(bool on, double nowMs);
  void twitchEars(double nowMs);
  void bubble(const Bubble& b, double nowMs);

  /** Advances the animation to nowMs and draws into the frame */
  void render(const Pose& pose, double nowMs, Image& frame);
  Pace pace() const { return pace_; }
  /** Whether a point of the window (pixels) is over the head, for petting */
  bool overHead(double x, double y) const;
  int width() const { return width_; }
  int height() const { return height_; }

 private:
  struct Arm { Pt paw; };
  struct LiveBubble { std::string label; double x, y, born; bool atMouse; };

  Pt pawTarget(Paw paw, const Pose& pose, double nowMs) const;
  void drawKeyboard(Image& frame, const Pose& pose, double nowMs);
  void buildKeyboardCache();
  void drawMouse(Image& frame, const Pose& pose, double nowMs);
  void drawMouseShape(Image& frame, const Mat& place, const Pose& pose, double nowMs);
  void drawArm(Image& frame, Paw paw, const Mat& body, const Pose& pose, Pt p);
  void drawEye(Image& frame, const Mat& face, bool left, const Pose& pose);
  void sprite(Image& frame, const std::string& name, const Mat& m, float opacity = 1.f, const Mask* mask = nullptr);

  std::string assetDir_;
  SpriteSet sprites_;
  Font bubbleFont_;
  std::string coat_;
  Skin skin_{};
  int width_ = 0, height_ = 0;
  double scale_ = 1;  // pixels per unit
  Mat view_;          // units -> pixels

  uint32_t keyboardColor_ = 0xefe9dc, mouseColor_ = 0xefe9dc, background_ = 0;
  KeyboardFeatures features_;
  bool laptop_ = false, touchpad_ = false;
  std::map<int, Pt> keyCenters_;
  struct Cap { int code; double x, y, w, h; bool accent; };
  std::vector<Cap> caps_;
  double mouseX_ = 30;
  Image keyboardCache_;
  bool keyboardDirty_ = true;
  bool numLock_ = true, capsLock_ = false;
  Image mouseCache_;
  int mouseKey_ = -1;
  uint32_t mouseColorCached_ = 0;

  // live state
  Pt paws_[2];
  double lean_ = 0;
  Pt mouseOffset_{0, 0};
  double lastFrame_ = -1;
  bool dragging_ = false;
  double landingAt_ = -1e9, yawnAt_ = -1e9, slamAt_ = -1e9;
  bool wasYawning_ = false, wasSlam_ = false;
  double twitchAt_ = -1e9, nextTwitch_ = 0;
  bool twitchLeft_ = true;
  // smoothed transforms (CSS transitions)
  double headX_ = 0, headY_ = 0, headRot_ = 0, faceX_ = 2.5, faceY_ = 0;
  double earRot_[2] = {0, 0}, earY_[2] = {0, 0};
  double feetY_ = 0, crouch_ = 0;
  Mat headMat_;  // units -> pixels for the head, kept for petting hits
  std::vector<LiveBubble> bubbles_;
  Pace pace_ = Pace::Busy;
};

}  // namespace gato
