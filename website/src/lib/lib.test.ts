import { describe, expect, it, vi } from "vitest";
import type { Plugin } from "../types";
import { emptyBooking, encodeForNetlify, isValidEmail, submitBooking, validateBooking } from "./booking";
import { formatDate, parseISODate, todayISO } from "./format";
import { splitGigs } from "./gigs";
import { checkoutUrlOf, downloadUrlOf, filterPlugins, pickFeaturedPlugin, sortPlugins } from "./plugins";
import { parseNowPlaying, resolveRadioMode } from "./radio";
import { matchRoute, normalizePath } from "./router";
import { instagramDmUrl, safeExternalUrl, soundcloudEmbedUrl, spotifyEmbedUrl } from "./urls";

const base: Plugin = {
  slug: "x",
  name: "X",
  published: true,
  tagline: "t",
  description: [],
  features: [],
  category: "fx",
  type: "Glitch & Stutter",
  status: "in-development",
  access: { type: "coming-soon" },
  accent: "#fff",
  os: [],
  formats: ["VST3"],
  testedDaws: [],
  addedDate: "2026-01-01",
  changelog: [],
};
const mk = (o: Partial<Plugin>): Plugin => ({ ...base, ...o });

describe("booking", () => {
  const good = {
    ...emptyBooking,
    name: "Alex",
    email: "alex@example.com",
    eventDate: "2026-12-01",
    location: "Club, Berlin, Germany",
    eventType: "Club night",
    message: "We'd love to book you for our night.",
    consent: true,
  };
  it("accepts a complete request", () => {
    expect(validateBooking(good, "2026-10-28")).toEqual({});
  });
  it("requires fields and consent", () => {
    const e = validateBooking(emptyBooking, "2026-10-28");
    expect(Object.keys(e).sort()).toEqual(["consent", "email", "eventDate", "eventType", "location", "message", "name"]);
  });
  it("optional fields stay optional", () => {
    const e = validateBooking({ ...good, organization: "", eventName: "", budget: "" }, "2026-10-28");
    expect(e).toEqual({});
  });
  it("rejects bad email, past date, unknown event type", () => {
    const e = validateBooking({ ...good, email: "nope@", eventDate: "2026-10-27", eventType: "Hack" }, "2026-10-28");
    expect(e.email).toBeTruthy();
    expect(e.eventDate).toMatch(/past/);
    expect(e.eventType).toBeTruthy();
  });
  it("today is allowed, invalid date is not", () => {
    expect(validateBooking({ ...good, eventDate: "2026-10-28" }, "2026-10-28").eventDate).toBeUndefined();
    expect(validateBooking({ ...good, eventDate: "2026-02-30" }, "2026-01-01").eventDate).toMatch(/valid/);
  });
  it("email check", () => {
    expect(isValidEmail("a@b.de")).toBe(true);
    expect(isValidEmail("a b@c.de")).toBe(false);
    expect(isValidEmail("a@b")).toBe(false);
  });
  it("encodes for Netlify with form-name", () => {
    const body = new URLSearchParams(encodeForNetlify("booking", good));
    expect(body.get("form-name")).toBe("booking");
    expect(body.get("email")).toBe("alex@example.com");
    expect(body.get("consent")).toBe("yes");
  });
  it("success only on 2xx, never fakes success", async () => {
    const ok = vi.fn().mockResolvedValue({ ok: true });
    const bad = vi.fn().mockResolvedValue({ ok: false });
    const boom = vi.fn().mockRejectedValue(new Error("offline"));
    expect(await submitBooking("booking", good, "", ok as unknown as typeof fetch)).toEqual({ ok: true });
    expect(await submitBooking("booking", good, "", bad as unknown as typeof fetch)).toEqual({ ok: false, reason: "server" });
    expect(await submitBooking("booking", good, "", boom as unknown as typeof fetch)).toEqual({ ok: false, reason: "network" });
    expect(ok.mock.calls[0][1].method).toBe("POST");
  });
});

describe("urls", () => {
  it("only http(s)", () => {
    expect(safeExternalUrl("javascript:alert(1)")).toBeNull();
    expect(safeExternalUrl("data:text/html,x")).toBeNull();
    expect(safeExternalUrl("https://x.com/a")).toBe("https://x.com/a");
    expect(safeExternalUrl("")).toBeNull();
  });
  it("soundcloud embed only for soundcloud.com", () => {
    expect(soundcloudEmbedUrl("https://soundcloud.com/a/b")).toMatch(/^https:\/\/w\.soundcloud\.com\/player\/\?url=/);
    expect(soundcloudEmbedUrl("https://evil.com/soundcloud.com")).toBeNull();
    expect(soundcloudEmbedUrl("http://soundcloud.com/a")).toBeNull();
  });
  it("spotify embed", () => {
    expect(spotifyEmbedUrl("https://open.spotify.com/intl-de/album/4aawyAB9vmqN3uQ7FjRGTy?si=1")).toBe(
      "https://open.spotify.com/embed/album/4aawyAB9vmqN3uQ7FjRGTy?theme=0",
    );
    expect(spotifyEmbedUrl("https://open.spotify.com/hack/123")).toBeNull();
    expect(spotifyEmbedUrl("https://open.spotify.evil.com/album/1")).toBeNull();
  });
  it("instagram dm", () => {
    expect(instagramDmUrl("@brain.bugz")).toBe("https://ig.me/m/brain.bugz");
    expect(instagramDmUrl("")).toBeNull();
    expect(instagramDmUrl("a/b")).toBeNull();
  });
});

