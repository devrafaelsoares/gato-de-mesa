#include "canvas.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "../../third_party/stb_image.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace gato {
namespace {

constexpr double PI = 3.14159265358979323846;
constexpr int SUBSAMPLES = 4;

inline uint32_t pack(float a, float r, float g, float b) {
  auto c = [](float v) { return static_cast<uint32_t>(std::clamp(v, 0.f, 255.f) + 0.5f); };
  return (c(a) << 24) | (c(r) << 16) | (c(g) << 8) | c(b);
}

/** Each channel of c times k/256 (k in 0..256) */
inline uint32_t scale256(uint32_t c, uint32_t k) {
  const uint32_t rb = ((c & 0x00ff00ffu) * k >> 8) & 0x00ff00ffu;
  const uint32_t ag = (((c >> 8) & 0x00ff00ffu) * k) & 0xff00ff00u;
  return rb | ag;
}

/** a towards b by w/256, per channel */
inline uint32_t mix256(uint32_t a, uint32_t b, uint32_t w) { return scale256(a, 256 - w) + scale256(b, w); }

/** Premultiplied src over dst */
inline void over(uint32_t& dst, uint32_t src) {
  const uint32_t sa = src >> 24;
  if (sa == 0) return;
  dst = sa == 255 ? src : src + scale256(dst, 256 - sa - (sa >> 7));
}

/** src over dst, src premultiplied and scaled by k (0..1) */
inline void blend(uint32_t& dst, uint32_t src, float k) {
  if (k <= 0.f) return;
  over(dst, k >= 1.f ? src : scale256(src, static_cast<uint32_t>(k * 256 + 0.5f)));
}

double signedArea(const std::vector<Pt>& pts) {
  double area = 0;
  for (size_t i = 0; i < pts.size(); i++) {
    const Pt& p = pts[i];
    const Pt& q = pts[(i + 1) % pts.size()];
    area += p.x * q.y - q.x * p.y;
  }
  return area / 2;
}

/** Adds w of coverage over [xa, xb) with exact partial pixels at the ends */
inline void addSpan(std::vector<float>& cov, double xa, double xb, float w) {
  const double width = static_cast<double>(cov.size());
  xa = std::clamp(xa, 0.0, width);
  xb = std::clamp(xb, 0.0, width);
  if (xb <= xa) return;
  const int ia = static_cast<int>(xa), ib = static_cast<int>(xb);
  if (ia == ib) { cov[ia] += static_cast<float>((xb - xa) * w); return; }
  cov[ia] += static_cast<float>((ia + 1 - xa) * w);
  for (int k = ia + 1; k < ib; k++) cov[k] += w;
  if (ib < static_cast<int>(cov.size())) cov[ib] += static_cast<float>((xb - ib) * w);
}

}  // namespace

Mat Mat::rotate(double deg) {
  const double r = deg * PI / 180, cs = std::cos(r), sn = std::sin(r);
  return {cs, sn, -sn, cs, 0, 0};
}

Mat Mat::inverse() const {
  const double det = a * d - b * c;
  if (std::abs(det) < 1e-12) return {};
  const double ia = d / det, ib = -b / det, ic = -c / det, id = a / det;
  return {ia, ib, ic, id, -(ia * e + ic * f), -(ib * e + id * f)};
}

Path& Path::moveTo(double x, double y) {
  contours_.push_back({{x, y}});
  return *this;
}

Path& Path::lineTo(double x, double y) {
  if (contours_.empty()) contours_.push_back({});
  contours_.back().push_back({x, y});
  return *this;
}

Path& Path::quadTo(double cx, double cy, double x, double y) {
  const Pt p0 = contours_.back().back();
  constexpr int STEPS = 10;
  for (int i = 1; i <= STEPS; i++) {
    const double t = static_cast<double>(i) / STEPS, u = 1 - t;
    lineTo(u * u * p0.x + 2 * u * t * cx + t * t * x, u * u * p0.y + 2 * u * t * cy + t * t * y);
  }
  return *this;
}

Path& Path::close() { return *this; }  // contours are always closed when filled

Path& Path::ellipse(double cx, double cy, double rx, double ry, bool hole) {
  constexpr int STEPS = 40;
  std::vector<Pt> pts;
  for (int i = 0; i < STEPS; i++) {
    const double a = 2 * PI * i / STEPS;
    pts.push_back({cx + rx * std::cos(a), cy + ry * std::sin(a)});
  }
  polygon(pts);
  if (hole) std::reverse(contours_.back().begin(), contours_.back().end());
  return *this;
}

