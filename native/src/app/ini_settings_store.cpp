#include "ini_settings_store.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

namespace gato {
namespace {

const std::map<std::string, Size> SIZE_NAMES = {{"big", Size::Big}, {"small", Size::Small}, {"mini", Size::Mini}};

std::string sizeName(Size size) {
  for (const auto& [name, s] : SIZE_NAMES)
    if (s == size) return name;
  return "small";
}

std::string hex(uint32_t color) {
  char buf[8];
  std::snprintf(buf, sizeof buf, "%06x", color & 0xffffff);
  return buf;
}

}  // namespace

Settings IniSettingsStore::load() {
  Settings s;
  std::ifstream in(std::filesystem::u8path(file_));
  std::string line;
  std::optional<int> x, y;
  while (std::getline(in, line)) {
    const auto eq = line.find('=');
    if (eq == std::string::npos) continue;
    const std::string key = line.substr(0, eq), value = line.substr(eq + 1);
    try {
      if (key == "skin") s.skinId = value;
      else if (key == "size" && SIZE_NAMES.count(value)) s.size = SIZE_NAMES.at(value);
      else if (key == "keyboardColor") s.keyboardColor = std::stoul(value, nullptr, 16);
      else if (key == "mouseColor") s.mouseColor = std::stoul(value, nullptr, 16);
      else if (key == "privacy") s.privacy = value == "1";
      else if (key == "greenScreen") s.greenScreen = value == "1";
      else if (key == "x") x = std::stoi(value);
      else if (key == "y") y = std::stoi(value);
    } catch (...) {
      // a damaged line keeps its default
    }
  }
  if (x && y) s.position = {{*x, *y}};
  return s;
}

void IniSettingsStore::save(const Settings& s) {
  const auto path = std::filesystem::u8path(file_);
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  std::ostringstream out;
  out << "skin=" << s.skinId << "\nsize=" << sizeName(s.size) << "\nkeyboardColor=" << hex(s.keyboardColor)
      << "\nmouseColor=" << hex(s.mouseColor) << "\nprivacy=" << (s.privacy ? 1 : 0) << "\ngreenScreen=" << (s.greenScreen ? 1 : 0) << "\n";
  if (s.position) out << "x=" << s.position->first << "\ny=" << s.position->second << "\n";
  // write then rename: a crash mid-write never leaves a half file
  const auto tmp = path.string() + ".tmp";
  {
    std::ofstream f(std::filesystem::u8path(tmp), std::ios::trunc);
    f << out.str();
  }
  std::filesystem::rename(std::filesystem::u8path(tmp), path, ec);
}

}  // namespace gato
