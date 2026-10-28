import type { Gig } from "../types";

// Gigs. Kommende und vergangene Gigs werden automatisch anhand des Datums getrennt.
// Nur echte, bestätigte Gigs eintragen.
// Beispiel:
//
// {
//   id: "2026-12-05-club-name",
//   date: "2026-12-05",
//   time: "02:00",
//   eventName: "Event-Name",
//   venue: "Club-Name",
//   city: "Berlin",
//   country: "Germany",
//   flyer: { src: "/media/gigs/2026-12-05.webp", alt: "Flyer Event-Name" },
//   ticketUrl: "https://...",
// },

export const gigs: Gig[] = [];
