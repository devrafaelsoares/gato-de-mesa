import type { Bubble, Paw, Pose } from "../domain/cat";
import { buildLayout, FULL_KEYBOARD, type KeyboardFeatures } from "../domain/keyboardLayout";
import type { Devices, Skin } from "../domain/types";

interface Point { x: number; y: number }

// The cat faces the viewer, so its left paw is on the viewer's right
// and its right paw (the mouse paw) on the viewer's left.
const SHOULDER: Record<Paw, Point> = { left: { x: 117, y: 108 }, right: { x: 83, y: 108 } };
const REST: Record<Paw, Point> = { left: { x: 122, y: 166 }, right: { x: 78, y: 166 } };
const MOUSE: Point = { x: 30, y: 194 };
const PIVOT: Point = { x: 100, y: 172 };
const LEAN_DEG = -10;
/** Longest a foreleg gets: beyond it the paw stops short instead of stretching */
const MAX_REACH = 92;
const PAWS: Paw[] = ["left", "right"];

const lerp = (a: number, b: number, t: number) => a + (b - a) * t;
const approach = (from: Point, to: Point, k: number): Point => ({ x: lerp(from.x, to.x, k), y: lerp(from.y, to.y, k) });

/** Pulls a paw target back so the leg never stretches past MAX_REACH */
function withinReach(shoulder: Point, target: Point): Point {
  const dx = target.x - shoulder.x, dy = target.y - shoulder.y, length = Math.hypot(dx, dy);
  if (length <= MAX_REACH) return target;
  return { x: shoulder.x + (dx / length) * MAX_REACH, y: shoulder.y + (dy / length) * MAX_REACH };
}

/** A point of the body after the body leans by `deg` around its base */
function leaned(p: Point, deg: number): Point {
  const a = (deg * Math.PI) / 180, dx = p.x - PIVOT.x, dy = p.y - PIVOT.y;
  return { x: PIVOT.x + dx * Math.cos(a) - dy * Math.sin(a), y: PIVOT.y + dx * Math.sin(a) + dy * Math.cos(a) };
}

// Keyboard in perspective, turned towards the cat: its nearest row (space
// bar) at the top, mirrored left/right, centred on the cat.
const KB_TOP = 175, KB_BOTTOM = 204, UNIT = 4.1, BACK_SCALE = 0.82;

/** Keys with a colored cap, like on a custom keyboard */
const ACCENT_KEYS = new Set([1, 28, 96]);

interface KeyboardDrawing { markup: string; centers: Map<number, Point>; mouseX: number }

function drawKeyboard(features: KeyboardFeatures): KeyboardDrawing {
  const { keys, width, rows } = buildLayout(features);
  const cx = PIVOT.x;
  const step = (KB_BOTTOM - KB_TOP - 3) / rows, keyH = step * 0.78;
  const scaleAt = (y: number) => lerp(BACK_SCALE, 1, (y - KB_TOP) / (KB_BOTTOM - KB_TOP));
  const f = (n: number) => n.toFixed(2);
  const centers = new Map<number, Point>();

  const caps = keys.map((k) => {
    const y = KB_TOP + 1.5 + (rows - 1 - k.row) * step;
    const s = scaleAt(y + keyH / 2);
    const x = cx + (width - k.x - k.w - width / 2) * UNIT * s + 0.45 * s;
    const w = k.w * UNIT * s - 0.9 * s;
    if (!centers.has(k.code)) centers.set(k.code, { x: x + w / 2, y: y + keyH / 2 });
    return `<g class="cap${ACCENT_KEYS.has(k.code) ? " accent" : ""}" data-code="${k.code}"><rect class="cap-side" x="${f(x)}" y="${f(y + 1.5)}" width="${f(w)}" height="${f(keyH)}" rx=".8"/>` +
      `<rect class="cap-top" x="${f(x)}" y="${f(y)}" width="${f(w)}" height="${f(keyH)}" rx=".8"/></g>`;
  }).join("");

  const half = (s: number) => (width / 2) * UNIT * s + 3;
  const tl = cx - half(BACK_SCALE), tr = cx + half(BACK_SCALE), bl = cx - half(1), br = cx + half(1);
  const B = KB_BOTTOM, T = KB_TOP;
  const front = `M${f(bl)} ${B - 2} L${f(br)} ${B - 2} L${f(br)} ${B + 6} Q${f(br)} ${B + 9} ${f(br - 3)} ${B + 9} L${f(bl + 3)} ${B + 9} Q${f(bl)} ${B + 9} ${f(bl)} ${B + 6} Z`;
  const deck = `M${f(tl + 2)} ${T - 1.5} L${f(tr - 2)} ${T - 1.5} Q${f(tr)} ${T - 1.5} ${f(tr + 1)} ${T + 1} L${f(br)} ${B - 1} Q${f(br)} ${B + 1} ${f(br - 3)} ${B + 1} ` +
    `L${f(bl + 3)} ${B + 1} Q${f(bl)} ${B + 1} ${f(bl)} ${B - 1} L${f(tl - 1)} ${T + 1} Q${f(tl)} ${T - 1.5} ${f(tl + 2)} ${T - 1.5} Z`;
  const markup = `
    <ellipse cx="${cx}" cy="${B + 9}" rx="${f((br - bl) / 2 + 2)}" ry="4" fill="rgba(0,0,0,.2)"/>
    <path d="${front}" fill="var(--kb)"/><path d="${front}" fill="url(#kb-front)"/>
    <path d="${deck}" fill="var(--kb)" stroke="rgba(0,0,0,.25)" stroke-width=".8"/>
    <path d="${deck}" fill="url(#kb-deck)"/>
    ${caps}
    <circle class="led led-num" cx="${f(bl + 7)}" cy="${B + 3.6}" r=".9"/>
    <circle class="led led-caps" cx="${f(bl + 10.5)}" cy="${B + 3.6}" r=".9"/>
    <circle class="led" cx="${f(bl + 14)}" cy="${B + 3.6}" r=".9"/>`;
  // the mouse sits beside the keyboard, on the cat's right
  return { markup, centers, mouseX: bl - 12 };
}

