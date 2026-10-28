import { useEffect, useId, useRef, useState } from "react";
import { radio as radioConfig } from "../config/site";
import { radioStatusLabel, useRadio } from "../lib/radio";
import { Link } from "../lib/router";

export function PlayButton({ large = false }: { large?: boolean }) {
  const r = useRadio();
  if (r.mode === "none") return null;
  const active = r.status === "playing" || r.status === "loading";
  return (
    <button type="button" className={`play-btn${large ? " play-btn--lg" : ""}`} onClick={r.toggle} aria-pressed={active}>
      <span className={`play-btn__icon ${active ? "is-pause" : "is-play"}`} aria-hidden="true" />
      <span className="sr-only">{active ? `Pause ${radioConfig.name}` : `Play ${radioConfig.name}`}</span>
    </button>
  );
}

export function VolumeSlider() {
  const r = useRadio();
  const id = useId();
  if (r.mode === "none") return null;
  return (
    <div className="volume">
      <label htmlFor={id}>Volume</label>
      <input
        id={id}
        type="range"
        min={0}
        max={100}
        step={1}
        value={Math.round(r.volume * 100)}
        onChange={(e) => r.setVolume(Number(e.target.value) / 100)}
      />
    </div>
  );
}

export function NowPlayingText() {
  const r = useRadio();
  if (!r.nowPlaying || r.status !== "playing") return null;
  const { artist, title } = r.nowPlaying;
  return (
    <p className="now-playing" aria-live="polite">
      <span className="kicker">Now playing</span> {[artist, title].filter(Boolean).join(" – ")}
    </p>
  );
}

/** Hinweis vor dem ersten Abspielen im Loop-Modus (Verbindung zu SoundCloud). */
export function LoopConsentNote() {
  const r = useRadio();
  if (r.mode !== "loop" || r.loopFrameSrc) return null;
  return (
    <p className="muted small">
      Pressing play loads the SoundCloud player (soundcloud.com), which may set cookies. See our{" "}
      <Link to="/datenschutz">privacy policy</Link>.
    </p>
  );
}

/** Persistenter Mini-Player unten. Zeigt ehrlich "offline", solange weder Stream noch Loop eingetragen ist. */
export function MiniPlayer() {
  const r = useRadio();
  const [expanded, setExpanded] = useState(false);
  const panelId = useId();
  const shownFrame = useRef(false);

  // Beim ersten Loop-Start den Player aufklappen, damit der SoundCloud-Player sichtbar ist
  useEffect(() => {
    if (r.loopFrameSrc && !shownFrame.current) {
      shownFrame.current = true;
      setExpanded(true);
    }
  }, [r.loopFrameSrc]);

  return (
    <aside className={`mini-player${expanded ? " is-expanded" : ""}`} aria-label="Radio player">
      <div className="mini-player__bar">
        <PlayButton />
        <div className="mini-player__info">
          <span className={`dot dot--${r.status}`} aria-hidden="true" />
          <span className="mini-player__name">{radioConfig.name}</span>
          <span className="mini-player__status" aria-live="polite">
            {radioStatusLabel(r.mode, r.status)}
          </span>
        </div>
        <button
          type="button"
          className="mini-player__toggle"
          aria-expanded={expanded}
          aria-controls={panelId}
          onClick={() => setExpanded((e) => !e)}
        >
          <span aria-hidden="true">{expanded ? "−" : "+"}</span>
          <span className="sr-only">{expanded ? "Collapse radio player" : "Expand radio player"}</span>
        </button>
      </div>
      {/* Das iframe bleibt immer im DOM (auch zugeklappt), damit die Musik beim Seitenwechsel weiterläuft */}
      <div id={panelId} className={`mini-player__panel${expanded ? "" : " is-collapsed"}`}>
        {r.mode === "none" && (
          <p>The BrainBugz radio stream isn't live yet. Until then, listen to our releases on the music page.</p>
        )}
        {r.mode === "loop" && (
          <>
            <p className="small">
              <strong>
                {radioConfig.loop.artist} – {radioConfig.loop.title}
              </strong>{" "}
              · Recorded set ({radioConfig.loop.note}), not a live stream.
            </p>
            <LoopConsentNote />
          </>
        )}
        {r.mode === "stream" && <NowPlayingText />}
        {r.status === "error" && <p role="alert">The player can't be reached right now. Please try again later.</p>}
        {r.loopFrameSrc && (
          <iframe
            ref={r.registerLoopFrame}
            className="embed-frame mini-player__frame"
            title={`SoundCloud player: ${radioConfig.loop.artist} – ${radioConfig.loop.title}`}
            src={r.loopFrameSrc}
            width="100%"
            height={120}
            allow="autoplay; encrypted-media"
            referrerPolicy="strict-origin-when-cross-origin"
          />
        )}
        <VolumeSlider />
        <div className="mini-player__links">
          <Link to="/radio" className="text-link">
            Radio page →
          </Link>
          <Link to="/music" className="text-link">
            Music →
          </Link>
        </div>
      </div>
    </aside>
  );
}
