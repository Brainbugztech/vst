// Kleiner eigener Router (History-API), damit die Seite ohne zusätzliche Abhängigkeiten auskommt.
import { useSyncExternalStore, type AnchorHTMLAttributes, type MouseEvent } from "react";

const NAV_EVENT = "bb:navigate";

function subscribe(cb: () => void) {
  window.addEventListener("popstate", cb);
  window.addEventListener(NAV_EVENT, cb);
  return () => {
    window.removeEventListener("popstate", cb);
    window.removeEventListener(NAV_EVENT, cb);
  };
}

const getPath = () => window.location.pathname;
const getSearch = () => window.location.search;

export function usePathname() {
  return useSyncExternalStore(subscribe, getPath, () => "/");
}

export function useSearch() {
  return useSyncExternalStore(subscribe, getSearch, () => "");
}

export function navigate(to: string, opts: { replace?: boolean; keepScroll?: boolean } = {}) {
  const url = new URL(to, window.location.origin);
  const samePath = url.pathname === window.location.pathname;
  if (opts.replace) window.history.replaceState(null, "", url);
  else window.history.pushState(null, "", url);
  window.dispatchEvent(new Event(NAV_EVENT));
  if (!opts.keepScroll && !samePath) {
    window.scrollTo({ top: 0, left: 0, behavior: "instant" as ScrollBehavior });
  }
}

/** Normalisiert Pfade: ohne abschließenden Slash (außer "/"). */
export function normalizePath(path: string) {
  if (path.length > 1 && path.endsWith("/")) return path.replace(/\/+$/, "");
  return path || "/";
}

export interface RouteMatch {
  name: string;
  params: Record<string, string>;
}

/** Muster wie "/plugins/:slug". */
export function matchRoute(routes: { name: string; path: string }[], pathname: string): RouteMatch | null {
  const parts = normalizePath(pathname).split("/").filter(Boolean);
  for (const route of routes) {
    const rParts = route.path.split("/").filter(Boolean);
    if (rParts.length !== parts.length) continue;
    const params: Record<string, string> = {};
    let ok = true;
    for (let i = 0; i < rParts.length; i++) {
      if (rParts[i].startsWith(":")) {
        try {
          params[rParts[i].slice(1)] = decodeURIComponent(parts[i]);
        } catch {
          ok = false;
          break;
        }
      } else if (rParts[i] !== parts[i]) {
        ok = false;
        break;
      }
    }
    if (ok) return { name: route.name, params };
  }
  return null;
}

type LinkProps = AnchorHTMLAttributes<HTMLAnchorElement> & { to: string };

/** Interner Link: lädt die Seite nicht neu. */
export function Link({ to, onClick, ...rest }: LinkProps) {
  function handle(e: MouseEvent<HTMLAnchorElement>) {
    onClick?.(e);
    if (e.defaultPrevented) return;
    if (e.button !== 0 || e.metaKey || e.ctrlKey || e.shiftKey || e.altKey) return;
    if (rest.target && rest.target !== "_self") return;
    e.preventDefault();
    navigate(to);
  }
  return <a href={to} onClick={handle} {...rest} />;
}
