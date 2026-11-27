#pragma once
#include <string>

#include "ports.hpp"

namespace gato {

/** Settings as "key=value" lines: no parser library needed */
class IniSettingsStore : public SettingsStore {
 public:
  explicit IniSettingsStore(std::string file) : file_(std::move(file)) {}
  Settings load() override;
  void save(const Settings& settings) override;

 private:
  std::string file_;
};

}  // namespace gato