// Mouse on the cat's right, buttons pointing away from the cat
const M = MOUSE.y;
const MOUSE_SVG = `
  <g class="mouse">
    <defs><clipPath id="mouse-shape"><ellipse cx="0" cy="${M}" rx="10" ry="13"/></clipPath></defs>
    <ellipse cx="1.5" cy="${M + 5}" rx="11.5" ry="13.5" fill="rgba(0,0,0,.2)"/>
    <ellipse cx="0" cy="${M + 3}" rx="10" ry="13" fill="var(--mouse)"/>
    <ellipse cx="0" cy="${M + 3}" rx="10" ry="13" fill="rgba(0,0,0,.32)"/>
    <ellipse cx="0" cy="${M}" rx="10" ry="13" fill="var(--mouse)"/>
    <ellipse cx="0" cy="${M}" rx="10" ry="13" fill="url(#mouse-shine)"/>
    <g clip-path="url(#mouse-shape)">
      <rect class="mb mb-right" x="-10" y="${M + 2}" width="10" height="12"/>
      <rect class="mb mb-left" x="0" y="${M + 2}" width="10" height="12"/>
      <ellipse cx="-3" cy="${M - 5}" rx="4" ry="5" fill="#fff" opacity=".18"/>
    </g>
    <path d="M-9.6 ${M + 2.4} Q0 ${M} 9.6 ${M + 2.4} M0 ${M + 1.2} L0 ${M + 13}" stroke="rgba(0,0,0,.4)" stroke-width=".9" fill="none"/>
    <rect class="wheel" x="-1.6" y="${M + 3.5}" width="3.2" height="6" rx="1.6"/>
    <path class="wheel-ridges" d="M-1.1 ${M + 5} L1.1 ${M + 5} M-1.1 ${M + 6.5} L1.1 ${M + 6.5} M-1.1 ${M + 8} L1.1 ${M + 8}" stroke="rgba(255,255,255,.55)" stroke-width=".5"/>
    <ellipse cx="0" cy="${M}" rx="10" ry="13" fill="none" stroke="rgba(0,0,0,.45)" stroke-width="1.4"/>
  </g>`;
// Buttons point away from the cat, so its left button is on the viewer's right

// Laptop users get a touchpad where the mouse would be
const TOUCHPAD_SVG = `
  <g class="touchpad">
    <rect x="-13" y="${MOUSE.y - 8}" width="26" height="20" rx="4" fill="rgba(0,0,0,.22)"/>
    <rect x="-13" y="${MOUSE.y - 10}" width="26" height="20" rx="4" fill="var(--mouse)" stroke="rgba(0,0,0,.45)" stroke-width="1.5"/>
    <path d="M-13 ${MOUSE.y + 5} L13 ${MOUSE.y + 5}" stroke="rgba(0,0,0,.3)" stroke-width="1"/>
  </g>`;

/** A whole foreleg, shoulder to paw, so the paw never detaches from the body */
const pawShape = (cls: string) => `<g class="${cls}"><ellipse rx="10.5" ry="6" fill="var(--paw)"/>` +
  `<path d="M-9 2.6 Q0 8.4 9 2.6" stroke="var(--seam)" stroke-width="1.6" fill="none" stroke-linecap="round"/></g>`;