Path& Path::rect(double x, double y, double w, double h) { return polygon({{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}}); }

Path& Path::roundRect(double x, double y, double w, double h, double r, bool hole) {
  r = std::min({r, w / 2, h / 2});
  std::vector<Pt> pts;
  auto corner = [&](double cx, double cy, double start) {
    for (int i = 0; i <= 6; i++) {
      const double a = (start + 90.0 * i / 6) * PI / 180;
      pts.push_back({cx + r * std::cos(a), cy + r * std::sin(a)});
    }
  };
  corner(x + w - r, y + r, -90);
  corner(x + w - r, y + h - r, 0);
  corner(x + r, y + h - r, 90);
  corner(x + r, y + r, 180);
  polygon(pts);
  if (hole && !contours_.empty()) std::reverse(contours_.back().begin(), contours_.back().end());
  return *this;
}

Path& Path::polygon(const std::vector<Pt>& pts) {
  if (pts.size() < 3) return *this;
  contours_.push_back(pts);
  if (signedArea(pts) < 0) std::reverse(contours_.back().begin(), contours_.back().end());
  return *this;
}

Path Path::transformed(const Mat& m) const {
  Path out;
  for (const auto& c : contours_) {
    std::vector<Pt> pts;
    pts.reserve(c.size());
    for (const auto& p : c) pts.push_back(m.apply(p));
    out.contours_.push_back(std::move(pts));
  }
  return out;
}

uint32_t premul(uint32_t rgb, double alpha) {
  const float a = static_cast<float>(std::clamp(alpha, 0.0, 1.0));
  return pack(255 * a, ((rgb >> 16) & 255) * a, ((rgb >> 8) & 255) * a, (rgb & 255) * a);
}

void fill(Image& dst, const Path& path, uint32_t color, const Mask* mask) {
  struct Edge { double x0, y0, x1, y1; int dir; };
  std::vector<Edge> edges;
  double minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9;
  for (const auto& c : path.contours()) {
    for (size_t i = 0; i < c.size(); i++) {
      const Pt p = c[i], q = c[(i + 1) % c.size()];
      minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
      minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
      if (p.y == q.y) continue;
      edges.push_back(p.y < q.y ? Edge{p.x, p.y, q.x, q.y, 1} : Edge{q.x, q.y, p.x, p.y, -1});
    }
  }
  if (edges.empty()) return;
  const int y0 = std::max(0, static_cast<int>(std::floor(minY)));
  const int y1 = std::min(dst.h - 1, static_cast<int>(std::ceil(maxY)));
  const int x0 = std::max(0, static_cast<int>(std::floor(minX)));
  const int x1 = std::min(dst.w - 1, static_cast<int>(std::ceil(maxX)));
  if (x0 > x1 || y0 > y1) return;

  std::vector<float> cov(dst.w, 0.f);
  std::vector<std::pair<double, int>> hits;
  for (int y = y0; y <= y1; y++) {
    std::fill(cov.begin() + x0, cov.begin() + x1 + 1, 0.f);
    bool any = false;
    for (int s = 0; s < SUBSAMPLES; s++) {
      const double sy = y + (s + 0.5) / SUBSAMPLES;
      hits.clear();
      for (const auto& e : edges)
        if (sy >= e.y0 && sy < e.y1) hits.push_back({e.x0 + (sy - e.y0) * (e.x1 - e.x0) / (e.y1 - e.y0), e.dir});
      if (hits.size() < 2) continue;
      std::sort(hits.begin(), hits.end());
      int wind = 0;
      for (size_t i = 0; i + 1 < hits.size(); i++) {
        wind += hits[i].second;
        if (wind != 0) { addSpan(cov, hits[i].first, hits[i + 1].first, 1.f / SUBSAMPLES); any = true; }
      }
    }
    if (!any) continue;
    uint32_t* row = &dst.px[static_cast<size_t>(y) * dst.w];
    for (int x = x0; x <= x1; x++) {
      float k = std::min(cov[x], 1.f);
      if (k <= 0.f) continue;
      if (mask) k *= (*mask)(x + 0.5, y + 0.5);
      blend(row[x], color, k);
    }
  }
}

