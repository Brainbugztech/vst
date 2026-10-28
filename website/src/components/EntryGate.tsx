import { useEffect, useRef, useState } from "react";
import { radio as radioConfig } from "../config/site";
import { useRadio } from "../lib/radio";
import { Link } from "../lib/router";

const KEY = "bb-entered";

function alreadyEntered() {
  try {
    return window.sessionStorage.getItem(KEY) === "1";
  } catch {
    return false;
  }
}

function rememberEntered() {
  try {
    window.sessionStorage.setItem(KEY, "1");
  } catch {
    /* ohne Speicher kommt der Screen eben beim nächsten Neuladen wieder */
  }
}

/**
 * Eingangs-Screen beim ersten Besuch (pro Browser-Sitzung).
 * "Enter with sound": dieser Klick ist die Zustimmung zu SoundCloud UND die Nutzer-Geste,
 * die Browser für Ton verlangen. Erst dann wird der Player geladen und startet leise.
 */
export function EntryGate() {
  const r = useRadio();
  const enabled = radioConfig.entryGate.enabled && r.mode !== "none";
  const [open, setOpen] = useState(() => enabled && !alreadyEntered());
  const soundBtn = useRef<HTMLButtonElement>(null);

  useEffect(() => {
    if (!open) return;
    document.body.classList.add("gate-open");
    // Alles hinter dem Screen für Tastatur und Screenreader sperren
    const behind = document.querySelectorAll<HTMLElement>(".skip-link, .site-header, main, .site-footer, .mini-player");
    behind.forEach((el) => el.setAttribute("inert", ""));
    soundBtn.current?.focus();
    const onKey = (e: KeyboardEvent) => {
      if (e.key === "Escape") close();
    };
    document.addEventListener("keydown", onKey);
    return () => {
      document.body.classList.remove("gate-open");
      behind.forEach((el) => el.removeAttribute("inert"));
      document.removeEventListener("keydown", onKey);
    };
  }, [open]);

  function close() {
    rememberEntered();
    setOpen(false);
  }

  if (!open) return null;

  const isLoop = r.mode === "loop";
  return (
    <div className="gate" role="dialog" aria-modal="true" aria-labelledby="gate-title" aria-describedby="gate-desc">
      <div className="gate__inner">
        <img className="gate__bug" src="/media/brand/bug.webp" alt="" width="730" height="368" />
        <h2 id="gate-title" className="gate__title">
          <img src="/media/brand/wordmark.webp" alt="BrainBugz" width="784" height="352" />
        </h2>
        <p id="gate-desc" className="gate__desc">
          {isLoop ? (
            <>
              Enter the lab with <strong>{radioConfig.loop.artist} – {radioConfig.loop.title}</strong> playing ({radioConfig.loop.note}).
            </>
          ) : (
            <>Enter the lab with {radioConfig.name} playing.</>
          )}
        </p>
        <div className="gate__actions">
          <button
            ref={soundBtn}
            type="button"
            className="btn btn--primary btn--lg"
            onClick={() => {
              r.setVolume(radioConfig.entryGate.volume);
              r.play();
              close();
            }}
          >
            Enter with sound
          </button>
          <button type="button" className="btn btn--ghost" onClick={close}>
            Enter without sound
          </button>
        </div>
        <p className="gate__note">
          {isLoop
            ? "“Enter with sound” loads the SoundCloud player (soundcloud.com), which may set cookies."
            : "“Enter with sound” connects to our stream server."}{" "}
          Starts at {Math.round(radioConfig.entryGate.volume * 100)} % volume, you can stop it anytime in the player.{" "}
          <Link to="/datenschutz" onClick={close}>
            Privacy policy
          </Link>{" "}
          ·{" "}
          <Link to="/impressum" onClick={close}>
            Impressum
          </Link>
        </p>
      </div>
    </div>
  );
}
