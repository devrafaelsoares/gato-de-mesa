// Dev aid: how long a frame takes. ./bench <coat> <width>
#include <chrono>
#include <cstdio>
#include <string>
#include "../core/catalog.hpp"
#include "../render/cat_renderer.hpp"
using namespace gato;
int main(int argc, char** argv) {
  Settings s; s.skinId = argc > 1 ? argv[1] : "felix";
  CatRenderer view(ASSET_DIR);
  view.setup(resolveSkin(s), argc > 2 ? std::stoi(argv[2]) : 310);
  Pose pose; Image frame;
  for (int i = 0; i < 30; i++) view.render(pose, i * 66.0, frame);
  auto t0 = std::chrono::steady_clock::now();
  const int N = 300;
  for (int i = 0; i < N; i++) view.render(pose, 2000 + i * 66.0, frame);
  double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / N;
  std::printf("%.3f ms/frame -> %.1f%% of a core at 15 fps, %.1f%% at 30 fps\n", ms, ms * 15 / 10, ms * 30 / 10);
}
