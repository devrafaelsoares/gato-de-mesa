import { spawn, type ChildProcess } from "node:child_process";
import { readFileSync } from "node:fs";
import type { InputSource, RawInput } from "../application/ports";

// struct input_event: timeval + u16 type + u16 code + s32 value
const EVENT_SIZE = process.arch.endsWith("64") ? 24 : 16;
const EV_KEY = 1, EV_REL = 2, EV_ABS = 3;
const REL_X = 0, REL_Y = 1, REL_HWHEEL = 6, REL_WHEEL = 8;
const BTN_LEFT = 0x110, BTN_MIDDLE = 0x112, BTN_TOUCH = 0x14a;
const TOUCHPAD_SCALE = 0.5;
// A resting palm or a nudged mouse is not a switch: the new pointer must
// really move (or click) before it takes over the drawing
const SWITCH_TRAVEL = 120;
const SWITCH_WINDOW_MS = 600;
// Palms brush the touchpad while typing: ignore it for a moment after each key
// (what libinput calls disable-while-typing), and ask it for a longer, deliberate move
const TYPING_GRACE_MS = 1200;
const TOUCHPAD_TRAVEL = 600;
// BTN_LEFT, BTN_RIGHT, BTN_MIDDLE are consecutive codes
const BUTTONS = ["left", "right", "middle"] as const;

// Laptop parts, recognised by the names the kernel gives them
const BUILT_IN_KEYBOARD = /AT Translated|AT Raw Set|i8042/i;
const BUILT_IN_POINTER = /touchpad|trackpad|trackpoint|MSFT\d|ELAN|SYNA|Synaptics/i;

interface Device { path: string; name: string; keys: bigint }

/** The device's key capability bitmap: does it have this key? */
const hasKey = (keys: bigint, code: number) => ((keys >> BigInt(code)) & 1n) === 1n;
const KEY_F1 = 59, KEY_KP0 = 82, KEY_FN = 464;

/**
 * Reads keyboards and mice straight from the kernel. Works under Wayland,
 * where apps cannot see global input. Needs the user in the "input" group.
 * Also tells which keyboard and pointer are being used right now.
 */
export class LinuxEvdevInput implements InputSource {
  private readers: ChildProcess[] = [];
  private active = { keyboard: "", pointer: "" };
  private travel = { path: "", distance: 0, since: 0 };
  private lastKeyAt = 0;

  constructor(private readonly onDenied: (device: string) => void) {}

  start(emit: (input: RawInput) => void): void {
    for (const device of this.inputDevices()) this.open(device, emit);
  }

  stop(): void {
    this.readers.forEach((r) => r.kill());
    this.readers = [];
  }

  /** Keyboards and pointers, from the kernel's device list */
  private inputDevices(): Device[] {
    return readFileSync("/proc/bus/input/devices", "utf8")
      .split("\n\n")
      .map((block) => ({
        name: /N: Name="(.*)"/.exec(block)?.[1] ?? "",
        handlers: /H: Handlers=(.*)/.exec(block)?.[1] ?? "",
        // "B: KEY=" lists 64-bit hex words, most significant first
        keys: (/B: KEY=(.*)/.exec(block)?.[1] ?? "0").trim().split(/\s+/).reduce((acc, w) => (acc << 64n) | BigInt(`0x${w}`), 0n),
      }))
      .filter((d) => /\b(kbd|mouse\d+)\b/.test(d.handlers))
      .map((d) => ({ ...d, event: /event\d+/.exec(d.handlers)?.[0] }))
      .filter((d) => !!d.event)
      .map((d) => ({ name: d.name, keys: d.keys, path: `/dev/input/${d.event}` }));
  }

