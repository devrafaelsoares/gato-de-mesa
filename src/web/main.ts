// Web demo: the same cat as the desktop app, reacting to what happens inside
// this page (a browser cannot see keys typed in other programs).
import { CatBrain, gazeToward } from "../domain/cat";
import { DESK_COLORS, PRESET_SKINS } from "../domain/catalog";
import { labelForScanCode } from "../domain/keyboard";
import type { CatInput, MouseButton } from "../domain/types";
import { CatView } from "../renderer/catView";
import { Purr } from "../renderer/purr";

// KeyboardEvent.code -> Linux evdev scan code, the numbering the cat understands
const CODES: Record<string, number> = {
  Escape: 1, Minus: 12, Equal: 13, Backspace: 14, Tab: 15, BracketLeft: 26, BracketRight: 27, Enter: 28,
  ControlLeft: 29, Semicolon: 39, Quote: 40, Backquote: 41, ShiftLeft: 42, Backslash: 43, Comma: 51,
  Period: 52, Slash: 53, ShiftRight: 54, NumpadMultiply: 55, AltLeft: 56, Space: 57, CapsLock: 58,
  NumLock: 69, NumpadSubtract: 74, NumpadAdd: 78, NumpadDecimal: 83, IntlBackslash: 86, F11: 87, F12: 88,
  IntlRo: 89, NumpadEnter: 96, ControlRight: 97, NumpadDivide: 98, AltRight: 100, ArrowUp: 103,
  ArrowLeft: 105, ArrowRight: 106, ArrowDown: 108, Delete: 111, MetaLeft: 125, ContextMenu: 127,
};
[..."1234567890"].forEach((d, i) => (CODES[`Digit${d}`] = 2 + i));
[..."QWERTYUIOP"].forEach((c, i) => (CODES[`Key${c}`] = 16 + i));
[..."ASDFGHJKL"].forEach((c, i) => (CODES[`Key${c}`] = 30 + i));
[..."ZXCVBNM"].forEach((c, i) => (CODES[`Key${c}`] = 44 + i));
for (let i = 1; i <= 10; i++) CODES[`F${i}`] = 58 + i;
[[7, 71], [8, 72], [9, 73], [4, 75], [5, 76], [6, 77], [1, 79], [2, 80], [3, 81], [0, 82]].forEach(([n, c]) => (CODES[`Numpad${n}`] = c));

// Phone keyboards report no key codes: map the typed character instead
const BY_LABEL = new Map<string, number>();
for (let code = 1; code < 128; code++) if (!BY_LABEL.has(labelForScanCode(code))) BY_LABEL.set(labelForScanCode(code), code);
const codeForChar = (ch: string): number => (ch === " " ? 57 : BY_LABEL.get(ch.toUpperCase()) ?? 30);

const host = document.getElementById("cat")!;
const view = new CatView(host);
const brain = new CatBrain();
const purr = new Purr();
let skinId = localStorageGet("skin") ?? "felix";
let deskColor = localStorageGet("desk") ?? DESK_COLORS[0].value;

function localStorageGet(key: string): string | null {
  try { return localStorage.getItem(`gato:${key}`); } catch { return null; }
}
function localStorageSet(key: string, value: string): void {
  try { localStorage.setItem(`gato:${key}`, value); } catch { /* private mode: just not remembered */ }
}

function paint(): void {
  const skin = PRESET_SKINS.find((s) => s.id === skinId) ?? PRESET_SKINS[0];
  view.paint(skin, deskColor, deskColor);
}

// Controls
const skinSelect = document.getElementById("skin") as HTMLSelectElement;
for (const s of PRESET_SKINS) skinSelect.add(new Option(s.name, s.id, false, s.id === skinId));
skinSelect.addEventListener("change", () => { skinId = skinSelect.value; localStorageSet("skin", skinId); paint(); });
const deskSelect = document.getElementById("desk") as HTMLSelectElement;
for (const c of DESK_COLORS) deskSelect.add(new Option(c.name, c.value, false, c.value === deskColor));
deskSelect.addEventListener("change", () => { deskColor = deskSelect.value; localStorageSet("desk", deskColor); paint(); });
paint();

const send = (input: CatInput) => {
  const bubble = brain.handle(input);
  if (bubble) view.bubble(bubble);
};
const typing = document.getElementById("type") as HTMLInputElement;
const inForm = (t: EventTarget | null) => t instanceof HTMLSelectElement;

// Keyboard: keys anywhere on the page
const held = new Set<string>();
window.addEventListener("keydown", (e) => {
  if (inForm(e.target)) return;
  const code = CODES[e.code];
  if (code === undefined) return;
  // keep Space, Tab and arrows from scrolling the page or moving focus
  if (e.target !== typing && ["Space", "Tab", "ArrowUp", "ArrowDown", "ArrowLeft", "ArrowRight", "Backspace"].includes(e.code)) e.preventDefault();
  if (held.has(e.code)) return; // auto-repeat
  held.add(e.code);
  send({ type: "key", code, label: labelForScanCode(code), down: true });
});
window.addEventListener("keyup", (e) => {
  held.delete(e.code);
  const code = CODES[e.code];
  if (code !== undefined) send({ type: "key", code, label: labelForScanCode(code), down: false });
});
window.addEventListener("blur", () => held.clear());
// phones: the typing box reports characters, not keys
typing.addEventListener("input", (e) => {
  const data = (e as InputEvent).data ?? "";
  for (const ch of data) {
    const code = codeForChar(ch);
    send({ type: "key", code, label: labelForScanCode(code), down: true });
    send({ type: "key", code, label: labelForScanCode(code), down: false });
  }
  if (typing.value.length > 40) typing.value = typing.value.slice(-20);
});

// Pointer: the eyes follow it, clicks and the wheel go to the mouse paw
let lastGaze = 0;
window.addEventListener("pointermove", (e) => {
  const now = performance.now();
  if (now - lastGaze < 33) return;
  lastGaze = now;
  const r = host.getBoundingClientRect();
  // the head sits at x = 100 of a drawing spanning -24..200
  send({ type: "gaze", ...gazeToward({ x: e.clientX, y: e.clientY }, { x: r.left + r.width * (124 / 224), y: r.top + r.height / 3 }) });
});
const BUTTONS: MouseButton[] = ["left", "middle", "right"];
window.addEventListener("pointerdown", (e) => { if (!inForm(e.target) && e.pointerType === "mouse") send({ type: "button", which: BUTTONS[e.button] ?? "left", down: true }); });
window.addEventListener("pointerup", (e) => { if (e.pointerType === "mouse") send({ type: "button", which: BUTTONS[e.button] ?? "left", down: false }); });
window.addEventListener("wheel", (e) => send({ type: "wheel", direction: e.deltaY < 0 ? 1 : -1 }), { passive: true });
host.addEventListener("contextmenu", (e) => e.preventDefault());

// Petting: strokes over the head (mouse or finger)
view.onPet(() => brain.stroke());

const frame = () => {
  const pose = brain.pose(false);
  view.render(pose);
  purr.set(pose.petting);
  requestAnimationFrame(frame);
};
requestAnimationFrame(frame);
