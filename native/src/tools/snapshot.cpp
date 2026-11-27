// Dev aid: draws one frame of the cat to a PAM image, no window needed.
//   ./snapshot <coat> <width> <out.pam> [key|mouse|sleep|pet|drag]
#include <cstdio>
#include <string>

#include "../core/catalog.hpp"
#include "../render/cat_renderer.hpp"

using namespace gato;

int main(int argc, char** argv) {
  if (argc < 4) { std::fprintf(stderr, "usage: snapshot <coat> <width> <out.pam> [state]\n"); return 1; }
  const std::string state = argc > 4 ? argv[4] : "";
  Settings settings;
  settings.skinId = argv[1];
  CatRenderer view(ASSET_DIR);
  if (!view.setup(resolveSkin(settings), std::stoi(argv[2]))) { std::fprintf(stderr, "no sprites\n"); return 1; }
  view.setDesk(0xefe9dc, 0xefe9dc, state == "green" ? 0xff00b140 : 0);
  Pose pose;
  if (state == "key") { pose.left = LeftPaw::Key; pose.pressed = 31; pose.right = RightPaw::Key; pose.chord = 29; }
  if (state == "mouse") { pose.right = RightPaw::Mouse; pose.button = MouseButton::Left; pose.gaze = {-0.6, 0.4}; }
  if (state == "sleep") { pose.sleeping = true; pose.eyes = Eyes::Closed; }
  if (state == "pet") { pose.petting = true; pose.eyes = Eyes::Happy; pose.excited = true; }
  if (state == "drag") view.setDragging(true, 0);
  if (state == "key") view.bubble({"Ctrl+S", Paw::Left, false}, 1900);
  Image frame;
  for (int i = 0; i <= 120; i++) view.render(pose, i * 16.0, frame);
  FILE* f = std::fopen(argv[3], "wb");
  std::fprintf(f, "P7\nWIDTH %d\nHEIGHT %d\nDEPTH 4\nMAXVAL 255\nTUPLTYPE RGB_ALPHA\nENDHDR\n", frame.w, frame.h);
  for (uint32_t p : frame.px) {
    const unsigned a = p >> 24;
    auto un = [&](unsigned c) { return static_cast<unsigned char>(a ? std::min(255u, c * 255 / a) : 0); };
    const unsigned char px[4] = {un((p >> 16) & 255), un((p >> 8) & 255), un(p & 255), static_cast<unsigned char>(a)};
    std::fwrite(px, 1, 4, f);
  }
  std::fclose(f);
  return 0;
}
