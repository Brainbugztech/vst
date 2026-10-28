import { useEffect } from "react";
import { site } from "../config/site";

function setMeta(attr: "name" | "property", key: string, value: string) {
  let el = document.head.querySelector<HTMLMetaElement>(`meta[${attr}="${key}"]`);
  if (!el) {
    el = document.createElement("meta");
    el.setAttribute(attr, key);
    document.head.appendChild(el);
  }
  el.setAttribute("content", value);
}

function setCanonical(href: string) {
  let el = document.head.querySelector<HTMLLinkElement>('link[rel="canonical"]');
  if (!el) {
    el = document.createElement("link");
    el.rel = "canonical";
    document.head.appendChild(el);
  }
  el.href = href;
}

/** Setzt Titel, Beschreibung und Social-Preview für die aktuelle Seite. */
export function usePageMeta(opts: { title?: string; description?: string; image?: string; noindex?: boolean }) {
  const { title, description, image, noindex } = opts;
  useEffect(() => {
    const fullTitle = title ? `${title} | ${site.name}` : `${site.name}: ${site.tagline}`;
    const desc = description ?? site.description;
    const base = site.url.replace(/\/$/, "");
    const img = image ? (image.startsWith("http") ? image : base + image) : `${base}/media/brand/og-image.jpg`;
    const url = base + window.location.pathname;
    document.title = fullTitle;
    setMeta("name", "description", desc);
    setMeta("property", "og:title", fullTitle);
    setMeta("property", "og:description", desc);
    setMeta("property", "og:image", img);
    setMeta("property", "og:url", url);
    setMeta("name", "twitter:title", fullTitle);
    setMeta("name", "twitter:description", desc);
    setMeta("name", "twitter:image", img);
    setMeta("name", "robots", noindex ? "noindex" : "index,follow");
    setCanonical(url);
  }, [title, description, image, noindex]);
}
