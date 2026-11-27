#include "menu.hpp"

#include "../core/catalog.hpp"

namespace gato {
namespace {

MenuItem label(std::string text) { return {MenuItem::Kind::Label, std::move(text)}; }
MenuItem separator() { return {MenuItem::Kind::Separator}; }
MenuItem submenu(std::string text, std::vector<MenuItem> children) {
  MenuItem m{MenuItem::Kind::Submenu, std::move(text)};
  m.children = std::move(children);
  return m;
}

}  // namespace

std::vector<MenuItem> buildMenu(const Settings& settings, const Devices& devices, const MenuActions& actions) {
  auto change = [&](auto edit) {
    return [settings, edit, update = actions.update] {
      Settings next = settings;
      edit(next);
      update(next);
    };
  };

  std::vector<MenuItem> coats;
  for (const auto& skin : presetSkins())
    coats.push_back({MenuItem::Kind::Radio, skin.name, skin.id == settings.skinId, change([id = skin.id](Settings& s) { s.skinId = id; })});

  std::vector<MenuItem> sizeItems;
  for (const auto& info : sizes())
    sizeItems.push_back({MenuItem::Kind::Radio, info.name, info.size == settings.size, change([size = info.size](Settings& s) { s.size = size; })});

  auto colors = [&](bool keyboard) {
    std::vector<MenuItem> items;
    const uint32_t current = keyboard ? settings.keyboardColor : settings.mouseColor;
    for (const auto& c : deskColors())
      items.push_back({MenuItem::Kind::Radio, c.name, c.value == current,
                       change([keyboard, value = c.value](Settings& s) { (keyboard ? s.keyboardColor : s.mouseColor) = value; })});
    return items;
  };

  const std::string kb = devices.keyboard.name.empty() ? "ainda não usado" : devices.keyboard.name;
  const std::string pt = devices.pointer.name.empty() ? "ainda não usado" : devices.pointer.name;
  return {
      label("Teclado: " + kb),
      label(std::string(devices.pointer.touchpad ? "Touchpad" : "Mouse") + ": " + pt),
      separator(),
      submenu("Pelagem", coats),
      submenu("Tamanho", sizeItems),
      submenu("Cor do teclado", colors(true)),
      submenu("Cor do mouse", colors(false)),
      separator(),
      {MenuItem::Kind::Check, "Modo privado (olhos fechados)", settings.privacy, change([](Settings& s) { s.privacy = !s.privacy; })},
      {MenuItem::Kind::Check, "Fundo verde (para live)", settings.greenScreen, change([](Settings& s) { s.greenScreen = !s.greenScreen; })},
      separator(),
      {MenuItem::Kind::Action, "Sair", false, actions.quit},
  };
}

}  // namespace gato
