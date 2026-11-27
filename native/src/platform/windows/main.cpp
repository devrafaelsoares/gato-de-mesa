// Windows entry point: a layered (per-pixel alpha) topmost window and
// low-level keyboard/mouse hooks. No libraries beyond the Win32 API.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>

#include <cmath>
#include <cstring>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "../../app/cat_app.hpp"
#include "../../app/ini_settings_store.hpp"

using namespace gato;

namespace {

std::wstring widen(const std::string& s) {
  if (s.empty()) return {};
  const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
  std::wstring w(n, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
  return w;
}

std::string narrow(const std::wstring& w) {
  if (w.empty()) return {};
  const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
  std::string s(n, '\0');
  WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
  return s;
}

// ---------------------------------------------------------------- input

/** Windows scan codes (set 1) that differ from Linux evdev numbering */
int toEvdev(DWORD scan, bool extended) {
  if (extended) {
    static const std::map<DWORD, int> EXT = {
        {0x1C, 96},  {0x1D, 97},  {0x35, 98},  {0x37, 99},  {0x38, 100}, {0x45, 69},  {0x47, 102}, {0x48, 103}, {0x49, 104}, {0x4B, 105},
        {0x4D, 106}, {0x4F, 107}, {0x50, 108}, {0x51, 109}, {0x52, 110}, {0x53, 111}, {0x5B, 125}, {0x5C, 126}, {0x5D, 127},
    };
    auto it = EXT.find(scan);
    return it == EXT.end() ? 0 : it->second;
  }
  if (scan == 0x73) return 89;  // ABNT2 /? key
  if (scan == 0x7E) return 83;  // ABNT2 keypad separator
  return static_cast<int>(scan);
}

/** Global keyboard and mouse hooks. They run on the main thread's message loop. */
class HookInput : public InputSource {
 public:
  void start(Emit emit) override {
    emit_ = std::move(emit);
    self = this;
    keyboard_ = SetWindowsHookExW(WH_KEYBOARD_LL, onKey, GetModuleHandleW(nullptr), 0);
    mouse_ = SetWindowsHookExW(WH_MOUSE_LL, onMouse, GetModuleHandleW(nullptr), 0);
  }
  void stop() override {
    if (keyboard_) UnhookWindowsHookEx(keyboard_);
    if (mouse_) UnhookWindowsHookEx(mouse_);
    keyboard_ = mouse_ = nullptr;
  }

 private:
  static LRESULT CALLBACK onKey(int code, WPARAM wp, LPARAM lp) {
    if (code == HC_ACTION && self) {
      const auto* k = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lp);
      const bool down = wp == WM_KEYDOWN || wp == WM_SYSKEYDOWN;
      const int evdev = toEvdev(k->scanCode, (k->flags & LLKHF_EXTENDED) != 0);
      // held keys repeat: only the first press counts
      const bool fresh = down ? self->held_.insert(evdev).second : self->held_.erase(evdev) > 0;
      if (evdev > 0 && fresh) {
        RawInput in{RawInput::Kind::Key};
        in.code = evdev;
        in.down = down;
        self->emit_(in);
      }
    }
    return CallNextHookEx(nullptr, code, wp, lp);
  }

  static LRESULT CALLBACK onMouse(int code, WPARAM wp, LPARAM lp) {
    if (code == HC_ACTION && self) {
      const auto* m = reinterpret_cast<const MSLLHOOKSTRUCT*>(lp);
      RawInput in{RawInput::Kind::Button};
      bool have = true;
      switch (wp) {
        case WM_LBUTTONDOWN: case WM_LBUTTONUP: in.which = MouseButton::Left; in.down = wp == WM_LBUTTONDOWN; break;
        case WM_RBUTTONDOWN: case WM_RBUTTONUP: in.which = MouseButton::Right; in.down = wp == WM_RBUTTONDOWN; break;
        case WM_MBUTTONDOWN: case WM_MBUTTONUP: in.which = MouseButton::Middle; in.down = wp == WM_MBUTTONDOWN; break;
        case WM_MOUSEWHEEL:
          in.kind = RawInput::Kind::Wheel;
          in.direction = static_cast<short>(HIWORD(m->mouseData)) > 0 ? 1 : -1;
          break;
        case WM_MOUSEMOVE:
          in.kind = RawInput::Kind::Point;
          in.x = m->pt.x;
          in.y = m->pt.y;
          break;
        default: have = false;
      }
      if (have) self->emit_(in);
    }
    return CallNextHookEx(nullptr, code, wp, lp);
  }

  static inline HookInput* self = nullptr;
  Emit emit_;
  HHOOK keyboard_ = nullptr, mouse_ = nullptr;
  std::set<int> held_;
};

// ---------------------------------------------------------------- window

class Win32Window : public CatWindow {
 public:
  bool create(HINSTANCE inst) {
    WNDCLASSEXW wc{sizeof wc};
    wc.lpfnWndProc = proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_HAND);
    wc.lpszClassName = L"GatoDeMesa";
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    RegisterClassExW(&wc);
    hwnd_ = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW, L"GatoDeMesa", L"Gato de Mesa", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr,
                            inst, this);
    return hwnd_ != nullptr;
  }
  void attach(CatApp* app) { app_ = app; }
  bool running() const { return running_; }

  void resize(int width, int height) override {
    w_ = width;
    h_ = height;
    if (dib_) DeleteObject(dib_);
    if (!memDC_) memDC_ = CreateCompatibleDC(nullptr);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w_;
    bi.bmiHeader.biHeight = -h_;  // top-down rows, like Image
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    dib_ = CreateDIBSection(memDC_, &bi, DIB_RGB_COLORS, &bits_, nullptr, 0);
    SelectObject(memDC_, dib_);
    SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, w_, h_, SWP_NOMOVE | SWP_NOACTIVATE);
    if (!shown_) {
      ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
      shown_ = true;
    }
  }

  void move(int x, int y) override {
    x_ = x;
    y_ = y;
    SetWindowPos(hwnd_, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
  }

  Point position() override { return {static_cast<double>(x_), static_cast<double>(y_)}; }

  Rect desktop() override {
    return {static_cast<double>(GetSystemMetrics(SM_XVIRTUALSCREEN)), static_cast<double>(GetSystemMetrics(SM_YVIRTUALSCREEN)),
            static_cast<double>(GetSystemMetrics(SM_CXVIRTUALSCREEN)), static_cast<double>(GetSystemMetrics(SM_CYVIRTUALSCREEN))};
  }

  Rect workArea(Point around) override {
    MONITORINFO mi{sizeof mi};
    GetMonitorInfoW(MonitorFromPoint({static_cast<LONG>(around.x), static_cast<LONG>(around.y)}, MONITOR_DEFAULTTOPRIMARY), &mi);
    const RECT& r = mi.rcWork;
    return {static_cast<double>(r.left), static_cast<double>(r.top), static_cast<double>(r.right - r.left), static_cast<double>(r.bottom - r.top)};
  }

  void present(const Image& frame) override {
    if (!bits_ || frame.w != w_ || frame.h != h_) return;
    // premultiplied BGRA is exactly what a layered window wants
    std::memcpy(bits_, frame.px.data(), frame.px.size() * 4);
    POINT src{0, 0}, dst{x_, y_};
    SIZE size{w_, h_};
    BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    HDC screen = GetDC(nullptr);
    UpdateLayeredWindow(hwnd_, screen, &dst, &size, memDC_, &src, 0, &blend, ULW_ALPHA);
    ReleaseDC(nullptr, screen);
  }

  void popupMenu(std::vector<MenuItem> items, int screenX, int screenY) override {
    std::vector<std::function<void()>> actions;
    HMENU menu = build(items, actions);
    SetForegroundWindow(hwnd_);
    const int chosen = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY, screenX, screenY, 0, hwnd_, nullptr);
    PostMessageW(hwnd_, WM_NULL, 0, 0);
    DestroyMenu(menu);
    if (chosen > 0 && chosen <= static_cast<int>(actions.size()) && actions[chosen - 1]) actions[chosen - 1]();
  }

  std::optional<Point> cursor() override {
    POINT p;
    if (!GetCursorPos(&p)) return std::nullopt;
    return Point{static_cast<double>(p.x), static_cast<double>(p.y)};
  }

  void quit() override { running_ = false; }

 private:
  static HMENU build(const std::vector<MenuItem>& items, std::vector<std::function<void()>>& actions) {
    HMENU menu = CreatePopupMenu();
    for (const auto& item : items) {
      const std::wstring label = widen(item.label);
      switch (item.kind) {
        case MenuItem::Kind::Separator: AppendMenuW(menu, MF_SEPARATOR, 0, nullptr); break;
        case MenuItem::Kind::Label: AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, label.c_str()); break;
        case MenuItem::Kind::Submenu:
          AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(build(item.children, actions)), label.c_str());
          break;
        default:
          actions.push_back(item.run);
          AppendMenuW(menu, MF_STRING | (item.checked ? MF_CHECKED : 0), actions.size(), label.c_str());
      }
    }
    return menu;
  }

  static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams));
    auto* self = reinterpret_cast<Win32Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!self || !self->app_) return DefWindowProcW(hwnd, msg, wp, lp);
    const double x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp), now = monotonicMs();
    switch (msg) {
      case WM_LBUTTONDOWN:
        SetCapture(hwnd);
        self->app_->pointerDown(MouseButton::Left, x, y, self->x_ + x, self->y_ + y, now);
        return 0;
      case WM_LBUTTONUP:
        ReleaseCapture();
        self->app_->pointerUp(MouseButton::Left, now);
        return 0;
      case WM_RBUTTONUP:
        self->app_->pointerDown(MouseButton::Right, x, y, self->x_ + x, self->y_ + y, now);
        return 0;
      case WM_MOUSEMOVE: self->app_->pointerMove(x, y, self->x_ + x, self->y_ + y, now); return 0;
      case WM_MOUSEACTIVATE: return MA_NOACTIVATE;  // clicking the cat never steals focus
      case WM_CLOSE: self->running_ = false; return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
  }

  HWND hwnd_ = nullptr;
  HDC memDC_ = nullptr;
  HBITMAP dib_ = nullptr;
  void* bits_ = nullptr;
  int w_ = 1, h_ = 1, x_ = 0, y_ = 0;
  bool shown_ = false, running_ = true;
  CatApp* app_ = nullptr;
};

