import { legal } from "../config/site";
import { PageHeader } from "../components/ui";
import { usePageMeta } from "../lib/meta";

// Hinweis: Die Texte sind eine Vorlage und keine Rechtsberatung.
// Bitte die Angaben in src/config/site.ts (legal) vollständig eintragen und den Text vor dem Livegang prüfen,
// z. B. mit einem kostenlosen Generator (e-recht24.de, datenschutz-generator.de).

function Missing({ what }: { what: string }) {
  return <span className="missing">[{what} eintragen]</span>;
}

function v(value: string, what: string) {
  return value ? value : <Missing what={what} />;
}

export function Impressum() {
  usePageMeta({ title: "Impressum", noindex: false });
  return (
    <div className="container page legal" lang="de">
      <PageHeader code="// LEGAL" title="Impressum" />
      <h2>Angaben gemäß § 5 DDG</h2>
      <p>
        {v(legal.responsibleName, "Vor- und Nachname")}
        <br />
        {v(legal.street, "Straße und Hausnummer")}
        <br />
        {v(legal.postalCodeCity, "PLZ und Ort")}
        <br />
        {legal.country}
      </p>
      <h2>Kontakt</h2>
      <p>
        E-Mail: {legal.email ? <a href={`mailto:${legal.email}`}>{legal.email}</a> : <Missing what="E-Mail-Adresse" />}
        {legal.phone && (
          <>
            <br />
            Telefon: {legal.phone}
          </>
        )}
      </p>
      <h2>Verantwortlich für den Inhalt nach § 18 Abs. 2 MStV</h2>
      <p>{v(legal.responsibleName, "Vor- und Nachname")}, Anschrift wie oben.</p>
      <h2>Haftung für Links</h2>
      <p>
        Diese Website enthält Links zu externen Websites Dritter, auf deren Inhalte wir keinen Einfluss haben. Für die Inhalte der
        verlinkten Seiten ist stets der jeweilige Anbieter oder Betreiber verantwortlich.
      </p>
    </div>
  );
}

export function Datenschutz() {
  usePageMeta({ title: "Datenschutz / Privacy" });
  return (
    <div className="container page legal" lang="de">
      <PageHeader code="// LEGAL" title="Datenschutz" />
      <p className="muted" lang="en">
        Privacy policy (German). In short: no tracking, no analytics, no ads. External players only load after you click.
      </p>

      <h2>1. Verantwortlicher</h2>
      <p>
        {v(legal.responsibleName, "Vor- und Nachname")}, {v(legal.street, "Straße")}, {v(legal.postalCodeCity, "PLZ und Ort")},{" "}
        {legal.country}. E-Mail: {legal.email ? <a href={`mailto:${legal.email}`}>{legal.email}</a> : <Missing what="E-Mail-Adresse" />}
      </p>

      <h2>2. Hosting</h2>
      <p>
        Diese Website wird bei Netlify, Inc. (512 2nd Street, Suite 200, San Francisco, CA 94107, USA) gehostet. Beim Aufruf der
        Seite verarbeitet Netlify technisch notwendige Daten (z. B. IP-Adresse, Datum und Uhrzeit, aufgerufene Seite, Browser) in
        Server-Logfiles, um die Website auszuliefern und abzusichern. Rechtsgrundlage ist Art. 6 Abs. 1 lit. f DSGVO (berechtigtes
        Interesse an einem sicheren und stabilen Betrieb). Eine Übermittlung in die USA erfolgt auf Grundlage der
        Standardvertragsklauseln bzw. des EU-US Data Privacy Framework.
      </p>

      <h2>3. Booking-Formular</h2>
      <p>
        Wenn du uns über das Booking-Formular eine Anfrage schickst, verarbeiten wir deine Angaben (Name, E-Mail-Adresse,
        Veranstalter, Event-Daten, Budget, Nachricht), um die Anfrage zu bearbeiten. Die Daten werden über den Formulardienst von
        Netlify (Netlify Forms) übermittelt und gespeichert und uns per E-Mail weitergeleitet. Rechtsgrundlage ist deine Einwilligung
        (Art. 6 Abs. 1 lit. a DSGVO) sowie die Anbahnung eines Vertrags (Art. 6 Abs. 1 lit. b DSGVO). Wir löschen die Anfragen,
        sobald sie erledigt sind und keine gesetzlichen Aufbewahrungspflichten bestehen. Deine Einwilligung kannst du jederzeit per
        E-Mail widerrufen.
      </p>

      <h2>4. SoundCloud- und Spotify-Player</h2>
      <p>
        Auf der Musik-Seite können offizielle Player von SoundCloud (SoundCloud Global Limited &amp; Co. KG, Berlin) und Spotify
        (Spotify AB, Stockholm) eingebunden sein. Sie werden erst geladen, wenn du auf den Button „Load … player“ klickst. Erst dann wird eine
        Verbindung zum jeweiligen Anbieter aufgebaut, der dabei u. a. deine IP-Adresse erhält und Cookies setzen kann.
        Rechtsgrundlage ist deine Einwilligung (Art. 6 Abs. 1 lit. a DSGVO). Wenn du „Always load … players“ wählst, speichern wir diese
        Entscheidung nur lokal in deinem Browser (localStorage). Du kannst sie löschen, indem du die Website-Daten in deinem Browser
        entfernst.
      </p>

      <h2>5. Radio-Stream</h2>
      <p>
        Wenn ein Radio-Stream angeboten wird und du auf „Play“ klickst, verbindet sich dein Browser mit dem Stream-Server. Dieser
        erhält dabei technisch bedingt deine IP-Adresse. Die gewählte Lautstärke wird nur lokal in deinem Browser gespeichert.
      </p>

      <h2>6. Links zu sozialen Netzwerken</h2>
      <p>
        Links zu Instagram, SoundCloud, Spotify und anderen Diensten sind normale Links. Es werden keine Daten an diese Dienste
        übertragen, bevor du einen Link anklickst.
      </p>

      <h2>7. Keine Cookies, kein Tracking</h2>
      <p>
        Wir verwenden keine Analyse- oder Werbe-Tools und setzen keine eigenen Cookies. Schriften werden von unserem eigenen Server
        geladen, nicht von Google.
      </p>

      <h2>8. Deine Rechte</h2>
      <p>
        Du hast das Recht auf Auskunft, Berichtigung, Löschung, Einschränkung der Verarbeitung, Datenübertragbarkeit und Widerspruch
        sowie das Recht, eine erteilte Einwilligung zu widerrufen. Außerdem kannst du dich bei einer Datenschutz-Aufsichtsbehörde
        beschweren.
      </p>
    </div>
  );
}
