import type { InputSource, Point, RawInput } from "../application/ports";

interface HookEvent { keycode: number; x: number; y: number; button: number; rotation: number }

// libuiohook numbers buttons 1 = left, 2 = right, 3 = middle
const BUTTONS = { 1: "left", 2: "right", 3: "middle" } as const;
const buttonOf = (n: number) => BUTTONS[n as 1 | 2 | 3] ?? "left";
interface UiohookApi {
  on(event: string, listener: (e: HookEvent) => void): void;
  removeAllListeners(): void;
  start(): void;
  stop(): void;
}

// libuiohook codes that differ from Linux evdev numbering
const TO_EVDEV: Record<number, number> = {
  0xe048: 103, 0xe04b: 105, 0xe04d: 106, 0xe050: 108, 0x0e53: 111, 0x0e5b: 125, 0x0e5d: 127,
  0x0e1d: 97, 0x0e38: 100, 0x0e1c: 96, 0x0e35: 98, 0x0e45: 69,
};
const evdev = (code: number) => TO_EVDEV[code] ?? code;

/** Global keyboard and mouse hook (libuiohook). Used on Windows. */
export class HookInput implements InputSource {
  private hook: UiohookApi | null = null;

  /** toDip converts physical pixels to the window system's coordinates */
  constructor(private readonly toDip: (p: Point) => Point) {}

  start(emit: (input: RawInput) => void): void {
    // Loaded lazily: the native module only exists where it is needed
    // eslint-disable-next-line @typescript-eslint/no-require-imports
    const hook = (require("uiohook-napi") as { uIOhook: UiohookApi }).uIOhook;
    hook.on("keydown", (e) => emit({ kind: "key", code: evdev(e.keycode), down: true }));
    hook.on("keyup", (e) => emit({ kind: "key", code: evdev(e.keycode), down: false }));
    hook.on("mousedown", (e) => emit({ kind: "button", which: buttonOf(e.button), down: true }));
    hook.on("mouseup", (e) => emit({ kind: "button", which: buttonOf(e.button), down: false }));
    // positive rotation scrolls down
    hook.on("wheel", (e) => emit({ kind: "wheel", direction: e.rotation > 0 ? -1 : 1 }));
    hook.on("mousemove", (e) => emit({ kind: "point", ...this.toDip({ x: e.x, y: e.y }) }));
    hook.start();
    this.hook = hook;
  }

  stop(): void {
    this.hook?.removeAllListeners();
    this.hook?.stop();
    this.hook = null;
  }
}
