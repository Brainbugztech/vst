import { useId, useState } from "react";
import { radio as radioConfig } from "../config/site";
import { radioStatusText, useRadio } from "../lib/radio";
import { Link } from "../lib/router";

export function PlayButton({ large = false }: { large?: boolean }) {
  const r = useRadio();
  if (r.status === "unconfigured") return null;
  const active = r.status === "playing" || r.status === "loading";
  return (
    <button type="button" className={`play-btn${large ? " play-btn--lg" : ""}`} onClick={r.toggle} aria-pressed={active}>
      <span className={`play-btn__icon ${active ? "is-pause" : "is-play"}`} aria-hidden="true" />
      <span className="sr-only">{active ? `Stop ${radioConfig.name}` : `Play ${radioConfig.name}`}</span>
    </button>
  );
}

export function VolumeSlider() {
  const r = useRadio();
  const id = useId();
  if (r.status === "unconfigured") return null;
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

/** Persistenter Mini-Player unten. Zeigt ehrlich "offline", solange kein Stream eingetragen ist. */
export function MiniPlayer() {
  const r = useRadio();
  const [expanded, setExpanded] = useState(false);
  const panelId = useId();
  const live = r.status === "playing";
  return (
    <aside className={`mini-player${expanded ? " is-expanded" : ""}`} aria-label="Radio player">
      <div className="mini-player__bar">
        <PlayButton />
        <div className="mini-player__info">
          <span className={`dot dot--${r.status}`} aria-hidden="true" />
          <span className="mini-player__name">{radioConfig.name}</span>
          <span className="mini-player__status" aria-live="polite">
            {radioStatusText[r.status]}
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
      <div id={panelId} className="mini-player__panel" hidden={!expanded}>
        {r.status === "unconfigured" ? (
          <p>The BrainBugz radio stream isn't live yet. Until then, listen to our releases on the music page.</p>
        ) : (
          <>
            <NowPlayingText />
            {!live && r.status === "error" && <p>The stream can't be reached right now. Please try again later.</p>}
            <VolumeSlider />
          </>
        )}
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
