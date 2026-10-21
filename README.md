# Brain Bugz Plugins

Eigene VST3-Effekte unter der Marke **Brain Bugz**, gebaut mit [JUCE](https://juce.com) 8.

## Brainbugz Haas

Stereo-Spreizer mit zwei Reglern.

- **Width**: verzögert links kurz und rechts länger (Standard bei 100 %: 6 ms / 30 ms). Ab 100 % beginnt der **Krass**-Bereich mit Side-Boost, Allpass-Dekorrelation und leicht wabernden Delays (pegelkompensiert).
- **Mono Bass**: alles unterhalb der Frequenz bleibt mono und unverzögert (unter 30 Hz = aus).
- **L ⇄ R** tauscht die Seiten, **Mono-Check** summiert zum Gegenhören auf Mono.
- Die L/R-Zeiten lassen sich direkt auf dem rechten Monitor einstellen (Plus/Minus, Ziehen, Mausrad, Doppelklick = Standard).

## Brainbugz BugFuzz

4-Band-Fuzz nach dem Quadrafuzz-Prinzip: das Signal wird in vier Bänder geteilt (Linkwitz-Riley, phasengleich), jedes Band wird einzeln verzerrt und danach wieder zusammengemischt. Läuft mit 8x Oversampling.

- **Drive**: Grund-Verzerrung für alle Bänder. Ab 50 % beginnt der **Krass**-Bereich: härtere, asymmetrische Klippung Richtung Gated-Fuzz.
- **Saturation**: Charakter der Verzerrung – wenig = warm und rund, Mitte = klassischer Fuzz, viel = hart Richtung Rechteck.
- **Mix**: Dry/Wet, zeitgleich gemischt (kein Kammfilter).
- **OCTA**: Gleichrichter-Oktave auf den oberen drei Bändern.
- **Auto-Gain**: Fuzz-Signal folgt dem Pegel des Originals (an = gleich laut beim Aufdrehen).
- **Output**: Ausgangspegel -24 bis +12 dB.
- **Band-Anzeige oben**: Weichen (Griffe mit Frequenz) seitlich verschieben, jedes Band hoch/runter ziehen = Drive des Bandes. Shift = fein, Doppelklick = Standard, Mausrad geht auch. Band auf 0 = bleibt sauber.

## Brainbugz Bleep Designer

Instrument für Bleep-Blop-Sounds: generiert 4 kurze Bleeps, friert sie als Samples ein und spielt sie per Arp in Mustern. Signalweg: Generator → 4 Slots → Arp/Sampler → Comb → Crush → Filter → Delay → Out.

- **Generator** (oben): **Osc** (Oktave, **Form** stufenlos Sinus → Triangle → Square → Saw, Pulsbreite), **Mod** (stummer Modulator, verbiegt die Phase: Oktave −4 bis +3, **PD** = Stärke, **Fine** = krummes Verhältnis für Metall-Sounds, **Pitch-Link**), **Pitch Env** (+ = fällt von oben), **LFO** (bis 500 Hz, auf Tonhöhe und PD), **Amp Env**.
- **GENERATE**: neuer Seed, alle nicht gesperrten Slots neu. Slot 1 bleibt nah an deinen Reglern, Slot 2–4 bekommen je einen von 14 Charakteren: Blip, Chirp, Warble, Zap, Chatter, Metal, Bloop, Droid, Chip-Arp, Ringmod, Throw, Stretch, Knatter, Long Tone. Dazu würfelt GENERATE Zusätze wie Delay-Throw, Bitcrush, Ringmod, Zerhacker oder Stufen-Tonhöhe. **Variation** = wie weit die Slots von deinen Reglern Richtung Charakter wandern. Der Pfeil-Knopf rendert mit gleichem Seed und den aktuellen Reglern neu. **Länge** ist die Obergrenze, die echte Länge ergibt sich aus dem Charakter.
- **Slots**: Klick auf die Wellenform = vorhören, Schloss = beim Generieren behalten, **Gewicht** (für Slot-Modus "Gewichtet") und **Pegel** pro Slot. Slots werden im Projekt mitgespeichert.
- **QUELLE** (oben rechts): **SLOTS** spielt die eingefrorenen Bleeps per Repitch, **LIVE** den Generator direkt.
- **Sequencer / Arp**: läuft, solange eine Note gehalten wird, im Takt von Ableton (steht der Transport, mit eigener Uhr ab dem Anschlag). Die gehaltene Note ist der Grundton.
  - **Rate**, **Steps**, **Swing**, **Gate**, **Modus** bei mehreren Noten, **Scale** (Chroma, Minor, Major, Phrygisch), **Range**, **Restart**, **Slots**-Modus (Random, No Repeat, Gewichtet, Cycle).
  - Step-Editor: Mitte ziehen = Tonhöhe (auch über mehrere Steps malen), Punkt oben = Akzent, unterer Balken ziehen = Wahrscheinlichkeit, Rechtsklick = Step sperren, Doppelklick = Tonhöhe 0.
  - **ROLL** würfelt die Tonhöhen der nicht gesperrten Steps neu, **Dichte** = wie viele Steps eigene Töne bekommen, **Auto** würfelt alle 1/2/4/8 Durchläufe neu.
  - **Zufall Seed**: Wahrscheinlichkeiten, Slot-Wahl und Auto-Roll hängen an **Seed** und Position, beim Abspielen, Aufnehmen und Bouncen identisch. **Free**: echt zufällig.
  - **ARP AUS**: MIDI-Noten spielen direkt, reihum auf die Slots verteilt.
- **Effekte** (Punkt = an/aus): **Comb** (Freq, Feedback, negativ = metallisch, Damping, Keytrack, 1–3 Combs, Mix), **Crush** (Bits, Samplerate, Mix), **Filter** (LP/BP/HP, Cutoff, Resonanz, Hüllkurve pro Step, Keytrack), **Delay** (synchron, Feedback, Ping-Pong, Low/High-Cut im Feedback, Mix), **Out** mit Limiter.
- **Mech**: schießt bei jedem Akzent-Step einen Laser auf den Slot, der gerade spielt.
- **Presets** (oben rechts): Pfeile blättern durch, Dropdown mit **Werk** (11 Werkspresets) und **Eigene**. **SAVE** speichert ein eigenes Preset (bei Werkspresets unter neuem Namen), **...** für Speichern unter, Löschen und Preset-Ordner öffnen. Ein Sternchen zeigt ungespeicherte Änderungen. Eigene Presets liegen in `Dokumente\Brain Bugz\Bleep Designer\Presets` (.bbleep) und enthalten die 4 Bleeps exakt so, wie sie klingen.
- Doppelklick auf einen Regler = Standardwert.

Titel-Schrift: Sedgwick Ave Display (SIL Open Font License), wird beim Build geladen.

## Brainbugz Bugz Align

Richtet die Phase des Basses automatisch an der Kick aus. Das Plugin kommt auf den **Bass-Kanal**, die Kick wird per **Sidechain** reingeroutet (Plugin-Fenster in Ableton aufklappen → Sidechain → Audio From: Kick-Spur).

Bei jeder Kick misst Bugz Align im Punch-Fenster, wie gut Kick-Sub und Bass-Sub zusammenpassen, und dreht die Phase des Bass-Subs auf den besten Winkel (Phase Rotation per Hilbert-Paar, kein Delay). Der neue Winkel wird per Look-ahead schon kurz vor der Kick weich eingeblendet. Feste Latenz von 180 ms, die Ableton automatisch ausgleicht (gedacht fürs Produzieren, nicht für Live).

- **Amount**: wie stark korrigiert wird (100 % = voller Winkel).
- **Split**: nur unterhalb dieser Frequenz wird gedreht, darüber bleibt der Bass unangetastet. Am besten über den Bass-Grundton setzen (Standard 180 Hz). Ganz rechts = **FULL**, dann wird der ganze Bass gedreht.
- **Window**: Länge des Punch-Fensters nach der Kick, in dem gemessen wird (kurze, knackige Kick = kürzer, lange 808 = länger).
- **Glide**: wie schnell zum neuen Winkel übergeblendet wird.
- **Kick Sens**: Empfindlichkeit der Kick-Erkennung.
- **Offset**: manueller Zusatz-Winkel (funktioniert auch ohne Sidechain als normaler Phasendreher).
- **HOLD** friert den aktuellen Winkel ein, **A/B VORHER** schaltet die Korrektur zum Vergleichen aus, **SUB CHECK** spielt nur den Sub von Bass + Kick (mono) zum Gegenhören.
- **Anzeige**: Kick (grün), Bass vorher (gestrichelt) und nachher (pink) im letzten Punch-Fenster, Phasen-Scheibe mit dem aktuellen Winkel, Korrelation vorher/nachher (höher = drückt mehr) und Kick-LED.

## Bauen

GitHub Actions baut bei jedem Push auf `main` automatisch die Windows-VST3 (`.github/workflows/build.yml`) und testet sie mit pluginval. Die fertigen Plugins liegen als Artefakt **Brainbugz-VST3** am jeweiligen Lauf und als ZIPs auf dem Branch `builds` (einzeln und als `Brainbugz-alle-VST3.zip`).

Lokal:

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## Ordner

- `Plugins/Haas` – Haas-Plugin (DSP in `HaasEngine.h`, Szene in `LabScene.cpp`)
- `Plugins/BugFuzz` – BugFuzz-Plugin (DSP in `BugFuzzEngine.h`, Band-Anzeige in `BandView.cpp`)
- `Plugins/BleepDesigner` – Bleep-Designer-Instrument, in Arbeit
- `Plugins/BugzAlign` – Kick/Bass-Phasenausrichtung (DSP in `AlignEngine.h`, Anzeige in `AlignView.cpp`)
- `Plugins/Phazer` – Phaser-Engine, noch in Arbeit, wird nicht gebaut
- `Shared` – gemeinsamer Look (Farben, Schriften, Regler)
- `Assets` – Grafiken (zerlegter Logo-Käfer, Labor-Hintergrund)
- `Prototypes` – Browser-Prototypen zum Reinhören

Schriften: Chakra Petch und Share Tech Mono (SIL Open Font License) werden beim Build geladen.
