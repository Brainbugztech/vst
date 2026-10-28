// Zentrale Einstellungen der Website.
// Leere Werte ("") sind gewollt: die zugehörigen Buttons/Links werden dann einfach nicht angezeigt.
// Hier stehen NUR öffentliche Werte. Keine Passwörter, Tokens oder API-Schlüssel eintragen.

export const site = {
  name: "BrainBugz",
  tagline: "Underground psychedelic HiTech duo",
  description:
    "BrainBugz is an underground psychedelic HiTech duo: music, sound design and our own audio plugins.",
  /** Öffentliche Adresse, z. B. "https://brainbugz.netlify.app" (für Social-Previews). Später auf die eigene Domain ändern. */
  url: "https://brainbugz.netlify.app",
  locale: "en-GB",
};

/** Social-Media-Profile. Volle URLs, z. B. "https://www.instagram.com/brainbugz". */
export const social = {
  instagram: "",
  soundcloud: "",
  spotify: "",
  youtube: "",
  bandcamp: "",
  tiktok: "",
};

/**
 * Instagram-Name OHNE @ (z. B. "brainbugz"). Wird für "DM for access" gebraucht.
 * Ist er leer, zeigt die Website bei DM-Plugins nur einen Hinweis statt eines Buttons.
 */
export const instagramHandle = "";

export const contact = {
  /**
   * Optional: öffentliche Booking-Adresse, die als Alternative zum Formular angezeigt wird.
   * Die Empfänger-Adresse für das FORMULAR wird NICHT hier eingetragen, sondern im Netlify-Dashboard
   * (Forms → Form notifications). So steht sie nirgends im Code.
   */
  publicBookingEmail: "",
};

export const booking = {
  /**
   * "netlify" = Formular wird über Netlify Forms verschickt (funktioniert nur auf der Netlify-Seite).
   * "none"    = Formular ist ausgeschaltet, es wird nur der Hinweis/die E-Mail angezeigt.
   */
  provider: "netlify" as "netlify" | "none",
  formName: "booking",
};

export const radio = {
  name: "BrainBugz Radio",
  /**
   * Direkte Stream-URL (Icecast/Shoutcast/AzuraCast), z. B. "https://radio.example.com/listen/brainbugz/radio.mp3".
   * Leer = kein Radio. Die Website zeigt dann ehrlich "offline" an.
   */
  streamUrl: "",
  /**
   * Optional: JSON mit dem aktuellen Titel. Unterstützt Icecast (status-json.xsl),
   * AzuraCast (/api/nowplaying/...) und einfache {"artist":"...","title":"..."}-Antworten.
   */
  metadataUrl: "",
  /**
   * Dauerschleife, solange es keinen echten Stream gibt (streamUrl hat Vorrang).
   * SoundCloud-Link, auch ein privater Link mit "/s-..." am Ende geht. Leer = aus.
   * Achtung: Wer auf der Website ist, kann das Set hören.
   */
  loop: {
    soundcloudUrl: "https://soundcloud.com/benidohitech/brainbugz-orange-sun-2h-2026/s-jBOKVoZwxJK",
    /**
     * Nur für PRIVATE SoundCloud-Links nötig: die Track-Nummer.
     * Zu finden auf SoundCloud unter Teilen → Einbetten (Embed): im Code steht "api.soundcloud.com/tracks/ZAHL".
     */
    trackId: "2378468972",
    title: "Orange Sun",
    artist: "BrainBugz",
    /** Kurzer Hinweis, z. B. Länge des Sets */
    note: "2h set on loop",
  },
  /**
   * Eingangs-Screen beim ersten Besuch: "Enter with sound" startet das Radio (Klick = Zustimmung + Browser erlaubt Ton).
   * volume: Startlautstärke 0–1 (0.3 = 30 %).
   */
  entryGate: {
    enabled: true,
    volume: 0.3,
  },
};

export const merch = {
  /** Link zum externen Shop. Leer = Merch wird als "Coming soon" gezeigt. */
  shopUrl: "",
};

/**
 * Angaben fürs Impressum (in Deutschland Pflicht, sobald die Seite öffentlich ist).
 * Bitte vollständig eintragen, bevor ihr die Seite teilt.
 */
export const legal = {
  responsibleName: "",
  street: "",
  postalCodeCity: "",
  country: "Deutschland",
  email: "",
  phone: "",
};
