import type { Release } from "../types";

// Releases. Neueste zuerst ist egal, die Seite sortiert selbst nach releaseDate.
// Beispiel (zum Kopieren, Werte durch echte ersetzen):
//
// {
//   slug: "track-name",
//   title: "Track Name",
//   type: "single",
//   releaseDate: "2026-11-20",
//   artwork: { src: "/media/releases/track-name.webp", alt: "Cover von Track Name" },
//   description: "Optionaler kurzer Text zum Track.",
//   soundcloudUrl: "https://soundcloud.com/DEIN-PROFIL/track-name",
//   spotifyUrl: "https://open.spotify.com/track/ID",
//   links: [{ label: "Bandcamp", url: "https://..." }],
//   featured: true,
// },

export const releases: Release[] = [];
