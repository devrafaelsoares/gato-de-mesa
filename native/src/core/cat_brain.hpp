#pragma once
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "keyboard.hpp"
#include "types.hpp"

namespace gato {

enum class Eyes { Open, Closed, Happy, Sleepy };
enum class LeftPaw { Rest, Key };
enum class RightPaw { Rest, Key, Mouse, Click };
enum class Wink { Off, L, R };

struct Gaze { double x = 0, y = 0; };

struct Pose {
  LeftPaw left = LeftPaw::Rest;
  RightPaw right = RightPaw::Rest;
  Eyes eyes = Eyes::Open;
  Gaze gaze;
  /** Scan code of the key being pressed */
  std::optional<int> pressed;
  bool sleeping = false;
  bool petting = false;
  /** Typing fast or being petted: livelier tail */
  bool excited = false;
  /** Mouse button held down, and the wheel spinning up (1) or down (-1) */
  std::optional<MouseButton> button;
  int scroll = 0;
  /** Keyboard lights, following the lock keys */
  bool capsLock = false;
  bool numLock = true;
  /** Modifier held in a shortcut like Ctrl+C: the other paw presses it */
  std::optional<int> chord;
  /** Enter hit hard */
  bool slam = false;
  /** Yawning and stretching just before falling asleep */
  bool yawning = false;
  /** The pointer darts close by: crouch, and now and then a swat */
  bool hunting = false;
  bool swat = false;
  /** One eye closed for a moment */
  Wink wink = Wink::Off;
};

/** A label to pop up, from the paw that typed or from the mouse */
struct Bubble { std::string label; Paw paw; bool atMouse; };

/** The cat's behaviour, free of rendering and OS details. Time is injected (ms). */
class CatBrain {
 public:
  using Clock = std::function<double()>;
  using Random = std::function<double()>;
  CatBrain(Clock now, Random random);

  /** Feeds one input; returns what to pop up as a bubble, if anything */
  std::optional<Bubble> handle(const CatInput& input);
  /** One stroke of the pointer over the head; enough strokes in a row is petting */
  void stroke();
  Pose pose(bool privacy);

 private:
  void noteDart(const Gaze& gaze, double t);

  Clock now_;
  Random random_;
  double leftUntil_ = 0, rightUntil_ = 0, clickUntil_ = 0, mouseUntil_ = 0;
  std::optional<std::pair<int, double>> pressed_;  // code, until
  Gaze gaze_;
  double lastInput_;
  double nextBlink_;
  Wink wink_ = Wink::Off;
  double blinkUntil_ = 0;
  double petUntil_ = 0;
  std::vector<double> strokes_;
  Paw alternate_ = Paw::Left;
  std::vector<double> recentKeys_;
  std::optional<MouseButton> held_;
  std::optional<std::pair<int, double>> scroll_;  // direction, until
  bool capsLock_ = false;
  bool numLock_ = true;
  std::set<int> modifiers_;
  std::optional<std::pair<int, double>> chord_;  // code, until
  double slamUntil_ = 0;
  struct { double x = 0, y = 0, t = 0; } lastGaze_;
  std::vector<double> darts_;
  double huntUntil_ = 0, swatUntil_ = 0, nextSwat_ = 0;
};

/** Unit-length (at most) direction from the cat's head to the pointer */
Gaze gazeToward(double px, double py, double headX, double headY);

}  // namespace gato
