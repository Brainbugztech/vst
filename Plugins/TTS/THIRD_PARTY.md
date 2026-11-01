# Brainbugz TTS: verwendete Fremd-Bausteine

| Baustein | Wofür | Lizenz | Woher |
|---|---|---|---|
| Kokoro-82M v1.0 (hexgrad) | Stimm-Modell + Stimmen | Apache-2.0 | ONNX-Export von thewh1teagle/kokoro-onnx (Release `model-files-v1.1`), wird beim ersten Start heruntergeladen |
| misaki (hexgrad) | Aussprache-Wörterbücher US/GB (`Assets/lexicon_*.txt.gz`), Regeln der Aussprache-Engine | Apache-2.0 | github.com/hexgrad/misaki @ fba1236 |
| ONNX Runtime 1.22.0 (Microsoft) | führt das Modell aus (`bbtts_onnxruntime.dll` in der VST3) | MIT | github.com/microsoft/onnxruntime |
| kokoro-onnx (thewh1teagle) | Vorlage für Ein-/Ausgabe des Modells | MIT | github.com/thewh1teagle/kokoro-onnx |

Alle Lizenzen erlauben auch den Verkauf des Plugins. Bei Weitergabe die Lizenztexte beilegen
(Apache-2.0: Hinweis auf Kokoro + misaki, MIT: Copyright-Vermerk von ONNX Runtime).

Lexika neu bauen: `python tools/make_lexicon.py <misaki>/misaki/data Assets`
