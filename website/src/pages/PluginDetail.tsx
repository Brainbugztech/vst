import { useState, type CSSProperties } from "react";
import { instagramHandle } from "../config/site";
import { defaultInstallSteps, plugins } from "../content/plugins";
import { PluginCover, statusTone } from "../components/PluginCard";
import { ExternalLink, Pill } from "../components/ui";
import { formatDate, formatPrice } from "../lib/format";
import { usePageMeta } from "../lib/meta";
import {
  categoryLabels,
  checkoutUrlOf,
  dmMessageFor,
  downloadUrlOf,
  statusLabels,
} from "../lib/plugins";
import { Link } from "../lib/router";
import { instagramDmUrl, instagramProfileUrl } from "../lib/urls";
import type { Plugin } from "../types";
import { NotFound } from "./NotFound";

function CopyButton({ text, label }: { text: string; label: string }) {
  const [state, setState] = useState<"idle" | "copied" | "failed">("idle");
  return (
    <>
      <button
        type="button"
        className="btn btn--ghost btn--sm"
        onClick={async () => {
          try {
            await navigator.clipboard.writeText(text);
            setState("copied");
          } catch {
            setState("failed");
          }
        }}
      >
        {label}
      </button>
      <span className="copy-status" role="status">
        {state === "copied" ? "Copied. Paste it into the DM." : state === "failed" ? "Couldn't copy, please copy the text above by hand." : ""}
      </span>
    </>
  );
}

function AccessPanel({ plugin }: { plugin: Plugin }) {
  const a = plugin.access;
  if (a.type === "free") {
    const url = downloadUrlOf(plugin);
    return (
      <div className="access-panel">
        <Pill tone="green">Free download</Pill>
        {url ? (
          <>
            <a className="btn btn--primary btn--lg" href={url} download={a.fileName || undefined} rel="noopener">
              Download {plugin.name}
              {plugin.version ? ` v${plugin.version}` : ""}
            </a>
            <p className="muted">
              {plugin.formats.join(", ")} · {plugin.os.join(", ")}
              {plugin.fileSize ? ` · ${plugin.fileSize}` : ""}
            </p>
          </>
        ) : (
          <p>The download isn't available yet. Check back soon.</p>
        )}
      </div>
    );
  }
  if (a.type === "dm") {
    const dm = instagramDmUrl(instagramHandle);
    const profile = instagramProfileUrl(instagramHandle);
    const msg = dmMessageFor(plugin);
    return (
      <div className="access-panel">
        <Pill tone="pink">Exclusive / DM for access</Pill>
        <p>
          {plugin.name} isn't a public download. Send us a direct message on Instagram and we'll get back to you. Access is
          granted manually, so sending a DM doesn't unlock anything automatically.
        </p>
        {dm ? (
          <>
            <p className="dm-message">
              <span className="kicker">Suggested message</span>
              <q>{msg}</q>
            </p>
            <div className="btn-row">
              <ExternalLink href={dm} className="btn btn--primary">
                Open Instagram DM
              </ExternalLink>
              <CopyButton text={msg} label="Copy message" />
            </div>
            <p className="muted small">
              Instagram doesn't support pre-filled messages, so copy the text and paste it into the chat.
              {profile && (
                <>
                  {" "}
                  Profile: <ExternalLink href={profile}>@{instagramHandle.replace(/^@/, "")}</ExternalLink>
                </>
              )}
            </p>
          </>
        ) : (
          <p className="muted">Our Instagram contact will be added here soon.</p>
        )}
      </div>
    );
  }
  if (a.type === "premium") {
    const checkout = checkoutUrlOf(plugin);
    return (
      <div className="access-panel">
        <Pill tone="blue">Premium</Pill>
        {a.price !== undefined && <p className="price">{formatPrice(a.price, a.currency)}</p>}
        {checkout ? (
          <>
            <ExternalLink href={checkout} className="btn btn--primary btn--lg">
              Buy {plugin.name}
            </ExternalLink>
            <p className="muted small">Payment is handled securely by {a.provider || "our payment provider"}. You'll be redirected.</p>
          </>
        ) : (
          <p>{plugin.name} isn't on sale yet.</p>
        )}
      </div>
    );
  }
  return (
    <div className="access-panel">
      <Pill tone="muted">Coming soon</Pill>
      <p>{plugin.name} isn't available yet. We'll announce it in the news as soon as it's ready.</p>
      <Link to="/news" className="text-link">
        News →
      </Link>
    </div>
  );
}