/** A tail as a filled shape, thick at the root and tapering to a round tip */
function taperedTail(w0: number, w1: number): string {
  const P = [{ x: 124, y: 160 }, { x: 170, y: 168 }, { x: 186, y: 134 }, { x: 168, y: 108 }];
  const at = (t: number) => {
    const u = 1 - t;
    const x = u ** 3 * P[0].x + 3 * u * u * t * P[1].x + 3 * u * t * t * P[2].x + t ** 3 * P[3].x;
    const y = u ** 3 * P[0].y + 3 * u * u * t * P[1].y + 3 * u * t * t * P[2].y + t ** 3 * P[3].y;
    const tx = 3 * u * u * (P[1].x - P[0].x) + 6 * u * t * (P[2].x - P[1].x) + 3 * t * t * (P[3].x - P[2].x);
    const ty = 3 * u * u * (P[1].y - P[0].y) + 6 * u * t * (P[2].y - P[1].y) + 3 * t * t * (P[3].y - P[2].y);
    const len = Math.hypot(tx, ty), w = lerp(w0, w1, t);
    return { a: `${(x - (ty / len) * w).toFixed(1)} ${(y + (tx / len) * w).toFixed(1)}`, b: `${(x + (ty / len) * w).toFixed(1)} ${(y - (tx / len) * w).toFixed(1)}`, tip: { x, y, tx: tx / len, ty: ty / len } };
  };
  const pts = Array.from({ length: 13 }, (_, i) => at(i / 12));
  const end = pts[12].tip;
  const cap = `${(end.x + end.tx * w1 * 1.3).toFixed(1)} ${(end.y + end.ty * w1 * 1.3).toFixed(1)}`;
  return `M${pts.map((p) => p.a).join(" L")} Q${cap} ${pts[12].b} L${pts.map((p) => p.b).reverse().slice(1).join(" L")} Z`;
}

/**
 * A leg grows out of the body: over the body it lets the fur behind show
 * through, past the body's edge it is fur colored, and along the forearm it
 * shades into the paw color. No outline, no shoulder patch, nothing glued on.
 */
const arm = (paw: Paw) => `<g class="arm arm-${paw}">
    <defs><clipPath id="arm-clip-${paw}"><path class="limb"/></clipPath>
    <linearGradient id="arm-shade-${paw}" class="shade" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="var(--paw)" stop-opacity="0"/><stop offset="1" stop-color="var(--paw)"/></linearGradient></defs>
    <g clip-path="url(#arm-clip-${paw})"><rect x="-60" y="0" width="320" height="260" fill="var(--body)" mask="url(#outside-body)"/><rect x="-60" y="0" width="320" height="260" fill="url(#arm-shade-${paw})"/></g>
    ${pawShape("pad")}</g>`;

/** Where the fur turns into paw color, as a fraction of the leg: a crisp sock, or Siamese points darkening slowly */
const SOCK = { from: 0.5, span: 0.12 }, POINTS = { from: 0.35, span: 0.4 };

/**
 * A foreleg's outline: thick at the shoulder, slimmer at the wrist,
 * bowed a little at the elbow, with a rounded shoulder.
 */
function foreleg(s: Point, p: Point, paw: Paw): string {
  const c = { x: (s.x + p.x) / 2 + (paw === "left" ? 3 : -3), y: (s.y + p.y) / 2 };
  const at = (t: number) => {
    const u = 1 - t;
    const x = u * u * s.x + 2 * u * t * c.x + t * t * p.x, y = u * u * s.y + 2 * u * t * c.y + t * t * p.y;
    const tx = 2 * u * (c.x - s.x) + 2 * t * (p.x - c.x), ty = 2 * u * (c.y - s.y) + 2 * t * (p.y - c.y);
    const len = Math.hypot(tx, ty) || 1;
    const w = lerp(9.5, 6.5, t) + Math.sin(t * Math.PI) * 0.8; // forearm muscle
    return { a: { x: x - (ty / len) * w, y: y + (tx / len) * w }, b: { x: x + (ty / len) * w, y: y - (tx / len) * w } };
  };
  const pts = Array.from({ length: 9 }, (_, i) => at(i / 8));
  const n = (v: Point) => `${v.x.toFixed(1)} ${v.y.toFixed(1)}`;
  const side = (from: number, key: "a" | "b") => pts.slice(from).map((q) => n(q[key]));
  const limb = `M${side(0, "a").join(" L")} L${side(0, "b").reverse().join(" L")} Z`;
  // a rounded shoulder on top
  const cx = s.x, cy = s.y + 3, rx = 9.5, ry = 11;
  const shoulder = `M${cx - rx} ${cy} A${rx} ${ry} 0 1 0 ${cx + rx} ${cy} A${rx} ${ry} 0 1 0 ${cx - rx} ${cy} Z`;
  return `${limb} ${shoulder}`;
}

// Almond eyes: rounder below, the outer corner a little higher than the inner one
const ALMOND = {
  L: "M-9 0 A9 9 0 1 0 9 0 A9 9 0 1 0 -9 0 Z",
  R: "M-9 0 A9 9 0 1 0 9 0 A9 9 0 1 0 -9 0 Z",
};

