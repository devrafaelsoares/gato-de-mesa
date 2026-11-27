import type { DeskApi } from "../main/preload";

/** The API the preload script exposes to the pages */
export const desk = (window as unknown as { desk: DeskApi }).desk;
