import { Menu, type MenuItemConstructorOptions } from "electron";
import { allSkins, DESK_COLORS, SIZES } from "../domain/catalog";
import type { Devices, Settings, Size } from "../domain/types";

export interface MenuActions {
  update(patch: Partial<Settings>): void;
  openEditor(): void;
  quit(): void;
}

const colorItems = (settings: Settings, key: "keyboardColor" | "mouseColor", actions: MenuActions): MenuItemConstructorOptions[] =>
  DESK_COLORS.map((c) => ({ label: c.name, type: "radio", checked: settings[key] === c.value, click: () => actions.update({ [key]: c.value }) }));

/** The cat's right-click menu: every setting lives here */
export function buildMenu(settings: Settings, devices: Devices, actions: MenuActions): Menu {
  return Menu.buildFromTemplate([
    { label: `Teclado: ${devices.keyboard.name || "ainda não usado"}`, enabled: false },
    { label: `${devices.pointer.touchpad ? "Touchpad" : "Mouse"}: ${devices.pointer.name || "ainda não usado"}`, enabled: false },
    { type: "separator" },
    {
      label: "Pelagem",
      submenu: allSkins(settings).map((s) => ({ label: s.name, type: "radio", checked: s.id === settings.skinId, click: () => actions.update({ skinId: s.id }) })),
    },
    { label: "Criar pelagem…", click: actions.openEditor },
    {
      label: "Tamanho",
      submenu: (Object.keys(SIZES) as Size[]).map((size) => ({ label: SIZES[size].name, type: "radio", checked: settings.size === size, click: () => actions.update({ size }) })),
    },
    { label: "Cor do teclado", submenu: colorItems(settings, "keyboardColor", actions) },
    { label: "Cor do mouse", submenu: colorItems(settings, "mouseColor", actions) },
    { type: "separator" },
    { label: "Modo privado (olhos fechados)", type: "checkbox", checked: settings.privacy, click: () => actions.update({ privacy: !settings.privacy }) },
    { label: "Fundo verde (para live)", type: "checkbox", checked: settings.greenScreen, click: () => actions.update({ greenScreen: !settings.greenScreen }) },
    { type: "separator" },
    { label: "Sair", click: actions.quit },
  ]);
}
