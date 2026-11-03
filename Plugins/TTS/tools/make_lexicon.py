# Baut aus den misaki-Wörterbüchern (Apache-2.0, github.com/hexgrad/misaki) kompakte Lexikon-Dateien fürs Plugin.
# Aufruf: python make_lexicon.py <pfad/zu/misaki/misaki/data> <ausgabe-ordner>
# Format pro Zeile: wort<TAB>phoneme   oder   wort<TAB>@TAG=phoneme;TAG=phoneme  (~ = kein Eintrag)
import gzip, json, sys, os
src, out = sys.argv[1], sys.argv[2]
for lang in ("us", "gb"):
    lines = []
    for kind in ("gold", "silver"):
        lines.append("#" + kind)
        d = json.load(open(os.path.join(src, f"{lang}_{kind}.json"), encoding="utf-8"))
        for k in sorted(d):
            v = d[k]
            if isinstance(v, str):
                lines.append(f"{k}\t{v}")
            elif isinstance(v, dict):
                parts = [f"{t}={'~' if p is None else p}" for t, p in v.items()]
                lines.append(f"{k}\t@" + ";".join(parts))
    data = ("\n".join(lines) + "\n").encode("utf-8")
    with gzip.GzipFile(os.path.join(out, f"lexicon_{lang}.txt.gz"), "wb", compresslevel=9, mtime=0) as f:
        f.write(data)
    print(lang, len(lines), "Zeilen")