describe("plugins", () => {
  const list = [
    mk({ slug: "a", name: "Alpha", category: "synth", addedDate: "2026-01-02", status: "released" }),
    mk({ slug: "b", name: "Beta", category: "fx", addedDate: "2026-03-01", features: ["Bitcrush"] }),
    mk({ slug: "c", name: "Gamma", category: "utility", addedDate: "2026-02-01", status: "beta" }),
  ];
  it("filters by category and multi-word search", () => {
    expect(filterPlugins(list, { category: "fx" }).map((p) => p.slug)).toEqual(["b"]);
    expect(filterPlugins(list, { query: "bitCRUSH glitch" }).map((p) => p.slug)).toEqual(["b"]);
    expect(filterPlugins(list, { query: "zzz" })).toEqual([]);
  });
  it("sorts", () => {
    expect(sortPlugins(list, "newest").map((p) => p.slug)).toEqual(["b", "c", "a"]);
    expect(sortPlugins(list, "category").map((p) => p.slug)).toEqual(["b", "a", "c"]);
    expect(sortPlugins(list, "status").map((p) => p.slug)).toEqual(["a", "c", "b"]);
    expect(sortPlugins(list, "name").map((p) => p.slug)).toEqual(["a", "b", "c"]);
  });
  it("featured", () => {
    expect(pickFeaturedPlugin(list)?.slug).toBe("b");
    expect(pickFeaturedPlugin([...list, mk({ slug: "f", featured: true })])?.slug).toBe("f");
    expect(pickFeaturedPlugin([])).toBeUndefined();
  });
  it("download only with real URL and free access", () => {
    expect(downloadUrlOf(mk({ access: { type: "free" } }))).toBeNull();
    expect(downloadUrlOf(mk({ access: { type: "free", downloadUrl: "javascript:x" } }))).toBeNull();
    expect(downloadUrlOf(mk({ access: { type: "free", downloadUrl: "https://x.com/f.zip" } }))).toBe("https://x.com/f.zip");
    expect(downloadUrlOf(mk({ access: { type: "dm" } }))).toBeNull();
    expect(checkoutUrlOf(mk({ access: { type: "premium", price: 10 } }))).toBeNull();
  });
});

describe("gigs & dates", () => {
  it("splits upcoming/past, today counts as upcoming", () => {
    const g = (id: string, date: string) => ({ id, date, eventName: id, venue: "v", city: "c", country: "x" });
    const { upcoming, past } = splitGigs([g("p2", "2026-01-01"), g("t", "2026-10-28"), g("u", "2026-12-01"), g("p1", "2026-05-01"), g("bad", "soon")], "2026-10-28");
    expect(upcoming.map((x) => x.id)).toEqual(["t", "u"]);
    expect(past.map((x) => x.id)).toEqual(["p1", "p2"]);
  });
  it("parses dates without timezone shift", () => {
    expect(parseISODate("2026-03-01")?.getDate()).toBe(1);
    expect(parseISODate("2026-13-01")).toBeNull();
    expect(todayISO(new Date(2026, 0, 5))).toBe("2026-01-05");
    expect(formatDate("2026-11-14")).toBe("14 Nov 2026");
  });
});

describe("router", () => {
  const routes = [
    { name: "home", path: "/" },
    { name: "plugin", path: "/plugins/:slug" },
  ];
  it("matches", () => {
    expect(matchRoute(routes, "/")?.name).toBe("home");
    expect(matchRoute(routes, "/plugins/haas/")).toEqual({ name: "plugin", params: { slug: "haas" } });
    expect(matchRoute(routes, "/plugins")).toBeNull();
    expect(matchRoute(routes, "/plugins/%E0%A4%A")).toBeNull();
    expect(normalizePath("/news/")).toBe("/news");
  });
});

describe("radio mode", () => {
  const sc = "https://soundcloud.com/benidohitech/brainbugz-orange-sun-2h-2026/s-jBOKVoZwxJK?si=abc&utm_source=clipboard";
  it("stream wins, then loop, else none", () => {
    expect(resolveRadioMode({ streamUrl: "https://radio.example.com/live.mp3", loop: { soundcloudUrl: sc } })).toBe("stream");
    expect(resolveRadioMode({ streamUrl: "", loop: { soundcloudUrl: sc } })).toBe("loop");
    expect(resolveRadioMode({ streamUrl: "", loop: { soundcloudUrl: "" } })).toBe("none");
    expect(resolveRadioMode({ streamUrl: "javascript:x", loop: { soundcloudUrl: "https://evil.com/x" } })).toBe("none");
  });
  it("public link: permalink without tracking params", () => {
    const u = new URL(soundcloudEmbedUrl("https://soundcloud.com/a/b?si=1&utm_source=x")!);
    expect(u.searchParams.get("url")).toBe("https://soundcloud.com/a/b");
    expect(u.searchParams.get("secret_token")).toBeNull();
  });
  it("private link: track id + secret_token (like SoundCloud's own embed)", () => {
    const u = new URL(soundcloudEmbedUrl(sc, { autoPlay: true, trackId: "2378468972" })!);
    expect(u.searchParams.get("url")).toBe("https://api.soundcloud.com/tracks/2378468972");
    expect(u.searchParams.get("secret_token")).toBe("s-jBOKVoZwxJK");
    expect(u.searchParams.get("auto_play")).toBe("true");
  });
});

describe("radio metadata", () => {
  it("reads icecast, azuracast and plain json", () => {
    expect(parseNowPlaying({ icestats: { source: { title: "Artist - Track" } } })).toEqual({ artist: "Artist", title: "Track" });
    expect(parseNowPlaying({ now_playing: { song: { artist: "A", title: "T" } } })).toEqual({ artist: "A", title: "T" });
    expect(parseNowPlaying({ artist: "A", title: "T" })).toEqual({ artist: "A", title: "T" });
    expect(parseNowPlaying(null)).toBeNull();
    expect(parseNowPlaying({ foo: 1 })).toBeNull();
  });
});
