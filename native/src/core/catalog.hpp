#pragma once
#include <string>
#include <vector>

#include "types.hpp"

namespace gato {

/** The preset coats, in menu order. Their sprites live in assets/<id>/ */
const std::vector<Skin>& presetSkins();

const Skin& resolveSkin(const Settings& settings);

struct SizeInfo { Size size; int width; const char* name; };
const std::vector<SizeInfo>& sizes();
int widthFor(Size size);

/** The drawing spans x -24..200 and y 0..222 */
constexpr double VIEW_X = -24, VIEW_W = 224, VIEW_H = 222;
constexpr double ASPECT = VIEW_H / VIEW_W;

struct DeskColor { const char* name; uint32_t value; };
const std::vector<DeskColor>& deskColors();

/** Legs follow the chest: white on tuxedo and bicolor coats */
uint32_t pawColor(const Skin& skin);

}  // namespace gato
