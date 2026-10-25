# Bugz HQ (Windows-App)

Ein Fenster mit zwei Ansichten (oben umschalten oder Strg+1 / Strg+2):
- **Board**: das Brain-Bugz-Projektboard (plus Discord-Overlay, gerade ausgeblendet)
- **Cloud**: euer gemeinsamer Ordner „Brain Bugz Cloud“ aus Google Drive

## Exe holen
GitHub → Actions → "Build Bugz HQ" → neuester Lauf → Artifact **BugzHQ** herunterladen,
entpacken, `BugzHQ-x.y.z.exe` auf den Desktop legen. Keine Installation nötig.

## Updates (automatisch über die Cloud)
Neue Versionen liegen als `BugzHQ-x.y.z.exe` im Cloud-Ordner **_HQ-App**. Google Drive verteilt sie an beide PCs.
Sobald eine neuere Version da ist, zeigt die App oben „Update bereit“ → **Jetzt neu starten**:
die App ersetzt ihre eigene Exe und startet neu. Einstellungen bleiben erhalten.

Neue Version veröffentlichen: die Exe aus GitHub Actions in `Brain Bugz Cloud/_HQ-App` legen
(per Drag & Drop in der Cloud-Ansicht oder im Explorer). Claude kann das auch direkt über die Verbindung zum PC machen.
In `_HQ-App` dürfen nur Exes aus GitHub Actions liegen, denn die App startet sie.

## Beim ersten Start
1. **Einstellungen** → Link zum Board eintragen (`https://claude.ai/artifact/…`), speichern.
   Im Board einmal bei claude.ai anmelden, das bleibt gespeichert.
2. **Discord-Bot-Token** eintragen. Der Token wird mit Windows verschlüsselt nur auf diesem PC gespeichert.
3. Server auswählen, speichern.

## Discord-Bot einrichten
1. https://discord.com/developers/applications → New Application → Bot.
2. Unter *Privileged Gateway Intents* alle drei einschalten: Presence, Server Members, Message Content.
3. OAuth2 → URL Generator → Scope `bot`, Rechte *View Channels*, *Send Messages*, *Read Message History* → Link öffnen und den Bot auf euren Server einladen.
4. Token unter *Bot → Reset Token* kopieren. Den Token nie in Chats, Repos oder Screenshots zeigen.

Jeder von euch (Benett, Dexter) trägt den Token bei sich lokal ein. Ihr könnt denselben Bot benutzen.

## Cloud einrichten
Die Cloud-Ansicht zeigt den lokalen Ordner, den **Google Drive für Desktop** synct.
Die App lädt nichts selbst hoch, das macht Google Drive im Hintergrund.

1. Google Drive für Desktop installieren und anmelden: https://www.google.com/drive/download/
2. **Benett** (einmalig): `cloud-setup/CLOUD_EINRICHTEN.bat` doppelklicken. Legt „Brain Bugz Cloud“
   mit allen Unterordnern in *Meine Ablage* an. Dann auf drive.google.com den Ordner mit Dexter teilen (Bearbeiter).
3. **Dexter**: auf drive.google.com unter *Für mich freigegeben* den Ordner per Rechtsklick
   → *Verknüpfung zu „Meine Ablage“ hinzufügen*.
4. Die App findet den Ordner von selbst. Sonst in der Cloud-Ansicht auf *Ordner selbst wählen*.

Tipp: Den Ordner in Google Drive für Desktop auf „Offline verfügbar“ stellen, damit Samples sofort laden.

### Was die Cloud-Ansicht kann
- Übersicht mit Kacheln (Projekte, Presets, Samples, Plugins, Tracks, Inbox) inkl. Anzahl, Größe und „NEU“ für Dateien der letzten 24 h
- Ordnerbaum links, Inhalt rechts, Doppelklick öffnet Datei/Ordner
- **Drag & Drop rein**: Dateien/Ordner aus dem Explorer auf den Inhalt, eine Kachel oder einen Ordner links ziehen
- **Rausziehen**: Dateien direkt in Ableton oder den Explorer ziehen
- **Verschieben**: innerhalb der App von einem Ordner in einen anderen ziehen
- **Vorhören**: Play-Knopf bei Audio-Dateien, Leertaste = Play/Pause
- Aktualisiert sich automatisch, wenn der andere etwas hochlädt
- Es wird nie etwas überschrieben: gleiche Namen bekommen „(2)“ dran. Löschen geht nur im Explorer.
