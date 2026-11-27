import { app, BrowserWindow, dialog, ipcMain, screen } from "electron";
import { join } from "node:path";
import { InputRelay } from "../application/inputRelay";
import type { InputSource, Rect } from "../application/ports";
import { ASPECT, DEFAULT_DEVICES, resolveSkin, SIZES } from "../domain/catalog";
import type { CatInput, CatView, Devices, Settings, Skin } from "../domain/types";
import { HookInput } from "../infrastructure/hookInput";
import { JsonSettingsStore } from "../infrastructure/jsonSettingsStore";
import { LinuxEvdevInput } from "../infrastructure/linuxEvdevInput";
import { buildMenu } from "./menu";

// GNOME/Wayland ignores "always on top" for native Wayland windows; under
// XWayland it is honoured, and transparency needs ARGB visuals.
if (process.platform === "linux") {
  app.commandLine.appendSwitch("ozone-platform", "x11");
  app.commandLine.appendSwitch("enable-transparent-visuals");
}

// One cat at a time: a second launch just quits
if (!app.requestSingleInstanceLock()) app.quit();

const STATIC = join(__dirname, "..", "static");
const store = new JsonSettingsStore(join(app.getPath("userData"), "settings.json"));
let settings: Settings = store.load();
let cat: BrowserWindow;
let editor: BrowserWindow | null = null;
let relay: InputRelay;
let devices: Devices = DEFAULT_DEVICES;

const view = (): CatView => ({ settings, skin: resolveSkin(settings), devices });

function update(patch: Partial<Settings>): void {
  const resized = patch.size && patch.size !== settings.size;
  settings = { ...settings, ...patch };
  store.save(settings);
  if (resized) resize();
  cat.webContents.send("view", view());
}

function catSize(): { width: number; height: number } {
  const width = SIZES[settings.size].width;
  return { width, height: Math.round(width * ASPECT) };
}

function resize(): void {
  // Non-resizable windows refuse programmatic resizes on some window managers
  cat.setResizable(true);
  cat.setContentSize(catSize().width, catSize().height);
  cat.setResizable(false);
}

function desktop(): Rect {
  const areas = screen.getAllDisplays().map((d) => d.bounds);
  const x = Math.min(...areas.map((a) => a.x));
  const y = Math.min(...areas.map((a) => a.y));
  return {
    x, y,
    width: Math.max(...areas.map((a) => a.x + a.width)) - x,
    height: Math.max(...areas.map((a) => a.y + a.height)) - y,
  };
}

function createCat(): void {
  // Born on the monitor the user is looking at (where the pointer is)
  const work = screen.getDisplayNearestPoint(screen.getCursorScreenPoint()).workArea;
  const { width, height } = catSize();
  cat = new BrowserWindow({
    width, height,
    x: settings.position?.x ?? work.x + work.width - width - 40,
    y: settings.position?.y ?? work.y + work.height - height - 20,
    transparent: true, frame: false, resizable: false, maximizable: false, fullscreenable: false,
    skipTaskbar: true, hasShadow: false, alwaysOnTop: true, backgroundColor: "#00000000",
    webPreferences: { preload: join(__dirname, "preload.js"), autoplayPolicy: "no-user-gesture-required" },
  });
  cat.setAlwaysOnTop(true, "screen-saver");
  cat.setVisibleOnAllWorkspaces(true);
  // GNOME only honours skip-taskbar once the window is mapped
  cat.once("show", () => cat.setSkipTaskbar(true));
  cat.loadFile(join(STATIC, "index.html"));
  snapshotIfRequested();
}

/** Dev aid: CAT_SNAPSHOT=file.png saves what the window draws, then quits */
function snapshotIfRequested(): void {
  const file = process.env.CAT_SNAPSHOT;
  if (!file) return;
  cat.webContents.once("did-finish-load", () => setTimeout(async () => {
    const { writeFileSync } = await import("node:fs");
    writeFileSync(file, (await cat.webContents.capturePage()).toPNG());
    app.quit();
  }, 1200));
}

function openEditor(): void {
  if (editor) return editor.focus();
  editor = new BrowserWindow({
    width: 620, height: 520, title: "Criar pelagem", autoHideMenuBar: true, backgroundColor: "#f6f1e8",
    webPreferences: { preload: join(__dirname, "preload.js") },
  });
  editor.loadFile(join(STATIC, "editor.html"));
  editor.on("closed", () => (editor = null));
}

function createInputSource(): InputSource {
  if (process.platform !== "linux") return new HookInput((p) => screen.screenToDipPoint(p));
  let warned = false;
  return new LinuxEvdevInput(() => {
    if (warned) return;
    warned = true;
    dialog.showMessageBox({
      type: "info",
      message: "O gato ainda não consegue ver o teclado",
      detail: "No Linux ele lê as teclas direto do sistema. Rode uma vez no terminal e depois saia e entre na sessão:\n\nsudo usermod -aG input $USER",
    });
  });
}

function startInput(): void {
  relay = new InputRelay(
    createInputSource(),
    () => {
      const b = cat.getBounds();
      // the head sits at x = 100 of a viewBox spanning -24..200
      return { x: b.x + b.width * (124 / 224), y: b.y + b.height / 3 };
    },
    desktop,
    (input: CatInput) => cat.webContents.send("input", input),
    (current) => {
      devices = current;
      cat.webContents.send("view", view());
    },
  );
  relay.start();

  // Where the platform reports the real pointer (Windows, X11, or over
  // XWayland windows on Wayland), use it to correct the estimate
  let last = screen.getCursorScreenPoint();
  setInterval(() => {
    const p = screen.getCursorScreenPoint();
    if (p.x !== last.x || p.y !== last.y) relay.syncPointer(p);
    last = p;
  }, 120);
}

ipcMain.handle("view", () => view());

// Dragging by any part of the cat: the page reports how far the pointer went
let dragOrigin = { x: 0, y: 0 };
ipcMain.on("drag:start", () => {
  const [x, y] = cat.getPosition();
  dragOrigin = { x, y };
});
ipcMain.on("drag:move", (_e, dx: number, dy: number) => cat.setPosition(Math.round(dragOrigin.x + dx), Math.round(dragOrigin.y + dy)));
ipcMain.on("drag:end", () => {
  const [x, y] = cat.getPosition();
  settings = { ...settings, position: { x, y } };
  store.save(settings);
});
ipcMain.on("menu", () => buildMenu(settings, devices, { update, openEditor, quit: () => app.quit() }).popup({ window: cat }));
ipcMain.handle("skin:save", (_e, skin: Skin) => {
  const others = settings.customSkins.filter((s) => s.id !== skin.id);
  update({ customSkins: [...others, skin], skinId: skin.id });
});

app.whenReady().then(() => {
  // Linux compositors need a moment before a transparent window is created
  setTimeout(() => {
    createCat();
    startInput();
  }, process.platform === "linux" ? 300 : 0);
});

app.on("window-all-closed", () => app.quit());
app.on("before-quit", () => relay?.stop());
