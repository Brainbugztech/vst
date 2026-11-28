# Brain Bugz: Regeln für Claude

Dieses Repo gehört Benett und Dexter. Wir bauen VST3-Plugins (JUCE, Windows, Build über GitHub Actions)
und die Windows-App `hq-app/` (Projektboard, Cloud-Ordner „Brain Bugz Cloud“ über Google Drive, Discord-Overlay).

## So arbeitest du hier
1. **Nie direkt auf `main`.** Lege für jeden Auftrag einen eigenen Branch an (`claude/<projekt>-<kurzbeschreibung>`)
   und öffne daraus einen Pull Request. Benett oder Dexter prüfen und klicken selbst auf Merge.
2. **Erst das Board lesen.** Das Projektboard "Brain Bugz HQ" (Link steht in der Übergabe, die dir Benett oder Dexter geben)
   enthält Projekte, Aufgaben, die Pinnwand und die Aufträge (Collection `jobs`).
   Ist bei einem Projekt `activeWho` gesetzt, arbeitet gerade jemand daran: frag erst nach.
3. **Auftrag melden.** Setz den Auftrag auf `läuft`, trag dich als `activeWho` ein. Wenn du fertig bist:
   Auftrag auf `erledigt`, `result` = ein Satz, `pr` = Link zum Pull Request, `activeWho` leeren,
   Aufgaben abhaken und einen Post auf die Pinnwand schreiben (was ist fertig, was kommt als Nächstes).
4. **Namen:** Wir benutzen Vornamen (Benett, Dexter), keine Künstlernamen.

## Code
- Plugins liegen unter `Plugins/<Name>/`, gemeinsame Grafiken unter `Assets/`.
- Schreib Commit-Nachrichten und Kommentare auf Deutsch, kurz und konkret.
- Prüfen kannst du Plugins nicht lokal bauen: der Build läuft in GitHub Actions (`.github/workflows/build.yml`).
  Schau nach dem Push, ob der Lauf grün ist, bevor du den Pull Request als fertig meldest.
- Keine Tokens, Passwörter oder Schlüssel ins Repo. Der Discord-Bot-Token bleibt lokal auf dem PC.

## Lizenz + Updates in jedem Plugin (Pflicht für neue Plugins)
Jedes neue Plugin bekommt das Brain Bugz Center (`Shared/bb_center`, Plugin-ID kleingeschrieben, z. B. `snarehead`):
- **CMake:** `if(BB_CENTER_ENABLED)` → `target_link_libraries(<Ziel> PRIVATE bb_center juce::juce_cryptography)` und
  `BUGZ_LICENSE=1`, sonst `BUGZ_LICENSE=0` (Vorlage: `Plugins/SnareHead/CMakeLists.txt`).
- **Processor:** `bbcenter::Kit bb { "<id>" };` · `bb.prepare (sampleRate);` in prepareToPlay ·
  `bb.process (buffer);` ganz am Ende von processBlock (alles in `#if BUGZ_LICENSE`).
- **Editor:** unsichtbare `bbcenter::CenterBar` als Logik + eigene Anzeige im Plugin-Look:
  Versions-Pille mit Status unten links (Klick = Menü: Update, Nach Updates suchen, Lizenz, VST-Ordner) und ein LIZENZ-Button.
  Vorlage: `CenterCorner` und `showCenterMenu()` in `Plugins/SnareHead/Source/PluginEditor.cpp`.
- **Version** in `juce_add_plugin(... VERSION x.y.z)` pflegen, das Modul liest sie automatisch.
- Das Plugin im Center (oder in Supabase-Tabelle `bb_plugins`) mit derselben ID anlegen.
- `bb_center_keys.h` nie committen: lokal liegt sie in `G:\Claude Ordner\shared\bb_center`, auf GitHub kommt sie aus dem Secret `BB_CENTER_KEYS`.
