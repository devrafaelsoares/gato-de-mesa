import type { CatInput, MouseButton } from "./types";
import { handForKey } from "./keyboard";

export type Eyes = "open" | "closed" | "happy" | "sleepy";
export type Paw = "left" | "right";

export interface Pose {
  left: "rest" | "key";
  right: "rest" | "key" | "mouse" | "click";
  eyes: Eyes;
  gaze: { x: number; y: number };
  /** Scan code of the key being pressed */
  pressed: number | null;
  sleeping: boolean;
  petting: boolean;
  /** Typing fast or being petted: livelier tail, wider pupils */
  excited: boolean;
  /** Mouse button held down, and the wheel spinning up (1) or down (-1) */
  button: MouseButton | null;
  scroll: 1 | -1 | 0;
  /** Keyboard lights, following the lock keys */
  capsLock: boolean;
  numLock: boolean;
  /** Modifier held in a shortcut like Ctrl+C: the other paw presses it */
  chord: number | null;
  /** Enter hit hard */
  slam: boolean;
  /** Yawning and stretching just before falling asleep */
  yawning: boolean;
  /** The pointer darts close by: crouch, wide pupils, and now and then a swat */
  hunting: boolean;
  swat: boolean;
  /** One eye closed for a moment */
  wink: "L" | "R" | null;
}

/** A label to pop up, from the paw that typed or from the mouse */
export interface Bubble { label: string; paw: Paw; at: "paw" | "mouse" }

const LABEL_OF: Record<number, string> = { 29: "Ctrl", 97: "Ctrl", 56: "Alt", 100: "AltGr", 125: "❖" };
const BUTTON_LABELS: Record<MouseButton, string> = { left: "Esq", right: "Dir", middle: "Meio" };
const KEY_CAPS_LOCK = 58, KEY_NUM_LOCK = 69;
const MODIFIERS = new Set([29, 97, 56, 100, 125]); // Ctrl, Alt, AltGr, Super
const ENTER = new Set([28, 96]);
const YAWN_MS = 3000;
// gaze is a direction scaled down only when the pointer is near the head
const NEAR = 0.95;
const DART_SPEED = 2.5; // gaze units per second
const DARTS_TO_HUNT = 3;
const HUNT_MS = 1600;
const SWAT_MS = 200;
const SWAT_COOLDOWN_MS = 1400;

const TAP_MS = 140;
const MOUSE_HOLD_MS = 1100;
const SLEEPY_MS = 45_000;
const SLEEP_MS = 120_000;
const PET_STROKES = 8;
const PET_WINDOW_MS = 1000;
const EXCITED_KEYS = 6;
const EXCITED_WINDOW_MS = 1500;
const SCROLL_MS = 220;

/** The cat's behaviour, free of rendering and OS details. Time is injected. */
export class CatBrain {
  private leftUntil = 0;
  private rightUntil = 0;
  private clickUntil = 0;
  private mouseUntil = 0;
  private pressed: { code: number; until: number } | null = null;
  private gaze = { x: 0, y: 0 };
  private lastInput: number;
  private nextBlink: number;
  private wink: "L" | "R" | null = null;
  private blinkUntil = 0;
  private petUntil = 0;
  private strokes: number[] = [];
  private alternate: Paw = "left";
  private recentKeys: number[] = [];
  private held: MouseButton | null = null;
  private scroll: { direction: 1 | -1; until: number } | null = null;
  private capsLock = false;
  private numLock = true;
  private modifiers = new Set<number>();
  private chord: { code: number; until: number } | null = null;
  private slamUntil = 0;
  private lastGaze = { x: 0, y: 0, t: 0 };
  private darts: number[] = [];
  private huntUntil = 0;
  private swatUntil = 0;
  private nextSwat = 0;

  constructor(private readonly now: () => number = Date.now, private readonly random: () => number = Math.random) {
    this.lastInput = now();
    this.nextBlink = this.lastInput + 3000;
  }

  /** Feeds one input; returns what to pop up as a bubble, if anything */
  handle(input: CatInput): Bubble | null {
    const t = this.now();
    this.lastInput = t;
    switch (input.type) {
      case "key": {
        if (MODIFIERS.has(input.code)) {
          if (input.down) this.modifiers.add(input.code); else this.modifiers.delete(input.code);
        }
        if (!input.down) return null;
        if (input.code === KEY_CAPS_LOCK) this.capsLock = !this.capsLock;
        if (input.code === KEY_NUM_LOCK) this.numLock = !this.numLock;
        this.recentKeys = [...this.recentKeys.filter((k) => t - k < EXCITED_WINDOW_MS), t];
        const paw = handForKey(input.code) ?? (this.alternate = this.alternate === "left" ? "right" : "left");
        if (paw === "left") this.leftUntil = t + TAP_MS;
        else { this.rightUntil = t + TAP_MS; this.mouseUntil = 0; }
        this.pressed = { code: input.code, until: t + TAP_MS };
        // a shortcut takes both paws: one holds the modifier, the other taps the key
        const modifier = MODIFIERS.has(input.code) ? undefined : [...this.modifiers][0];
        if (modifier !== undefined) {
          this.chord = { code: modifier, until: t + TAP_MS * 2 };
          this.leftUntil = this.rightUntil = t + TAP_MS * 2;
          this.mouseUntil = 0;
        }
        if (ENTER.has(input.code)) this.slamUntil = t + 260;
        const label = modifier !== undefined ? `${LABEL_OF[modifier]}+${input.label}` : input.label;
        return { label, paw, at: "paw" };
      }
      case "button":
        if (!input.down) {
          if (this.held === input.which) this.held = null;
          return null;
        }
        this.held = input.which;
        this.clickUntil = t + TAP_MS;
        this.mouseUntil = t + MOUSE_HOLD_MS;
        return { label: BUTTON_LABELS[input.which], paw: "right", at: "mouse" };
      case "wheel": {
        // one bubble per scroll gesture, not one per wheel notch
        const fresh = !this.scroll || t > this.scroll.until || this.scroll.direction !== input.direction;
        this.scroll = { direction: input.direction, until: t + SCROLL_MS };
        this.mouseUntil = t + MOUSE_HOLD_MS;
        return fresh ? { label: input.direction > 0 ? "↑" : "↓", paw: "right", at: "mouse" } : null;
      }
      case "gaze": {
        this.noteDart(input, t);
        this.gaze = { x: input.x, y: input.y };
        this.mouseUntil = Math.max(this.mouseUntil, t + MOUSE_HOLD_MS);
        return null;
      }
    }
  }