const eye = (x: number, side: "L" | "R") => `
  <g class="eye eye-${side}" transform="translate(${x} 70) scale(1.15)">
    <defs><clipPath id="eye-clip-${side}"><path d="${ALMOND[side]}"/></clipPath></defs>
    <g class="eye-open">
      <path d="${ALMOND[side]}" fill="var(--eye${side})"/>
      <g clip-path="url(#eye-clip-${side})"><g class="pupil"><circle r="5.6" fill="#111"/></g></g>
    </g>
    <path class="eye-line" d="M-8 0 Q0 6 8 0 M${side === "L" ? "7 1 L10 -1.5" : "-7 1 L-10 -1.5"}" stroke="var(--lid)"/>
    <path class="eye-happy" d="M-7.5 3 Q0 -5 7.5 3" stroke="var(--lid)"/>
  </g>`;

/** Wider than tall, flat on top, full cheeks: a cat head, not a ball */
const HEAD = "M100 38 C125 38 142 49 145 68 C148 90 129 107 100 107 C71 107 52 90 55 68 C58 49 75 38 100 38 Z";

const BIB = "M84 92 C93 99 107 99 116 92 C125 106 130 128 130 170 L70 170 C70 128 75 106 84 92 Z";

const BODY = "M72 84 C60 94 55 114 55 134 C55 150 57 160 63 166 C80 172 120 172 137 166 C143 160 145 150 145 134 C145 114 140 94 128 84 Z";

const ear = (side: Paw, outer: string, inner: string, tufts: string) => `
  <g class="ear ear-${side}"><g transform="translate(${side === "left" ? 76 : 124} 54) rotate(${side === "left" ? -6 : 6}) scale(1.14) translate(${side === "left" ? -76 : -124} -54)">
    <path d="${outer}" fill="var(--body)"/><path class="p-pointed" d="${outer}" fill="var(--accent)"/>
    <path d="${inner}" fill="var(--nose)" opacity=".55"/>
    <path class="m-fluffy" d="${tufts}" stroke="var(--belly)" stroke-width="1.1" stroke-linecap="round" fill="none" opacity=".9"/>
  </g></g>`;

