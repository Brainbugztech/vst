import { radio as radioConfig, social } from "../config/site";
import { LoopConsentNote, NowPlayingText, PlayButton, VolumeSlider } from "../components/MiniPlayer";
import { ExternalLink, PageHeader } from "../components/ui";
import { usePageMeta } from "../lib/meta";
import { radioStatusLabel, useRadio } from "../lib/radio";
import { Link } from "../lib/router";

export function Radio() {
  usePageMeta({ title: "Radio", description: "BrainBugz Radio: the planned 24/7 psychedelic HiTech stream." });
  const r = useRadio();
  return (
    <div className="container page">
      <PageHeader code="// RADIO" title="BrainBugz Radio" />
      <section className="radio-console" aria-label="Radio player">
        <div className="radio-console__screen">
          <div className={`radio-console__wave${r.status === "playing" ? " is-live" : ""}`} aria-hidden="true">
            {Array.from({ length: 32 }, (_, i) => (
              <span key={i} style={{ animationDelay: `${(i % 8) * -0.12}s` }} />
            ))}
          </div>
          <p className="radio-console__status" aria-live="polite">
            <span className={`dot dot--${r.status}`} aria-hidden="true" /> {radioStatusLabel(r.mode, r.status)}
          </p>
          <NowPlayingText />
        </div>
        {r.mode === "none" ? (
          <div className="radio-console__offline">
            <h2>Not on air yet</h2>
            <p>
              We're preparing a BrainBugz stream. There's no live stream right now, so there's nothing to play here yet.
              The player in the bottom bar switches on automatically as soon as the stream goes live.
            </p>
            <p>
              Until then: <Link to="/music">listen to our releases</Link>
              {social.soundcloud && (
                <>
                  {" "}
                  or visit us on <ExternalLink href={social.soundcloud}>SoundCloud</ExternalLink>
                </>
              )}
              .
            </p>
          </div>
        ) : (
          <div className="radio-console__controls">
            {r.mode === "loop" && (
              <div>
                <h2>
                  {radioConfig.loop.artist} – {radioConfig.loop.title}
                </h2>
                <p>
                  Recorded set ({radioConfig.loop.note}). It starts over by itself when it ends. This isn't a live stream yet; a
                  real 24/7 stream is planned.
                </p>
              </div>
            )}
            <PlayButton large />
            <VolumeSlider />
            <LoopConsentNote />
            {r.status === "error" && <p role="alert">The player can't be reached right now. Please try again later.</p>}
            <p className="muted">{radioConfig.name} keeps playing while you browse the site.</p>
          </div>
        )}
      </section>
    </div>
  );
}
