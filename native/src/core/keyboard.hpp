#pragma once
// Scan codes as Linux evdev numbers them. The Windows adapter translates
// its scan codes into these, so one table serves both.
#include <optional>
#include <string>
#include <vector>

#include "types.hpp"

namespace gato {

enum class Paw { Left, Right };

constexpr int SPACE_CODE = 57;

/** Text shown in the bubble for a key (UTF-8) */
std::string labelForScanCode(int code);

/** Which paw types this key; nothing for the space bar (either one) */
std::optional<Paw> handForKey(int code);

/** One key, in key units (1u = a letter key), row 0 = farthest from the typist */
struct LayoutKey { int code; double x; int row; double w; };

struct KeyboardLayout { std::vector<LayoutKey> keys; double width; int rows; };

/** Builds the drawing's keyboard (ABNT2) from what the real one has */
KeyboardLayout buildLayout(const KeyboardFeatures& features);

}  // namespace gato
