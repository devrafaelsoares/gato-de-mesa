import type { Pose } from "../domain/cat";
import { PATTERN_NAMES, PRESET_SKINS } from "../domain/catalog";
import type { Pattern, Skin } from "../domain/types";
import { desk } from "./bridge";
import { CatView } from "./catView";

const COLOR_FIELDS: { key: keyof Skin; label: string }[] = [
  { key: "body", label: "Pelo" },
  { key: "belly", label: "Peito e barriga" },
  { key: "accent", label: "Manchas / listras" },
  { key: "accent2", label: "Segunda mancha" },
  { key: "eyeLeft", label: "Olho esquerdo" },
  { key: "eyeRight", label: "Olho direito" },
  { key: "nose", label: "Nariz e orelhas" },
];

const STILL: Pose = { left: "rest", right: "rest", eyes: "open", gaze: { x: 0, y: 0 }, pressed: null, sleeping: false, petting: false, excited: false, button: null, scroll: 0, capsLock: false, numLock: true, chord: null, slam: false, yawning: false, hunting: false, swat: false, wink: null };

const form = document.querySelector("form")!;
const preview = new CatView(document.getElementById("preview")!);
let skin: Skin = { ...PRESET_SKINS[0], id: `custom-${Date.now()}`, name: "Meu gato" };

const fields = form.querySelector(".colors")!;
fields.innerHTML = COLOR_FIELDS.map((f) => `<label>${f.label}<input type="color" name="${f.key}"></label>`).join("");
const patternSelect = form.querySelector<HTMLSelectElement>("[name=pattern]")!;
patternSelect.innerHTML = (Object.keys(PATTERN_NAMES) as Pattern[]).map((p) => `<option value="${p}">${PATTERN_NAMES[p]}</option>`).join("");

function fill(): void {
  for (const input of form.querySelectorAll<HTMLInputElement | HTMLSelectElement>("[name]")) {
    input.value = String(skin[input.name as keyof Skin]);
  }
  draw();
}

function draw(): void {
  preview.paint(skin, "#2b2b30", "#efe9dc");
  preview.render(STILL);
}

form.addEventListener("input", (e) => {
  const input = e.target as HTMLInputElement;
  skin = { ...skin, [input.name]: input.value };
  draw();
});

form.addEventListener("submit", async (e) => {
  e.preventDefault();
  await desk.saveSkin({ ...skin, name: skin.name.trim() || "Meu gato" });
  window.close();
});

fill();