const TEMPLATE = `
<svg viewBox="-24 0 224 222" xmlns="http://www.w3.org/2000/svg">
  <defs>
    <clipPath id="tail-tip"><rect x="150" y="90" width="50" height="42"/></clipPath>
    <clipPath id="clip-body"><path d="${BODY}"/></clipPath>
    <mask id="outside-body" maskUnits="userSpaceOnUse" x="-60" y="0" width="320" height="260"><rect x="-60" y="0" width="320" height="260" fill="#fff"/><path d="${BODY}" fill="#000"/></mask>
    <linearGradient id="cap-shine" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#ffffff"/><stop offset="1" stop-color="#ebe5d8"/>
    </linearGradient>
    <!-- long-haired coats: edges break into tufts -->
    <filter id="fur" x="-25%" y="-25%" width="150%" height="150%">
      <feTurbulence type="fractalNoise" baseFrequency=".9 .35" numOctaves="1" seed="7"/>
      <feDisplacementMap in="SourceGraphic" scale="1.8" xChannelSelector="R" yChannelSelector="G"/>
    </filter>
    <filter id="chin-shadow" x="-30%" y="-100%" width="160%" height="300%"><feGaussianBlur stdDeviation="2.2"/></filter>
    <filter id="soft-fur" x="-10%" y="-10%" width="120%" height="120%"><feGaussianBlur stdDeviation=".45"/></filter>
    <linearGradient id="ear-inner" x1="0" y1="1" x2="0" y2="0">
      <stop offset="0" stop-color="var(--nose)" stop-opacity=".9"/><stop offset="1" stop-color="var(--nose)" stop-opacity=".35"/>
    </linearGradient>
    <linearGradient id="kb-deck" x1="0" y1="0" x2="1" y2="1">
      <stop offset="0" stop-color="#fff" stop-opacity=".35"/><stop offset="1" stop-color="#000" stop-opacity=".12"/>
    </linearGradient>
    <linearGradient id="kb-front" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#000" stop-opacity=".18"/><stop offset="1" stop-color="#000" stop-opacity=".42"/>
    </linearGradient>
    <radialGradient id="mouse-shine" cx=".35" cy=".3" r=".8">
      <stop offset="0" stop-color="#fff" stop-opacity=".45"/><stop offset=".55" stop-color="#fff" stop-opacity="0"/><stop offset="1" stop-color="#000" stop-opacity=".22"/>
    </radialGradient>
    <!-- light from the top left, shadow bottom right: volume on any coat -->
    <radialGradient id="shade" cx="35%" cy="25%" r="85%">
      <stop offset="0" stop-color="#fff" stop-opacity=".16"/>
      <stop offset=".55" stop-color="#fff" stop-opacity="0"/>
      <stop offset="1" stop-color="#000" stop-opacity=".3"/>
    </radialGradient>
    <clipPath id="clip-head"><path d="${HEAD}"/></clipPath>
  </defs>
  <g class="keyboard"></g>
  ${MOUSE_SVG}
  ${TOUCHPAD_SVG}
  <g class="lean">
  <g class="tail"><g class="tail-wag">
    <path class="tail-short" d="${taperedTail(4, 3.6)}" fill="var(--body)"/>
    <path class="tail-long" d="${taperedTail(5.5, 5)}" fill="var(--body)"/>
    <g class="p-pointed" clip-path="url(#tail-tip)"><path class="tail-short" d="${taperedTail(4, 3.6)}" fill="var(--accent)"/><path class="tail-long" d="${taperedTail(5.5, 5)}" fill="var(--accent)"/></g>
  </g></g>
  <g class="breath">
    <g class="feet">
      <g transform="translate(64 168) rotate(14) scale(1.1)">${pawShape("foot")}</g>
      <g transform="translate(136 168) rotate(-14) scale(1.1)">${pawShape("foot")}</g>
    </g>
    <g class="torso"><path d="${BODY}" fill="var(--body)"/>
    <g clip-path="url(#clip-body)">
      <path d="${BIB}" fill="var(--belly)"/>
      <path class="m-fluffy" d="M80 126 L84 114 L88 122 L92 110 L96 120 L100 108 L104 120 L108 110 L112 122 L116 114 L120 126 Z" fill="var(--belly)"/>
      <rect class="p-bicolor" x="0" y="128" width="200" height="80" fill="var(--belly)"/>
      <g class="p-calico"><ellipse cx="126" cy="142" rx="20" ry="18" fill="var(--accent)"/><ellipse cx="72" cy="152" rx="14" ry="16" fill="var(--accent2)"/></g>
      <path class="p-tabby" stroke="var(--accent)" stroke-width="3.2" stroke-linecap="round" fill="none"
        d="M62 128 L77 133 M60 141 L75 145 M138 128 L123 133 M140 141 L125 145"/>
    </g></g>
    <g class="head"><g transform="translate(8 0) scale(.92)">
      ${ear("left", "M58 62 C57 46 59 30 63 22 Q66 17 71 21 C79 28 88 36 94 45 Z", "M65 53 C64 43 65 34 67 29 Q69 26 72 29 C77 34 82 39 86 46 Z", "M68 50 Q63 41 65 32 M72 48 Q68 40 70 33 M76 47 Q74 41 76 36 M80 46 Q80 42 82 39")}
      ${ear("right", "M142 62 C143 46 141 30 137 22 Q134 17 129 21 C121 28 112 36 106 45 Z", "M135 53 C136 43 135 34 133 29 Q131 26 128 29 C123 34 118 39 114 46 Z", "M132 50 Q137 41 135 32 M128 48 Q132 40 130 33 M124 47 Q126 41 124 36 M120 46 Q120 42 118 39")}
      <g class="head-base"><path class="m-fluffy" d="M60 68 L48 77 L58 81 L49 89 L61 91 Z M140 68 L152 77 L142 81 L151 89 L139 91 Z" fill="var(--body)"/>
      <path class="head-shape" d="${HEAD}" fill="var(--body)"/></g>
      <g class="face-patterns" clip-path="url(#clip-head)"><g class="face-shift">
        <ellipse class="p-tuxedo p-bicolor" cx="100" cy="90" rx="19" ry="13" fill="var(--belly)"/>
        <g class="p-calico"><ellipse cx="74" cy="58" rx="26" ry="20" fill="var(--accent)"/><ellipse cx="130" cy="80" rx="18" ry="22" fill="var(--accent2)"/></g>
        <path class="p-tabby" stroke="var(--accent)" stroke-width="3.2" stroke-linecap="round" fill="none"
          d="M92 40 L94 52 M100 38 L100 53 M108 40 L106 52 M57 70 L70 73 M57 79 L70 79 M143 70 L130 73 M143 79 L130 79"/>
        <path class="m-blaze" d="M100 42 C99.5 56 97 70 92 86 L100 86 C100.5 70 101.5 56 100 42 Z" fill="var(--belly)"/>
        <g class="m-mustache" fill="var(--body)">
          <path d="M95.5 84 C94.5 79.5 97.5 77 101.5 76.5 C107 76 113 77 121 79 C122 85 120 91 115.5 93.5 C112 95 108.5 94.6 105.5 93.4 C103.5 94 101 92.6 99.5 91 C97.5 89.5 96 87 95.5 84 Z"/>
        </g>
        <path class="m-frosted" d="M88 50 l1 2.5 M93 46 l.6 2.4 M107 47 l-.8 2.4 M112 52 l-1 2.2 M85 57 l1.2 2 M115 58 l-1 2" stroke="var(--belly)" stroke-width="1" stroke-linecap="round" opacity=".7"/>
        <ellipse class="p-pointed" cx="100" cy="84" rx="23" ry="19" fill="var(--accent)" opacity=".92"/>
      </g></g>
      <g class="face-shift">${eye(78, "L")}${eye(122, "R")}
      <path class="nose" d="M96 84 L104 84 L100 89 Z" fill="var(--nose)"/>
      <path class="mouth" d="M100 89 Q96 94 92 92 M100 89 Q104 94 108 92" stroke="var(--mouth)" stroke-width="1.1" fill="none" stroke-linecap="round"/>
      <ellipse class="yawn" cx="100" cy="94" rx="4.5" ry="5.5" fill="#5a2630" stroke="var(--mouth)" stroke-width=".8"/>
      <g class="whiskers" stroke="var(--whisker)" fill="none" stroke-linecap="round">
        <g class="whiskers-l"><path d="M88 87 Q70 81 50 82" stroke-width="1"/><path d="M88 90 Q70 89 48 92" stroke-width=".9"/><path d="M88 92 Q71 96 52 101" stroke-width=".8"/></g>
        <g class="whiskers-r"><path d="M112 87 Q130 81 150 82" stroke-width="1"/><path d="M112 90 Q130 89 152 92" stroke-width=".9"/><path d="M112 92 Q129 96 148 101" stroke-width=".8"/></g>
      </g>
      </g>
      <ellipse class="pet-zone" cx="100" cy="62" rx="46" ry="40" fill="transparent"/>
    </g></g>
    ${arm("left")}${arm("right")}
  </g></g>
  <text class="zzz" x="140" y="40">z z</text>
  <text class="heart" x="100" y="30" text-anchor="middle">♥</text>
  <g class="bubbles"></g>
</svg>`;

