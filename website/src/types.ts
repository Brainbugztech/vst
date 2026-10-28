// Datenmodell der Website. Inhalte liegen in src/content/*, Einstellungen in src/config/site.ts.

/** Datum im Format JJJJ-MM-TT, z. B. "2026-11-14". */
export type ISODate = string;

export interface Image {
  src: string;
  alt: string;
}

export interface ExternalLink {
  label: string;
  url: string;
}

// ---------- Musik ----------

export interface Release {
  slug: string;
  title: string;
  type: "single" | "ep" | "album" | "remix" | "compilation";
  releaseDate: ISODate;
  artwork?: Image;
  /** Label oder Netlabel, nur wenn es das wirklich gibt. */
  label?: string;
  description?: string;
  /** Offizielle Track-/Playlist-URL, z. B. https://soundcloud.com/brainbugz/track */
  soundcloudUrl?: string;
  /** Offizielle Spotify-URL, z. B. https://open.spotify.com/album/ID */
  spotifyUrl?: string;
  /** Weitere Streaming-Links (Bandcamp, Beatport, YouTube ...). */
  links?: ExternalLink[];
  /** true = wird auf der Startseite gezeigt. Sonst das neueste Release. */
  featured?: boolean;
}

// ---------- Gigs ----------

export interface Gig {
  id: string;
  date: ISODate;
  /** Optional, z. B. "23:30" */
  time?: string;
  eventName: string;
  venue: string;
  city: string;
  country: string;
  flyer?: Image;
  ticketUrl?: string;
  eventUrl?: string;
  note?: string;
  cancelled?: boolean;
}

// ---------- News ----------

export type NewsCategory = "release" | "plugin" | "event" | "behind-the-scenes" | "dev";

export interface NewsPost {
  slug: string;
  date: ISODate;
  title: string;
  category: NewsCategory;
  excerpt: string;
  /** Absätze */
  body: string[];
  image?: Image;
  links?: ExternalLink[];
}

// ---------- Plugins ----------

export type PluginCategory = "fx" | "synth" | "presets" | "utility";

export type PluginStatus = "released" | "beta" | "in-development" | "concept";

/**
 * Wie man an das Plugin kommt.
 * - free:        öffentlicher Download, Button erscheint NUR wenn downloadUrl gesetzt ist
 * - dm:          exklusiv, Zugang per Instagram-DM (wird von Hand freigegeben)
 * - coming-soon: noch nicht erhältlich
 * - premium:     späterer Verkauf. Kauf-Button erscheint NUR wenn checkoutUrl gesetzt ist
 */
export type PluginAccess =
  | { type: "free"; downloadUrl?: string; fileName?: string }
  | { type: "dm" }
  | { type: "coming-soon" }
  | {
      type: "premium";
      /** Preis in der Hauptwährungseinheit, z. B. 19 */
      price?: number;
      currency?: "EUR" | "USD" | "GBP";
      /** Link zur Kasse eines echten Zahlungsanbieters (Lemon Squeezy, Gumroad, Paddle ...) */
      checkoutUrl?: string;
      provider?: string;
    };

export interface ChangelogEntry {
  version: string;
  date?: ISODate;
  changes: string[];
}

export interface Plugin {
  slug: string;
  /**
   * false = Plugin kommt gar nicht erst auf die öffentliche Website (nicht im Code, keine Bilder).
   * Umschalten per GitHub-Button (Actions → "Plugin veröffentlichen") oder hier von Hand.
   */
  published: boolean;
  /** Wird beim Build gesetzt: true = unveröffentlichte Vorschau (nur lokal mit "npm run dev:drafts"). */
  draft?: boolean;
  name: string;
  tagline: string;
  /** Absätze für die Detailseite */
  description: string[];
  features: string[];
  category: PluginCategory;
  /** z. B. "Glitch & Stutter", "Delay & Spatial" */
  type: string;
  status: PluginStatus;
  access: PluginAccess;
  /** In plugins.json: { "file": "x.webp", "alt": "...", "fit": "cover", "overlayName": true } */
  cover?: Image & { fit?: "cover" | "contain"; overlayName?: boolean };
  /** Akzentfarbe für generierte Cover und Details */
  accent: string;
  os: string[];
  /** Nur Formate, die wirklich gebaut werden */
  formats: string[];
  /** Nur DAWs, in denen ihr wirklich getestet habt */
  testedDaws: string[];
  version?: string;
  /** z. B. "4.2 MB", nur bei echter Datei */
  fileSize?: string;
  /** Datum, an dem das Plugin ins Projekt kam (für "Neueste zuerst") */
  addedDate: ISODate;
  releaseDate?: ISODate;
  changelog: ChangelogEntry[];
  /** Eigene Installationsschritte, sonst gilt die Standard-Anleitung für VST3 */
  install?: string[];
  featured?: boolean;
}

// ---------- Merch ----------

export type MerchCategory = "tshirt" | "hoodie" | "sticker" | "accessory";

export interface MerchProduct {
  id: string;
  name: string;
  category: MerchCategory;
  description: string;
  images: Image[];
  sizes?: string[];
  price?: number;
  currency?: "EUR" | "USD" | "GBP";
  availability: "available" | "sold-out" | "coming-soon";
  /** Link zum echten Shop (Shopify, Spreadshirt, Bandcamp Merch ...). Ohne Link kein Kauf-Button. */
  shopUrl?: string;
}

// ---------- Spätere Bezahl-Produkte (nur Datenmodell, wird NICHT im Browser verarbeitet) ----------
// Bestellungen, Lizenzen und Download-Berechtigungen gehören auf einen Server
// (z. B. Netlify Function + Zahlungsanbieter-Webhook). Diese Typen beschreiben nur die Form.

export interface Order {
  orderReference: string;
  provider: string;
  pluginSlug: string;
  customerEmail: string;
  amount: number;
  currency: string;
  createdAt: string;
}

export interface License {
  key: string;
  pluginSlug: string;
  status: "active" | "revoked" | "refunded";
  orderReference: string;
  maxActivations: number;
}

export interface DownloadEntitlement {
  pluginSlug: string;
  orderReference: string;
  /** Kurzlebige, serverseitig signierte URL. Nie im Frontend erzeugen. */
  expiresAt: string;
}
