import { social } from "../config/site";
import { safeExternalUrl } from "../lib/urls";
import { ExternalLink } from "./ui";

const labels: Record<keyof typeof social, string> = {
  instagram: "Instagram",
  soundcloud: "SoundCloud",
  spotify: "Spotify",
  youtube: "YouTube",
  bandcamp: "Bandcamp",
  tiktok: "TikTok",
};

/** Nur Profile, die in src/config/site.ts wirklich eingetragen sind. */
export function configuredSocials() {
  return (Object.keys(social) as (keyof typeof social)[])
    .map((k) => ({ key: k, label: labels[k], url: safeExternalUrl(social[k]) }))
    .filter((s): s is { key: keyof typeof social; label: string; url: string } => !!s.url);
}

export function SocialLinks({ className = "" }: { className?: string }) {
  const list = configuredSocials();
  if (!list.length) return null;
  return (
    <ul className={`social ${className}`}>
      {list.map((s) => (
        <li key={s.key}>
          <ExternalLink href={s.url} className="social__link">
            {s.label}
          </ExternalLink>
        </li>
      ))}
    </ul>
  );
}
