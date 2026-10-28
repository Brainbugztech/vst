import { useId, useMemo } from "react";
import { plugins } from "../content/plugins";
import { PluginCard } from "../components/PluginCard";
import { EmptyState, PageHeader } from "../components/ui";
import { usePageMeta } from "../lib/meta";
import {
  categoryLabels,
  categoryOrder,
  filterPlugins,
  isCategory,
  isSortKey,
  sortLabels,
  sortPlugins,
  type SortKey,
} from "../lib/plugins";
import { navigate, useSearch } from "../lib/router";
import type { PluginCategory } from "../types";

function useFilters() {
  const search = useSearch();
  const params = new URLSearchParams(search);
  const cat = params.get("category");
  const sort = params.get("sort");
  return {
    category: isCategory(cat) ? cat : null,
    query: params.get("q") ?? "",
    sort: (isSortKey(sort) ? sort : "newest") as SortKey,
    update(next: { category?: PluginCategory | null; query?: string; sort?: SortKey }) {
      const p = new URLSearchParams(window.location.search);
      const set = (k: string, v: string | null | undefined, def = "") => {
        if (v === undefined) return;
        if (!v || v === def) p.delete(k);
        else p.set(k, v);
      };
      set("category", next.category);
      set("q", next.query);
      set("sort", next.sort, "newest");
      const qs = p.toString();
      navigate(`/plugins${qs ? `?${qs}` : ""}`, { replace: true, keepScroll: true });
    },
  };
}

export function Plugins() {
  usePageMeta({
    title: "Plugins",
    description: "BrainBugz VST3 plugins: FX, synthesizers, presets and experimental sound design tools.",
  });
  const f = useFilters();
  const searchId = useId();
  const sortId = useId();
  const results = useMemo(
    () => sortPlugins(filterPlugins(plugins, { category: f.category, query: f.query }), f.sort),
    [f.category, f.query, f.sort],
  );
  const counts = useMemo(() => {
    const c: Record<string, number> = {};
    for (const p of filterPlugins(plugins, { query: f.query })) c[p.category] = (c[p.category] ?? 0) + 1;
    return c;
  }, [f.query]);
  const totalForQuery = Object.values(counts).reduce((a, b) => a + b, 0);

  return (
    <div className="container page">
      <PageHeader
        code="// BRAINBUGZ PLUGINS"
        title="Plugins"
        intro="Audio tools from the BrainBugz lab: FX, synths and experimental sound design. Built in-house as VST3 for Windows."
      />

      {plugins.length === 0 ? (
        <EmptyState title="Our first plugins are coming soon">
          <p>We're still finishing them in the lab. They'll be released here, with downloads, install guides and changelogs.</p>
        </EmptyState>
      ) : (
        <>
      <div className="plugin-toolbar" role="search">
        <div className="field field--search">
          <label htmlFor={searchId}>Search plugins</label>
          <input
            id={searchId}
            type="search"
            placeholder="e.g. glitch, saturation, synth"
            value={f.query}
            onChange={(e) => f.update({ query: e.target.value })}
            autoComplete="off"
          />
        </div>
        <div className="field field--sort">
          <label htmlFor={sortId}>Sort by</label>
          <select id={sortId} value={f.sort} onChange={(e) => f.update({ sort: e.target.value as SortKey })}>
            {(Object.keys(sortLabels) as SortKey[]).map((k) => (
              <option key={k} value={k}>
                {sortLabels[k]}
              </option>
            ))}
          </select>
        </div>
      </div>

      <div className="chips" role="group" aria-label="Filter by category">
        <button type="button" className="chip" aria-pressed={!f.category} onClick={() => f.update({ category: null })}>
          All <span className="chip__count">{totalForQuery}</span>
        </button>
        {categoryOrder.map((c) => (
          <button
            key={c}
            type="button"
            className="chip"
            aria-pressed={f.category === c}
            onClick={() => f.update({ category: f.category === c ? null : c })}
          >
            {categoryLabels[c]} <span className="chip__count">{counts[c] ?? 0}</span>
          </button>
        ))}
      </div>

      <p className="results-count" aria-live="polite">
        {results.length} {results.length === 1 ? "plugin" : "plugins"}
        {f.category ? ` in ${categoryLabels[f.category]}` : ""}
        {f.query ? ` matching "${f.query}"` : ""}
      </p>

      {results.length ? (
        <div className="plugin-grid">
          {results.map((p) => (
            <PluginCard key={p.slug} plugin={p} />
          ))}
        </div>
      ) : (
        <EmptyState title={f.category === "presets" && !f.query ? "No preset packs yet" : "No plugins found"}>
          <p>
            {f.category === "presets" && !f.query
              ? "Preset and sound design packs are planned. They'll show up here when they're ready."
              : "Try another search term or category."}
          </p>
          <button type="button" className="btn btn--ghost btn--sm" onClick={() => f.update({ category: null, query: "" })}>
            Reset filters
          </button>
        </EmptyState>
      )}
        </>
      )}

      <section className="access-explainer" aria-labelledby="access-h">
        <h2 id="access-h">How to get our plugins</h2>
        <div className="access-explainer__grid">
          <div>
            <h3>Free download</h3>
            <p>Public freeware with a direct download, version info and install guide.</p>
          </div>
          <div>
            <h3>Exclusive / DM for access</h3>
            <p>Some tools are shared personally. Send us a DM on Instagram. Access is granted by hand, not automatically.</p>
          </div>
          <div>
            <h3>Coming soon / Premium</h3>
            <p>Plugins still in development. Paid releases will go through a proper payment provider once they launch.</p>
          </div>
        </div>
      </section>
    </div>
  );
}
