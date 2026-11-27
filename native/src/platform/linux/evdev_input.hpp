#pragma once
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "../../app/ports.hpp"

namespace gato {

/**
 * Reads keyboards and mice straight from the kernel. Works under Wayland,
 * where apps cannot see global input. Needs the user in the "input" group.
 * Also tells which keyboard and pointer are being used right now.
 * Non-blocking: the main loop polls fds() and calls readable().
 */
class EvdevInput : public InputSource {
 public:
  explicit EvdevInput(std::function<void(const std::string&)> onDenied) : onDenied_(std::move(onDenied)) {}
  ~EvdevInput() override { stop(); }
  void start(Emit emit) override;
  void stop() override;
  std::vector<int> fds() const;
  void readable(int fd);

 private:
  struct Device {
    std::string path, name;
    std::vector<unsigned long long> keyWords;  // capability bitmap, least significant word first
    int fd = -1;
    bool builtInKeyboard = false, builtInPointer = false;  // laptop parts
    struct { bool active = false; bool hasX = false, hasY = false; int x = 0, y = 0; } touch;
  };
  bool hasKey(const Device& d, int code) const;
  void noteDevice(Device& d, const RawInput& input);
  bool travelled(const std::string& path, double step, double needed);

  std::function<void(const std::string&)> onDenied_;
  Emit emit_;
  std::map<int, Device> devices_;  // by fd
  std::string activeKeyboard_, activePointer_;
  struct { std::string path; double distance = 0, since = 0; } travel_;
  double lastKeyAt_ = -1e9;
};

}  // namespace gato
