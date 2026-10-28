import { about } from "../content/about";
import { gigs } from "../content/gigs";
import { news } from "../content/news";
import { plugins } from "../content/plugins";
import { releases } from "../content/releases";
import { GigCard, NewsCard, ReleaseCard } from "../components/Cards";
import { PlayButton } from "../components/MiniPlayer";
import { PluginCard, PluginCover } from "../components/PluginCard";
import { SocialLinks, configuredSocials } from "../components/Social";
import { EmptyState, SectionHeader } from "../components/ui";
import { todayISO } from "../lib/format";
import { splitGigs } from "../lib/gigs";
import { usePageMeta } from "../lib/meta";
import { categoryLabels, pickFeaturedPlugin, sortPlugins } from "../lib/plugins";
import { radioStatusText, useRadio } from "../lib/radio";
import { Link } from "../lib/router";

export function Home() {
  usePageMeta({});
  const radio = useRadio();
  const featuredRelease = releases.find((r) => r.featured) ?? [...releases].sort((a, b) => b.releaseDate.localeCompare(a.releaseDate))[0];
  const upcoming = splitGigs(gigs, todayISO()).upcoming.slice(0, 3);
  const latestNews = [...news].sort((a, b) => b.date.localeCompare(a.date)).slice(0, 3);
  const featuredPlugin = pickFeaturedPlugin(plugins);
  const morePlugins = sortPlugins(plugins, "newest").filter((p) => p !== featuredPlugin).slice(0, 3);
  const hasSocials = configuredSocials().length > 0;

  return (
    <>
      <section className="hero" aria-labelledby="page-title">
        <div className="hero__noise" aria-hidden="true" />
        <div className="hero__inner">
          <p className="kicker hero__kicker">// Psychedelic · HiTech · Duo</p>
          <h1 id="page-title" tabIndex={-1} className="hero__title">
            <img
              className="hero__wordmark"
              src="/media/brand/wordmark.webp"
              alt="BrainBugz"
              width="784"
              height="352"
              fetchPriority="high"
            />
          </h1>
          <div className="hero__bug" aria-hidden="true">
            <img src="/media/brand/bug.webp" alt="" width="730" height="368" />
            <img className="hero__bug-ghost hero__bug-ghost--a" src="/media/brand/bug.webp" alt="" />
            <img className="hero__bug-ghost hero__bug-ghost--b" src="/media/brand/bug.webp" alt="" />
          </div>
          <p className="hero__lead">Underground psychedelic HiTech duo. Music, sound design and the plugins we build for it.</p>
          <div className="hero__cta">
            <Link to="/music" className="btn btn--primary">
              Listen
            </Link>
            <Link to="/plugins" className="btn btn--ghost">
              Plugins
            </Link>
            <Link to="/booking" className="btn btn--ghost">
              Book BrainBugz
            </Link>
          </div>
        </div>
      </section>

      <section className="section intro">
        <div className="container intro__grid">
          <div>
            <SectionHeader code="01 / ABOUT" title={about.headline} />
            <p className="lead">{about.paragraphs[0]}</p>
            <Link to="/about" className="text-link">
              More about BrainBugz <span aria-hidden="true">→</span>
            </Link>
          </div>
          <ul className="pillars">
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
      </section>

      <section className="section">
        <div className="container">
          <SectionHeader code="02 / MUSIC" title="Featured release" action={{ to: "/music", label: "All music" }} />
          {featuredRelease ? (
            <ReleaseCard release={featuredRelease} headingLevel={3} />
          ) : (
            <EmptyState title="First releases incoming" compact>
              New music will appear here as soon as it's out.
            </EmptyState>
          )}
        </div>
      </section>

      {featuredPlugin && (
        <section className="section section--lab">
          <div className="container">
            <SectionHeader code="03 / PLUGINS" title="From the lab" action={{ to: "/plugins", label: "All plugins" }} />
            <article className="featured-plugin" style={{ ["--accent" as string]: featuredPlugin.accent }}>
              <PluginCover plugin={featuredPlugin} />
              <div className="featured-plugin__body">
                <p className="kicker">
                  {categoryLabels[featuredPlugin.category]} · {featuredPlugin.type}
                </p>
                <h3 className="featured-plugin__title">{featuredPlugin.name}</h3>
                <p>{featuredPlugin.tagline}</p>
                <ul className="feature-list">
                  {featuredPlugin.features.slice(0, 4).map((f) => (
                    <li key={f}>{f}</li>
                  ))}
                </ul>
                <Link to={`/plugins/${featuredPlugin.slug}`} className="btn btn--primary">
                  View {featuredPlugin.name}
                </Link>
              </div>
            </article>
            {morePlugins.length > 0 && (
              <div className="plugin-grid plugin-grid--compact">
                {morePlugins.map((p) => (
                  <PluginCard key={p.slug} plugin={p} />
                ))}
              </div>
            )}
          </div>
        </section>
      )}

      <section className="section">
        <div className="container split">
          <div>
            <SectionHeader code="04 / GIGS" title="Upcoming gigs" action={{ to: "/gigs", label: "All gigs" }} />
            {upcoming.length ? (
              <div className="gig-list">
                {upcoming.map((g) => (
                  <GigCard key={g.id} gig={g} />
                ))}
              </div>
            ) : (
              <EmptyState title="No upcoming gigs announced" compact>
                Want BrainBugz at your event? <Link to="/booking">Send a booking request</Link>.
              </EmptyState>
            )}
          </div>
          <div>
            <SectionHeader code="05 / RADIO" title="BrainBugz Radio" />
            <div className="radio-teaser">
              <div className="radio-teaser__row">
                <PlayButton />
                <div>
                  <p className="radio-teaser__status">
                    <span className={`dot dot--${radio.status}`} aria-hidden="true" /> {radioStatusText[radio.status]}
                  </p>
                  <p className="muted">
                    {radio.status === "unconfigured"
                      ? "A 24/7 BrainBugz stream is planned. It isn't on air yet."
                      : "Our stream, straight from the lab."}
                  </p>
                </div>
              </div>
              <Link to="/radio" className="text-link">
                Radio page <span aria-hidden="true">→</span>
              </Link>
            </div>
          </div>
        </div>
      </section>

      <section className="section">
        <div className="container">
          <SectionHeader code="06 / NEWS" title="Latest news" action={latestNews.length ? { to: "/news", label: "All news" } : undefined} />
          {latestNews.length ? (
            <div className="news-grid">
              {latestNews.map((n) => (
                <NewsCard key={n.slug} post={n} />
              ))}
            </div>
          ) : (
            <EmptyState title="No news yet" compact>
              Release, plugin and event updates will show up here.
            </EmptyState>
          )}
        </div>
      </section>

      <section className="section cta-band">
        <div className="container cta-band__inner">
          <div>
            <p className="kicker">// Booking</p>
            <h2 className="glitch" data-text="Bring the bugz to your floor">
              Bring the bugz to your floor
            </h2>
            <p>Clubs, festivals, open airs: send us the details and we'll get back to you.</p>
          </div>
          <div className="cta-band__actions">
            <Link to="/booking" className="btn btn--primary btn--lg">
              Booking request
            </Link>
            {hasSocials && <SocialLinks className="social--inline" />}
          </div>
        </div>
      </section>
    </>
  );
}