std::string exeDir() {
  wchar_t path[MAX_PATH];
  GetModuleFileNameW(nullptr, path, MAX_PATH);
  return narrow(std::filesystem::path(path).parent_path().wstring());
}

std::string settingsFile() {
  wchar_t appdata[MAX_PATH];
  const DWORD n = GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH);
  const std::filesystem::path base = n > 0 && n < MAX_PATH ? std::filesystem::path(appdata) : std::filesystem::path(exeDir());
  return narrow((base / L"GatoDeMesa" / L"settings.ini").wstring());
}

/** Window and pointer coordinates in real pixels on every monitor */
void beDpiAware() {
  using SetContext = BOOL(WINAPI*)(HANDLE);
  if (auto set = reinterpret_cast<SetContext>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetProcessDpiAwarenessContext"))) {
    if (set(reinterpret_cast<HANDLE>(-4))) return;  // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
  }
  SetProcessDPIAware();
}

}  // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int) {
  // one cat at a time: a second launch just quits
  CreateMutexW(nullptr, TRUE, L"GatoDeMesa.SingleInstance");
  if (GetLastError() == ERROR_ALREADY_EXISTS) return 0;
  beDpiAware();

  Win32Window window;
  if (!window.create(inst)) return 1;
  HookInput input;
  IniSettingsStore store(settingsFile());
  CatApp app(exeDir() + "/assets", store, input, window);
  window.attach(&app);
  app.start(monotonicMs());

  // a precise timer, so 60 fps frames are not rounded to the 15.6 ms system tick
  HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, 0x00000002 /* CREATE_WAITABLE_TIMER_HIGH_RESOLUTION */, TIMER_ALL_ACCESS);
  if (!timer) timer = CreateWaitableTimerW(nullptr, FALSE, nullptr);

  MSG msg;
  while (window.running()) {
    const double wait = app.tick(monotonicMs());
    LARGE_INTEGER due;
    due.QuadPart = -static_cast<LONGLONG>(wait * 10000);  // 100 ns units, relative
    SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE);
    MsgWaitForMultipleObjectsEx(1, &timer, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) window.quit();
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
  }
  app.stop();
  CloseHandle(timer);
  return 0;
}
