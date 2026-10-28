/// <reference types="vitest/config" />
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { defineConfig, type Plugin as VitePlugin } from "vite";
import react from "@vitejs/plugin-react";

const root = path.dirname(fileURLToPath(import.meta.url));
const DATA_FILE = path.join(root, "src/content/plugins.json");
const MEDIA_DIR = path.join(root, "src/content/plugin-media");
const VIRTUAL_ID = "virtual:brainbugz-plugins";
const RESOLVED_ID = "\0" + VIRTUAL_ID;

const REQUIRED = ["slug", "name", "tagline", "category", "type", "status", "access", "accent", "addedDate"] as const;
const CATEGORIES = ["fx", "synth", "presets", "utility"];
const STATUSES = ["released", "beta", "in-development", "concept"];
const ACCESS = ["free", "dm", "coming-soon", "premium"];

/**
 * Lädt src/content/plugins.json und gibt NUR veröffentlichte Plugins an die Website weiter.
 * Unveröffentlichte Plugins (und ihre Cover-Bilder) landen dadurch gar nicht erst im Build.
 * Mit "npm run dev:drafts" / "npm run build:drafts" (mode "drafts") werden lokal alle gezeigt.
 * Auf Netlify ist die Vorschau immer aus.
 */
function pluginData(): VitePlugin {
  let showDrafts = false;
  return {
    name: "brainbugz-plugin-data",
    configResolved(config) {
      showDrafts = config.mode === "drafts" && process.env.NETLIFY !== "true";
      if (showDrafts) config.logger.warn("\n  [plugins] Vorschau-Modus: unveröffentlichte Plugins werden angezeigt. Nicht deployen!\n");
    },
    resolveId(id) {
      return id === VIRTUAL_ID ? RESOLVED_ID : undefined;
    },
    load(id) {
      if (id !== RESOLVED_ID) return;
      this.addWatchFile(DATA_FILE);
      const all = JSON.parse(fs.readFileSync(DATA_FILE, "utf8"));
      if (!Array.isArray(all)) throw new Error("plugins.json muss eine Liste sein");

      const slugs = new Set<string>();
      for (const p of all) {
        const missing = REQUIRED.filter((k) => p[k] === undefined || p[k] === "");
        if (missing.length) throw new Error(`plugins.json: "${p.slug ?? p.name ?? "?"}" fehlt: ${missing.join(", ")}`);
        if (!/^[a-z0-9-]+$/.test(p.slug)) throw new Error(`plugins.json: ungültiger slug "${p.slug}" (nur a-z, 0-9, -)`);
        if (slugs.has(p.slug)) throw new Error(`plugins.json: slug "${p.slug}" doppelt`);
        slugs.add(p.slug);
        if (typeof p.published !== "boolean") throw new Error(`plugins.json: "${p.slug}" braucht "published": true oder false`);
        if (!CATEGORIES.includes(p.category)) throw new Error(`plugins.json: "${p.slug}" hat unbekannte category "${p.category}"`);
        if (!STATUSES.includes(p.status)) throw new Error(`plugins.json: "${p.slug}" hat unbekannten status "${p.status}"`);
        if (!ACCESS.includes(p.access?.type)) throw new Error(`plugins.json: "${p.slug}" hat unbekannten access.type`);
        if (!/^\d{4}-\d{2}-\d{2}$/.test(p.addedDate)) throw new Error(`plugins.json: "${p.slug}" addedDate muss JJJJ-MM-TT sein`);
      }

      const visible = all.filter((p) => p.published === true || showDrafts);
      const imports: string[] = [];
      const items = visible.map((p, i) => {
        const { cover, ...rest } = p;
        const item: Record<string, unknown> = {
          description: [],
          features: [],
          os: [],
          formats: [],
          testedDaws: [],
          changelog: [],
          ...rest,
          draft: p.published !== true,
        };
        let coverExpr = "undefined";
        if (cover?.file) {
          if (!/^[\w.-]+$/.test(cover.file)) throw new Error(`plugins.json: ungültiger Dateiname "${cover.file}"`);
          const file = path.join(MEDIA_DIR, cover.file);
          if (!fs.existsSync(file)) throw new Error(`plugins.json: Cover fehlt: src/content/plugin-media/${cover.file}`);
          imports.push(`import cover${i} from ${JSON.stringify(file)};`);
          coverExpr = `{ src: cover${i}, alt: ${JSON.stringify(cover.alt ?? "")}, fit: ${JSON.stringify(cover.fit ?? "cover")}, overlayName: ${cover.overlayName === true} }`;
        }
        return `{ ...${JSON.stringify(item)}, cover: ${coverExpr} }`;
      });
      return `${imports.join("\n")}\nexport default [\n${items.join(",\n")}\n];\n`;
    },
  };
}

export default defineConfig({
  plugins: [react(), pluginData()],
  build: {
    target: "es2020",
    assetsInlineLimit: 0,
    sourcemap: false,
  },
  test: {
    environment: "node",
    include: ["src/**/*.test.ts"],
  },
});
