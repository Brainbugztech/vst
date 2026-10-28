// Radio-Zustand für die ganze Seite (Mini-Player + Radio-Seite teilen sich denselben Player).
// Modus:
//  - "stream": echter Live-Stream (radio.streamUrl), alle hören dasselbe
//  - "loop":   SoundCloud-Set in Dauerschleife (radio.loop.soundcloudUrl), solange es keinen Stream gibt
//  - "none":   nichts eingetragen → ehrlich "offline", nichts wird vorgetäuscht
import { createContext, useCallback, useContext, useEffect, useMemo, useRef, useState, type ReactNode } from "react";
import { radio as radioConfig } from "../config/site";
import { safeExternalUrl, soundcloudEmbedUrl } from "./urls";
import { readNumber, writeNumber } from "./storage";

export type RadioMode = "stream" | "loop" | "none";
export type RadioStatus = "unconfigured" | "idle" | "loading" | "playing" | "paused" | "error";

export interface NowPlaying {
  artist?: string;
  title?: string;
}

interface RadioState {
  mode: RadioMode;
  status: RadioStatus;
  volume: number;
  nowPlaying: NowPlaying | null;
  /** Nur im Loop-Modus: Adresse des SoundCloud-Players, erst nach dem ersten Klick gesetzt (DSGVO). */
  loopFrameSrc: string | null;
  /** Mini-Player meldet hier sein iframe an. */
  registerLoopFrame: (el: HTMLIFrameElement | null) => void;
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
  const az = Array.isArray(j) ? j[0]?.now_playing?.song : j.now_playing?.song;
  if (az && (az.title || az.artist)) return { artist: az.artist || undefined, title: az.title || undefined };
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

export function resolveRadioMode(cfg: { streamUrl: string; loop?: { soundcloudUrl: string } }): RadioMode {
  if (safeExternalUrl(cfg.streamUrl)) return "stream";
  if (soundcloudEmbedUrl(cfg.loop?.soundcloudUrl)) return "loop";
  return "none";
}

// ---------- SoundCloud Widget API (offizielles Script, wird erst beim ersten Play geladen) ----------

interface SCWidget {
  bind(event: string, cb: (e?: unknown) => void): void;
  play(): void;
  pause(): void;
  seekTo(ms: number): void;
  setVolume(v: number): void;
}
interface SCGlobal {
  Widget: ((el: HTMLIFrameElement) => SCWidget) & { Events: Record<string, string> };
}
declare global {
  interface Window {
    SC?: SCGlobal;
  }
}

let scriptPromise: Promise<SCGlobal> | null = null;
function loadSoundCloudApi(): Promise<SCGlobal> {
  if (window.SC) return Promise.resolve(window.SC);
  if (!scriptPromise) {
    scriptPromise = new Promise((resolve, reject) => {
      const s = document.createElement("script");
      s.src = "https://w.soundcloud.com/player/api.js";
      s.async = true;
      s.onload = () => (window.SC ? resolve(window.SC) : reject(new Error("SC fehlt")));
      s.onerror = () => {
        scriptPromise = null;
        reject(new Error("SoundCloud-API nicht erreichbar"));
      };
      document.head.appendChild(s);
    });
  }
  return scriptPromise;
}

export function RadioProvider({ children }: { children: ReactNode }) {
  const mode = resolveRadioMode(radioConfig);
  const streamUrl = safeExternalUrl(radioConfig.streamUrl);
  const metadataUrl = safeExternalUrl(radioConfig.metadataUrl);
  const audioRef = useRef<HTMLAudioElement | null>(null);
  const widgetRef = useRef<SCWidget | null>(null);
  const frameRef = useRef<HTMLIFrameElement | null>(null);
  const [status, setStatus] = useState<RadioStatus>(mode === "none" ? "unconfigured" : "idle");
  const [volume, setVolumeState] = useState(() => Math.min(1, Math.max(0, readNumber("bb-radio-volume", 0.8))));
  const [nowPlaying, setNowPlaying] = useState<NowPlaying | null>(null);
  const [loopFrameSrc, setLoopFrameSrc] = useState<string | null>(null);
  const volumeRef = useRef(volume);
  volumeRef.current = volume;

  // ----- Stream -----
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

  // ----- Loop: Widget an das iframe binden, sobald iframe + API da sind -----
  const bindWidget = useCallback(async () => {
    const frame = frameRef.current;
    if (!frame || widgetRef.current) return;
    try {
      const SC = await loadSoundCloudApi();
      if (frameRef.current !== frame) return;
      const w = SC.Widget(frame);
      const E = SC.Widget.Events;
      w.bind(E.READY, () => {
        w.setVolume(Math.round(volumeRef.current * 100));
        w.play();
      });
      w.bind(E.PLAY, () => setStatus("playing"));
      w.bind(E.PAUSE, () => setStatus("paused"));
      // Dauerschleife: am Ende wieder von vorne
      w.bind(E.FINISH, () => {
        w.seekTo(0);
        w.play();
      });
      w.bind(E.ERROR, () => setStatus("error"));
      widgetRef.current = w;
    } catch {
      setStatus("error");
    }
  }, []);

  const registerLoopFrame = useCallback(
    (el: HTMLIFrameElement | null) => {
      if (el === frameRef.current) return;
      frameRef.current = el;
      widgetRef.current = null;
      if (el) void bindWidget();
    },
    [bindWidget],
  );

  const play = useCallback(() => {
    if (mode === "stream" && streamUrl) {
      const a = getAudio();
      a.volume = volumeRef.current;
      a.src = streamUrl; // bei jedem Start neu verbinden (live, nicht zeitversetzt)
      setStatus("loading");
      a.play().catch(() => setStatus("error"));
      return;
    }
    if (mode === "loop") {
      if (widgetRef.current) {
        widgetRef.current.play();
        return;
      }
      // Erster Klick: SoundCloud-Player laden (erst jetzt Verbindung zu SoundCloud)
      setStatus("loading");
      setLoopFrameSrc(soundcloudEmbedUrl(radioConfig.loop.soundcloudUrl, { autoPlay: true }));
    }
  }, [mode, streamUrl, getAudio]);

  const pause = useCallback(() => {
    if (mode === "stream") {
      const a = audioRef.current;
      if (!a) return;
      a.pause();
      a.removeAttribute("src");
      a.load();
      setStatus("idle");
      return;
    }
    if (mode === "loop") widgetRef.current?.pause();
  }, [mode]);

  const toggle = useCallback(() => {
    if (status === "playing" || status === "loading") pause();
    else play();
  }, [status, play, pause]);

  const setVolume = useCallback((v: number) => {
    const clamped = Math.min(1, Math.max(0, v));
    setVolumeState(clamped);
    writeNumber("bb-radio-volume", clamped);
    if (audioRef.current) audioRef.current.volume = clamped;
    widgetRef.current?.setVolume(Math.round(clamped * 100));
  }, []);

  // Titel-Infos: Stream fragt die Metadaten ab, Loop zeigt das eingetragene Set
  useEffect(() => {
    if (mode === "loop") {
      setNowPlaying(status === "playing" ? { artist: radioConfig.loop.artist, title: radioConfig.loop.title } : null);
      return;
    }
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
  }, [mode, metadataUrl, status]);

  useEffect(() => () => audioRef.current?.pause(), []);

  const value = useMemo(
    () => ({ mode, status, volume, nowPlaying, loopFrameSrc, registerLoopFrame, play, pause, toggle, setVolume }),
    [mode, status, volume, nowPlaying, loopFrameSrc, registerLoopFrame, play, pause, toggle, setVolume],
  );
  return <RadioContext.Provider value={value}>{children}</RadioContext.Provider>;
}

export function useRadio() {
  const ctx = useContext(RadioContext);
  if (!ctx) throw new Error("useRadio braucht RadioProvider");
  return ctx;
}

export function radioStatusLabel(mode: RadioMode, status: RadioStatus): string {
  if (mode === "none") return "Offline: no stream yet";
  const loop = mode === "loop";
  switch (status) {
    case "loading":
      return "Connecting…";
    case "playing":
      return loop ? `Playing: ${radioConfig.loop.note}` : "Live";
    case "paused":
      return "Paused";
    case "error":
      return loop ? "Player unavailable" : "Stream unavailable";
    default:
      return loop ? radioConfig.loop.note : "Ready";
  }
}
