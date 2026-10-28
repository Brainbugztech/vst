import { Link } from "../lib/router";
import { navItems } from "./Header";
import { SocialLinks } from "./Social";

export function Footer() {
  return (
    <footer className="site-footer">
      <div className="site-footer__inner">
        <div className="site-footer__brand">
          <img src="/media/brand/bug.webp" alt="" width="730" height="368" loading="lazy" />
          <p>Underground psychedelic HiTech. Music, sound design and plugins from the BrainBugz lab.</p>
          <SocialLinks />
        </div>
        <nav aria-label="Footer">
          <ul className="site-footer__nav">
            {navItems.map((n) => (
              <li key={n.to}>
                <Link to={n.to}>{n.label}</Link>
              </li>
            ))}
            <li>
              <Link to="/booking">Booking</Link>
            </li>
          </ul>
        </nav>
        <div className="site-footer__legal">
          <Link to="/impressum">Impressum</Link>
          <Link to="/datenschutz">Datenschutz / Privacy</Link>
          <span>© {new Date().getFullYear()} BrainBugz</span>
        </div>
      </div>
    </footer>
  );
}
