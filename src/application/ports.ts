import type { KeyboardFeatures } from "../domain/keyboardLayout";
import type { MouseButton, Settings } from "../domain/types";

/** Raw input as an OS adapter sees it: scan codes and pointer motion */
export type RawInput =
  | { kind: "key"; code: number; down: boolean }
  | { kind: "button"; which: MouseButton; down: boolean }
  | { kind: "wheel"; direction: 1 | -1 }
  | { kind: "move"; dx: number; dy: number }
  | { kind: "point"; x: number; y: number }
  /** The user switched to this keyboard or pointer; builtIn = part of a laptop */
  | { kind: "device"; role: "keyboard" | "pointer"; name: string; builtIn: boolean; features?: KeyboardFeatures };

export interface InputSource {
  start(emit: (input: RawInput) => void): void;
  stop(): void;
}

export interface SettingsStore {
  load(): Settings;
  save(settings: Settings): void;
}

export interface Point { x: number; y: number }
export interface Rect extends Point { width: number; height: number }
