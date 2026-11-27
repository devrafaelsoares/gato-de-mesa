#pragma once
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>

#include <memory>
#include <string>
#include <vector>

#include "../../app/ports.hpp"
#include "../../render/assets.hpp"

namespace gato {

class CatApp;

/** Right-click menu drawn by the app: popups that stack to the right */
class X11Menu {
 public:
  X11Menu(Display* dpy, Visual* visual, Colormap cmap, const std::string& assetDir);
  ~X11Menu();
  void open(std::vector<MenuItem> items, int x, int y, double nowMs);
  void close();
  bool isOpen() const { return !levels_.empty(); }
  bool owns(Window w) const;
  void handle(const XEvent& e, double nowMs);
  /** Closes a menu the pointer left long ago (Wayland apps never report clicks to it) */
  void tick(double nowMs);

 private:
  struct Level {
    std::vector<MenuItem>* items;
    Window win;
    int x, y, w, h;
    std::vector<int> rowY;
    int hover = -1;
    Image img;
  };
  void openLevel(std::vector<MenuItem>* items, int x, int y);
  void draw(Level& level);
  int levelAt(int rx, int ry) const;
  int rowAt(const Level& level, int ry) const;
  void activate(Level& level, int row);

  Display* dpy_;
  Visual* visual_;
  Colormap cmap_;
  GC gc_ = nullptr;
  Font font_;
  std::vector<MenuItem> items_;
  std::vector<std::unique_ptr<Level>> levels_;
  double openedAt_ = 0, leftAt_ = -1;
};

/** The cat's window on X11 (and XWayland): ARGB, borderless, always on top */
class X11Window : public CatWindow {
 public:
  explicit X11Window(const std::string& assetDir);
  ~X11Window() override;
  bool ok() const { return win_ != 0; }
  int fd() const { return ConnectionNumber(dpy_); }
  bool running() const { return running_; }
  void stopRunning() { running_ = false; }
  /** Dispatches every pending X event to the app or the menu */
  void processEvents(CatApp& app, double nowMs);
  X11Menu& menu() { return *menu_; }

  void resize(int width, int height) override;
  void move(int x, int y) override;
  Point position() override { return {static_cast<double>(x_), static_cast<double>(y_)}; }
  Rect desktop() override;
  Rect workArea(Point near) override;
  void present(const Image& frame) override;
  void popupMenu(std::vector<MenuItem> items, int screenX, int screenY) override;
  std::optional<Point> cursor() override;
  void quit() override { running_ = false; }

 private:
  void createImage();
  void destroyImage();
  void put();
  void setFixedSize();
  void refreshPosition();

  Display* dpy_ = nullptr;
  int screen_ = 0;
  Window root_ = 0, win_ = 0;
  Visual* visual_ = nullptr;
  Colormap cmap_ = 0;
  GC gc_ = nullptr;
  XImage* image_ = nullptr;
  XShmSegmentInfo shm_{};
  bool useShm_ = false;
  int w_ = 1, h_ = 1, x_ = 0, y_ = 0;
  bool running_ = true;
  bool hasFrame_ = false;
  std::unique_ptr<X11Menu> menu_;
};

}  // namespace gato
