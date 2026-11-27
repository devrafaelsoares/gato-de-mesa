import { build } from "esbuild";

const common = { bundle: true, sourcemap: true, logLevel: "info", target: "es2022" };

await Promise.all([
  build({ ...common, entryPoints: ["src/main/main.ts", "src/main/preload.ts"], outdir: "dist", platform: "node", format: "cjs", external: ["electron", "uiohook-napi"] }),
  build({ ...common, entryPoints: ["src/renderer/app.ts", "src/renderer/editor.ts"], outdir: "dist", platform: "browser", format: "iife" }),
]);
