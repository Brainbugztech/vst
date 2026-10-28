import type { AnchorHTMLAttributes, ReactNode } from "react";
import { Link } from "../lib/router";
import { safeExternalUrl } from "../lib/urls";

/** Externer Link: neuer Tab, ohne Referrer-Leak. Ungültige URLs werden gar nicht erst gerendert. */
export function ExternalLink({
  href,
  children,
  ...rest
}: AnchorHTMLAttributes<HTMLAnchorElement> & { href: string | undefined }) {
  const safe = safeExternalUrl(href);
  if (!safe) return null;
  return (
    <a href={safe} target="_blank" rel="noopener noreferrer" {...rest}>
      {children}
      <span className="sr-only"> (opens in a new tab)</span>
    </a>
  );
}

export function PageHeader({ code, title, intro, children }: { code: string; title: string; intro?: ReactNode; children?: ReactNode }) {
  return (
    <header className="page-header">
      <p className="kicker" aria-hidden="true">
        {code}
      </p>
      <h1 className="glitch" data-text={title} tabIndex={-1} id="page-title">
        {title}
      </h1>
      {intro && <p className="page-intro">{intro}</p>}
      {children}
    </header>
  );
}

export function SectionHeader({ code, title, action }: { code: string; title: string; action?: { to: string; label: string } }) {
  return (
    <div className="section-header">
      <div>
        <p className="kicker" aria-hidden="true">
          {code}
        </p>
        <h2>{title}</h2>
      </div>
      {action && (
        <Link className="text-link" to={action.to}>
          {action.label} <span aria-hidden="true">→</span>
        </Link>
      )}
    </div>
  );
}

export function EmptyState({ title, children, compact }: { title: string; children?: ReactNode; compact?: boolean }) {
  return (
    <div className={`empty${compact ? " empty--compact" : ""}`} role="status">
      <span className="empty__signal" aria-hidden="true" />
      <div>
        <p className="empty__title">{title}</p>
        {children && <div className="empty__text">{children}</div>}
      </div>
    </div>
  );
}

export function Pill({ children, tone = "default" }: { children: ReactNode; tone?: "default" | "green" | "blue" | "violet" | "pink" | "muted" }) {
  return <span className={`pill pill--${tone}`}>{children}</span>;
}
