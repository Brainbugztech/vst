// Radio-Zustand für die ganze Seite (Mini-Player + Radio-Seite teilen sich denselben Player).
// Ohne streamUrl in src/config/site.ts bleibt alles im Zustand "unconfigured": nichts spielt, nichts wird vorgetäuscht.
import { createContext, useCallback, useContext, useEffect, useMemo, useRef, useState, type ReactNode } from "react";
import { radio as radioConfig } from "../config/site";
import { safeExternalUrl } from "./urls";
import { readNumber, writeNumber } from "./storage";

export type RadioStatus = "unconfigured" | "idle" | "loading" | "playing" | "error";

export interface NowPlaying {
  artist?: string;
  title?: string;
}

interface RadioState {
  status: RadioStatus;
  volume: number;
  nowPlaying: NowPlaying | null;
  play: () => void;
  pause: () => void;
  toggle: () => void;
  setVolume: (v: number) => void;
}

const RadioContext = createContext<RadioState | null>(null);

/** Liest gängige Now-Playing-Formate (Icecast, AzuraCast, einfaches JSON). */
export function parseNowPlaying(json: unknown): NowPlaying | null {
  if (!json || typeof json !== "object") return null;
  const j = json as Record<string, any>;
  // AzuraCast: { now_playing: { song: { artist, title } } } (auch als Array)
  const az = Array.isArray(j) ? j[0]?.now_playing?.song : j.now_playing?.song;
  if (az && (az.title || az.artist)) return { artist: az.artist || undefined, title: az.title || undefined };
  // Icecast status-json.xsl: { icestats: { source: {title} | [{title}] } }
  const src = j.icestats?.source;
  const s = Array.isArray(src) ? src[0] : src;
  if (s && (s.title || s.artist)) {
    if (!s.artist && typeof s.title === "string" && s.title.includes(" - ")) {
      const [artist, ...rest] = s.title.split(" - ");
      return { artist, title: rest.join(" - ") };
    }
    return { artist: s.artist || undefined, title: s.title || undefined };
  }
  if (typeof j.title === "string" || typeof j.artist === "string") {
    return { artist: j.artist || undefined, title: j.title || undefined };
  }
  return null;
}

export function RadioProvider({ children }: { children: ReactNode }) {
  const streamUrl = safeExternalUrl(radioConfig.streamUrl);
  const metadataUrl = safeExternalUrl(radioConfig.metadataUrl);
  const audioRef = useRef<HTMLAudioElement | null>(null);
  const [status, setStatus] = useState<RadioStatus>(streamUrl ? "idle" : "unconfigured");
  const [volume, setVolumeState] = useState(() => Math.min(1, Math.max(0, readNumber("bb-radio-volume", 0.8))));
  const [nowPlaying, setNowPlaying] = useState<NowPlaying | null>(null);

  const getAudio = useCallback(() => {
    if (!audioRef.current) {
      const a = new Audio();
      a.preload = "none";
      a.addEventListener("playing", () => setStatus("playing"));
      a.addEventListener("waiting", () => setStatus("loading"));
      a.addEventListener("error", () => {
        if (a.getAttribute("src")) setStatus("error");
      });
      audioRef.current = a;
    }
    return audioRef.current;
  }, []);

  const play = useCallback(() => {
    if (!streamUrl) return;
    const a = getAudio();
    a.volume = volume;
    // Live-Stream: bei jedem Start neu verbinden, damit man nicht zeitversetzt hört.
    a.src = streamUrl;
    setStatus("loading");
    a.play().catch(() => setStatus("error"));
  }, [streamUrl, getAudio, volume]);

  const pause = useCallback(() => {
    const a = audioRef.current;
    if (!a) return;
    a.pause();
    a.removeAttribute("src");
    a.load();
    setStatus(streamUrl ? "idle" : "unconfigured");
  }, [streamUrl]);

  const toggle = useCallback(() => {
    if (status === "playing" || status === "loading") pause();
    else play();
  }, [status, play, pause]);

  const setVolume = useCallback((v: number) => {
    const clamped = Math.min(1, Math.max(0, v));
    setVolumeState(clamped);
    writeNumber("bb-radio-volume", clamped);
    if (audioRef.current) audioRef.current.volume = clamped;
  }, []);

  // Titel-Infos nur abfragen, solange wirklich gespielt wird.
  useEffect(() => {
    if (!metadataUrl || status !== "playing") return;
    let cancelled = false;
    const load = async () => {
      try {
        const res = await fetch(metadataUrl, { cache: "no-store" });
        if (!res.ok) return;
        const np = parseNowPlaying(await res.json());
        if (!cancelled) setNowPlaying(np);
      } catch {
        /* Metadaten sind optional */
      }
    };
    load();
    const id = window.setInterval(load, 20000);
    return () => {
      cancelled = true;
      window.clearInterval(id);
    };
  }, [metadataUrl, status]);

  useEffect(() => () => audioRef.current?.pause(), []);

  const value = useMemo(
    () => ({ status, volume, nowPlaying, play, pause, toggle, setVolume }),
    [status, volume, nowPlaying, play, pause, toggle, setVolume],
  );
  return <RadioContext.Provider value={value}>{children}</RadioContext.Provider>;
}

export function useRadio() {
  const ctx = useContext(RadioContext);
  if (!ctx) throw new Error("useRadio braucht RadioProvider");
  return ctx;
}

export const radioStatusText: Record<RadioStatus, string> = {
  unconfigured: "Offline: no stream yet",
  idle: "Ready",
  loading: "Connecting…",
  playing: "Live",
  error: "Stream unavailable",
};