Path strokePath(const std::vector<Pt>& pts, double width) {
  Path path;
  const double r = width / 2;
  for (size_t i = 0; i < pts.size(); i++) {
    path.ellipse(pts[i].x, pts[i].y, r, r);
    if (i + 1 == pts.size()) break;
    const Pt p = pts[i], q = pts[i + 1];
    const double len = std::hypot(q.x - p.x, q.y - p.y);
    if (len < 1e-9) continue;
    const double nx = -(q.y - p.y) / len * r, ny = (q.x - p.x) / len * r;
    path.polygon({{p.x + nx, p.y + ny}, {q.x + nx, q.y + ny}, {q.x - nx, q.y - ny}, {p.x - nx, p.y - ny}});
  }
  return path;
}

float sampleAlpha(const Image& img, double x, double y) {
  x -= 0.5;
  y -= 0.5;
  const int ix = static_cast<int>(std::floor(x)), iy = static_cast<int>(std::floor(y));
  const float fx = static_cast<float>(x - ix), fy = static_cast<float>(y - iy);
  auto at = [&](int px, int py) -> float {
    if (px < 0 || py < 0 || px >= img.w || py >= img.h) return 0.f;
    return (img.px[static_cast<size_t>(py) * img.w + px] >> 24) / 255.f;
  };
  return (at(ix, iy) * (1 - fx) + at(ix + 1, iy) * fx) * (1 - fy) + (at(ix, iy + 1) * (1 - fx) + at(ix + 1, iy + 1) * fx) * fy;
}

void blit(Image& dst, const Image& src, const Mat& m, float opacity, const Mask* mask, const uint32_t* tint, int sx, int sy, int sw, int sh) {
  if (sw < 0) sw = src.w;
  if (sh < 0) sh = src.h;
  if (sw <= 0 || sh <= 0 || opacity <= 0.f) return;
  // where the source rectangle lands
  const Pt corners[4] = {m.apply({0, 0}), m.apply({double(sw), 0}), m.apply({0, double(sh)}), m.apply({double(sw), double(sh)})};
  double minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9;
  for (const auto& p : corners) {
    minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
    minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
  }
  const int x0 = std::max(0, static_cast<int>(std::floor(minX))), x1 = std::min(dst.w - 1, static_cast<int>(std::ceil(maxX)));
  const int y0 = std::max(0, static_cast<int>(std::floor(minY))), y1 = std::min(dst.h - 1, static_cast<int>(std::ceil(maxY)));
  if (x0 > x1 || y0 > y1) return;

  const uint32_t* base = &src.px[static_cast<size_t>(sy) * src.w + sx];
  const int stride = src.w;
  auto texel = [&](int px, int py) -> uint32_t {
    if (px < 0 || py < 0 || px >= sw || py >= sh) return 0;
    return base[static_cast<size_t>(py) * stride + px];
  };
  const uint32_t k0 = static_cast<uint32_t>(std::clamp(opacity, 0.f, 1.f) * 256 + 0.5f);
  auto put = [&](uint32_t& d, uint32_t color, int x, int y) {
    uint32_t k = k0;
    if (mask) k = static_cast<uint32_t>(k * std::clamp((*mask)(x + 0.5, y + 0.5), 0.f, 1.f) + 0.5f);
    if (tint) color = scale256(*tint, (((color >> 24) + ((color >> 24) >> 7)) * k) >> 8);
    else if (k < 256) color = scale256(color, k);
    over(d, color);
  };

  // whole-pixel moves (menus, text) copy straight, no filtering
  if (m.a == 1 && m.b == 0 && m.c == 0 && m.d == 1 && m.e == std::floor(m.e) && m.f == std::floor(m.f)) {
    const int ox = static_cast<int>(m.e), oy = static_cast<int>(m.f);
    for (int y = std::max(y0, oy); y <= std::min(y1, oy + sh - 1); y++) {
      uint32_t* row = &dst.px[static_cast<size_t>(y) * dst.w];
      const uint32_t* srow = base + static_cast<size_t>(y - oy) * stride;
      for (int x = std::max(x0, ox); x <= std::min(x1, ox + sw - 1); x++)
        if (const uint32_t c = srow[x - ox]) put(row[x], c, x, y);
    }
    return;
  }

  // bilinear, in 16.16 fixed point, stepping along each row
  const Mat inv = m.inverse();
  const double du = inv.a, dv = inv.b;
  for (int y = y0; y <= y1; y++) {
    // texel coordinates of pixel centre x: u = u0 + du * x (minus half a texel)
    const double u0 = inv.c * (y + 0.5) + inv.e - 0.5 + du * 0.5, v0 = inv.d * (y + 0.5) + inv.f - 0.5 + dv * 0.5;
    // only the x range where the 2x2 footprint can touch the source
    double lo = x0, hi = x1 + 1.0;
    auto limit = [&](double start, double step, double size) {
      if (std::abs(step) < 1e-12) {
        if (start <= -1 || start >= size) hi = lo - 1;
        return;
      }
      double a = (-1 - start) / step, b = (size - start) / step;
      if (a > b) std::swap(a, b);
      lo = std::max(lo, std::floor(a));
      hi = std::min(hi, std::ceil(b));
    };
    limit(u0, du, sw);
    limit(v0, dv, sh);
    const int xa = std::max(x0, static_cast<int>(lo)), xb = std::min(x1, static_cast<int>(hi));
    if (xa > xb) continue;
    uint32_t* row = &dst.px[static_cast<size_t>(y) * dst.w];
    long long U = std::llround((u0 + du * xa) * 65536), V = std::llround((v0 + dv * xa) * 65536);
    const long long dU = std::llround(du * 65536), dV = std::llround(dv * 65536);
    for (int x = xa; x <= xb; x++, U += dU, V += dV) {
      const int ix = static_cast<int>(U >> 16), iy = static_cast<int>(V >> 16);
      const uint32_t fx = static_cast<uint32_t>((U >> 8) & 255), fy = static_cast<uint32_t>((V >> 8) & 255);
      uint32_t c00, c10, c01, c11;
      if (ix >= 0 && iy >= 0 && ix + 1 < sw && iy + 1 < sh) {
        const uint32_t* p = base + static_cast<size_t>(iy) * stride + ix;
        c00 = p[0]; c10 = p[1]; c01 = p[stride]; c11 = p[stride + 1];
      } else {
        c00 = texel(ix, iy); c10 = texel(ix + 1, iy); c01 = texel(ix, iy + 1); c11 = texel(ix + 1, iy + 1);
      }
      if ((c00 | c10 | c01 | c11) == 0) continue;
      const uint32_t color = mix256(mix256(c00, c10, fx + (fx >> 7)), mix256(c01, c11, fx + (fx >> 7)), fy + (fy >> 7));
      if (color) put(row[x], color, x, y);
    }
  }
}

