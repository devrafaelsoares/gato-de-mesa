// Exports the cat as sprites for the native app.
//   npm run sprites
// Photographs each part of every preset coat on a transparent canvas, crops
// it and writes native/assets/<coat>/<part>.png plus a manifest with where
// each crop sits. Also writes glyph atlases for bubbles and menus.
const { app, BrowserWindow } = require("electron");
const fs = require("node:fs");
const path = require("node:path");

const SCALE = 2; // pixels per drawing unit
const VIEW = { x: -24, y: 0, width: 224, height: 222 };
const OUT = path.join(__dirname, "..", "..", "native", "assets");

const MENU_CHARS = (() => {
  let s = "";
  for (let c = 32; c < 127; c++) s += String.fromCharCode(c);
  for (let c = 160; c < 256; c++) s += String.fromCharCode(c);
  return s + "▸✓●…·–—";
})();

/** Bounding box of the non-transparent pixels of a BGRA bitmap */
function opaqueBox(bitmap, width, height) {
  let x0 = width, y0 = height, x1 = -1, y1 = -1;
  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      if (bitmap[(y * width + x) * 4 + 3] === 0) continue;
      if (x < x0) x0 = x;
      if (x > x1) x1 = x;
      if (y < y0) y0 = y;
      if (y > y1) y1 = y;
    }
  }
  return x1 < 0 ? null : { x: x0, y: y0, width: x1 - x0 + 1, height: y1 - y0 + 1 };
}

async function main() {
  const width = VIEW.width * SCALE, height = VIEW.height * SCALE;
  const win = new BrowserWindow({
    width, height, show: false, transparent: true, frame: false, backgroundColor: "#00000000",
    webPreferences: { offscreen: true },
  });
  win.webContents.setFrameRate(5);
  await win.loadFile(path.join(__dirname, "page.html"));
  const call = (expr) => win.webContents.executeJavaScript(expr);

  const skins = await call("skins()");
  const layers = await call("layers()");
  const park = await call("pawPark()");
  for (const skin of skins) {
    const dir = path.join(OUT, skin);
    fs.mkdirSync(dir, { recursive: true });
    const manifest = [`scale ${SCALE}`, `origin ${VIEW.x} ${VIEW.y}`];
    for (const layer of layers) {
      await call(`prepare(${JSON.stringify(skin)}, ${JSON.stringify(layer)})`);
      await new Promise((r) => setTimeout(r, 60));
      const image = await win.webContents.capturePage();
      const size = image.getSize();
      const box = opaqueBox(image.toBitmap(), size.width, size.height);
      if (!box) continue; // this coat has nothing in that part
      fs.writeFileSync(path.join(dir, `${layer}.png`), image.crop(box).toPNG());
      // crops are placed in drawing units; the paw is stored relative to its centre
      let ux = box.x / SCALE + VIEW.x, uy = box.y / SCALE + VIEW.y;
      if (layer === "paw") { ux -= park.x; uy -= park.y; }
      manifest.push(`${layer} ${ux.toFixed(2)} ${uy.toFixed(2)}`);
    }
    fs.writeFileSync(path.join(dir, "manifest.txt"), manifest.join("\n") + "\n");
    console.log(`coat ${skin}: ${manifest.length - 2} parts`);
  }

  // bubble labels: 700 8px in the drawing, at SCALE; menus: 13px at 1x and 2x
  const bubbleChars = await call("bubbleChars()");
  const atlases = [
    ["glyphs-bubble", `700 ${8 * SCALE}px system-ui, sans-serif`, bubbleChars, 8 * SCALE + 8],
    ["glyphs-menu", "400 13px system-ui, sans-serif", MENU_CHARS, 20],
    ["glyphs-menu-2x", "400 26px system-ui, sans-serif", MENU_CHARS, 38],
  ];
  for (const [name, font, chars, cell] of atlases) {
    const { png, metrics } = await call(`glyphs(${JSON.stringify(font)}, ${JSON.stringify(chars)}, ${cell})`);
    fs.writeFileSync(path.join(OUT, `${name}.png`), Buffer.from(png.split(",")[1], "base64"));
    fs.writeFileSync(path.join(OUT, `${name}.txt`), metrics);
  }
  console.log("glyph atlases written");
  app.quit();
}

app.disableHardwareAcceleration();
app.whenReady().then(main).catch((e) => { console.error(e); app.exit(1); });
