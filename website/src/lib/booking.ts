import { parseISODate } from "./format";

export const eventTypes = [
  "Club night",
  "Festival",
  "Open air",
  "Private event",
  "Livestream",
  "Other",
] as const;

export interface BookingValues {
  name: string;
  email: string;
  organization: string;
  eventName: string;
  eventDate: string;
  location: string;
  eventType: string;
  budget: string;
  message: string;
  consent: boolean;
}

export type BookingField = keyof BookingValues;
export type BookingErrors = Partial<Record<BookingField, string>>;

export const emptyBooking: BookingValues = {
  name: "",
  email: "",
  organization: "",
  eventName: "",
  eventDate: "",
  location: "",
  eventType: "",
  budget: "",
  message: "",
  consent: false,
};

export const MAX = { short: 120, message: 4000 };

// Pragmatische Prüfung: ein @, keine Leerzeichen, Domain mit Punkt.
const EMAIL_RE = /^[^\s@]+@[^\s@]+\.[^\s@]{2,}$/;

export function isValidEmail(v: string) {
  return v.length <= 254 && EMAIL_RE.test(v.trim());
}

/** Prüft das Formular. today = "JJJJ-MM-TT". */
export function validateBooking(v: BookingValues, today: string): BookingErrors {
  const e: BookingErrors = {};
  const req = (field: BookingField, label: string) => {
    if (!String(v[field]).trim()) e[field] = `Please enter ${label}.`;
  };
  req("name", "your name");
  req("email", "your email address");
  req("eventDate", "the event date");
  req("location", "the venue and location");
  req("eventType", "the event type");
  req("message", "a message");

  if (!e.email && !isValidEmail(v.email)) e.email = "Please enter a valid email address, e.g. name@example.com.";
  if (!e.eventDate) {
    if (!parseISODate(v.eventDate)) e.eventDate = "Please enter a valid date.";
    else if (v.eventDate < today) e.eventDate = "The event date can't be in the past.";
  }
  if (!e.eventType && v.eventType && !(eventTypes as readonly string[]).includes(v.eventType)) {
    e.eventType = "Please choose an event type.";
  }
  for (const f of ["name", "email", "organization", "eventName", "location", "budget"] as BookingField[]) {
    if (!e[f] && String(v[f]).length > MAX.short) e[f] = `Please keep this under ${MAX.short} characters.`;
  }
  if (!e.message && v.message.trim().length < 10) e.message = "Please tell us a bit more (at least 10 characters).";
  if (!e.message && v.message.length > MAX.message) e.message = `Please keep the message under ${MAX.message} characters.`;
  if (!v.consent) e.consent = "Please agree to the privacy policy so we can process your request.";
  return e;
}

/** Baut den Body für Netlify Forms (application/x-www-form-urlencoded). */
export function encodeForNetlify(formName: string, v: BookingValues, botField = "") {
  const data: Record<string, string> = {
    "form-name": formName,
    "bot-field": botField,
    name: v.name.trim(),
    email: v.email.trim(),
    organization: v.organization.trim(),
    eventName: v.eventName.trim(),
    eventDate: v.eventDate,
    location: v.location.trim(),
    eventType: v.eventType,
    budget: v.budget.trim(),
    message: v.message.trim(),
    consent: v.consent ? "yes" : "no",
  };
  return new URLSearchParams(data).toString();
}

export type SubmitResult = { ok: true } | { ok: false; reason: "network" | "server" | "disabled" };

/**
 * Schickt das Formular an Netlify Forms. Erfolg NUR bei HTTP 2xx.
 * Läuft die Seite nicht auf Netlify (z. B. lokal), kommt ein Fehler zurück, kein falscher Erfolg.
 */
export async function submitBooking(
  formName: string,
  v: BookingValues,
  botField: string,
  fetchImpl: typeof fetch = fetch,
): Promise<SubmitResult> {
  try {
    const res = await fetchImpl("/", {
      method: "POST",
      headers: { "Content-Type": "application/x-www-form-urlencoded", Accept: "text/html" },
      body: encodeForNetlify(formName, v, botField),
    });
    return res.ok ? { ok: true } : { ok: false, reason: "server" };
  } catch {
    return { ok: false, reason: "network" };
  }
}