Image resize(const Image& src, int w, int h) {
  Image out;
  out.resize(w, h);
  if (src.w == 0 || src.h == 0 || w == 0 || h == 0) return out;
  const double rx = static_cast<double>(src.w) / w, ry = static_cast<double>(src.h) / h;
  // weights of source cells [i, i+1) inside [a, b)
  auto spans = [](double a, double b, int limit) {
    std::vector<std::pair<int, float>> list;
    for (int i = static_cast<int>(std::floor(a)); i < static_cast<int>(std::ceil(b)) && i < limit; i++) {
      const double wgt = std::min(b, i + 1.0) - std::max(a, static_cast<double>(i));
      if (wgt > 0 && i >= 0) list.push_back({i, static_cast<float>(wgt)});
    }
    return list;
  };
  for (int y = 0; y < h; y++) {
    const auto ys = spans(y * ry, (y + 1) * ry, src.h);
    for (int x = 0; x < w; x++) {
      const auto xs = spans(x * rx, (x + 1) * rx, src.w);
      float acc[4] = {0, 0, 0, 0}, total = 0;
      for (const auto& [sy, wy] : ys)
        for (const auto& [sx, wx] : xs) {
          const uint32_t c = src.px[static_cast<size_t>(sy) * src.w + sx];
          const float wgt = wx * wy;
          acc[0] += (c >> 24) * wgt; acc[1] += ((c >> 16) & 255) * wgt; acc[2] += ((c >> 8) & 255) * wgt; acc[3] += (c & 255) * wgt;
          total += wgt;
        }
      if (total > 0) out.px[static_cast<size_t>(y) * w + x] = pack(acc[0] / total, acc[1] / total, acc[2] / total, acc[3] / total);
    }
  }
  return out;
}

Image loadPng(const std::string& path) {
  Image img;
  std::ifstream in(std::filesystem::u8path(path), std::ios::binary);
  if (!in) return img;
  const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  int w = 0, h = 0, n = 0;
  unsigned char* data = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &w, &h, &n, 4);
  if (!data) return img;
  img.resize(w, h);
  for (int i = 0; i < w * h; i++) {
    const unsigned char* p = data + i * 4;
    const float a = p[3] / 255.f;
    img.px[i] = pack(p[3], p[0] * a, p[1] * a, p[2] * a);
  }
  stbi_image_free(data);
  return img;
}

}  // namespace gato
