// Linux entry point: X11 window (works under XWayland too) + evdev input.
#include <fcntl.h>
#include <poll.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <unistd.h>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "../../app/cat_app.hpp"
#include "../../app/ini_settings_store.hpp"
#include "evdev_input.hpp"
#include "x11_window.hpp"

using namespace gato;
namespace fs = std::filesystem;

namespace {

volatile std::sig_atomic_t stopRequested = 0;

/** assets/ next to the program, or one level up while developing */
std::string findAssets() {
  if (const char* env = std::getenv("GATO_ASSETS")) return env;
  std::error_code ec;
  const fs::path exe = fs::read_symlink("/proc/self/exe", ec).parent_path();
  for (const fs::path& dir : {exe / "assets", exe.parent_path() / "assets", exe / "../share/gato-de-mesa/assets"})
    if (fs::exists(dir / "glyphs-menu.png")) return dir.string();
  return "assets";
}

std::string settingsFile() {
  const char* xdg = std::getenv("XDG_CONFIG_HOME");
  const char* home = std::getenv("HOME");
  const fs::path base = xdg && *xdg ? fs::path(xdg) : fs::path(home ? home : ".") / ".config";
  return (base / "gato-de-mesa" / "settings.ini").string();
}

/** One cat at a time: a second launch just quits */
bool firstInstance() {
  const char* runtime = std::getenv("XDG_RUNTIME_DIR");
  const std::string lock = std::string(runtime && *runtime ? runtime : "/tmp") + "/gato-de-mesa.lock";
  const int fd = open(lock.c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0600);
  return fd >= 0 && flock(fd, LOCK_EX | LOCK_NB) == 0;  // fd stays open while we run
}

/** A desktop notification, without a shell */
void notify(const char* title, const char* body) {
  std::fprintf(stderr, "%s: %s\n", title, body);
  if (fork() == 0) {
    execlp("notify-send", "notify-send", "-a", "Gato de Mesa", title, body, static_cast<char*>(nullptr));
    _exit(127);
  }
}

}  // namespace

int main() {
  if (!firstInstance()) return 0;
  std::signal(SIGINT, [](int) { stopRequested = 1; });
  std::signal(SIGTERM, [](int) { stopRequested = 1; });
  std::signal(SIGCHLD, SIG_IGN);  // notify-send reaps itself

  const std::string assets = findAssets();
  X11Window window(assets);
  if (!window.ok()) {
    std::fprintf(stderr, "Gato de Mesa: não consegui abrir uma janela transparente no X11/XWayland.\n");
    return 1;
  }

  bool warned = false;
  EvdevInput input([&](const std::string&) {
    if (warned) return;
    warned = true;
    notify("O gato ainda não consegue ver o teclado",
           "No Linux ele lê as teclas direto do sistema. Rode uma vez no terminal e depois saia e entre na sessão: "
           "sudo usermod -aG input $USER");
  });
  IniSettingsStore store(settingsFile());
  CatApp app(assets, store, input, window);
  app.start(monotonicMs());

  std::vector<pollfd> fds;
  while (window.running() && !stopRequested) {
    double now = monotonicMs();
    window.processEvents(app, now);
    window.menu().tick(now);
    double wait = app.tick(now);
    if (window.menu().isOpen()) wait = std::min(wait, 250.0);

    fds.clear();
    fds.push_back({window.fd(), POLLIN, 0});
    for (int fd : input.fds()) fds.push_back({fd, POLLIN, 0});
    const int ready = poll(fds.data(), fds.size(), static_cast<int>(wait + 0.999));
    if (ready <= 0) continue;
    for (size_t i = 1; i < fds.size(); i++)
      if (fds[i].revents & (POLLIN | POLLERR | POLLHUP)) input.readable(fds[i].fd);
  }
  app.stop();
  return 0;
}
