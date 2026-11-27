import { FULL_KEYBOARD } from "./keyboardLayout";
import type { Devices, Pattern, Settings, Size, Skin } from "./types";

const skin = (id: string, name: string, pattern: Pattern, body: string, belly: string, accent: string, accent2: string, eyes: string, nose: string, eyeRight = eyes): Skin =>
  ({ id, name, pattern, body, belly, accent, accent2, eyeLeft: eyes, eyeRight, nose });

export const PRESET_SKINS: readonly Skin[] = [
  // Felix: long-haired tuxedo, thin blaze off to one side, black nose patch running
  // into a mustache, white flecks on the forehead, honey-amber eyes
  { ...skin("felix", "Felix", "tuxedo", "#17161a", "#f6f4f0", "#17161a", "#17161a", "#cfa23b", "#2a2224"), marks: ["blaze", "mustache", "fluffy", "frosted"] },
  skin("preto-verde", "Preto · olhos verdes", "solid", "#17171a", "#17171a", "#17171a", "#17171a", "#b9d35a", "#2e2e33"),
  skin("preto-dourado", "Preto · olhos dourados", "solid", "#17171a", "#17171a", "#17171a", "#17171a", "#f2b632", "#2e2e33"),
  skin("frajola", "Frajola", "tuxedo", "#1c1c20", "#f7f5f0", "#1c1c20", "#1c1c20", "#b9d35a", "#e8a0a8"),
  skin("laranja", "Laranja rajado", "tabby", "#e8913a", "#f6c48a", "#c56a1f", "#c56a1f", "#8fbf4a", "#d9776a"),
  skin("tricolor", "Tricolor", "calico", "#f7f3ec", "#f7f3ec", "#e58a3a", "#2a2523", "#d6a531", "#f0a3ad"),
  skin("escaminha", "Escaminha", "calico", "#2a2421", "#2a2421", "#c9642b", "#e8a35f", "#8fbf4a", "#6b4a44"),
  skin("cinza", "Cinza", "solid", "#7d8494", "#8d94a3", "#7d8494", "#7d8494", "#f0a830", "#c98b93"),
  skin("cinza-branco", "Cinza e branco", "bicolor", "#6f7787", "#f5f3ef", "#6f7787", "#6f7787", "#e9a53a", "#e8a0a8"),
  skin("rajado-cinza", "Rajado cinza", "tabby", "#9aa0a6", "#c9ccd0", "#5d6268", "#5d6268", "#7fc06b", "#c98b93"),
  skin("branco", "Branco", "solid", "#f4f2ee", "#f4f2ee", "#f4f2ee", "#f4f2ee", "#5aa7e0", "#f0a3ad", "#e9b23a"),
  skin("siames", "Siamês", "pointed", "#efe3cf", "#f6eee0", "#5b4334", "#5b4334", "#4aa3e8", "#4a3a32"),
  skin("creme", "Creme", "tabby", "#e9cfa6", "#f5e7cf", "#cfa978", "#cfa978", "#d9a441", "#e59a8e"),
];

export const PATTERN_NAMES: Record<Pattern, string> = {
  solid: "Liso", tuxedo: "Frajola", bicolor: "Bicolor", tabby: "Rajado", calico: "Tricolor", pointed: "Siamês",
};

/** Window width in pixels for each size */
export const SIZES: Record<Size, { width: number; name: string }> = {
  big: { width: 440, name: "Grande" },
  small: { width: 310, name: "Pequeno" },
  mini: { width: 190, name: "Mini" },
};

/** Height / width of the drawing (viewBox 224 x 210) */
export const ASPECT = 222 / 224;

export const DESK_COLORS: readonly { name: string; value: string }[] = [
  { name: "Creme", value: "#efe9dc" },
  { name: "Cinza", value: "#b9bcc4" },
  { name: "Rosa", value: "#f3c1cf" },
  { name: "Menta", value: "#bfe3d3" },
  { name: "Azul", value: "#2f4a8a" },
  { name: "Amarelo", value: "#f2c14e" },
];

export const DEFAULT_SETTINGS: Settings = {
  skinId: "preto-verde",
  size: "small",
  keyboardColor: "#efe9dc",
  mouseColor: "#efe9dc",
  privacy: false,
  greenScreen: false,
  customSkins: [],
};

export const DEFAULT_DEVICES: Devices = {
  keyboard: { name: "", laptop: false, features: FULL_KEYBOARD },
  pointer: { name: "", touchpad: false },
};

export const allSkins = (settings: Settings): Skin[] => [...PRESET_SKINS, ...settings.customSkins];

export const resolveSkin = (settings: Settings): Skin =>
  allSkins(settings).find((s) => s.id === settings.skinId) ?? PRESET_SKINS[0];
