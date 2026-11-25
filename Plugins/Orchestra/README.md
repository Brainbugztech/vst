# Brainbugz Orchestra v2

VST3-Instrument mit vier Engines (eine pro Instanz), Samples: VSCO 2 CE (CC0).

- **Strings**: Violinen, Violas, Celli (Split Celli < C3 < Violas < G3 < Violinen), optional Kontrabass eine Oktave tiefer
- **Cellos**: Cello Section + Kontrabass-Layer
- Artikulationen: Sustain, Marcato, Spiccato, Pizzicato, Tremolo, Swell (Keyswitches C0 - F0)
- Legato mit Glide, Ensemble, Humanize
- **Rhythm Engine** (tempo-synchron): GATE (Lautstaerke-Gate) oder STRIKE (schlaegt gehaltene Noten im Pattern neu an),
  12 Patterns, 5 Raten, Swing, Depth
- **Atmos Pads**: 10 Quellen (Strings, Cellos, Horns, Low Brass, Trumpets, Flutes, Reeds, Organ, Deep Bass, Pure Synth),
  Quelle A/B mit Morph und Evolve, Air, Shimmer, Motion
- **Drums**: 5 Kits - Epic (36), Percussion (36), Metal & FX (36), Timpani chromatisch (Hits C1-B2, Rolls C3-B4),
  Trailer (24 Hybrid-Hits mit Sub, Riser, Whoosh, Reverse) + Sub, Stack, Punch, Decay
- FX: Drive, FDN-Hall (Reverb/Space), Width, Tone; 40 Werkspresets + eigene Presets (SAVE)

## Samples
`prep_samples.py` trimmt die WAVs, gleicht Pegel pro Grundton an, backt Crossfade-Loops in Sustains
und schreibt `Samples/*.ogg` und `Source/SampleManifest.h`:

    python3 prep_samples.py <VSCO-2-CE-Ordner> <VSCO-2-CE-SFZ-Branch> Samples Source/SampleManifest.h

Samples werden pro Engine im Hintergrund geladen und zwischen Instanzen geteilt.
