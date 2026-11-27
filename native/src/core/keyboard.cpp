#include "keyboard.hpp"

#include <map>
#include <set>

namespace gato {
namespace {

std::map<int, std::string> buildLabels() {
  std::map<int, std::string> labels;
  auto run = [&](int start, const char* chars) {
    for (int i = 0; chars[i]; i++) labels[start + i] = std::string(1, chars[i]);
  };
  run(2, "1234567890");
  run(16, "QWERTYUIOP");
  run(30, "ASDFGHJKL");
  run(44, "ZXCVBNM");
  run(71, "789");
  run(75, "456");
  run(79, "123");
  for (int i = 0; i < 10; i++) labels[59 + i] = "F" + std::to_string(i + 1);
  const std::map<int, std::string> named = {
      {1, "Esc"}, {12, "-"}, {13, "="}, {14, "⌫"}, {15, "Tab"}, {26, "´"}, {27, "["}, {28, "↵"}, {29, "Ctrl"},
      {39, "Ç"}, {40, "~"}, {41, "'"}, {42, "⇧"}, {43, "]"}, {51, ","}, {52, "."}, {53, ";"}, {54, "⇧"},
      {55, "*"}, {56, "Alt"}, {57, "␣"}, {58, "Caps"}, {69, "Num"}, {74, "-"}, {78, "+"}, {82, "0"}, {83, ","},
      {86, "\\"}, {87, "F11"}, {88, "F12"}, {89, "/"}, {96, "↵"}, {97, "Ctrl"}, {98, "/"}, {100, "AltGr"},
      {103, "↑"}, {105, "←"}, {106, "→"}, {108, "↓"}, {111, "Del"}, {125, "❖"}, {127, "☰"}, {464, "Fn"},
  };
  for (const auto& [code, label] : named) labels[code] = label;
  return labels;
}

// Touch-typing zones: these belong to the left hand, the rest to the right
const std::set<int> LEFT_HAND = {1,  41, 2,  3,  4,  5,  6,  15, 16, 17,  18,  19, 20, 58, 30, 31, 32, 33,
                                 34, 42, 86, 44, 45, 46, 47, 48, 29, 464, 125, 56, 59, 60, 61, 62, 63};

// A row spec: a key {code, width} or a gap {0, width}
struct Spec { int code; double w; };

std::vector<Spec> run(int start, int count) {
  std::vector<Spec> specs;
  for (int i = 0; i < count; i++) specs.push_back({start + i, 1});
  return specs;
}

std::vector<Spec> join(std::initializer_list<std::vector<Spec>> parts) {
  std::vector<Spec> all;
  for (const auto& p : parts) all.insert(all.end(), p.begin(), p.end());
  return all;
}

const std::vector<Spec> FUNCTION_ROW = {{1, 1},  {0, 1},  {59, 1}, {60, 1}, {61, 1}, {62, 1}, {0, .5}, {63, 1},
                                        {64, 1}, {65, 1}, {66, 1}, {0, .5}, {67, 1}, {68, 1}, {87, 1}, {88, 1}};

// ABNT2, 15u wide
const std::vector<std::vector<Spec>> MAIN_ROWS = {
    join({{{41, 1}}, run(2, 10), {{12, 1}, {13, 1}, {14, 2}}}),
    join({{{15, 1.5}}, run(16, 10), {{26, 1}, {27, 1}, {28, 1.5}}}),
    join({{{58, 1.75}}, run(30, 9), {{39, 1}, {40, 1}, {43, 1}, {28, 1.25}}}),
    join({{{42, 1.25}, {86, 1}}, run(44, 7), {{51, 1}, {52, 1}, {53, 1}, {89, 1}, {54, 1.75}}}),
};

std::vector<Spec> bottomRow(bool fn) {
  std::vector<Spec> row = {{29, 1.25}};
  if (fn) row.push_back({464, 1});
  row.insert(row.end(), {{125, 1.25}, {56, 1.25}, {57, fn ? 6.5 : 7.5}, {100, 1.25}, {127, 1.25}, {97, 1.25}});
  return row;
}

const std::vector<std::vector<Spec>> NUMPAD_ROWS = {
    {{69, 1}, {98, 1}, {55, 1}, {74, 1}},
    {{71, 1}, {72, 1}, {73, 1}, {78, 1}},
    {{75, 1}, {76, 1}, {77, 1}},
    {{79, 1}, {80, 1}, {81, 1}, {96, 1}},
    {{82, 2}, {83, 1}},
};

constexpr double MAIN_WIDTH = 15, BLOCK_GAP = 0.5, ARROWS_WIDTH = 3, NUMPAD_WIDTH = 4;

void place(std::vector<LayoutKey>& keys, const std::vector<Spec>& specs, int row, double x) {
  for (const auto& s : specs) {
    if (s.code != 0) keys.push_back({s.code, x, row, s.w});
    x += s.w;
  }
}

}  // namespace

std::string labelForScanCode(int code) {
  static const std::map<int, std::string> labels = buildLabels();
  auto it = labels.find(code);
  return it == labels.end() ? "•" : it->second;
}

std::optional<Paw> handForKey(int code) {
  if (code == SPACE_CODE) return std::nullopt;
  return LEFT_HAND.count(code) ? Paw::Left : Paw::Right;
}

KeyboardLayout buildLayout(const KeyboardFeatures& features) {
  const int top = features.functionRow ? 1 : 0;
  const int rows = top + static_cast<int>(MAIN_ROWS.size()) + 1;
  std::vector<LayoutKey> keys;
  if (features.functionRow) place(keys, FUNCTION_ROW, 0, 0);
  for (size_t i = 0; i < MAIN_ROWS.size(); i++) place(keys, MAIN_ROWS[i], top + static_cast<int>(i), 0);
  place(keys, bottomRow(features.fn), rows - 1, 0);

  const double arrowsX = MAIN_WIDTH + BLOCK_GAP;
  place(keys, {{0, 1}, {103, 1}}, rows - 2, arrowsX);
  place(keys, {{105, 1}, {108, 1}, {106, 1}}, rows - 1, arrowsX);

  double width = arrowsX + ARROWS_WIDTH;
  if (features.numpad) {
    const double numpadX = width + BLOCK_GAP;
    for (size_t i = 0; i < NUMPAD_ROWS.size(); i++) place(keys, NUMPAD_ROWS[i], top + static_cast<int>(i), numpadX);
    width = numpadX + NUMPAD_WIDTH;
  }
  return {keys, width, rows};
}

}  // namespace gato
