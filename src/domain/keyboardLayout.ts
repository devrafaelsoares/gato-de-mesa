/** What a physical keyboard has, as reported by the device */
export interface KeyboardFeatures {
  functionRow: boolean;
  numpad: boolean;
  fn: boolean;
}

/** One key, in key units (1u = a letter key), row 0 = farthest from the typist */
export interface LayoutKey { code: number; label: string; x: number; row: number; w: number }

export interface KeyboardLayout { keys: LayoutKey[]; width: number; rows: number }

export const FULL_KEYBOARD: KeyboardFeatures = { functionRow: true, numpad: true, fn: false };

// A row spec: a key [scan code, label, width] or a number = empty gap in units
type Spec = [number, string, number?] | number;

const run = (start: number, chars: string): Spec[] => [...chars].map((c, i) => [start + i, c]);

const FUNCTION_ROW: Spec[] = [
  [1, "Esc"], 1, [59, "F1"], [60, "F2"], [61, "F3"], [62, "F4"], 0.5,
  [63, "F5"], [64, "F6"], [65, "F7"], [66, "F8"], 0.5, [67, "F9"], [68, "F10"], [87, "F11"], [88, "F12"],
];

// ABNT2, 15u wide
const MAIN_ROWS: Spec[][] = [
  [[41, "'"], ...run(2, "1234567890"), [12, "-"], [13, "="], [14, "⌫", 2]],
  [[15, "Tab", 1.5], ...run(16, "QWERTYUIOP"), [26, "´"], [27, "["], [28, "↵", 1.5]],
  [[58, "Caps", 1.75], ...run(30, "ASDFGHJKL"), [39, "Ç"], [40, "~"], [43, "]"], [28, "↵", 1.25]],
  [[42, "⇧", 1.25], [86, "\\"], ...run(44, "ZXCVBNM"), [51, ","], [52, "."], [53, ";"], [89, "/"], [54, "⇧", 1.75]],
];

const bottomRow = (fn: boolean): Spec[] => [
  [29, "Ctrl", 1.25], ...(fn ? [[464, "Fn"] as Spec] : []), [125, "❖", 1.25], [56, "Alt", 1.25],
  [57, "", fn ? 6.5 : 7.5], [100, "AltGr", 1.25], [127, "☰", 1.25], [97, "Ctrl", 1.25],
];

const NUMPAD_ROWS: Spec[][] = [
  [[69, "Num"], [98, "/"], [55, "*"], [74, "-"]],
  [[71, "7"], [72, "8"], [73, "9"], [78, "+"]],
  [[75, "4"], [76, "5"], [77, "6"]],
  [[79, "1"], [80, "2"], [81, "3"], [96, "↵"]],
  [[82, "0", 2], [83, ","]],
];

const MAIN_WIDTH = 15, BLOCK_GAP = 0.5, ARROWS_WIDTH = 3, NUMPAD_WIDTH = 4;

function place(specs: Spec[], row: number, startX: number): LayoutKey[] {
  const keys: LayoutKey[] = [];
  let x = startX;
  for (const spec of specs) {
    if (typeof spec === "number") { x += spec; continue; }
    const [code, label, w = 1] = spec;
    keys.push({ code, label, x, row, w });
    x += w;
  }
  return keys;
}

/** Builds the drawing's keyboard from what the real one has */
export function buildLayout(features: KeyboardFeatures): KeyboardLayout {
  const top = features.functionRow ? 1 : 0;
  const rows = top + MAIN_ROWS.length + 1;
  const keys: LayoutKey[] = [];

  if (features.functionRow) keys.push(...place(FUNCTION_ROW, 0, 0));
  MAIN_ROWS.forEach((specs, i) => keys.push(...place(specs, top + i, 0)));
  keys.push(...place(bottomRow(features.fn), rows - 1, 0));

  const arrowsX = MAIN_WIDTH + BLOCK_GAP;
  keys.push(...place([1, [103, "↑"]], rows - 2, arrowsX), ...place([[105, "←"], [108, "↓"], [106, "→"]], rows - 1, arrowsX));

  let width = arrowsX + ARROWS_WIDTH;
  if (features.numpad) {
    const numpadX = width + BLOCK_GAP;
    NUMPAD_ROWS.forEach((specs, i) => keys.push(...place(specs, top + i, numpadX)));
    width = numpadX + NUMPAD_WIDTH;
  }
  return { keys, width, rows };
}
