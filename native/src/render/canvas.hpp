#pragma once
// A tiny software 2D renderer: premultiplied ARGB images, anti-aliased
// polygon fill, strokes and transformed image blits. Just what the cat needs.
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace gato {

struct Pt { double x, y; };

/** Premultiplied 0xAARRGGBB pixels, rows top to bottom */
struct Image {
  int w = 0, h = 0;
  std::vector<uint32_t> px;
  void resize(int width, int height) { w = width; h = height; px.assign(static_cast<size_t>(w) * h, 0); }
  void clear(uint32_t color) { std::fill(px.begin(), px.end(), color); }
};

/** Affine transform like SVG's matrix(a b c d e f): x' = a x + c y + e, y' = b x + d y + f */
struct Mat {
  double a = 1, b = 0, c = 0, d = 1, e = 0, f = 0;
  static Mat translate(double x, double y) { return {1, 0, 0, 1, x, y}; }
  static Mat scale(double sx, double sy) { return {sx, 0, 0, sy, 0, 0}; }
  static Mat scale(double s) { return scale(s, s); }
  static Mat rotate(double deg);
  /** Same as SVG transform-origin: move origin to (cx, cy), apply m, move back */
  static Mat around(double cx, double cy, const Mat& m) { return translate(cx, cy) * m * translate(-cx, -cy); }
  /** (A * B)(p) = A(B(p)): B applies first, like an SVG transform list read right to left */
  Mat operator*(const Mat& m) const {
    return {a * m.a + c * m.b, b * m.a + d * m.b, a * m.c + c * m.d, b * m.c + d * m.d, a * m.e + c * m.f + e, b * m.e + d * m.f + f};
  }
  Pt apply(Pt p) const { return {a * p.x + c * p.y + e, b * p.x + d * p.y + f}; }
  Mat inverse() const;
};

/** Contours of straight segments; curves are flattened as they are added */
class Path {
 public:
  Path& moveTo(double x, double y);
  Path& lineTo(double x, double y);
  Path& quadTo(double cx, double cy, double x, double y);
  Path& close();
  Path& ellipse(double cx, double cy, double rx, double ry, bool hole = false);
  Path& rect(double x, double y, double w, double h);
  /** A hole cuts out of the shapes it overlaps (it winds the other way) */
  Path& roundRect(double x, double y, double w, double h, double r, bool hole = false);
  /** A contour as given, turned to wind like the others so overlaps add up */
  Path& polygon(const std::vector<Pt>& pts);
  Path transformed(const Mat& m) const;
  const std::vector<std::vector<Pt>>& contours() const { return contours_; }

 private:
  std::vector<std::vector<Pt>> contours_;
};

/** Pixel-centre coverage multiplier in device pixels, 0..1 */
using Mask = std::function<float(double x, double y)>;

uint32_t premul(uint32_t rgb, double alpha = 1.0);

/** Premultiplied color for a pixel centre in device pixels */
using Shader = std::function<uint32_t(double x, double y)>;

/** Fills a path given in device pixels (nonzero rule), 4x vertical + exact horizontal AA.
 *  With a shader, the color comes from it pixel by pixel. */
void fill(Image& dst, const Path& path, uint32_t color, const Mask* mask = nullptr, const Shader* shader = nullptr);

/** Premultiplied color of an image at a fractional pixel position (bilinear, transparent outside) */
uint32_t sampleColor(const Image& img, double x, double y);

/** a towards b by t (0..1), premultiplied colors */
uint32_t mixColor(uint32_t a, uint32_t b, double t);

/** Path outline of a polyline with round joins and caps, in the same space as the points */
Path strokePath(const std::vector<Pt>& pts, double width);

/** Draws src (or a sub-rectangle of it) through m: src pixels -> dst pixels. Bilinear.
 *  With tint, only src alpha is used and tint gives the color. */
void blit(Image& dst, const Image& src, const Mat& m, float opacity = 1.f, const Mask* mask = nullptr,
          const uint32_t* tint = nullptr, int sx = 0, int sy = 0, int sw = -1, int sh = -1);

/** Alpha of an image at a fractional pixel position (bilinear, 0 outside) */
float sampleAlpha(const Image& img, double x, double y);

/** Area-averaging resize, for scaling sprites down to the window size once */
Image resize(const Image& src, int w, int h);

/** Loads a PNG into premultiplied ARGB; empty image on failure */
Image loadPng(const std::string& path);

}  // namespace gato
