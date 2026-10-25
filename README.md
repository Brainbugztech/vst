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

Bei jeder Kick misst Bugz Align im Punch-Fenster, wie gut Kick-Sub und Bass-Sub zusammenpassen, und dreht die Phase des Bass-Subs auf den besten Winkel. Der neue Winkel wird per Look-ahead schon kurz vor der Kick weich eingeblendet. Feste Latenz von ca. 148 ms, die Ableton automatisch ausgleicht (gedacht fürs Produzieren, nicht für Live).

**Clean:** Der Signalweg ist linearphasig (FIR-Tiefpass + FIR-Hilbert per FFT-Faltung). Ausgang = Original + Korrektur des Subs. Bei 0 Grad kommt das Signal bit-genau so raus, wie es reingeht, und oberhalb des Splits wird nichts angefasst (keine Phasenverschiebung der Höhen).

- **Amount**: wie stark korrigiert wird (100 % = voller Winkel).
- **Split**: nur unterhalb dieser Frequenz wird gedreht. Am besten über den Bass-Grundton setzen (Standard 180 Hz), direkt am Split überlagern sich gedrehter und ungedrehter Anteil. Ganz rechts = **FULL**, dann wird der ganze Bass gedreht.
- **Window**: Länge des Punch-Fensters nach der Kick, in dem gemessen wird (kurze, knackige Kick = kürzer, lange 808 = länger).
- **Glide**: wie schnell zum neuen Winkel übergeblendet wird.
- **Kick Sens**: Empfindlichkeit der Kick-Erkennung.
- **Offset**: manueller Zusatz-Winkel (funktioniert auch ohne Sidechain als normaler Phasendreher).
- **HOLD** friert den aktuellen Winkel ein, **A/B VORHER** schaltet die Korrektur zum Vergleichen aus, **SUB CHECK** spielt nur den Sub von Bass + Kick (mono) zum Gegenhören.
- **Anzeige**: Verlauf über die ganze Breite (bis 8 s) mit Sub von Kick (grün), Bass vorher (grau, per **VORHER** ein/aus) und nachher (pink), genau so, wie es im Ausgang ankommt. Darunter die Phasen-Spur, grüne Linien = erkannte Kicks. **Mausrad** oder **+/−** = Zoom (10 ms bis 8 s), **PAUSE** hält an, dann mit der Maus ziehen = zurückscrollen, **Doppelklick** = zurück zu Live. Unten: Kick-LED, Phase jetzt/erkannt, Korrelation vorher/nachher.

## Brainbugz Snare Head

Snare-Synth für metallisch klingende Psy/Hitech-Snares (Instrument, per MIDI anschlagen, jede Note = Snare). Kein tiefer Körper, sondern ein klingender Ton-Cluster + Rauschen + eckige Block-Hüllkurve. Signalweg: Ring + Noise → Res-EQ → Xtreme Noisy → Reverb mit Gate → Width/Out.

- **Ring**: **Tune** (700 Hz – 1,6 kHz = F5 bis G6, Ton-Cluster aus 6 Resonanzen), **Spread** (zieht den Cluster auseinander = Schweben/Schimmern), **Metal** (von harmonisch zu unharmonischem Blech-Klang), **Punch** (Klick + kurzer Pitch-Drop), **Ring** (Pegel des Tons).
- **Noise**: **Level** und **Tone** (bandbegrenztes Rauschen, 1,5 – 9 kHz).
- **Shape**: **Hold** (wie lange die Snare voll steht) und **Release** (wie schnell sie danach weg ist, kurz = harter Cut).
- **Res EQ**: Resonanz-Boost **700 Hz – 1,6 kHz** mit **Freq**, **Boost** (bis +18 dB) und **Q**. Anzeige mit Notenname und den Ring-Teiltönen. In der Anzeige ziehen = Freq/Boost, Mausrad = Q. **LINK** legt den EQ automatisch auf den Ring-Ton.
- **Reverb**: großer Hall (**Size**, **Mix**) mit **Gate**: **Gate** = wie lange der Hall nachklingen darf (30 ms – 2 s, ganz rechts = aus), **Cut** = harter Schnitt bis kurzes Ausblenden, darunter **Sync** auf Notenwerte (1/64 – 1/1, folgt dem Ableton-Tempo). In der Anzeige ziehen = Gate-Zeit. Jeder Schlag startet das Gate neu.
- **Xtreme Noisy**: ein Regler für Comb (Blechdose), Ringmod, Drive und Bitcrush. Ab 30 % klebt der XTREME-NOISY-Sticker drauf.
- **Out**: **Width** (0 = mono, Mitte = natürlich, rechts = extra breit), **Volume**, **KEY TRACK** (MIDI-Note stimmt um, C3 = Tune; aus = jede Note klingt gleich).
- **Hz / NOTE** (Knopf an Tune und Res-Freq): im Note-Modus zeigt der Regler den Ton an und rastet auf Halbtöne.
- **Werte eintippen**: Wert unter einem Regler anklicken und eintippen. Bei Tune/Freq gehen Hz (`880`, `1.2k`) und Noten (`A`, `A5`, `F#`, `Bb6`). Liegt die Oktave außerhalb von 700 Hz – 1,6 kHz, wird sie automatisch reingeschoben (`A1` → A5 = 880 Hz).
- **Logo anklicken** = Snare anspielen (weiter unten geklickt = härter).
- **Presets**: 26 Werkspresets (12 Psy/Hitech, 8 Xtreme Noisy/Industrial, 6 Acajou = nachgebaute Snares), Pfeile blättern, **SAVE** / **...** wie beim Bleep Designer. Eigene Presets liegen in `Dokumente\Brain Bugz\Snare Head\Presets` (.snarehead).
- Doppelklick auf einen Regler = Standardwert.
- `Plugins/SnareHead/tools/render.cpp` rendert alle Presets als WAV (Engine ist ohne JUCE testbar).

