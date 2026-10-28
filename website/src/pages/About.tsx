import { about } from "../content/about";
import { SocialLinks } from "../components/Social";
import { PageHeader } from "../components/ui";
import { usePageMeta } from "../lib/meta";
import { Link } from "../lib/router";

export function About() {
  usePageMeta({ title: "About", description: "About BrainBugz, an underground psychedelic HiTech duo." });
  return (
    <div className="container page">
      <PageHeader code="// ABOUT" title="About" />
      <div className="about">
        <div className="about__text">
          <h2>{about.headline}</h2>
          {about.paragraphs.map((p) => (
            <p key={p}>{p}</p>
          ))}
          <SocialLinks />
          <div className="btn-row">
            <Link to="/booking" className="btn btn--primary">
              Booking request
            </Link>
            <Link to="/music" className="btn btn--ghost">
              Music
            </Link>
          </div>
        </div>
        <figure className="about__visual">
          <img src="/media/brand/logo-full.webp" alt="BrainBugz logo: a brain-shaped mechanical bug" width="730" height="583" loading="lazy" />
        </figure>
      </div>
      <ul className="pillars pillars--wide">
        {about.pillars.map((p, i) => (
          <li key={p.title}>
            <span className="pillars__n" aria-hidden="true">
              0{i + 1}
            </span>
            <strong>{p.title}</strong>
            <span>{p.text}</span>
          </li>
        ))}
      </ul>
    </div>
  );
}
