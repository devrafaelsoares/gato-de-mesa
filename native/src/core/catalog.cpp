#include "catalog.hpp"

namespace gato {

const std::vector<Skin>& presetSkins() {
  static const std::vector<Skin> skins = {
      {"felix", "Felix", Pattern::Tuxedo, 0x17161a, 0xf6f4f0, 0x17161a, 0xcfa23b, 0xcfa23b},
      {"preto-verde", "Preto · olhos verdes", Pattern::Solid, 0x17171a, 0x17171a, 0x17171a, 0xb9d35a, 0xb9d35a},
      {"preto-dourado", "Preto · olhos dourados", Pattern::Solid, 0x17171a, 0x17171a, 0x17171a, 0xf2b632, 0xf2b632},
      {"frajola", "Frajola", Pattern::Tuxedo, 0x1c1c20, 0xf7f5f0, 0x1c1c20, 0xb9d35a, 0xb9d35a},
      {"laranja", "Laranja rajado", Pattern::Tabby, 0xe8913a, 0xf6c48a, 0xc56a1f, 0x8fbf4a, 0x8fbf4a},
      {"tricolor", "Tricolor", Pattern::Calico, 0xf7f3ec, 0xf7f3ec, 0xe58a3a, 0xd6a531, 0xd6a531},
      {"escaminha", "Escaminha", Pattern::Calico, 0x2a2421, 0x2a2421, 0xc9642b, 0x8fbf4a, 0x8fbf4a},
      {"cinza", "Cinza", Pattern::Solid, 0x7d8494, 0x8d94a3, 0x7d8494, 0xf0a830, 0xf0a830},
      {"cinza-branco", "Cinza e branco", Pattern::Bicolor, 0x6f7787, 0xf5f3ef, 0x6f7787, 0xe9a53a, 0xe9a53a},
      {"rajado-cinza", "Rajado cinza", Pattern::Tabby, 0x9aa0a6, 0xc9ccd0, 0x5d6268, 0x7fc06b, 0x7fc06b},
      {"branco", "Branco", Pattern::Solid, 0xf4f2ee, 0xf4f2ee, 0xf4f2ee, 0x5aa7e0, 0xe9b23a},
      {"siames", "Siamês", Pattern::Pointed, 0xefe3cf, 0xf6eee0, 0x5b4334, 0x4aa3e8, 0x4aa3e8},
      {"creme", "Creme", Pattern::Tabby, 0xe9cfa6, 0xf5e7cf, 0xcfa978, 0xd9a441, 0xd9a441},
  };
  return skins;
}

const Skin& resolveSkin(const Settings& settings) {
  for (const auto& s : presetSkins())
    if (s.id == settings.skinId) return s;
  return presetSkins().front();
}

const std::vector<SizeInfo>& sizes() {
  static const std::vector<SizeInfo> list = {
      {Size::Big, 440, "Grande"}, {Size::Small, 310, "Pequeno"}, {Size::Mini, 190, "Mini"}};
  return list;
}

int widthFor(Size size) {
  for (const auto& s : sizes())
    if (s.size == size) return s.width;
  return 310;
}

const std::vector<DeskColor>& deskColors() {
  static const std::vector<DeskColor> colors = {
      {"Creme", 0xefe9dc}, {"Cinza", 0xb9bcc4}, {"Rosa", 0xf3c1cf},
      {"Menta", 0xbfe3d3}, {"Azul", 0x2f4a8a},  {"Amarelo", 0xf2c14e},
  };
  return colors;
}

uint32_t pawColor(const Skin& skin) {
  switch (skin.pattern) {
    case Pattern::Tuxedo:
    case Pattern::Bicolor: return skin.belly;
    case Pattern::Pointed: return skin.accent;
    default: return skin.body;
  }
}

}  // namespace gato