Titel-Schrift: Creepster (SIL Open Font License), wird beim Build geladen.

## Brainbugz Brainfuzz

4-Band-Sättigung mit Stereo-Breite und Effekten pro Band. Das Signal wird an drei Weichen in vier Bänder geteilt, jedes Band bekommt eigenen Drive, Level, Width und Spread, dazu Crush, Chorus und Reverb. Sättigung läuft mit 4x Oversampling (linearphasig, Latenz 28 Samples, wird automatisch ausgeglichen).

- **Analyser** (oben): Live-Spektrum jedes Bandes in seiner Farbe, weiße Linie = Gesamt-Ausgang. **Grenzen ziehen** = Weichen verschieben, **Griff hoch/runter** = Drive des Bandes. Shift = fein, Doppelklick = Standard, Mausrad ueber einem Band = Drive. **Frequenz- oder Drive-Wert anklicken** = eintippen (z. B. `2.5k`).
- **Band-Module**: **S** Solo, **M** Mute, **B** Bypass (Band sauber), **Drive** (0–36 dB, 0 = sauber), **Level**, **Width** (vorhandene Breite, 0 % = mono, 200 % = doppelt), **Spread** (zusätzliche Breite aus der Mitte). Unten das Stereo-Bild mit Korrelation.
- **Mono-sicher**: Width und Spread verändern die Mono-Summe nicht (L+R bleibt gleich), Spread funktioniert auch auf Mono-Quellen.
- **Effekte pro Band** (Reiter Crush / Chorus / Reverb, Klick = anzeigen, Doppelklick oder **ON** = an/aus):
  - **Crush**: Bits (24 bis 1), Down (Downsampling bis 40x), Mix
  - **Chorus**: 1–3 Stimmen, Rate, Depth, Mix
  - **Reverb**: Size, Length, Width, Pre-Delay, Lo Cut, Hi Cut, Mix
  - **PRE / POST**: vor oder nach der Sättigung. **BAND / WIDTH / BOTH**: auf dem Band, nur auf der zusätzlichen Breite (Spread) oder beides. Reverb nur auf WIDTH = breiter Hall, der in Mono verschwindet.
- **Saturation**: 7 Typen von hart nach warm: **Splat**, **Fuzz**, **Fold**, **Diode**, **Crunch**, **Tube**, **Tape**. **Bite** formt, wie hart der Typ zubeißt. Daneben die Kennlinie.
- **Filter**: 8 Weichen-Typen (**Butterworth, Linkwitz-Riley, Chebyshev I/II, Elliptic, Bessel, Legendre, Gaussian**) mit 12/24/48 dB. Linkwitz-Riley summiert perfekt flach, die anderen haben ihren eigenen Charakter an den Übergängen (max. +-3 dB).
- **Tight**: Verzerrung erzeugt Anteile außerhalb des Bandes (z. B. ein verzerrtes Hi-Hat-Band, das bis in den Sub reicht). **HP** / **LP** filtern nur diese Verzerrungs-Anteile an den Bandgrenzen (24/48 dB), das saubere Band bleibt unangetastet.
- **Mono Bass**: alles unterhalb der Frequenz bleibt in der Mitte.
- **Master**: **Input** / **Output** (mit **LINK**: Input hoch = Output runter), Pegelanzeigen, **LIMIT** (-0.1 dBFS, GR-Anzeige). **FX Master** Crush / Chorus / Reverb (alle Bänder zusammen, 0–200 %). **Parallel**: **Sat** (Sättigung gegen sauber), **Spread** (gesamte Zusatz-Breite), **Mix** (gesamt, phasengleich gemischt).
- **Presets**: 40 Werkspresets, pegelangeglichen, in Gruppen (Basics, Crossover Types, Saturation, Bass, Drums, Synth & Vocals, FX Combos). **LOCK** behält Input, Output, Effekte oder Tight beim Blättern. **SAVE** / **OPTIONS** wie bei den anderen Plugins, eigene Presets in `Dokumente\Brain Bugz\Brainfuzz\Presets` (.bfz).
- **UNDO / REDO**, **BYPASS** (ganzes Plugin), **OPTIONS** → Fenstergröße (75–200 %), die Fensterecke lässt sich auch ziehen.
- Tools ohne JUCE: `Plugins/BrainFuzz/tools/engine_test.cpp` (Tests: Latenz, Summe, Mono-Sicherheit, Tight, CPU), `level_match.cpp` (berechnet den Pegel-Abgleich der Presets, `--wav` rendert Hörbeispiele).