  private open(device: Device, emit: (input: RawInput) => void): void {
    // One `cat` per device: a pipe is event driven, while fs streams would
    // park a blocking read on libuv's 4-thread pool per device and starve
    // the keyboard once more than four devices are open
    const reader = spawn("cat", [device.path], { stdio: ["ignore", "pipe", "pipe"] });
    const touch = { active: false, x: NaN, y: NaN };
    let pending = Buffer.alloc(0);

    reader.stdout!.on("data", (chunk: Buffer) => {
      pending = Buffer.concat([pending, chunk]);
      while (pending.length >= EVENT_SIZE) {
        const base = EVENT_SIZE - 8;
        const type = pending.readUInt16LE(base);
        const code = pending.readUInt16LE(base + 2);
        const value = pending.readInt32LE(base + 4);
        pending = pending.subarray(EVENT_SIZE);
        const input = decode(type, code, value, touch);
        if (!input) continue;
        if (input.kind === "key") this.lastKeyAt = Date.now();
        const palm = BUILT_IN_POINTER.test(device.name) && input.kind !== "key" && Date.now() - this.lastKeyAt < TYPING_GRACE_MS;
        if (palm) continue;
        this.noteDevice(device, input, emit);
        emit(input);
      }
    });
    reader.stderr!.on("data", (msg: Buffer) => {
      if (/permiss|denied/i.test(msg.toString())) this.onDenied(device.path);
    });
    this.readers.push(reader);
  }

  /** Whether this pointer has moved far enough, recently enough, to count as in use */
  private travelled(path: string, step: number, needed: number): boolean {
    const now = Date.now();
    if (this.travel.path !== path || now - this.travel.since > SWITCH_WINDOW_MS) this.travel = { path, distance: 0, since: now };
    this.travel.distance += step;
    return this.travel.distance >= needed;
  }

  /** Announces the device when the user switches keyboard or pointer */
  private noteDevice(device: Device, input: RawInput, emit: (input: RawInput) => void): void {
    const role = input.kind === "key" ? "keyboard" : "pointer";
    if (this.active[role] === device.path) return;
    const needed = BUILT_IN_POINTER.test(device.name) ? TOUCHPAD_TRAVEL : SWITCH_TRAVEL;
    if (input.kind === "move" && !this.travelled(device.path, Math.abs(input.dx) + Math.abs(input.dy), needed)) return;
    this.active[role] = device.path;
    const builtIn = (role === "keyboard" ? BUILT_IN_KEYBOARD : BUILT_IN_POINTER).test(device.name);
    const features = role === "keyboard"
      ? { functionRow: hasKey(device.keys, KEY_F1), numpad: hasKey(device.keys, KEY_KP0), fn: builtIn || hasKey(device.keys, KEY_FN) }
      : undefined;
    emit({ kind: "device", role, name: device.name, builtIn, features });
  }
}

function decode(type: number, code: number, value: number, touch: { active: boolean; x: number; y: number }): RawInput | null {
  if (type === EV_KEY) {
    if (value === 2) return null; // auto-repeat
    if (code === BTN_TOUCH) {
      touch.active = value === 1;
      touch.x = touch.y = NaN;
      return null;
    }
    if (code >= BTN_LEFT && code <= BTN_MIDDLE) return { kind: "button", which: BUTTONS[code - BTN_LEFT], down: value === 1 };
    if (code < 0x100) return { kind: "key", code, down: value === 1 };
    return null;
  }
  if (type === EV_REL) {
    if (code === REL_X) return { kind: "move", dx: value, dy: 0 };
    if (code === REL_Y) return { kind: "move", dx: 0, dy: value };
    if (code === REL_WHEEL || code === REL_HWHEEL) return { kind: "wheel", direction: value > 0 ? 1 : -1 };
    return null;
  }
  if (type === EV_ABS && touch.active && (code === 0 || code === 1)) {
    const axis = code === 0 ? "x" : "y";
    const previous = touch[axis];
    touch[axis] = value;
    if (Number.isNaN(previous)) return null;
    const delta = (value - previous) * TOUCHPAD_SCALE;
    return axis === "x" ? { kind: "move", dx: delta, dy: 0 } : { kind: "move", dx: 0, dy: delta };
  }
  return null;
}
