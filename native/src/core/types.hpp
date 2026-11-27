#pragma once
// Plain data shared by every layer. No OS, no drawing.
#include <cstdint>
#include <optional>
#include <string>

namespace gato {

enum class MouseButton { Left, Right, Middle };

/** What a physical keyboard has, as reported by the device */
struct KeyboardFeatures {
  bool functionRow = true;
  bool numpad = true;
  bool fn = false;
  bool operator==(const KeyboardFeatures& o) const { return functionRow == o.functionRow && numpad == o.numpad && fn == o.fn; }
  bool operator!=(const KeyboardFeatures& o) const { return !(*this == o); }
};

enum class Pattern { Solid, Tuxedo, Bicolor, Tabby, Calico, Pointed };

/** A coat. The drawing itself comes from its sprites; these colors are the
 *  parts drawn live (eyes, legs). Colors are 0xRRGGBB. */
struct Skin {
  std::string id;
  std::string name;
  Pattern pattern;
  uint32_t body, belly, accent, eyeLeft, eyeRight;
};

enum class Size { Big, Small, Mini };

struct Settings {
  std::string skinId = "preto-verde";
  Size size = Size::Small;
  uint32_t keyboardColor = 0xefe9dc;
  uint32_t mouseColor = 0xefe9dc;
  bool privacy = false;
  bool greenScreen = false;
  std::optional<std::pair<int, int>> position;
};

/** What the cat reacts to, already independent of OS and device */
struct CatInput {
  enum class Type { Key, Button, Wheel, Gaze } type;
  int code = 0;
  std::string label;
  bool down = false;
  MouseButton which = MouseButton::Left;
  int direction = 0;
  double x = 0, y = 0;
};

/** The keyboard and pointer the user is touching right now */
struct Devices {
  struct { std::string name; bool laptop = false; KeyboardFeatures features; } keyboard;
  struct { std::string name; bool touchpad = false; } pointer;
};

}  // namespace gato
