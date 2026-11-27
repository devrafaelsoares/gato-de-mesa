import type { KeyboardFeatures } from "./keyboardLayout";

/** Optional details on top of the pattern */
export type Mark = "blaze" | "mustache" | "fluffy" | "frosted";

export type Pattern = "solid" | "tuxedo" | "bicolor" | "tabby" | "calico" | "pointed";

/** A coat: every color the drawing needs. Presets and custom coats share it. */
export interface Skin {
  id: string;
  name: string;
  pattern: Pattern;
  body: string;
  belly: string;
  accent: string;
  accent2: string;
  eyeLeft: string;
  eyeRight: string;
  nose: string;
  marks?: Mark[];
}

export type Size = "big" | "small" | "mini";

export interface Settings {
  skinId: string;
  size: Size;
  keyboardColor: string;
  mouseColor: string;
  privacy: boolean;
  greenScreen: boolean;
  position?: { x: number; y: number };
  customSkins: Skin[];
}

export type MouseButton = "left" | "right" | "middle";

/** What the cat reacts to, already independent of OS and device */
export type CatInput =
  | { type: "key"; code: number; label: string; down: boolean }
  | { type: "button"; which: MouseButton; down: boolean }
  | { type: "wheel"; direction: 1 | -1 }
  | { type: "gaze"; x: number; y: number };

/** The keyboard and pointer the user is touching right now */
export interface Devices {
  keyboard: { name: string; laptop: boolean; features: KeyboardFeatures };
  pointer: { name: string; touchpad: boolean };
}

/** Everything the window needs to draw: settings, the resolved coat, the devices in use */
export interface CatView {
  settings: Settings;
  skin: Skin;
  devices: Devices;
}
