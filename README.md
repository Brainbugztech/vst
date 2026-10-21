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
- **Mix**: Dry/Wet, zeitgleich gemischt (kein Kammfilter).
- **OCTA**: Gleichrichter-Oktave auf den oberen drei Bändern.
- **Auto-Gain**: Fuzz-Signal folgt dem Pegel des Originals (an = gleich laut beim Aufdrehen).
- **Output**: in der Anzeige ziehen, Mausrad, Doppelklick = 0 dB.
- **Rechter Monitor**: die vier Band-Säulen (Drive je Band) und die drei Weichen-Frequenzen direkt ziehen; Shift = fein, Doppelklick = Standard. Band auf 0 = bleibt sauber.

## Bauen

GitHub Actions baut bei jedem Push auf `main` automatisch die Windows-VST3 (`.github/workflows/build.yml`) und testet sie mit pluginval. Die fertigen Plugins liegen als Artefakt **Brainbugz-VST3** am jeweiligen Lauf und als ZIPs auf dem Branch `builds` (einzeln und als `Brainbugz-alle-VST3.zip`).

Lokal:

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## Ordner

- `Plugins/Haas` – Haas-Plugin (DSP in `HaasEngine.h`, Szene in `LabScene.cpp`)
- `Plugins/BugFuzz` – BugFuzz-Plugin (DSP in `BugFuzzEngine.h`, Szene in `LabScene.cpp`)
- `Plugins/Phazer` – Phaser-Engine, noch in Arbeit, wird nicht gebaut
- `Shared` – gemeinsamer Look (Farben, Schriften, Regler)
- `Assets` – Grafiken (zerlegter Logo-Käfer, Labor-Hintergrund)
- `Prototypes` – Browser-Prototypen zum Reinhören

Schriften: Chakra Petch und Share Tech Mono (SIL Open Font License) werden beim Build geladen.
