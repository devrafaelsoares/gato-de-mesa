import { gazeToward } from "../domain/cat";
import { labelForScanCode } from "../domain/keyboard";
import { DEFAULT_DEVICES } from "../domain/catalog";
import { FULL_KEYBOARD } from "../domain/keyboardLayout";
import type { CatInput, Devices } from "../domain/types";
import type { InputSource, Point, RawInput, Rect } from "./ports";

const GAZE_INTERVAL_MS = 33;

/**
 * Turns raw OS input into what the cat understands. Keeps a pointer
 * estimate (Wayland only gives relative motion) and throttles gaze updates.
 */
export class InputRelay {
  private pointer: Point = { x: 0, y: 0 };
  private gazeTimer: ReturnType<typeof setTimeout> | null = null;
  private devices: Devices = DEFAULT_DEVICES;

  constructor(
    private readonly source: InputSource,
    private readonly head: () => Point,
    private readonly desktop: () => Rect,
    private readonly send: (input: CatInput) => void,
    private readonly onDevices: (devices: Devices) => void,
  ) {}

  start(): void {
    const d = this.desktop();
    this.pointer = { x: d.x + d.width / 2, y: d.y + d.height / 2 };
    this.source.start((raw) => this.onRaw(raw));
  }

  stop(): void {
    this.source.stop();
    if (this.gazeTimer) clearTimeout(this.gazeTimer);
  }

  /** Exact pointer position, when the platform can tell it */
  syncPointer(point: Point): void {
    this.pointer = point;
    this.scheduleGaze();
  }

  private onRaw(raw: RawInput): void {
    switch (raw.kind) {
      case "key": return this.send({ type: "key", code: raw.code, label: labelForScanCode(raw.code), down: raw.down });
      case "button": return this.send({ type: "button", which: raw.which, down: raw.down });
      case "wheel": return this.send({ type: "wheel", direction: raw.direction });
      case "move": {
        const d = this.desktop();
        this.pointer = {
          x: Math.min(Math.max(this.pointer.x + raw.dx, d.x), d.x + d.width),
          y: Math.min(Math.max(this.pointer.y + raw.dy, d.y), d.y + d.height),
        };
        return this.scheduleGaze();
      }
      case "point": return this.syncPointer(raw);
      case "device":
        this.devices = raw.role === "keyboard"
          ? { ...this.devices, keyboard: { name: raw.name, laptop: raw.builtIn, features: raw.features ?? FULL_KEYBOARD } }
          : { ...this.devices, pointer: { name: raw.name, touchpad: raw.builtIn } };
        return this.onDevices(this.devices);
    }
  }

  private scheduleGaze(): void {
    if (this.gazeTimer) return;
    this.gazeTimer = setTimeout(() => {
      this.gazeTimer = null;
      this.send({ type: "gaze", ...gazeToward(this.pointer, this.head()) });
    }, GAZE_INTERVAL_MS);
  }
}