/** Relative luminance, to pick whiskers that show on any coat */
function luminance(hex: string): number {
  const n = parseInt(hex.slice(1), 16);
  const [r, g, b] = [n >> 16, (n >> 8) & 255, n & 255].map((v) => v / 255);
  return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}

/** The color around the mouth: white on tuxedo and bicolor coats */
const muzzleColor = (skin: Skin): string =>
  skin.pattern === "tuxedo" || skin.pattern === "bicolor" ? skin.belly : skin.pattern === "pointed" ? skin.accent : skin.body;

const pawColor = (skin: Skin): string =>
  skin.pattern === "tuxedo" || skin.pattern === "bicolor" ? skin.belly
  : skin.pattern === "pointed" ? skin.accent
  : skin.body;

/** Draws the cat and the desk. Knows nothing about input or timing rules. */
export class CatView {
  private readonly svg: SVGSVGElement;
  private readonly bubbles: SVGGElement;
  private shown: Record<string, string> = {};
  private pressedCaps: Element[] = [];
  private paws: Record<Paw, Point> = { left: { ...REST.left }, right: { ...REST.right } };
  private lean = 0;
  private mouseOffset: Point = { x: 0, y: 0 };
  private readonly arms: Record<Paw, { limb: SVGPathElement; shade: SVGLinearGradientElement; pad: SVGGElement }>;
  private pointed = false;
  private readonly leanGroup: SVGGElement;
  private readonly mouse: SVGGElement;
  private readonly keyboard: SVGGElement;
  private readonly touchpad: SVGGElement;
  private keyCenters = new Map<number, Point>();
  private keyboardShape = "";
  private mouseX = MOUSE.x;

  constructor(host: HTMLElement) {
    host.innerHTML = TEMPLATE;
    this.svg = host.querySelector("svg")!;
    this.bubbles = this.svg.querySelector(".bubbles")!;
    this.leanGroup = this.svg.querySelector(".lean")!;
    this.mouse = this.svg.querySelector(".mouse")!;
    this.keyboard = this.svg.querySelector(".keyboard")!;
    this.touchpad = this.svg.querySelector(".touchpad")!;
    this.useKeyboard(FULL_KEYBOARD);
    const armOf = (paw: Paw) => ({ limb: this.svg.querySelector<SVGPathElement>(`.arm-${paw} .limb`)!, shade: this.svg.querySelector<SVGLinearGradientElement>(`.arm-${paw} .shade`)!, pad: this.svg.querySelector<SVGGElement>(`.arm-${paw} .pad`)! });
    this.arms = { left: armOf("left"), right: armOf("right") };
    this.twitchEars();
  }

  /** Now and then one ear flicks, twice, like a real cat listening */
  twitchEars(): void {
    setTimeout(() => {
      const ear = this.svg.querySelector(Math.random() < 0.5 ? ".ear-left" : ".ear-right")!;
      const flick = (delay: number) => setTimeout(() => {
        ear.classList.add("twitch");
        setTimeout(() => ear.classList.remove("twitch"), 130);
      }, delay);
      flick(0);
      flick(260);
      this.twitchEars();
    }, 4000 + Math.random() * 6000);
  }

  onPet(stroke: () => void): void {
    this.svg.querySelector(".pet-zone")!.addEventListener("pointermove", stroke);
  }

