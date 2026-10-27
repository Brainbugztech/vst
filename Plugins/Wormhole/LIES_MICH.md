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
  3. **RENDER** drücken, dann **IN ABLETON ZIEHEN** (oder die Wellenform anfassen) und in eine Spur ziehen.
  Die fertigen Dateien liegen in `Dokumente/Brain Bugz/Wormhole`.

**Formant Lock** hält den Stimmcharakter beim Pitchen. **Attack Lock** hält Drums und Anschläge knackig
(Attacks laufen in Originalgeschwindigkeit, kein Vorecho).

## Engine (`Engine/WormholeEngine.h`)
Eigener Phase-Vocoder, ohne Lizenzkosten:
- zwei Auflösungen (4096 unter 300 Hz, 2048 darüber), Phasen-Kopplung um Spektral-Spitzen
- gemeinsame Phase für L/R, damit das Stereobild stehen bleibt
- Attack-Erkennung mit Phasen-Reset, Zeit-Verriegelung und Vorecho-Maskierung
- Formant-Erhalt über cepstrale Hüllkurve
- FFT: PFFFT (BSD-artige Lizenz, `Engine/pffft/LICENSE.txt`)

Messwerte gegen Signalsmith Stretch (Testsignale, siehe `tools/`):
Tonhöhe auf 0,1 Cent genau, Reinheit 65–75 dB (Vergleich 51–57 dB),
Vorecho an Drums 27–34 dB unter dem Schlag (Vergleich 20–25 dB),
Formant-Abweichung halb so groß.

## Tests (`tools/`)
- `stretch_cli.cpp`: `stretch_cli in.wav out.wav <Längenfaktor> <Halbtöne> [formant 0/1] [live 0/1]`
- `gen_signals.py` erzeugt Testsignale, `analyze.py` misst Tonhöhe, Reinheit, Vorecho, Formanten
  (Pfade zu `cli` und Referenz oben in der Datei anpassen)
- `gui_snapshot.cpp`: rendert die echte Oberfläche als PNG (Konsolen-App)

Schrift: Inter (OFL, `Assets/Inter-LICENSE.txt`). Logo: Clean-Vektor aus dem Brain-Bugz-3D-Kit.
