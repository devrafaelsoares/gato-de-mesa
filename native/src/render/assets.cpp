#include "assets.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace gato {

bool SpriteSet::load(const std::string& assetDir, const std::string& coat, double pxPerUnit) {
  sprites_.clear();
  const std::string dir = assetDir + "/" + coat + "/";
  std::ifstream manifest(std::filesystem::u8path(dir + "manifest.txt"));
  if (!manifest) return false;
  double sourceScale = 2;
  std::string line;
  while (std::getline(manifest, line)) {
    std::istringstream in(line);
    std::string name;
    in >> name;
    if (name == "scale") { in >> sourceScale; continue; }
    if (name == "origin" || name.empty()) continue;
    Sprite s;
    in >> s.ux >> s.uy;
    const Image full = loadPng(dir + name + ".png");
    if (full.w == 0) continue;
    // scale once to the window size; blits then run close to 1:1
    const double r = pxPerUnit / sourceScale;
    const int w = std::max(1, static_cast<int>(std::lround(full.w * r)));
    const int h = std::max(1, static_cast<int>(std::lround(full.h * r)));
    s.img = (w == full.w && h == full.h) ? full : resize(full, w, h);
    s.sx = w / (full.w / sourceScale);
    s.sy = h / (full.h / sourceScale);
    sprites_[name] = std::move(s);
  }
  return !sprites_.empty();
}

const Sprite* SpriteSet::get(const std::string& name) const {
  auto it = sprites_.find(name);
  return it == sprites_.end() ? nullptr : &it->second;
}

std::vector<uint32_t> codepoints(const std::string& s) {
  std::vector<uint32_t> out;
  for (size_t i = 0; i < s.size();) {
    const unsigned char c = s[i];
    uint32_t cp;
    int extra;
    if (c < 0x80) { cp = c; extra = 0; }
    else if ((c >> 5) == 6) { cp = c & 0x1f; extra = 1; }
    else if ((c >> 4) == 14) { cp = c & 0x0f; extra = 2; }
    else { cp = c & 0x07; extra = 3; }
    for (int k = 1; k <= extra && i + k < s.size(); k++) cp = (cp << 6) | (s[i + k] & 0x3f);
    out.push_back(cp);
    i += extra + 1;
  }
  return out;
}

bool Font::load(const std::string& assetDir, const std::string& name) {
  atlas_ = loadPng(assetDir + "/" + name + ".png");
  std::ifstream metrics(std::filesystem::u8path(assetDir + "/" + name + ".txt"));
  if (atlas_.w == 0 || !metrics) return false;
  metrics >> cell_ >> baseline_ >> pad_;
  uint32_t cp;
  Glyph g;
  while (metrics >> cp >> g.x >> g.y >> g.advance) glyphs_[cp] = g;
  return true;
}

double Font::measure(const std::string& utf8) const {
  double w = 0;
  for (uint32_t cp : codepoints(utf8)) {
    auto it = glyphs_.find(cp);
    w += it == glyphs_.end() ? cell_ * 0.5 : it->second.advance;
  }
  return w;
}

void Font::draw(Image& dst, const std::string& utf8, double x, double y, uint32_t color, double scale) const {
  for (uint32_t cp : codepoints(utf8)) {
    auto it = glyphs_.find(cp);
    if (it == glyphs_.end()) { x += cell_ * 0.5 * scale; continue; }
    const Glyph& g = it->second;
    // the glyph was drawn at (cell x + pad, cell y + baseline)
    double ox = x - pad_ * scale, oy = y - baseline_ * scale;
    if (scale == 1) { ox = std::round(ox); oy = std::round(oy); }
    blit(dst, atlas_, Mat::translate(ox, oy) * Mat::scale(scale), 1.f, nullptr, &color, g.x, g.y, cell_, cell_);
    x += g.advance * scale;
  }
}

}  // namespace gato