  paint(skin: Skin, keyboardColor: string, mouseColor: string): void {
    const vars: Record<string, string> = {
      body: skin.body, belly: skin.belly, accent: skin.accent, accent2: skin.accent2,
      eyeL: skin.eyeLeft, eyeR: skin.eyeRight, nose: skin.nose, paw: pawColor(skin),
      line: luminance(skin.body) < 0.35 ? "rgba(255,255,255,.55)" : "rgba(0,0,0,.5)",
      seam: luminance(pawColor(skin)) < 0.35 ? "rgba(255,255,255,.18)" : "rgba(0,0,0,.14)",
      whisker: luminance(skin.body) < 0.35 ? "rgba(255,255,255,.85)" : "rgba(40,36,40,.55)",
      lid: luminance(skin.body) < 0.35 ? "#e4ded3" : "#3a3236",
      // a muzzle split between white and black needs a mouth that shows on both
      mouth: skin.marks?.includes("mustache") ? "#8a8a92" : luminance(muzzleColor(skin)) < 0.35 ? "rgba(255,255,255,.6)" : "rgba(0,0,0,.55)",
      kb: keyboardColor, mouse: mouseColor,
    };
    for (const [name, value] of Object.entries(vars)) this.svg.style.setProperty(`--${name}`, value);
    this.svg.dataset.pattern = skin.pattern;
    this.pointed = skin.pattern === "pointed";
    this.svg.dataset.marks = (skin.marks ?? []).join(" ");
  }

  /** Draws the keyboard and pointer the user actually has in hand */
  showDevices(devices: Devices): void {
    this.useKeyboard(devices.keyboard.features);
    this.set("keyboard", devices.keyboard.laptop ? "laptop" : "external");
    this.set("pointer", devices.pointer.touchpad ? "touchpad" : "mouse");
  }

  render(pose: Pose): void {
    this.animate(pose);
    this.set("left", pose.left);
    this.set("right", pose.right);
    this.set("eyes", pose.eyes);
    this.set("sleeping", String(pose.sleeping));
    this.set("petting", String(pose.petting));

    this.set("excited", String(pose.excited));
    this.set("button", pose.button ?? "none");
    this.set("scroll", String(pose.scroll));
    this.set("caps", String(pose.capsLock));
    this.set("num", String(pose.numLock));
    this.set("slam", String(pose.slam));
    this.set("yawning", String(pose.yawning));
    this.set("hunting", String(pose.hunting));
    this.set("wink", pose.wink ?? "none");
    const { x, y } = pose.gaze;
    this.vars({
      // pupils, head and the mouse all follow the pointer, each a little less
      px: `${(x * 3.2).toFixed(1)}px`, py: `${(y * 2.6).toFixed(1)}px`,
      fx: `${(2.5 + x * 5).toFixed(1)}px`, fy: `${(y * 2.5).toFixed(1)}px`,
      hx: `${(x * 2.5).toFixed(1)}px`, hy: `${(y * 1.6).toFixed(1)}px`, tilt: `${(x * 5).toFixed(1)}deg`,
    });

    const down = [pose.pressed, pose.chord].filter((c) => c !== null)
      .flatMap((c) => [...this.keyboard.querySelectorAll(`[data-code="${c}"]`)]);
    if (down.length !== this.pressedCaps.length || down.some((c, i) => c !== this.pressedCaps[i])) {
      this.pressedCaps.forEach((c) => c.classList.remove("down"));
      down.forEach((c) => c.classList.add("down"));
      this.pressedCaps = down;
    }
  }

  /** Redraws the keyboard when the one in use has a different layout */
  private useKeyboard(features: KeyboardFeatures): void {
    const shape = JSON.stringify(features);
    if (shape === this.keyboardShape) return;
    this.keyboardShape = shape;
    const drawing = drawKeyboard(features);
    this.keyboard.innerHTML = drawing.markup;
    this.keyCenters = drawing.centers;
    this.mouseX = drawing.mouseX;
    this.touchpad.setAttribute("transform", `translate(${drawing.mouseX.toFixed(1)} 0)`);
    this.pressedCaps = [];
  }

  /** Springs the body, arms and mouse towards where the pose wants them */
  private animate(pose: Pose): void {
    const onMouse = pose.right === "mouse" || pose.right === "click";
    this.lean = lerp(this.lean, onMouse ? LEAN_DEG : 0, 0.18);
    this.mouseOffset = approach(this.mouseOffset, onMouse ? { x: pose.gaze.x * 6, y: pose.gaze.y * 3 } : { x: 0, y: 0 }, 0.3);
    this.leanGroup.setAttribute("transform", `rotate(${this.lean.toFixed(2)} ${PIVOT.x} ${PIVOT.y})`);
    this.mouse.setAttribute("transform", `translate(${(this.mouseX + this.mouseOffset.x).toFixed(1)} ${this.mouseOffset.y.toFixed(1)})`);

    for (const paw of PAWS) {
      const pressing = pose[paw] === "key" || (paw === "right" && pose.right === "click");
      const s = SHOULDER[paw];
      const target = withinReach(s, leaned(this.pawTarget(paw, pose), -this.lean));
      this.paws[paw] = approach(this.paws[paw], target, pressing ? 0.55 : 0.22);
      // the paw lifts while travelling and lands on the target: an arc, not a slide
      const lift = Math.min(6, Math.hypot(target.x - this.paws[paw].x, target.y - this.paws[paw].y) * 0.35);
      const p = { x: this.paws[paw].x, y: this.paws[paw].y - lift };
      const n = (v: number) => v.toFixed(1);
      this.arms[paw].limb.setAttribute("d", foreleg(s, p, paw));
      const { from, span } = this.pointed ? POINTS : SOCK;
      const along = (t: number) => ({ x: n(s.x + (p.x - s.x) * t), y: n(s.y + (p.y - s.y) * t) });
      const a = along(from), b = along(from + span);
      const shade = this.arms[paw].shade;
      shade.setAttribute("x1", a.x); shade.setAttribute("y1", a.y); shade.setAttribute("x2", b.x); shade.setAttribute("y2", b.y);
      // the paw squashes while it presses, more on a hard Enter
      const slam = pressing && pose.slam;
      const [sx, sy] = slam ? [1.14, 0.7] : pressing ? [1, 0.83] : [0.86, 1];
      this.arms[paw].pad.setAttribute("transform", `translate(${n(p.x)} ${n(p.y)}) scale(${sx} ${sy})`);
    }
  }

