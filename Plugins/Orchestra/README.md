# Brainbugz Orchestra

VST3-Instrument mit vier Engines (eine pro Instanz):

- **Strings**: Violinen, Violas, Celli als Ensemble (Split: Celli < C3 < Violas < G3 < Violinen, Überlappung C3-G4 geschichtet)
- **Cellos**: Cello Section
- **Atmos Pads**: geloopte Streicher-Layer + Air-Synth (Saws + Rauschen), Shimmer (Oktave drüber), Motion (Filter/Chorus-Bewegung)
- **Drums**: 28 Orchester-Percussions auf C1 bis D#3 (Bass Drum, Taikos, Timpani, Snare, Becken, Gong, ...), Pads in der GUI anklickbar

Artikulationen (Strings/Cellos): Sustain, Spiccato, Pizzicato, Tremolo.
Mod Wheel (CC1) und CC11 = Dynamik, Sustain-Pedal (CC64), Pitchbend ±2.

## Samples
VSCO 2 Community Edition (CC0, https://github.com/sgossner/VSCO-2-CE).
`prep_samples.py` trimmt die WAVs, gleicht Pegel pro Grundton an, backt Crossfade-Loops in die Sustains
und schreibt OGG-Dateien nach `Samples/` sowie `Source/SampleManifest.h`:

    python3 prep_samples.py <VSCO-2-CE-Ordner> Samples Source/SampleManifest.h

Die OGGs werden als BinaryData ins Plugin eingebaut (ca. 23 MB) und pro Engine erst bei Bedarf
im Hintergrund dekodiert (geteilt zwischen allen Instanzen).
