import { contextBridge, ipcRenderer } from "electron";
import type { CatInput, CatView, Skin } from "../domain/types";

const api = {
  getView: (): Promise<CatView> => ipcRenderer.invoke("view"),
  onView: (cb: (view: CatView) => void) => ipcRenderer.on("view", (_e, v: CatView) => cb(v)),
  onInput: (cb: (input: CatInput) => void) => ipcRenderer.on("input", (_e, i: CatInput) => cb(i)),
  openMenu: () => ipcRenderer.send("menu"),
  dragStart: () => ipcRenderer.send("drag:start"),
  dragMove: (dx: number, dy: number) => ipcRenderer.send("drag:move", dx, dy),
  dragEnd: () => ipcRenderer.send("drag:end"),
  saveSkin: (skin: Skin): Promise<void> => ipcRenderer.invoke("skin:save", skin),
};

export type DeskApi = typeof api;

contextBridge.exposeInMainWorld("desk", api);