  private dragging = false;

  /** Picked up: paws dangle, feet hang, a startled face; put down: a little landing bounce */
  setDragging(on: boolean): void {
    if (this.dragging === on) return;
    this.dragging = on;
    this.set("dragging", String(on));
    if (on) return;
    this.svg.classList.remove("landing");
    void this.svg.getBoundingClientRect();
    this.svg.classList.add("landing");
  }

  private pawTarget(paw: Paw, pose: Pose): Point {
    if (this.dragging) {
      const s = leaned(SHOULDER[paw], this.lean);
      const sway = Math.sin(performance.now() / 160 + (paw === "left" ? 0 : 1.4)) * 2.5;
      return { x: s.x + (paw === "left" ? 4 : -4) + sway, y: s.y + 50 };
    }
    if (paw === "right" && (pose.right === "mouse" || pose.right === "click")) {
      // fingers move onto the button being pressed; the wheel gets a rolling nudge
      const press = pose.button === "left" ? { x: 3.5, y: 3 }
        : pose.button === "right" ? { x: -3.5, y: 3 }
        : pose.button === "middle" || pose.scroll ? { x: 0, y: 3.5 }
        : { x: 0, y: 0 };
      const roll = pose.scroll ? Math.sin(performance.now() / 40) * 1.2 : 0;
      return {
        x: this.mouseX + this.mouseOffset.x + press.x,
        y: MOUSE.y - 5 + this.mouseOffset.y + press.y + roll + (pose.right === "click" ? 1.5 : 0),
      };
    }
    if (pose.swat && paw === "right") {
      const s = leaned(SHOULDER.right, this.lean);
      return { x: s.x + pose.gaze.x * MAX_REACH, y: s.y + 20 + pose.gaze.y * 30 };
    }
    if (pose.yawning) {
      // stretch: both paws reach out over the keyboard
      return { x: REST[paw].x + (paw === "left" ? 10 : -10), y: REST[paw].y + 22 };
    }
    if (pose[paw] === "key") {
      // in a shortcut the cat's left paw (outer side of the keyboard) holds the modifier
      const code = pose.chord !== null && paw === "left" ? pose.chord : pose.pressed;
      const key = code === null ? undefined : this.keyCenters.get(code);
      return key ? { x: key.x, y: key.y - 1 } : { x: REST[paw].x, y: 187 };
    }
    return leaned(REST[paw], this.lean);
  }

  /** A keycap label that pops up from the paw that typed it */
  bubble({ label, paw, at }: Bubble): void {
    const width = 8 + label.length * 5.2;
    const x = at === "mouse" ? this.mouseX + this.mouseOffset.x : REST[paw].x + (paw === "left" ? 14 : -14);
    const y = at === "mouse" ? 167 : 146;
    const g = document.createElementNS("http://www.w3.org/2000/svg", "g");
    g.setAttribute("class", "bubble");
    // rapid clicks replace the mouse label instead of stacking copies
    if (at === "mouse") this.bubbles.querySelector(".bubble[data-at=mouse]")?.remove();
    g.dataset.at = at;
    g.innerHTML = `<rect x="${x - width / 2}" y="${y}" width="${width}" height="12" rx="3"/><text x="${x}" y="${y + 9}" text-anchor="middle">${label}</text>`;
    this.bubbles.append(g);
    if (this.bubbles.childElementCount > 4) this.bubbles.firstElementChild?.remove();
    g.addEventListener("animationend", () => g.remove());
  }

  private vars(values: Record<string, string>): void {
    for (const [name, value] of Object.entries(values)) {
      if (this.shown[`--${name}`] === value) continue;
      this.shown[`--${name}`] = value;
      this.svg.style.setProperty(`--${name}`, value);
    }
  }

  private set(name: string, value: string): void {
    if (this.shown[name] === value) return;
    this.shown[name] = value;
    this.svg.dataset[name] = value;
  }
}
