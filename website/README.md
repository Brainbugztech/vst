# BrainBugz Website

Die Website von BrainBugz: Musik, Radio, Gigs, Plugins, Merch, News, About und Booking.
Gebaut mit **React + TypeScript + Vite**, gehostet kostenlos bei **Netlify**.

- Alle Inhalte stehen in Datendateien unter `src/content/`, alle Einstellungen in `src/config/site.ts`.
- Was nicht eingetragen ist, wird auch nicht angezeigt (kein Fake-Inhalt): leere Bereiche zeigen einen sauberen Hinweis.
- **Plugins sind standardmäßig unveröffentlicht** und landen dann gar nicht erst auf der Website.

---

## 1. Lokal starten

Einmalig [Node.js](https://nodejs.org) installieren (Version 22.12 oder neuer, einfach die aktuelle „LTS“-Version nehmen).

```bash
cd website
npm install          # einmalig, lädt die Werkzeuge
npm run dev          # startet die Seite auf http://localhost:5173
```

| Befehl | Was es macht |
| --- | --- |
| `npm run dev` | Seite lokal starten, Änderungen erscheinen sofort |
| `npm run dev:drafts` | wie `dev`, aber **mit allen unveröffentlichten Plugins** (zum Prüfen vor dem Veröffentlichen) |
| `npm run check` | Typen prüfen + Tests + Build. Läuft auch auf Netlify; schlägt das fehl, geht nichts live |
| `npm run build` / `npm run preview` | fertige Seite bauen (`dist/`) und lokal anschauen |

Das Booking-Formular funktioniert **nur auf Netlify**. Lokal zeigt es beim Absenden absichtlich eine Fehlermeldung
(es gibt dort keinen Empfänger), nie einen falschen „Gesendet“-Hinweis.

---

## 2. Kostenlos online stellen (Netlify)

Kosten: **0 €**. Netlify Free reicht für die Seite (Monatslimit für Traffic, Build-Minuten und Formular-Anfragen;
Netlify zeigt den Verbrauch im Dashboard an, es wird nichts automatisch berechnet).
Kosten entstehen nur, wenn ihr selbst einen bezahlten Tarif oder eine eigene Domain kauft.

1. Auf [app.netlify.com](https://app.netlify.com) mit GitHub anmelden.
2. **Add new project → Import an existing project → GitHub**.
   Fehlt `Brainbugztech/vst` in der Liste: „Configure Netlify on GitHub“ klicken und das Repo freigeben.
3. Repo `Brainbugztech/vst` wählen, Branch **`main`**, **Base directory: `website`**.
   Build command und Publish directory kommen automatisch aus `website/netlify.toml` (`npm run check`, `dist`).
4. **Deploy** klicken. Nach 1–2 Minuten ist die Seite unter `irgendwas.netlify.app` online.
5. Namen ändern: **Project configuration → Change project name** → `brainbugz` → `brainbugz.netlify.app`.
   Ist der Name schon vergeben, einen anderen wählen und `site.url` in `src/config/site.ts` + `og:image` in `index.html` anpassen.
6. **Booking-Formular einschalten:**
   - **Forms → Enable form detection**, danach unter **Deploys → Trigger deploy → Deploy site** einmal neu bauen.
   - **Forms → Form notifications → Add notification → Email notification**, Formular `booking`,
     eure Booking-E-Mail eintragen. Die Adresse steht damit nur bei Netlify, nicht im Code.
   - Test: auf der Live-Seite eine Anfrage schicken → sie erscheint unter **Forms → booking** und kommt per Mail.

Ab dann: Jede Änderung im Ordner `website/` auf `main` geht automatisch live. Änderungen nur an Plugins oder der HQ-App
lösen keinen Website-Build aus (`ignore` in `netlify.toml`), und Website-Änderungen lösen keinen Windows-Plugin-Build aus.

### Später: eigene Domain
Domain kaufen (z. B. bei INWX, ca. 5–15 €/Jahr), dann in Netlify **Domain management → Add a domain** und den
Anweisungen folgen (DNS-Einträge beim Domain-Anbieter). HTTPS macht Netlify automatisch und kostenlos.
Danach `site.url` in `src/config/site.ts` und `og:image` in `index.html` auf die neue Domain ändern.

---

## 3. Plugins veröffentlichen

Plugins stehen in `src/content/plugins.json`, Cover-Bilder in `src/content/plugin-media/`.
Nur Plugins mit `"published": true` kommen auf die Website. Unveröffentlichte Plugins sind nicht im Code der Seite,
nicht per URL erreichbar und ihre Bilder werden nicht hochgeladen.

**Der Button:** GitHub → Repo → **Actions → „Plugin veröffentlichen (Website)“ → Run workflow** →
Plugin und „veröffentlichen“ oder „zurückziehen“ wählen → **Run workflow**.
Der Button testet die Seite, speichert die Änderung auf `main`, und Netlify baut automatisch neu.
Nur Leute mit Schreibrechten am Repo (Benett, Dexter) können ihn benutzen.

**Vorher prüfen:** `npm run dev:drafts`. Unveröffentlichte Plugins tragen dort den Hinweis „Draft · not public“.

**Vor dem Veröffentlichen ausfüllen** (in `plugins.json`):

| Feld | Bedeutung |
| --- | --- |
| `status` | `released`, `beta`, `in-development`, `concept` |
| `access` | `{"type": "free", "downloadUrl": "...", "fileName": "Name.zip"}`, `{"type": "dm"}`, `{"type": "coming-soon"}` oder `{"type": "premium", "price": 19, "currency": "EUR", "checkoutUrl": "...", "provider": "..."}` |
| `downloadUrl` | **öffentlich erreichbare** Datei. Das private Repo geht nicht. Entweder die ZIP nach `public/downloads/` legen (`"/downloads/Name.zip"`) oder ein öffentlicher Link (z. B. GitHub Release eines öffentlichen Repos). Ohne URL gibt es keinen Download-Button. |
| `fileSize`, `testedDaws`, `changelog`, `releaseDate` | nur echte Angaben |

**DM-Zugang:** `instagramHandle` in `src/config/site.ts` eintragen (ohne @). Der Button öffnet `ig.me/m/<name>`.
Instagram kann keine vorausgefüllten Nachrichten, deshalb gibt es einen „Copy message“-Button mit dem Plugin-Namen.
Der Zugang wird von euch von Hand freigegeben.

**Premium:** Bezahlt wird nie auf der Website selbst. Ein Kauf-Button erscheint nur mit `checkoutUrl` eines echten
Anbieters (z. B. Lemon Squeezy, Gumroad, Paddle; die nehmen Gebühren pro Verkauf). Lizenzen und geschützte Downloads
gehören auf einen Server (z. B. Netlify Function + Webhook des Anbieters). Die Datentypen dafür stehen schon in
`src/types.ts` (`Order`, `License`, `DownloadEntitlement`).

**Neues Plugin anlegen:** Eintrag in `plugins.json` kopieren, `slug` (nur a–z, 0–9, -) und Texte anpassen,
`"published": false` lassen, Cover nach `src/content/plugin-media/` legen. Für den Button den `slug` zusätzlich in
`.github/workflows/website-plugin-publish.yml` unter `options` eintragen. Der Build prüft die Datei und bricht mit einer
verständlichen Meldung ab, wenn etwas fehlt.

---

## 4. Inhalte pflegen

| Was | Datei | Hinweis |
| --- | --- | --- |
| Releases | `src/content/releases.ts` | SoundCloud-/Spotify-URL eintragen, der offizielle Player wird automatisch eingebaut (lädt erst nach Klick, DSGVO) |
| Gigs | `src/content/gigs.ts` | kommende/vergangene werden automatisch nach Datum getrennt |
| News | `src/content/news.ts` | erscheint auf Startseite + `/news/<slug>` |
| Merch | `src/content/merch.ts` | ohne `shopUrl` kein Kauf-Button; Shop-Link allgemein: `merch.shopUrl` in `site.ts` |
| About | `src/content/about.ts` | allgemeiner Platzhaltertext, bis eure Bio da ist |
| Social Links | `src/config/site.ts` → `social` | leere Einträge werden nicht angezeigt |
| Radio | `src/config/site.ts` → `radio` | ohne `streamUrl` zeigt die Seite ehrlich „offline“; mit URL spielt der Mini-Player den Stream, `metadataUrl` (Icecast/AzuraCast) zeigt den aktuellen Titel |
| Impressum/Datenschutz | `src/config/site.ts` → `legal` | **Pflicht in Deutschland, bevor die Seite geteilt wird.** Fehlende Angaben stehen pink als „[... eintragen]“ auf der Seite |

Bilder für Releases, Gigs, News, Merch nach `public/media/...` legen (am besten `.webp`, max. ~1600 px breit) und mit
`/media/...` verlinken. Grafiken aus `Assets/` neu erzeugen: `python3 website/tools/prepare-images.py` (aus dem Repo-Root).

---

## 5. Sicherheit

- Keine Schlüssel, Passwörter oder Tokens im Code. Die Booking-Empfänger-Adresse steht nur bei Netlify.
- Strenge Security-Header (Content-Security-Policy, kein Einbetten in fremde Seiten, keine Kamera/Mikro-Rechte usw.) in `netlify.toml`.
  Neue externe Player (z. B. YouTube) müssen dort unter `frame-src` ergänzt werden.
- Alle externen Links aus den Datendateien werden geprüft (nur `http(s)`), öffnen in neuem Tab mit `noopener noreferrer`.
- Booking: Pflichtfeld- und E-Mail-Prüfung, Honeypot gegen Spam-Bots, Netlify filtert zusätzlich Spam.
  „Gesendet“ erscheint nur, wenn Netlify die Anfrage wirklich angenommen hat.
- Keine Cookies, kein Tracking, Schriften liegen auf dem eigenen Server (nicht Google).

## Ordner

```
website/
├─ index.html              Grundgerüst, Social-Preview, verstecktes Netlify-Formular
├─ netlify.toml            Build, Weiterleitungen, Security-Header
├─ vite.config.ts          Build + Plugin-Filter (nur veröffentlichte Plugins)
├─ public/                 Dateien, die 1:1 online gehen (Logos, Favicon, robots.txt)
├─ src/config/site.ts      alle Einstellungen
├─ src/content/            alle Inhalte (+ plugins.json, plugin-media/)
├─ src/pages/              die einzelnen Seiten
├─ src/components/         Bausteine (Header, Mini-Player, Karten ...)
├─ src/lib/                Logik (Router, Booking, Filter, Radio ...) + Tests
├─ src/styles/global.css   Design (Farben als Variablen ganz oben)
└─ tools/                  Bild-Skript, Veröffentlichen-Skript
```
