// Scan codes as Linux evdev numbers them. The Windows adapter translates
// libuiohook's few different codes into these, so one table serves both.
const LABELS: Record<number, string> = {};
const run = (start: number, chars: string) => [...chars].forEach((c, i) => (LABELS[start + i] = c));
run(2, "1234567890");
run(16, "QWERTYUIOP");
run(30, "ASDFGHJKL");
run(44, "ZXCVBNM");
run(71, "789");
run(75, "456");
run(79, "123");
for (let i = 0; i < 10; i++) LABELS[59 + i] = `F${i + 1}`;
Object.assign(LABELS, {
  1: "Esc", 12: "-", 13: "=", 14: "⌫", 15: "Tab", 26: "´", 27: "[", 28: "↵", 29: "Ctrl", 39: "Ç", 40: "~",
  41: "'", 42: "⇧", 43: "]", 51: ",", 52: ".", 53: ";", 54: "⇧", 55: "*", 56: "Alt", 57: "␣", 58: "Caps",
  69: "Num", 74: "-", 78: "+", 82: "0", 83: ",", 86: "\\", 87: "F11", 88: "F12", 89: "/", 96: "↵", 97: "Ctrl",
  98: "/", 100: "AltGr", 103: "↑", 105: "←", 106: "→", 108: "↓", 111: "Del", 125: "❖", 127: "☰", 464: "Fn",
});

export const SPACE_CODE = 57;

export const labelForScanCode = (code: number): string => LABELS[code] ?? "•";

// Touch-typing zones: these belong to the left hand, the rest to the right
const LEFT_HAND = new Set([
  1, 41, 2, 3, 4, 5, 6, 15, 16, 17, 18, 19, 20, 58, 30, 31, 32, 33, 34, 42, 86, 44, 45, 46, 47, 48,
  29, 464, 125, 56, 59, 60, 61, 62, 63,
]);

/** Which paw types this key; null for the space bar (either one) */
export function handForKey(code: number): "left" | "right" | null {
  if (code === SPACE_CODE) return null;
  return LEFT_HAND.has(code) ? "left" : "right";
}
