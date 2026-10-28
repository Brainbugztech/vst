import { useId, useState } from "react";
import { Link } from "../lib/router";
import { readFlag, writeFlag } from "../lib/storage";
import { soundcloudEmbedUrl, spotifyEmbedUrl } from "../lib/urls";
import { ExternalLink } from "./ui";

type Provider = "soundcloud" | "spotify";

const info: Record<Provider, { name: string; host: string; height: number }> = {
  soundcloud: { name: "SoundCloud", host: "soundcloud.com", height: 166 },
  spotify: { name: "Spotify", host: "spotify.com", height: 152 },
};

const consentKey = (p: Provider) => `bb-consent-${p}`;

/**
 * Offizieller Player von SoundCloud/Spotify mit Zwei-Klick-Lösung:
 * Erst nach Klick wird eine Verbindung zum Anbieter aufgebaut (DSGVO).
 */
export function MusicEmbed({ provider, url, title }: { provider: Provider; url: string; title: string }) {
  const src = provider === "soundcloud" ? soundcloudEmbedUrl(url) : spotifyEmbedUrl(url);
  const [allowed, setAllowed] = useState(() => readFlag(consentKey(provider)));
  const [remember, setRemember] = useState(false);
  const rememberId = useId();
  const p = info[provider];
  if (!src) return null;

  if (!allowed) {
    return (
      <div className="embed-gate">
        <p>
          Load the {p.name} player for <strong>{title}</strong>? This connects to {p.host}, which may set cookies. See our{" "}
          <Link to="/datenschutz">privacy policy</Link>.
        </p>
        <div className="embed-gate__actions">
          <button
            type="button"
            className="btn btn--ghost btn--sm"
            onClick={() => {
              if (remember) writeFlag(consentKey(provider), true);
              setAllowed(true);
            }}
          >
            Load {p.name} player
          </button>
          <label className="check check--sm" htmlFor={rememberId}>
            <input id={rememberId} type="checkbox" checked={remember} onChange={(e) => setRemember(e.target.checked)} />
            <span>Always load {p.name} players</span>
          </label>
          <ExternalLink href={url} className="text-link">
            Open on {p.name}
          </ExternalLink>
        </div>
      </div>
    );
  }
  return (
    <iframe
      className="embed-frame"
      title={`${p.name} player: ${title}`}
      src={src}
      width="100%"
      height={p.height}
      loading="lazy"
      allow="autoplay; clipboard-write; encrypted-media; fullscreen; picture-in-picture"
      referrerPolicy="strict-origin-when-cross-origin"
    />
  );
}
