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

export const releases: Release[] = [
  {
    slug: "finger-on-the-trigger",
    title: "Finger on the Trigger",
    type: "track",
    artists: "Brainbugz (Benido & Artificial Organism) & Lilith",
    releaseDate: "2026-05-22",
    label: "Speeding Records",
    artwork: { src: "/media/releases/rapido-y-furioso-va.webp", alt: "Cover of Rapido Y Furioso VA: a red mech robot in front of a blue lightning tunnel" },
    appearsOn: {
      title: "Rapido Y Furioso VA",
      url: "https://speedingrecords.bandcamp.com/album/rapido-y-furioso-va",
    },
    description: "Our first release: 185 BPM together with Lilith, out on Speeding Records.",
    bandcamp: { albumId: "520798435", trackId: "2364707309" },
    links: [{ label: "Track on Bandcamp", url: "https://speedingrecords.bandcamp.com/track/finger-on-the-trigger-185" }],
    featured: true,
  },
];
