import type { Gig, NewsPost, Release } from "../types";
import { formatDate, parseISODate } from "../lib/format";
import { Link } from "../lib/router";
import { safeAssetUrl, safeExternalUrl } from "../lib/urls";
import { MusicEmbed } from "./Embed";
import { ExternalLink, Pill } from "./ui";

const releaseTypeLabel: Record<Release["type"], string> = {
  single: "Single",
  ep: "EP",
  album: "Album",
  remix: "Remix",
  compilation: "Compilation",
  track: "Track",
};

export function ReleaseCard({ release, headingLevel = 2 }: { release: Release; headingLevel?: 2 | 3 }) {
  const H = headingLevel === 2 ? "h2" : "h3";
  const art = safeAssetUrl(release.artwork?.src);
  const links = (release.links ?? []).filter((l) => safeExternalUrl(l.url));
  return (
    <article className="release">
      <div className="release__art">
        {art ? (
          <img src={art} alt={release.artwork?.alt ?? ""} loading="lazy" decoding="async" />
        ) : (
          <div className="release__art-placeholder" aria-hidden="true">
            <img src="/media/brand/bug.webp" alt="" />
          </div>
        )}
      </div>
      <div className="release__body">
        <p className="kicker">
          {releaseTypeLabel[release.type]} · <time dateTime={release.releaseDate}>{formatDate(release.releaseDate)}</time>
          {release.label ? ` · ${release.label}` : ""}
        </p>
        <H className="release__title">{release.title}</H>
        {release.artists && <p className="release__artists">{release.artists}</p>}
        {release.appearsOn && (
          <p className="release__appears">
            <span className="kicker">Appears on</span> {release.appearsOn.title}
          </p>
        )}
        {release.description && <p>{release.description}</p>}
        {release.appearsOn?.url && (
          <ExternalLink href={release.appearsOn.url} className="btn btn--primary btn--sm release__va-btn">
            Full VA on Bandcamp
          </ExternalLink>
        )}
        <div className="release__embeds">
          {release.soundcloudUrl && <MusicEmbed provider="soundcloud" url={release.soundcloudUrl} title={release.title} />}
          {release.spotifyUrl && <MusicEmbed provider="spotify" url={release.spotifyUrl} title={release.title} />}
          {release.bandcamp && (
            <MusicEmbed
              provider="bandcamp"
              url={release.links?.find((l) => /bandcamp\.com/.test(l.url))?.url ?? release.appearsOn?.url ?? "https://bandcamp.com"}
              title={release.title}
              bandcamp={release.bandcamp}
            />
          )}
        </div>
        {links.length > 0 && (
          <ul className="link-list">
            {links.map((l) => (
              <li key={l.url}>
                <ExternalLink href={l.url} className="btn btn--ghost btn--sm">
                  {l.label}
                </ExternalLink>
              </li>
            ))}
          </ul>
        )}
      </div>
    </article>
  );
}

export function GigCard({ gig, past = false }: { gig: Gig; past?: boolean }) {
  const d = parseISODate(gig.date);
  const flyer = safeAssetUrl(gig.flyer?.src);
  return (
    <article className={`gig${past ? " gig--past" : ""}${gig.cancelled ? " gig--cancelled" : ""}`}>
      <time className="gig__date" dateTime={gig.date}>
        <span className="gig__day">{d ? String(d.getDate()).padStart(2, "0") : "--"}</span>
        <span className="gig__month">{formatDate(gig.date, { month: "short" })}</span>
        <span className="gig__year">{d?.getFullYear()}</span>
      </time>
      <div className="gig__body">
        <h3 className="gig__title">{gig.eventName}</h3>
        <p className="gig__where">
          {gig.venue} · {gig.city}, {gig.country}
          {gig.time ? ` · ${gig.time}` : ""}
        </p>
        {gig.note && <p className="gig__note">{gig.note}</p>}
        <div className="gig__actions">
          {gig.cancelled && <Pill tone="pink">Cancelled</Pill>}
          {!past && !gig.cancelled && gig.ticketUrl && (
            <ExternalLink href={gig.ticketUrl} className="btn btn--primary btn--sm">
              Tickets
            </ExternalLink>
          )}
          {gig.eventUrl && (
            <ExternalLink href={gig.eventUrl} className="btn btn--ghost btn--sm">
              Event info
            </ExternalLink>
          )}
        </div>
      </div>
      {flyer && <img className="gig__flyer" src={flyer} alt={gig.flyer?.alt ?? ""} loading="lazy" decoding="async" />}
    </article>
  );
}

export const newsCategoryLabel: Record<NewsPost["category"], string> = {
  release: "Release",
  plugin: "Plugin update",
  event: "Event",
  "behind-the-scenes": "Behind the scenes",
  dev: "Development",
};

export function NewsCard({ post }: { post: NewsPost }) {
  const img = safeAssetUrl(post.image?.src);
  return (
    <article className="news-card">
      {img && <img src={img} alt={post.image?.alt ?? ""} loading="lazy" decoding="async" />}
      <div className="news-card__body">
        <p className="kicker">
          {newsCategoryLabel[post.category]} · <time dateTime={post.date}>{formatDate(post.date)}</time>
        </p>
        <h3>
          <Link to={`/news/${post.slug}`} className="stretched">
            {post.title}
          </Link>
        </h3>
        <p>{post.excerpt}</p>
      </div>
    </article>
  );
}
