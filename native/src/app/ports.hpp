#pragma once
// What the application needs from the operating system.
#include <functional>
#include <optional>
#include <string>

#include "../core/types.hpp"

namespace gato {

/** Raw input as an OS adapter sees it: scan codes and pointer motion */
struct RawInput {
  enum class Kind { Key, Button, Wheel, Move, Point, Device } kind;
  int code = 0;
  bool down = false;
  MouseButton which = MouseButton::Left;
  int direction = 0;
  double dx = 0, dy = 0;  // Move
  double x = 0, y = 0;    // Point
  // Device: the user switched to this keyboard or pointer; builtIn = part of a laptop
  bool keyboard = false;
  std::string name;
  bool builtIn = false;
  std::optional<KeyboardFeatures> features;
};

class InputSource {
 public:
  using Emit = std::function<void(const RawInput&)>;
  virtual ~InputSource() = default;
  virtual void start(Emit emit) = 0;
  virtual void stop() = 0;
};

class SettingsStore {
 public:
  virtual ~SettingsStore() = default;
  virtual Settings load() = 0;
  virtual void save(const Settings& settings) = 0;
};

struct Point { double x, y; };
struct Rect { double x, y, width, height; };

}  // namespace gato

#include <vector>

#include "../render/canvas.hpp"
#include "menu.hpp"

namespace gato {

/** The cat's window, as each OS provides it. Positions are screen pixels. */
class CatWindow {
 public:
  virtual ~CatWindow() = default;
  virtual void resize(int width, int height) = 0;
  virtual void move(int x, int y) = 0;
  virtual Point position() = 0;
  /** The whole desktop, all monitors */
  virtual Rect desktop() = 0;
  /** The work area of the monitor under this point, without panels */
  virtual Rect workArea(Point around) = 0;
  virtual void present(const Image& frame) = 0;
  virtual void popupMenu(std::vector<MenuItem> items, int screenX, int screenY) = 0;
  /** Where the pointer is, when the platform can tell */
  virtual std::optional<Point> cursor() = 0;
  virtual void quit() = 0;
};

}  // namespace gato
