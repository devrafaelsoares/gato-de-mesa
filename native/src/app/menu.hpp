#pragma once
// The cat's right-click menu, described once. Each platform only shows it.
#include <functional>
#include <string>
#include <vector>

#include "../core/types.hpp"

namespace gato {

struct MenuItem {
  enum class Kind { Action, Radio, Check, Separator, Label, Submenu } kind = Kind::Action;
  std::string label;  // UTF-8
  bool checked = false;
  std::function<void()> run;
  std::vector<MenuItem> children;
};

struct MenuActions {
  std::function<void(const Settings&)> update;
  std::function<void()> quit;
};

/** Every setting lives in this menu */
std::vector<MenuItem> buildMenu(const Settings& settings, const Devices& devices, const MenuActions& actions);

}  // namespace gato
