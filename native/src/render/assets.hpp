#pragma once
#include <map>
#include <string>

#include "canvas.hpp"

namespace gato {

/** One part of the cat, already scaled to the window */
struct Sprite {
  Image img;
  /** Top-left corner in drawing units */
  double ux = 0, uy = 0;
  /** Pixels per drawing unit of this image, per axis */
  double sx = 1, sy = 1;
  /** Image pixels -> drawing units */
  Mat toUnits() const { return Mat::translate(ux, uy) * Mat::scale(1 / sx, 1 / sy); }
};

/** Every part of one coat, loaded from assets/<coat>/ */
class SpriteSet {
 public:
  bool load(const std::string& assetDir, const std::string& coat, double pxPerUnit);
  const Sprite* get(const std::string& name) const;

 private:
  std::map<std::string, Sprite> sprites_;
};

/** Glyphs drawn by the exporter with the system font */
class Font {
 public:
  bool load(const std::string& assetDir, const std::string& name);
  /** Width of text in atlas pixels */
  double measure(const std::string& utf8) const;
  /** Draws text with its baseline starting at (x, y) in dst pixels, scaled by `scale` */
  void draw(Image& dst, const std::string& utf8, double x, double y, uint32_t color, double scale = 1) const;
  int cell() const { return cell_; }

 private:
  struct Glyph { int x, y; double advance; };
  Image atlas_;
  int cell_ = 0, baseline_ = 0, pad_ = 0;
  std::map<uint32_t, Glyph> glyphs_;
};

/** Code points of a UTF-8 string */
std::vector<uint32_t> codepoints(const std::string& utf8);

}  // namespace gato