export function PluginDetail({ slug }: { slug: string }) {
  const plugin = plugins.find((p) => p.slug === slug);
  usePageMeta(
    plugin
      ? { title: `${plugin.name} plugin`, description: `${plugin.name}: ${plugin.tagline}`, image: plugin.cover?.src, noindex: plugin.draft }
      : { title: "Plugin not found", noindex: true },
  );
  if (!plugin) return <NotFound />;
  const install = plugin.install?.length ? plugin.install : defaultInstallSteps;

  return (
    <div className="container page plugin-detail" style={{ "--accent": plugin.accent } as CSSProperties}>
      <nav aria-label="Breadcrumb" className="breadcrumb">
        <Link to="/plugins">Plugins</Link> <span aria-hidden="true">/</span> <span aria-current="page">{plugin.name}</span>
      </nav>

      {plugin.draft && (
        <p className="draft-banner" role="note">
          Preview: this plugin is not published and won't appear on the public website.
        </p>
      )}

      <div className="plugin-detail__top">
        <PluginCover plugin={plugin} eager />
        <div className="plugin-detail__head">
          <p className="kicker">
            {categoryLabels[plugin.category]} · {plugin.type}
          </p>
          <h1 id="page-title" tabIndex={-1} className="glitch" data-text={plugin.name}>
            {plugin.name}
          </h1>
          <p className="lead">{plugin.tagline}</p>
          <div className="plugin-card__pills">
            <Pill tone={statusTone[plugin.status]}>{statusLabels[plugin.status]}</Pill>
          </div>
          <AccessPanel plugin={plugin} />
        </div>
      </div>

      <div className="plugin-detail__grid">
        <section aria-labelledby="about-h">
          <h2 id="about-h">About</h2>
          {plugin.description.map((p) => (
            <p key={p}>{p}</p>
          ))}
          {plugin.features.length > 0 && (
            <>
              <h3>Features</h3>
              <ul className="feature-list">
                {plugin.features.map((f) => (
                  <li key={f}>{f}</li>
                ))}
              </ul>
            </>
          )}
          <h2 id="install-h">Installation</h2>
          <ol className="steps">
            {install.map((s) => (
              <li key={s}>{s}</li>
            ))}
          </ol>
          <h2 id="changelog-h">Changelog</h2>
          {plugin.changelog.length ? (
            <ol className="changelog">
              {plugin.changelog.map((c) => (
                <li key={c.version}>
                  <p className="changelog__head">
                    <strong>v{c.version}</strong>
                    {c.date && <time dateTime={c.date}>{formatDate(c.date)}</time>}
                  </p>
                  <ul>
                    {c.changes.map((x) => (
                      <li key={x}>{x}</li>
                    ))}
                  </ul>
                </li>
              ))}
            </ol>
          ) : (
            <p className="muted">No public releases yet, so there's no changelog.</p>
          )}
        </section>

        <aside className="spec-sheet" aria-labelledby="spec-h">
          <h2 id="spec-h">Specs</h2>
          <dl>
            <dt>Category</dt>
            <dd>{categoryLabels[plugin.category]}</dd>
            <dt>Type</dt>
            <dd>{plugin.type}</dd>
            <dt>Status</dt>
            <dd>{statusLabels[plugin.status]}</dd>
            <dt>Formats</dt>
            <dd>{plugin.formats.join(", ") || "Not specified"}</dd>
            <dt>Operating systems</dt>
            <dd>{plugin.os.join(", ") || "Not specified"}</dd>
            <dt>Tested DAWs</dt>
            <dd>{plugin.testedDaws.length ? plugin.testedDaws.join(", ") : "Not listed yet"}</dd>
            {plugin.version && (
              <>
                <dt>Version</dt>
                <dd>{plugin.version}</dd>
              </>
            )}
            {plugin.fileSize && (
              <>
                <dt>File size</dt>
                <dd>{plugin.fileSize}</dd>
              </>
            )}
            {plugin.releaseDate && (
              <>
                <dt>Released</dt>
                <dd>
                  <time dateTime={plugin.releaseDate}>{formatDate(plugin.releaseDate)}</time>
                </dd>
              </>
            )}
          </dl>
        </aside>
      </div>
    </div>
  );
}