Titel-Schrift: Audiowide (SIL Open Font License), wird beim Build geladen.

## Brainbugz Prizm

Harmonic Sweetener: zieht beliebiges Audio in eine Tonart. Schiefe Pads, Noise, Drums oder ganze Loops klingen danach „in Key“. Beide Modi benutzen denselben Filter-Kern (eine Bandpass-Bank, ein Filter pro Note der Tonart) und klingen bei gleichen Einstellungen gleich:

- **Zero Latency**: die Filterbank wird direkt gerechnet. Keine Latenz, gut für Live und Recording.
- **High Quality**: dieselbe Filterkurve als spektrale Maske (Phase-Vocoder), ohne das Verschmieren der Filter, Transienten bleiben klarer. Latenz 4096 Samples (bei 88,2/96 kHz 8192), wird dem Host gemeldet. Zusätzlich gibt es hier **Shift**.

Regler:

- **Key / Scale**: Grundton und Skala (Dur, Moll, Kirchentonarten, Pentatonik, Blues, Dur-/Moll-Akkord, chromatisch).
- **Noten-Leiste**: grün = erlaubter Ton, Klick schaltet einen Ton an/aus (Skala wechselt auf Custom). **Hoch/runter ziehen** stellt die Gewichtung der Note ein (0–100 %, Shift = fein, Doppelklick = 100 %), die Füllhöhe der Taste zeigt sie an. Das Leuchten zeigt live, welche Töne gerade rauskommen.
- **MIDI KEY**: Noten auf den MIDI-Eingang des Plugins bestimmen die Tonart, der letzte Akkord bleibt aktiv (Leiste wird pink).
- **SIDECHAIN**: die Tonart folgt den Noten einer anderen Spur (z. B. Bass oder Keys), die in der DAW auf den Sidechain-Eingang von Prizm geroutet ist. Erkannt werden nur Grundtöne, bei Stille bleibt der letzte Akkord stehen (Leiste wird cyan). Wenn MIDI KEY auch an ist und Noten kommen, hat MIDI Vorrang.
- **PRESET** (oben rechts): 12 Werkspresets und eigene Presets (**SAVE**, gespeichert in `Dokumente\Brain Bugz\Prizm\Presets` als `.przm`). Key, Scale, Custom-Noten, Tune, MIDI KEY und Sidechain bleiben beim Wechsel stehen, damit man im Song durchblättern kann.
- **Strength**: wie viel vom gefilterten Signal das Original ersetzt.
- **Color**: Filtergüte. 0 % weich, 100 % deutlich tonal, **über 100 %** (pinker Bogen) klingen die Töne lange nach, bei 200 % mehrere Sekunden.
- **Clear**: Anschläge kommen trocken durch, alte Resonanzen werden bei neuen Hits gedämpft.
- **Mix** (latenzkorrigiert) und **Output** (−24 bis +12 dB, Soft-Limiter am Ausgang).
- **Range Low / Range High**: nur dieser Frequenzbereich wird in die Tonart gezogen, darunter und darüber bleibt das Signal unverändert (Linkwitz-Riley-Weichen). So bleiben Kick, Sub und Becken sauber. OFF = ganzer Bereich.
- **Gate**: drückt leise Noten relativ zur lautesten herunter, räumt Rauschen zwischen den Tönen auf.
- **PROCESS** (unteres Panel, rechts oben): **Stereo**, **Mid only** oder **Side only**. Bei Mid/Side wird nur dieser Anteil in die Tonart gezogen, der andere bleibt unverändert (z. B. nur die Breite eines Pads stimmen, Mitte mit Kick und Bass bleibt sauber).
- **Shift** (nur High Quality): rastet schiefe Noten auf die nächste Note der Tonart ein, statt sie nur zu dämpfen. Prizm erkennt dafür polyphon, welche Teiltöne zu welcher Note gehören, und verschiebt jede Note mit all ihren Obertönen gemeinsam (Phase-Locking um die Peaks), dadurch bleiben Klänge harmonisch und kaum Phasing.
- **Pitch Matrix** (unter den Tasten): zeigt live, wo jede Note landet. Oben die eingehenden Noten (die Tasten leuchten mit dem, was reinkommt), unten die Ausgangsnoten (leuchten mit dem, was rauskommt), dazwischen Lichtstrahlen: **grün geradeaus** = bleibt, **cyan Kurve** = SHIFT, **pinke Kurve** = Map, **verblasst mit ×** = rausgefiltert bzw. **OFF**. Gestrichelt grau = Anteil Original (Strength/Mix unter 100 %), gestrichelt pink = Map gesetzt, wirkt aber nur in High Quality.
- **Noten-Map** über die Matrix: einen Strahl zu einer Note unten **ziehen** = umleiten (z. B. E → Eb). Nur die gespielte Note wandert, Obertöne anderer Noten bleiben stehen. **Rechtsklick** = Menü (Ziel wählen, OFF, alle zurücksetzen), **Doppelklick** = zurück auf normal. OFF wirkt in beiden Modi, Verschieben braucht High Quality. **MAJ > MIN** / **MIN > MAJ** setzen Terz, Sexte und Septime relativ zur Tonart, **RESET MAP** stellt alles zurück.
- **Erklärungen**: Mit der Maus kurz über einem Regler, Knopf, einer Taste oder einem Strahl stehen bleiben, dann erscheint eine kurze Erklärung (Titel + ein, zwei Sätze, auf Deutsch). Über Tasten und Strahlen beschreibt sie live, was mit genau dieser Note gerade passiert.
- **Formant** (nur High Quality): erhält die Klangfarbe beim Verschieben, damit Stimmen bei größeren Sprüngen nicht nach Mickey Mouse klingen.
- **Learn**: hört 8 Sekunden hörbares Material am Eingang und stellt Key, Scale (Dur/Moll) und Tune selbst ein. Danach steht das Ergebnis über der Noten-Leiste, bei unsicherem Ergebnis mit Hinweis. Nochmal klicken bricht ab.
- **Tune A4**: Stimmton 415–466 Hz (Standard 440). Wichtig für alte Samples und Vinyl, die oft nicht auf 440 Hz liegen, sonst zieht Prizm alles in die falschen Töne. Learn setzt ihn automatisch.

