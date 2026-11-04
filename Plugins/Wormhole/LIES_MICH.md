# Brainbugz Wormhole – Time & Pitch

Eigene Alternative zu Pitch 'n Time als VST3, gebaut für Ableton (kein ARA nötig).

## Zwei Modi
- **Live:** Pitch (±24 Halbtöne, Fine ±100 Cent) und Formant laufen in Echtzeit auf der Spur.
  Latenz ca. 93 ms bei 44,1 kHz, Ableton gleicht sie automatisch aus.
- **Render:** Wie Pitch 'n Time offline in voller Qualität.
  1. Audio ins Plugin ziehen (aus dem Ableton-Browser oder Explorer), über **LADEN** öffnen
     oder mit **AUFNEHMEN** direkt von der Spur mitschneiden (Play drücken, dann STOP).
  2. Tempo einstellen: Regler in %, oder Ziel-BPM eintippen. **PROJEKT** übernimmt das Ableton-Tempo.
     Die Quell-BPM werden automatisch erkannt und lassen sich überschreiben.
  3. **HÖREN** spielt das Ergebnis sofort im Plugin ab, ohne zu rendern. Klick in die Wellenform startet an der Stelle.
     Regler wirken beim Vorhören direkt.
  4. **RENDER** drücken, dann **IN ABLETON ZIEHEN** (oder die Wellenform anfassen) und in eine Spur ziehen.
  Die fertigen Dateien liegen in `Dokumente/Brain Bugz/Wormhole` (32-Bit-Float-WAV, kann nicht übersteuern).
  Ein Render, der nie nach Ableton gezogen wurde, wird beim nächsten Render bzw. beim Schließen gelöscht.
  Gezogene Dateien bleiben. Tipp: In Ableton „Alles sammeln und speichern“ kopiert sie ins Projekt.

**Tempo 0,1 % bis 400 %.** Unter 25 % arbeitet der **Extrem-Modus** (bis 1000-fach länger).
Der Regler **TEXTUR** (erscheint nur dort) stellt die Fensterlänge ein: kurz = körniger, lang = weicher, ganz links = automatisch.
Render-Ergebnisse sind auf 2 Stunden begrenzt; Vorhören geht bei jeder Länge.

**Formant Lock** hält den Stimmcharakter beim Pitchen. **Attack Lock** hält Drums und Anschläge knackig
(Attacks laufen in Originalgeschwindigkeit, kein Vorecho).

## Engine (`Engine/WormholeEngine.h`)
Liefert das Ergebnis stückweise (`StretchStream`), dadurch Vorhören und stundenlange Renders ohne riesigen Speicher.
Normalbereich – eigener Phase-Vocoder, ohne Lizenzkosten:
- zwei Auflösungen (4096 unter 300 Hz, 2048 darüber), Phasen-Kopplung um Spektral-Spitzen
- gemeinsame Phase für L/R, damit das Stereobild stehen bleibt
- Attack-Erkennung mit Phasen-Reset, Zeit-Verriegelung und Vorecho-Maskierung
- Formant-Erhalt über cepstrale Hüllkurve
- FFT: PFFFT (BSD-artige Lizenz, `Engine/pffft/LICENSE.txt`)

Extrembereich – Spektral-Textur nach dem Prinzip von Paulstretch, aber hybrid und stereo-kohärent:
- klare Teiltöne laufen phasenrichtig weiter (keine Schwebung), Rauschen/Textur bekommt Zufallsphasen
- alle Kanäle teilen die Zufallsphase, das Stereobild bleibt stehen
- Formant-Erhalt und Pitch auch hier
Messung bei 1 % und 0,1 %: Pegel exakt, Ton-Reinheit 125 dB, Pegelschwankung 0,09 dB
(klassisches Zufallsphasen-Verfahren: ca. 2,5 dB Schwebung), Rauschen so gleichmäßig wie das Original.

Messwerte gegen Signalsmith Stretch (Testsignale, siehe `tools/`):
Tonhöhe auf 0,1 Cent genau, Reinheit 65–75 dB (Vergleich 51–57 dB),
Vorecho an Drums 27–34 dB unter dem Schlag (Vergleich 20–25 dB),
Formant-Abweichung halb so groß.

## Tests (`tools/`)
- `stretch_cli.cpp`: `stretch_cli in.wav out.wav <Längenfaktor> <Halbtöne> [formant 0/1] [live 0/1]`
- `extreme_cli.cpp`: `extreme_cli in.wav out.wav <Längenfaktor> <Halbtöne> <formant> <max. Sekunden> [Start-Sekunde]`
- `gui_film.cpp`: filmt die Oberfläche (Bilder pro Frame; `WORMHOLE_FIXED_DT=1` für gleichmäßige Animation)
- `gen_signals.py` erzeugt Testsignale, `analyze.py` misst Tonhöhe, Reinheit, Vorecho, Formanten
  (Pfade zu `cli` und Referenz oben in der Datei anpassen)
- `gui_snapshot.cpp`: rendert die echte Oberfläche als PNG (Konsolen-App)

Schrift: Inter (OFL, `Assets/Inter-LICENSE.txt`). Logo: Clean-Vektor aus dem Brain-Bugz-3D-Kit.
