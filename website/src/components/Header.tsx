import { useEffect, useRef, useState } from "react";
import { Link, normalizePath, usePathname } from "../lib/router";

export const navItems = [
  { to: "/", label: "Home" },
  { to: "/music", label: "Music" },
  { to: "/radio", label: "Radio" },
  { to: "/gigs", label: "Gigs" },
  { to: "/plugins", label: "Plugins" },
  { to: "/merch", label: "Merch" },
  { to: "/news", label: "News" },
  { to: "/about", label: "About" },
];

function isActive(path: string, to: string) {
  if (to === "/") return path === "/";
  return path === to || path.startsWith(to + "/");
}

export function Header() {
  const path = normalizePath(usePathname());
  const [open, setOpen] = useState(false);
  const toggleRef = useRef<HTMLButtonElement>(null);

  // Menü bei Seitenwechsel schließen
  useEffect(() => setOpen(false), [path]);

  useEffect(() => {
    if (!open) return;
    const onKey = (e: KeyboardEvent) => {
      if (e.key === "Escape") {
        setOpen(false);
        toggleRef.current?.focus();
      }
    };
    document.addEventListener("keydown", onKey);
    document.body.classList.add("menu-open");
    return () => {
      document.removeEventListener("keydown", onKey);
      document.body.classList.remove("menu-open");
    };
  }, [open]);

  return (
    <header className={`site-header${open ? " is-open" : ""}`}>
      <div className="site-header__inner">
        <Link to="/" className="brand" aria-label="BrainBugz home">
          <img src="/media/brand/wordmark.webp" alt="" width="784" height="352" />
        </Link>
        <button
          ref={toggleRef}
          type="button"
          className="menu-toggle"
          aria-expanded={open}
          aria-controls="site-nav"
          onClick={() => setOpen((o) => !o)}
        >
          <span className="menu-toggle__bars" aria-hidden="true" />
          <span className="sr-only">{open ? "Close menu" : "Open menu"}</span>
        </button>
        <nav id="site-nav" className="site-nav" aria-label="Main">
          <ul>
            {navItems.map((item) => (
              <li key={item.to}>
                <Link to={item.to} aria-current={isActive(path, item.to) ? "page" : undefined}>
                  {item.label}
                </Link>
              </li>
            ))}
            <li>
              <Link to="/booking" className="btn btn--primary btn--sm" aria-current={isActive(path, "/booking") ? "page" : undefined}>
                Booking
              </Link>
            </li>
          </ul>
        </nav>
      </div>
    </header>
  );
}