Gemeinsam für beide Modi im Processor: Bereichsweichen, Pegelausgleich, Strength, Clear, Mix. Unterschiede gibt es nur noch durch das Prinzip selbst: Zero Latency verschmiert Anschläge etwas, High Quality kann sehr eng beieinander liegende Teiltöne (unter ca. 25 Hz Abstand) nicht ganz sauber trennen.

Test ohne DAW: `cmake -B build -DPRIZM_BUILD_TESTS=ON`, dann `prizm_render_test` bauen und starten (prüft u. a. dass beide Modi übereinstimmen, Tonhöhe, Bereich, Gate, Learn, Tune, Noten-Map, Formant, Gewichtung, Mitte/Seite, Sidechain, Presets, Latenz, MIDI-Tonart, Pegel, Stabilität und schreibt Hörbeispiele nach `renders/`, darunter `compare_*_zl.wav` / `compare_*_hq.wav` zum direkten Vergleich). Der Hintergrund wird mit `Plugins/Prizm/tools/make_backdrop.py` erzeugt.

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
- `Plugins/SnareHead` – Snare-Synth (DSP in `SnareEngine.h`, Presets in `Presets.h`)
- `Plugins/BrainFuzz` – 4-Band-Sättigung (DSP in `Source/dsp`, Oberfläche in `Source/ui`, Tests in `tools`)
- `Plugins/Prizm` – Harmonic Sweetener (gemeinsamer Filter-Kern in `ResponseModel.h`, Notenerkennung in `NoteGrouper.h`, Pitch Matrix in `PitchMatrix.*`, Learn in `KeyAnalyzer.h`, Sidechain in `ChordTracker.h`, Werkspresets in `Presets.h`, Zero Latency in `FilterBankEngine.*`, High Quality in `SpectralEngine.*`, Test in `tools`)
- `Plugins/Phazer` – Phaser-Engine, noch in Arbeit, wird nicht gebaut
- `Shared` – gemeinsamer Look (Farben, Schriften, Regler)
- `Assets` – Grafiken (zerlegter Logo-Käfer, Labor-Hintergrund)
- `Prototypes` – Browser-Prototypen zum Reinhören

Schriften: Chakra Petch und Share Tech Mono (SIL Open Font License) werden beim Build geladen.
