import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname } from "node:path";
import type { SettingsStore } from "../application/ports";
import { DEFAULT_SETTINGS } from "../domain/catalog";
import type { Settings } from "../domain/types";

export class JsonSettingsStore implements SettingsStore {
  constructor(private readonly file: string) {}

  load(): Settings {
    try {
      return { ...DEFAULT_SETTINGS, ...JSON.parse(readFileSync(this.file, "utf8")) };
    } catch {
      return { ...DEFAULT_SETTINGS };
    }
  }

  save(settings: Settings): void {
    mkdirSync(dirname(this.file), { recursive: true });
    writeFileSync(this.file, JSON.stringify(settings, null, 2));
  }
}
