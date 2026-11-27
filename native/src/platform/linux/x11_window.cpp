#include "x11_window.hpp"

#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <algorithm>
#include <cstring>

#include "../../app/cat_app.hpp"

namespace gato {
namespace {

Atom atom(Display* d, const char* name) { return XInternAtom(d, name, False); }

/** Asks the window manager to add (1) or remove (0) a _NET_WM_STATE */
void netState(Display* d, Window root, Window w, const char* state, long add) {
  XEvent e{};
  e.xclient.type = ClientMessage;
  e.xclient.window = w;
  e.xclient.message_type = atom(d, "_NET_WM_STATE");
  e.xclient.format = 32;
  e.xclient.data.l[0] = add;
  e.xclient.data.l[1] = static_cast<long>(atom(d, state));
  e.xclient.data.l[3] = 1;
  XSendEvent(d, root, False, SubstructureRedirectMask | SubstructureNotifyMask, &e);
}

XImage* plainImage(Display* d, Visual* v, int w, int h) {
  char* data = static_cast<char*>(std::calloc(static_cast<size_t>(w) * h, 4));
  return XCreateImage(d, v, 32, ZPixmap, 0, data, w, h, 32, 0);
}

void copyInto(XImage* img, const Image& frame) {
  for (int y = 0; y < frame.h && y < img->height; y++)
    std::memcpy(img->data + static_cast<size_t>(y) * img->bytes_per_line, &frame.px[static_cast<size_t>(y) * frame.w],
                static_cast<size_t>(std::min(frame.w, img->width)) * 4);
}

// Menu look
constexpr int ROW = 26, SEPARATOR = 9, PAD = 5, TEXT_LEFT = 30, MIN_WIDTH = 200;
constexpr uint32_t MENU_BG = 0xfbfaf7, MENU_BORDER = 0xc9c2b6, MENU_HOVER = 0xebe3d5, MENU_TEXT = 0x1d1d1f, MENU_MUTED = 0x8b8780;

bool actionable(const MenuItem& item) {
  return item.kind != MenuItem::Kind::Separator && item.kind != MenuItem::Kind::Label;
}

}  // namespace

// ---------------------------------------------------------------- menu

X11Menu::X11Menu(Display* dpy, Visual* visual, Colormap cmap, const std::string& assetDir) : dpy_(dpy), visual_(visual), cmap_(cmap) {
  font_.load(assetDir, "glyphs-menu");
}

X11Menu::~X11Menu() {
  close();
  if (gc_) XFreeGC(dpy_, gc_);
}

bool X11Menu::owns(Window w) const {
  return std::any_of(levels_.begin(), levels_.end(), [&](const auto& l) { return l->win == w; });
}

void X11Menu::open(std::vector<MenuItem> items, int x, int y, double nowMs) {
  close();
  items_ = std::move(items);
  openedAt_ = nowMs;
  leftAt_ = -1;
  openLevel(&items_, x, y);
  // clicks anywhere else close the menu
  XGrabPointer(dpy_, levels_.front()->win, True, ButtonPressMask | ButtonReleaseMask | PointerMotionMask | EnterWindowMask | LeaveWindowMask,
               GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
  XGrabKeyboard(dpy_, levels_.front()->win, True, GrabModeAsync, GrabModeAsync, CurrentTime);
  XFlush(dpy_);
}

void X11Menu::openLevel(std::vector<MenuItem>* items, int x, int y) {
  auto level = std::make_unique<Level>();
  level->items = items;
  int width = MIN_WIDTH, top = PAD;
  for (const auto& item : *items) {
    level->rowY.push_back(top);
    top += item.kind == MenuItem::Kind::Separator ? SEPARATOR : ROW;
    width = std::max(width, static_cast<int>(font_.measure(item.label)) + TEXT_LEFT + 34);
  }
  level->w = width;
  level->h = top + PAD;
  // keep it on screen
  const int sw = DisplayWidth(dpy_, DefaultScreen(dpy_)), sh = DisplayHeight(dpy_, DefaultScreen(dpy_));
  level->x = std::clamp(x, 0, std::max(0, sw - level->w));
  level->y = std::clamp(y, 0, std::max(0, sh - level->h));

  XSetWindowAttributes a{};
  a.colormap = cmap_;
  a.border_pixel = 0;
  a.background_pixel = 0;
  a.override_redirect = True;
  a.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | EnterWindowMask | LeaveWindowMask | KeyPressMask;
  level->win = XCreateWindow(dpy_, DefaultRootWindow(dpy_), level->x, level->y, level->w, level->h, 0, 32, InputOutput, visual_,
                             CWColormap | CWBorderPixel | CWBackPixel | CWOverrideRedirect | CWEventMask, &a);
  const Atom popup = atom(dpy_, "_NET_WM_WINDOW_TYPE_POPUP_MENU");
  XChangeProperty(dpy_, level->win, atom(dpy_, "_NET_WM_WINDOW_TYPE"), XA_ATOM, 32, PropModeReplace, reinterpret_cast<const unsigned char*>(&popup), 1);
  if (!gc_) gc_ = XCreateGC(dpy_, level->win, 0, nullptr);
  XMapRaised(dpy_, level->win);
  draw(*level);
  levels_.push_back(std::move(level));
}

void X11Menu::close() {
  if (levels_.empty()) return;
  XUngrabPointer(dpy_, CurrentTime);
  XUngrabKeyboard(dpy_, CurrentTime);
  for (auto& l : levels_) XDestroyWindow(dpy_, l->win);
  levels_.clear();
  XFlush(dpy_);
}

void X11Menu::draw(Level& level) {
  Image& img = level.img;
  img.resize(level.w, level.h);
  fill(img, Path().roundRect(0, 0, level.w, level.h, 7), premul(MENU_BORDER));
  fill(img, Path().roundRect(1, 1, level.w - 2, level.h - 2, 6), premul(MENU_BG));
  for (size_t i = 0; i < level.items->size(); i++) {
    const MenuItem& item = (*level.items)[i];
    const int y = level.rowY[i];
    if (item.kind == MenuItem::Kind::Separator) {
      fill(img, Path().rect(8, y + SEPARATOR / 2.0 - 0.5, level.w - 16, 1), premul(MENU_BORDER));
      continue;
    }
    if (static_cast<int>(i) == level.hover && actionable(item)) fill(img, Path().roundRect(4, y, level.w - 8, ROW, 5), premul(MENU_HOVER));
    const uint32_t color = premul(item.kind == MenuItem::Kind::Label ? MENU_MUTED : MENU_TEXT);
    if (item.checked) font_.draw(img, "✓", 11, y + 18, color);
    font_.draw(img, item.label, TEXT_LEFT, y + 18, color);
    if (item.kind == MenuItem::Kind::Submenu) font_.draw(img, "▸", level.w - 20, y + 18, color);
  }
  XImage* xi = plainImage(dpy_, visual_, level.w, level.h);
  copyInto(xi, img);
  XPutImage(dpy_, level.win, gc_, xi, 0, 0, 0, 0, level.w, level.h);
  XDestroyImage(xi);
  XFlush(dpy_);
}

int X11Menu::levelAt(int rx, int ry) const {
  for (int i = static_cast<int>(levels_.size()) - 1; i >= 0; i--) {
    const Level& l = *levels_[i];
    if (rx >= l.x && rx < l.x + l.w && ry >= l.y && ry < l.y + l.h) return i;
  }
  return -1;
}

int X11Menu::rowAt(const Level& level, int ry) const {
  const int y = ry - level.y;
  for (size_t i = 0; i < level.items->size(); i++) {
    const int h = (*level.items)[i].kind == MenuItem::Kind::Separator ? SEPARATOR : ROW;
    if (y >= level.rowY[i] && y < level.rowY[i] + h) return static_cast<int>(i);
  }
  return -1;
}

void X11Menu::activate(Level& level, int row) {
  if (row < 0) return;
  const MenuItem& item = (*level.items)[row];
  if (!actionable(item) || item.kind == MenuItem::Kind::Submenu) return;
  const auto run = item.run;  // the menu is gone before it runs
  close();
  if (run) run();
}

void X11Menu::handle(const XEvent& e, double nowMs) {
  if (levels_.empty()) return;
  switch (e.type) {
    case Expose:
      for (auto& l : levels_)
        if (l->win == e.xexpose.window) draw(*l);
      return;
    case KeyPress:
      if (XLookupKeysym(const_cast<XKeyEvent*>(&e.xkey), 0) == XK_Escape) close();
      return;
    case EnterNotify: leftAt_ = -1; return;
    case LeaveNotify: leftAt_ = nowMs; return;
    case MotionNotify: {
      const int li = levelAt(e.xmotion.x_root, e.xmotion.y_root);
      if (li < 0) return;
      leftAt_ = -1;
      Level& level = *levels_[li];
      const int row = rowAt(level, e.xmotion.y_root);
      if (row == level.hover) return;
      level.hover = row;
      // a deeper popup follows the hovered row
      while (static_cast<int>(levels_.size()) > li + 1) {
        XDestroyWindow(dpy_, levels_.back()->win);
        levels_.pop_back();
      }
      draw(level);
      if (row >= 0 && (*level.items)[row].kind == MenuItem::Kind::Submenu) {
        const int sw = DisplayWidth(dpy_, DefaultScreen(dpy_));
        int x = level.x + level.w - 4;
        auto* children = &(*level.items)[row].children;
        // no room on the right: open to the left
        if (x + MIN_WIDTH > sw) x = level.x - MIN_WIDTH - 40;
        openLevel(children, x, level.y + level.rowY[row] - PAD);
      }
      return;
    }
    case ButtonPress: {
      const int li = levelAt(e.xbutton.x_root, e.xbutton.y_root);
      if (li < 0) return close();
      activate(*levels_[li], rowAt(*levels_[li], e.xbutton.y_root));
      return;
    }
    case ButtonRelease: {
      // press on the cat, drag onto an item, release: that picks it too
      if (nowMs - openedAt_ < 350) return;
      const int li = levelAt(e.xbutton.x_root, e.xbutton.y_root);
      if (li >= 0) activate(*levels_[li], rowAt(*levels_[li], e.xbutton.y_root));
      return;
    }
  }
}

void X11Menu::tick(double nowMs) {
  if (!levels_.empty() && leftAt_ >= 0 && nowMs - leftAt_ > 3000) close();
}

// ---------------------------------------------------------------- window

X11Window::X11Window(const std::string& assetDir) {
  dpy_ = XOpenDisplay(nullptr);
  if (!dpy_) return;
  screen_ = DefaultScreen(dpy_);
  root_ = RootWindow(dpy_, screen_);
  XVisualInfo vi;
  if (!XMatchVisualInfo(dpy_, screen_, 32, TrueColor, &vi)) return;  // no transparency available
  visual_ = vi.visual;
  cmap_ = XCreateColormap(dpy_, root_, visual_, AllocNone);

  XSetWindowAttributes a{};
  a.colormap = cmap_;
  a.border_pixel = 0;
  a.background_pixel = 0;
  a.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | StructureNotifyMask;
  win_ = XCreateWindow(dpy_, root_, 0, 0, w_, h_, 0, 32, InputOutput, visual_, CWColormap | CWBorderPixel | CWBackPixel | CWEventMask, &a);

  XStoreName(dpy_, win_, "Gato de Mesa");
  XClassHint cls{const_cast<char*>("gato-de-mesa"), const_cast<char*>("Gato de Mesa")};
  XSetClassHint(dpy_, win_, &cls);
  // no title bar or border
  const long motif[5] = {2, 0, 0, 0, 0};
  XChangeProperty(dpy_, win_, atom(dpy_, "_MOTIF_WM_HINTS"), atom(dpy_, "_MOTIF_WM_HINTS"), 32, PropModeReplace,
                  reinterpret_cast<const unsigned char*>(motif), 5);
  // above other windows, on every workspace, out of the taskbar
  const Atom states[4] = {atom(dpy_, "_NET_WM_STATE_ABOVE"), atom(dpy_, "_NET_WM_STATE_STICKY"), atom(dpy_, "_NET_WM_STATE_SKIP_TASKBAR"),
                          atom(dpy_, "_NET_WM_STATE_SKIP_PAGER")};
  XChangeProperty(dpy_, win_, atom(dpy_, "_NET_WM_STATE"), XA_ATOM, 32, PropModeReplace, reinterpret_cast<const unsigned char*>(states), 4);
  const Atom utility = atom(dpy_, "_NET_WM_WINDOW_TYPE_UTILITY");
  XChangeProperty(dpy_, win_, atom(dpy_, "_NET_WM_WINDOW_TYPE"), XA_ATOM, 32, PropModeReplace, reinterpret_cast<const unsigned char*>(&utility), 1);

  gc_ = XCreateGC(dpy_, win_, 0, nullptr);
  int major, minor;
  Bool pixmaps;
  useShm_ = XShmQueryVersion(dpy_, &major, &minor, &pixmaps);
  menu_ = std::make_unique<X11Menu>(dpy_, visual_, cmap_, assetDir);
}

X11Window::~X11Window() {
  menu_.reset();
  destroyImage();
  if (dpy_) {
    if (gc_) XFreeGC(dpy_, gc_);
    if (win_) XDestroyWindow(dpy_, win_);
    XCloseDisplay(dpy_);
  }
}

void X11Window::setFixedSize() {
  XSizeHints* hints = XAllocSizeHints();
  hints->flags = PMinSize | PMaxSize | USPosition | PPosition;
  hints->min_width = hints->max_width = w_;
  hints->min_height = hints->max_height = h_;
  hints->x = x_;
  hints->y = y_;
  XSetWMNormalHints(dpy_, win_, hints);
  XFree(hints);
}

void X11Window::resize(int width, int height) {
  const bool first = !hasFrame_ && image_ == nullptr;
  w_ = width;
  h_ = height;
  setFixedSize();
  XResizeWindow(dpy_, win_, w_, h_);
  destroyImage();
  createImage();
  if (first) {
    XMapWindow(dpy_, win_);
    // some window managers only honour these once the window is mapped
    netState(dpy_, root_, win_, "_NET_WM_STATE_ABOVE", 1);
    netState(dpy_, root_, win_, "_NET_WM_STATE_SKIP_TASKBAR", 1);
  }
  XFlush(dpy_);
}

void X11Window::move(int x, int y) {
  x_ = x;
  y_ = y;
  XMoveWindow(dpy_, win_, x, y);
  XFlush(dpy_);
}

void X11Window::refreshPosition() {
  int x, y;
  Window child;
  if (XTranslateCoordinates(dpy_, win_, root_, 0, 0, &x, &y, &child)) {
    x_ = x;
    y_ = y;
  }
}

Rect X11Window::desktop() { return {0, 0, static_cast<double>(DisplayWidth(dpy_, screen_)), static_cast<double>(DisplayHeight(dpy_, screen_))}; }

Rect X11Window::workArea(Point) {
  Atom type;
  int format;
  unsigned long count, after;
  unsigned char* data = nullptr;
  Rect area = desktop();
  if (XGetWindowProperty(dpy_, root_, atom(dpy_, "_NET_WORKAREA"), 0, 4, False, XA_CARDINAL, &type, &format, &count, &after, &data) == Success &&
      data && count >= 4) {
    const long* v = reinterpret_cast<const long*>(data);
    area = {static_cast<double>(v[0]), static_cast<double>(v[1]), static_cast<double>(v[2]), static_cast<double>(v[3])};
  }
  if (data) XFree(data);
  return area;
}

void X11Window::createImage() {
  if (useShm_) {
    image_ = XShmCreateImage(dpy_, visual_, 32, ZPixmap, nullptr, &shm_, w_, h_);
    if (image_) {
      shm_.shmid = shmget(IPC_PRIVATE, static_cast<size_t>(image_->bytes_per_line) * image_->height, IPC_CREAT | 0600);
      shm_.shmaddr = image_->data = static_cast<char*>(shmat(shm_.shmid, nullptr, 0));
      shm_.readOnly = False;
      if (shm_.shmaddr != reinterpret_cast<char*>(-1) && XShmAttach(dpy_, &shm_)) {
        XSync(dpy_, False);
        shmctl(shm_.shmid, IPC_RMID, nullptr);  // freed once both sides detach
        return;
      }
      XDestroyImage(image_);
      image_ = nullptr;
    }
    useShm_ = false;
  }
  image_ = plainImage(dpy_, visual_, w_, h_);
}

void X11Window::destroyImage() {
  if (!image_) return;
  if (useShm_) {
    XShmDetach(dpy_, &shm_);
    XSync(dpy_, False);
    image_->data = nullptr;
    XDestroyImage(image_);
    shmdt(shm_.shmaddr);
  } else {
    XDestroyImage(image_);
  }
  image_ = nullptr;
}

void X11Window::put() {
  if (!image_) return;
  if (useShm_) XShmPutImage(dpy_, win_, gc_, image_, 0, 0, 0, 0, w_, h_, False);
  else XPutImage(dpy_, win_, gc_, image_, 0, 0, 0, 0, w_, h_);
  XFlush(dpy_);
}

void X11Window::present(const Image& frame) {
  if (!image_ || frame.w != w_ || frame.h != h_) return;
  copyInto(image_, frame);
  hasFrame_ = true;
  put();
}

void X11Window::popupMenu(std::vector<MenuItem> items, int screenX, int screenY) {
  menu_->open(std::move(items), screenX, screenY, monotonicMs());
}

std::optional<Point> X11Window::cursor() {
  Window r, child;
  int rx, ry, wx, wy;
  unsigned mask;
  if (!XQueryPointer(dpy_, root_, &r, &child, &rx, &ry, &wx, &wy, &mask)) return std::nullopt;
  return Point{static_cast<double>(rx), static_cast<double>(ry)};
}

void X11Window::processEvents(CatApp& app, double nowMs) {
  while (XPending(dpy_)) {
    XEvent e;
    XNextEvent(dpy_, &e);
    if (menu_->isOpen() && (menu_->owns(e.xany.window) || e.type == ButtonPress || e.type == MotionNotify || e.type == KeyPress)) {
      menu_->handle(e, nowMs);
      continue;
    }
    if (e.xany.window != win_) continue;
    auto button = [](unsigned b) { return b == 1 ? MouseButton::Left : b == 3 ? MouseButton::Right : MouseButton::Middle; };
    switch (e.type) {
      case Expose:
        if (hasFrame_) put();
        break;
      case ConfigureNotify: refreshPosition(); break;
      case ButtonPress:
        if (e.xbutton.button <= 3) app.pointerDown(button(e.xbutton.button), e.xbutton.x, e.xbutton.y, e.xbutton.x_root, e.xbutton.y_root, nowMs);
        break;
      case ButtonRelease:
        if (e.xbutton.button <= 3) app.pointerUp(button(e.xbutton.button), nowMs);
        break;
      case MotionNotify: app.pointerMove(e.xmotion.x, e.xmotion.y, e.xmotion.x_root, e.xmotion.y_root, nowMs); break;
    }
  }
}

}  // namespace gato