  /** Fast pointer moves near the head wake the hunter */
  private noteDart(gaze: { x: number; y: number }, t: number): void {
    const dt = Math.max(t - this.lastGaze.t, 1);
    const speed = (Math.hypot(gaze.x - this.lastGaze.x, gaze.y - this.lastGaze.y) / dt) * 1000;
    this.lastGaze = { ...gaze, t };
    const near = Math.hypot(gaze.x, gaze.y) < NEAR;
    // strokes over the head are petting, not prey
    const petted = this.strokes.some((s) => t - s < PET_WINDOW_MS);
    if (!near || speed < DART_SPEED || petted) return;
    this.darts = [...this.darts.filter((d) => t - d < 500), t];
    if (this.darts.length < DARTS_TO_HUNT) return;
    this.huntUntil = t + HUNT_MS;
    if (t >= this.nextSwat) {
      this.swatUntil = t + SWAT_MS;
      this.nextSwat = t + SWAT_COOLDOWN_MS;
    }
  }

  /** One stroke of the pointer over the head; enough strokes in a row is petting */
  stroke(): void {
    const t = this.now();
    this.strokes = [...this.strokes.filter((s) => t - s < PET_WINDOW_MS), t];
    if (this.strokes.length >= PET_STROKES) {
      this.petUntil = t + 1500;
      this.huntUntil = this.swatUntil = 0;
      this.lastInput = t;
    }
  }

  pose(privacy: boolean): Pose {
    const t = this.now();
    if (t >= this.nextBlink) {
      const r = this.random();
      // mostly single blinks, some doubles, now and then a slow wink
      this.wink = r < 0.08 ? (this.random() < 0.5 ? "L" : "R") : null;
      this.blinkUntil = t + (this.wink ? 320 : 140);
      this.nextBlink = r > 0.75 ? t + 280 : t + 2500 + this.random() * 4000;
    }
    const idle = t - this.lastInput;
    const sleeping = idle > SLEEP_MS;
    const yawning = !privacy && idle > SLEEP_MS - YAWN_MS && !sleeping;
    const petting = t < this.petUntil;
    const onMouse = t < this.mouseUntil;

    const eyes: Eyes = petting ? "happy"
      : privacy || sleeping || yawning ? "closed"
      : idle > SLEEPY_MS ? "sleepy"
      : t < this.blinkUntil && !this.wink ? "closed"
      : "open";

    return {
      left: t < this.leftUntil ? "key" : "rest",
      right: t < this.rightUntil ? "key" : t < this.clickUntil ? "click" : onMouse ? "mouse" : "rest",
      eyes,
      gaze: eyes === "open" || eyes === "sleepy" ? this.gaze : { x: 0, y: 0 },
      pressed: !privacy && this.pressed && t < this.pressed.until ? this.pressed.code : null,
      sleeping,
      petting,
      excited: petting || t < this.huntUntil || this.recentKeys.filter((k) => t - k < EXCITED_WINDOW_MS).length >= EXCITED_KEYS,
      button: this.held,
      scroll: this.scroll && t < this.scroll.until ? this.scroll.direction : 0,
      capsLock: this.capsLock,
      numLock: this.numLock,
      chord: this.chord && t < this.chord.until ? this.chord.code : null,
      slam: t < this.slamUntil,
      yawning,
      wink: eyes === "open" && t < this.blinkUntil ? this.wink : null,
      hunting: !privacy && !sleeping && !petting && t < this.huntUntil,
      swat: !privacy && !sleeping && !petting && t < this.swatUntil,
    };
  }
}

/** Unit-length (at most) direction from the cat's head to the pointer */
export function gazeToward(pointer: { x: number; y: number }, head: { x: number; y: number }): { x: number; y: number } {
  const dx = pointer.x - head.x;
  const dy = pointer.y - head.y;
  const scale = 1 / Math.max(Math.hypot(dx, dy), 250);
  return { x: dx * scale, y: dy * scale };
}
