import type { CSSProperties } from "react";
import type { Plugin } from "../types";
import { accessLabels, categoryLabels, statusLabels } from "../lib/plugins";
import { Link } from "../lib/router";
import { safeAssetUrl } from "../lib/urls";
import { Pill } from "./ui";

/** Cover: echte Grafik, wenn vorhanden, sonst ein generiertes "Modul" in der Akzentfarbe. */
export function PluginCover({ plugin, eager = false }: { plugin: Plugin; eager?: boolean }) {
  const src = safeAssetUrl(plugin.cover?.src);
  const style = { "--accent": plugin.accent } as CSSProperties;
  if (src && plugin.cover) {
    return (
      <div className={`plugin-cover plugin-cover--${plugin.cover.fit ?? "cover"}`} style={style}>
        <img src={src} alt={plugin.cover.alt} loading={eager ? "eager" : "lazy"} decoding="async" />
        {plugin.cover.overlayName && (
          <span className="plugin-cover__overlay" aria-hidden="true">
            <span className="plugin-cover__name">{plugin.name}</span>
          </span>
        )}
      </div>
    );
  }
  return (
    <div className="plugin-cover plugin-cover--generated" style={style} role="img" aria-label={`${plugin.name} cover`}>
      <span className="plugin-cover__grid" aria-hidden="true" />
      <span className="plugin-cover__name" aria-hidden="true">
        {plugin.name}
      </span>
      <span className="plugin-cover__type" aria-hidden="true">
        {plugin.type}
      </span>
    </div>
  );
}

const statusTone = { released: "green", beta: "blue", "in-development": "violet", concept: "muted" } as const;
const accessTone = { free: "green", dm: "pink", "coming-soon": "muted", premium: "blue" } as const;

export function PluginCard({ plugin }: { plugin: Plugin }) {
  return (
    <article className="plugin-card" style={{ "--accent": plugin.accent } as CSSProperties}>
      <PluginCover plugin={plugin} />
      <div className="plugin-card__body">
        <div className="plugin-card__meta">
          <span>{categoryLabels[plugin.category]}</span>
          <span aria-hidden="true">/</span>
          <span>{plugin.type}</span>
        </div>
        <h3 className="plugin-card__title">
          <Link to={`/plugins/${plugin.slug}`} className="stretched">
            {plugin.name}
          </Link>
        </h3>
        <p className="plugin-card__tagline">{plugin.tagline}</p>
        <div className="plugin-card__pills">
          {plugin.draft && <Pill tone="pink">Draft · not public</Pill>}
          <Pill tone={statusTone[plugin.status]}>{statusLabels[plugin.status]}</Pill>
          <Pill tone={accessTone[plugin.access.type]}>{accessLabels[plugin.access.type]}</Pill>
        </div>
        <dl className="spec-row">
          <div>
            <dt>Format</dt>
            <dd>{plugin.formats.join(", ")}</dd>
          </div>
          <div>
            <dt>OS</dt>
            <dd>{plugin.os.map((o) => o.split(" ")[0]).join(", ")}</dd>
          </div>
          {plugin.version && (
            <div>
              <dt>Version</dt>
              <dd>{plugin.version}</dd>
            </div>
          )}
        </dl>
      </div>
    </article>
  );
}

export { statusTone, accessTone };
