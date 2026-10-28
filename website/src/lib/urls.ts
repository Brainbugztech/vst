// Prüft externe URLs, damit aus Tippfehlern in den Datendateien keine kaputten oder unsicheren Links werden.

/** Gibt die URL zurück, wenn sie http(s) ist, sonst null. */
export function safeExternalUrl(value: string | undefined | null): string | null {
  if (!value) return null;
  try {
    const u = new URL(value);
    return u.protocol === "https:" || u.protocol === "http:" ? u.toString() : null;
  } catch {
    return null;
  }
}

/** Interne Datei (z. B. "/media/...") oder sichere externe URL. */
export function safeAssetUrl(value: string | undefined | null): string | null {
  if (!value) return null;
  if (value.startsWith("/") && !value.startsWith("//")) return value;
  return safeExternalUrl(value);
}

function hostIs(u: URL, ...hosts: string[]) {
  return hosts.includes(u.hostname.toLowerCase());
}

/** Offizieller SoundCloud-Player für eine soundcloud.com-URL. */
export function soundcloudEmbedUrl(
  trackUrl: string | undefined,
  opts: { autoPlay?: boolean; visual?: boolean; trackId?: string } = {},
): string | null {
  const safe = safeExternalUrl(trackUrl);
  if (!safe) return null;
  const u = new URL(safe);
  if (u.protocol !== "https:" || !hostIs(u, "soundcloud.com", "www.soundcloud.com", "m.soundcloud.com")) return null;
  if (u.pathname.split("/").filter(Boolean).length < 1) return null;
  // Tracking-Parameter (si, utm_...) entfernen, der private "/s-..."-Teil bleibt im Pfad erhalten
  const path = u.pathname.replace(/\/+$/, "");
  // Private Links ("/s-XXXX" am Ende) spielt der Player nur über Track-Nummer + secret_token
  const secret = /\/(s-[A-Za-z0-9]+)$/.exec(path)?.[1];
  const usePrivate = secret && opts.trackId && /^\d+$/.test(opts.trackId);
  const clean = usePrivate ? `https://api.soundcloud.com/tracks/${opts.trackId}` : `https://soundcloud.com${path}`;
  const params = new URLSearchParams({
    url: clean,
    color: "#7b5cff",
    auto_play: opts.autoPlay ? "true" : "false",
    hide_related: "true",
    show_comments: "false",
    show_user: "true",
    show_reposts: "false",
    visual: opts.visual ? "true" : "false",
  });
  if (usePrivate) params.set("secret_token", secret);
  return `https://w.soundcloud.com/player/?${params.toString()}`;
}

const SPOTIFY_TYPES = ["track", "album", "playlist", "artist", "episode", "show"];

/** Offizieller Spotify-Player für eine open.spotify.com-URL. */
export function spotifyEmbedUrl(spotifyUrl: string | undefined): string | null {
  const safe = safeExternalUrl(spotifyUrl);
  if (!safe) return null;
  const u = new URL(safe);
  if (u.protocol !== "https:" || !hostIs(u, "open.spotify.com")) return null;
  // Pfade wie /album/ID, /intl-de/album/ID oder schon /embed/album/ID
  const parts = u.pathname
    .split("/")
    .filter(Boolean)
    .filter((p) => !p.startsWith("intl-") && p !== "embed");
  const [type, id] = parts;
  if (!type || !id || !SPOTIFY_TYPES.includes(type) || !/^[A-Za-z0-9]+$/.test(id)) return null;
  return `https://open.spotify.com/embed/${type}/${id}?theme=0`;
}

function cleanHandle(handle: string) {
  const clean = handle.trim().replace(/^@/, "");
  return /^[A-Za-z0-9._]{1,30}$/.test(clean) ? clean : null;
}

/** Instagram-DM-Link (ig.me) für einen Instagram-Namen. */
export function instagramDmUrl(handle: string): string | null {
  const clean = cleanHandle(handle);
  return clean ? `https://ig.me/m/${clean}` : null;
}

export function instagramProfileUrl(handle: string): string | null {
  const clean = cleanHandle(handle);
  return clean ? `https://www.instagram.com/${clean}/` : null;
}
