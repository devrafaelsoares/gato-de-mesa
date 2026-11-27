// Runs inside the exporter's offscreen window. Draws the real CatView and
// exposes helpers the exporter calls to isolate one part of the cat at a time.
import type { Pose } from "../../src/domain/cat";
import { PRESET_SKINS } from "../../src/domain/catalog";
import { labelForScanCode } from "../../src/domain/keyboard";
import { CatView } from "../../src/renderer/catView";

const STILL: Pose = { left: "rest", right: "rest", eyes: "open", gaze: { x: 0, y: 0 }, pressed: null, sleeping: false, petting: false, excited: false, button: null, scroll: 0, capsLock: false, numLock: true, chord: null, slam: false, yawning: false, hunting: false, swat: false, wink: null };

/** Where the paw is parked while it is photographed, so it lands on the canvas */
export const PAW_PARK = { x: 100, y: 100 };

/** Each exported layer: the parts to show, plus tweaks to draw them at rest */
const LAYERS: Record<string, { show: string; css?: string; setup?: (svg: SVGSVGElement) => void }> = {
  tail: { show: ".tail" },
  torso: { show: ".torso" },
  feet: { show: ".feet" },
  "ear-left": { show: ".ear-left" },
  "ear-right": { show: ".ear-right" },
  "head-base": { show: ".head-base" },
  "head-shape": { show: ".head-shape", css: ".head-shape { fill: #fff !important; }" },
  face: { show: ".face-patterns", css: ".face-patterns { clip-path: none !important; }" },
  nose: { show: ".nose" },
  mouth: { show: ".mouth" },
  yawn: { show: ".yawn", css: ".yawn { display: inline !important; }" },
  "whiskers-l": { show: ".whiskers-l" },
  "whiskers-r": { show: ".whiskers-r" },
  "eye-line-L": { show: ".eye-L .eye-line", css: ".eye-line { opacity: 1 !important; }" },
  "eye-line-R": { show: ".eye-R .eye-line", css: ".eye-line { opacity: 1 !important; }" },
  "eye-happy-L": { show: ".eye-L .eye-happy", css: ".eye-happy { opacity: 1 !important; }" },
  "eye-happy-R": { show: ".eye-R .eye-happy", css: ".eye-happy { opacity: 1 !important; }" },
  paw: {
    show: ".arm-left .pad",
    setup: (svg) => svg.querySelector(".arm-left .pad")!.setAttribute("transform", `translate(${PAW_PARK.x} ${PAW_PARK.y})`),
  },
  // where a leg may show over the chest: the arm mask's soft bib
  bib: {
    show: ".export-bib",
    setup: (svg) => {
      const bib = svg.querySelector("#arm-fade path")!.cloneNode() as SVGPathElement;
      bib.setAttribute("class", "export-bib");
      svg.querySelector(".breath")!.append(bib);
    },
  },
  zzz: { show: ".zzz", css: ".zzz { display: inline !important; }" },
  heart: { show: ".heart", css: ".heart { display: inline !important; }" },
};

const FROZEN = `
  * { animation: none !important; transition: none !important; }
  .lean, .breath, .head, .face-shift, .tail, .tail-wag, .ear, .whiskers-l, .whiskers-r, .feet { transform: none !important; }
  svg *:not(defs, defs *) { visibility: hidden !important; }`;

const host = document.getElementById("cat")!;
let view: CatView;
const style = document.createElement("style");
document.head.append(style);

function draw(skinId: string): SVGSVGElement {
  view = new CatView(host);
  const skin = PRESET_SKINS.find((s) => s.id === skinId)!;
  view.paint(skin, "#efe9dc", "#efe9dc");
  for (let i = 0; i < 80; i++) view.render(STILL); // let the springs settle
  return host.querySelector("svg")!;
}

Object.assign(window, {
  skins: () => PRESET_SKINS.map((s) => s.id),
  layers: () => Object.keys(LAYERS),
  pawPark: () => PAW_PARK,
  /** Shows only one part of one coat */
  prepare(skinId: string, layer: string) {
    const svg = draw(skinId);
    const spec = LAYERS[layer];
    spec.setup?.(svg);
    style.textContent = `${FROZEN}\n${spec.show}, ${spec.show.split(",").map((s) => `${s} *`).join(", ")} { visibility: visible !important; }\n${spec.css ?? ""}`;
    svg.getBoundingClientRect();
  },
  /** Glyph atlas for keycap bubbles and menus, drawn with the system font */
  glyphs(fontCss: string, chars: string, cell: number) {
    const list = [...new Set([...chars])];
    const canvas = document.createElement("canvas");
    const cols = 16;
    canvas.width = cols * cell;
    canvas.height = Math.ceil(list.length / cols) * cell;
    const ctx = canvas.getContext("2d")!;
    ctx.font = fontCss;
    ctx.fillStyle = "#fff";
    ctx.textBaseline = "alphabetic";
    const baseline = Math.round(cell * 0.75);
    const metrics = list.map((ch, i) => {
      const x = (i % cols) * cell, y = Math.floor(i / cols) * cell;
      ctx.fillText(ch, x + 2, y + baseline);
      return `${ch.codePointAt(0)} ${x} ${y} ${ctx.measureText(ch).width.toFixed(2)}`;
    });
    return { png: canvas.toDataURL("image/png"), metrics: `${cell} ${baseline} 2\n${metrics.join("\n")}\n` };
  },
  bubbleChars() {
    let all = "EsqDirMeio+↑↓•CtrlAltGr❖0123456789";
    for (let code = 0; code < 520; code++) all += labelForScanCode(code);
    return all;
  },
});
