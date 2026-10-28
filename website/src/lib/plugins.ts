import type { Plugin, PluginAccess, PluginCategory, PluginStatus } from "../types";
import { safeExternalUrl } from "./urls";

export const categoryLabels: Record<PluginCategory, string> = {
  fx: "FX Plugins",
  synth: "Synthesizers",
  presets: "Presets & Sound Design",
  utility: "Utilities & Experimental",
};

export const categoryOrder: PluginCategory[] = ["fx", "synth", "presets", "utility"];

export const statusLabels: Record<PluginStatus, string> = {
  released: "Released",
  beta: "Beta",
  "in-development": "In development",
  concept: "Concept",
};

const statusOrder: PluginStatus[] = ["released", "beta", "in-development", "concept"];

export const accessLabels: Record<PluginAccess["type"], string> = {
  free: "Free download",
  dm: "Exclusive / DM for access",
  "coming-soon": "Coming soon",
  premium: "Premium",
};

export type SortKey = "newest" | "category" | "status" | "name";

export const sortLabels: Record<SortKey, string> = {
  newest: "Newest",
  category: "Category",
  status: "Status",
  name: "Name (A–Z)",
};

export function isSortKey(v: string | null): v is SortKey {
  return v === "newest" || v === "category" || v === "status" || v === "name";
}

export function isCategory(v: string | null): v is PluginCategory {
  return v === "fx" || v === "synth" || v === "presets" || v === "utility";
}

function haystack(p: Plugin) {
  return [p.name, p.tagline, p.type, categoryLabels[p.category], ...p.features, ...p.formats].join(" ").toLowerCase();
}

export function filterPlugins(list: Plugin[], opts: { category?: PluginCategory | null; query?: string }) {
  const terms = (opts.query ?? "").toLowerCase().split(/\s+/).filter(Boolean);
  return list.filter((p) => {
    if (opts.category && p.category !== opts.category) return false;
    if (!terms.length) return true;
    const h = haystack(p);
    return terms.every((t) => h.includes(t));
  });
}

export function sortPlugins(list: Plugin[], key: SortKey) {
  const byName = (a: Plugin, b: Plugin) => a.name.localeCompare(b.name);
  const byNewest = (a: Plugin, b: Plugin) => b.addedDate.localeCompare(a.addedDate) || byName(a, b);
  const sorted = [...list];
  switch (key) {
    case "newest":
      return sorted.sort(byNewest);
    case "name":
      return sorted.sort(byName);
    case "category":
      return sorted.sort(
        (a, b) => categoryOrder.indexOf(a.category) - categoryOrder.indexOf(b.category) || byNewest(a, b),
      );
    case "status":
      return sorted.sort((a, b) => statusOrder.indexOf(a.status) - statusOrder.indexOf(b.status) || byNewest(a, b));
  }
}

export function pickFeaturedPlugin(list: Plugin[]): Plugin | undefined {
  return list.find((p) => p.featured) ?? sortPlugins(list, "newest")[0];
}

/** Download-URL nur, wenn das Plugin wirklich frei ist UND eine gültige Datei hinterlegt ist. */
export function downloadUrlOf(p: Plugin): string | null {
  if (p.access.type !== "free") return null;
  const url = p.access.downloadUrl;
  if (!url) return null;
  if (url.startsWith("/") && !url.startsWith("//")) return url;
  return safeExternalUrl(url);
}

export function checkoutUrlOf(p: Plugin): string | null {
  if (p.access.type !== "premium") return null;
  return safeExternalUrl(p.access.checkoutUrl);
}

export function dmMessageFor(p: Plugin) {
  return `Hi BrainBugz! I'd like to request access to the ${p.name} plugin.`;
}
