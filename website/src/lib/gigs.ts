import type { Gig, ISODate } from "../types";
import { parseISODate } from "./format";

/** Trennt Gigs in kommende (heute zählt dazu, aufsteigend) und vergangene (absteigend). Ungültige Daten fliegen raus. */
export function splitGigs(list: Gig[], today: ISODate) {
  const valid = list.filter((g) => parseISODate(g.date));
  const upcoming = valid.filter((g) => g.date >= today).sort((a, b) => a.date.localeCompare(b.date));
  const past = valid.filter((g) => g.date < today).sort((a, b) => b.date.localeCompare(a.date));
  return { upcoming, past };
}
