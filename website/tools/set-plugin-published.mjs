#!/usr/bin/env node
// Schaltet ein Plugin auf der Website sichtbar/unsichtbar.
// Aufruf: node tools/set-plugin-published.mjs <slug> <true|false>
// Wird vom GitHub-Button "Plugin veröffentlichen" benutzt, geht aber auch lokal.
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const file = path.join(path.dirname(fileURLToPath(import.meta.url)), "../src/content/plugins.json");
const [slug, value] = process.argv.slice(2);
if (!slug || !["true", "false"].includes(value)) {
  console.error("Aufruf: node tools/set-plugin-published.mjs <slug> <true|false>");
  process.exit(1);
}
const plugins = JSON.parse(fs.readFileSync(file, "utf8"));
const p = plugins.find((x) => x.slug === slug);
if (!p) {
  console.error(`Unbekanntes Plugin "${slug}". Vorhanden: ${plugins.map((x) => x.slug).join(", ")}`);
  process.exit(1);
}
const next = value === "true";
if (p.published === next) {
  console.log(`${p.name} ist schon ${next ? "veröffentlicht" : "unveröffentlicht"}. Nichts zu tun.`);
} else {
  p.published = next;
  fs.writeFileSync(file, JSON.stringify(plugins, null, 2) + "\n");
  console.log(`${p.name}: published = ${next}`);
}
