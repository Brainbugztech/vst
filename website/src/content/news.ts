import type { NewsPost } from "../types";

// News-Beiträge. Kategorien: "release", "plugin", "event", "behind-the-scenes", "dev".
// Beispiel:
//
// {
//   slug: "website-online",
//   date: "2026-11-01",
//   title: "Our website is online",
//   category: "dev",
//   excerpt: "Kurzer Teaser für die Übersicht.",
//   body: ["Erster Absatz.", "Zweiter Absatz."],
//   image: { src: "/media/news/website.webp", alt: "..." },
// },

export const news: NewsPost[] = [];
