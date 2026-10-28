"""Erzeugt die Web-Grafiken aus den Original-Assets im Repo.
Aufruf (aus dem Repo-Root): python3 website/tools/prepare-images.py
Benötigt: pip install pillow"""
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "website" / "public" / "media"

def save_webp(im, path, max_w=None, q=82):
    if max_w and im.width > max_w:
        im = im.resize((max_w, round(im.height * max_w / im.width)), Image.LANCZOS)
    path.parent.mkdir(parents=True, exist_ok=True)
    im.save(path, "WEBP", quality=q, method=6)
    print(path.relative_to(ROOT), im.size, path.stat().st_size // 1024, "KB")

# Marke: Schriftzug und Käfer (aus dem kombinierten Logo der HQ-App)
wordmark = Image.open(ROOT / "Assets/logo_text.png").convert("RGBA")
save_webp(wordmark.crop(wordmark.getbbox()), OUT / "brand/wordmark.webp")
full = Image.open(ROOT / "hq-app/renderer/logo.webp").convert("RGBA")
bug = full.crop((0, 215, full.width, full.height))
bug = bug.crop(bug.getbbox())
save_webp(bug, OUT / "brand/bug.webp")
save_webp(full.crop(full.getbbox()), OUT / "brand/logo-full.webp")

# Favicon + Apple-Icon aus dem Käfer
def square(im, size, bg=None):
    s = max(im.size); c = Image.new("RGBA", (s, s), bg or (0, 0, 0, 0))
    c.paste(im, ((s - im.width) // 2, (s - im.height) // 2), im)
    return c.resize((size, size), Image.LANCZOS)
square(bug, 64).save(OUT.parent / "favicon.png")
square(bug, 180, (5, 6, 12, 255)).convert("RGB").save(OUT.parent / "apple-touch-icon.png")

# Social-Preview 1200x630
og = Image.new("RGB", (1200, 630), (5, 6, 12))
b = bug.copy(); b.thumbnail((560, 400)); og.paste(b, (600 - b.width // 2, 230), b)
w = wordmark.crop(wordmark.getbbox()); w.thumbnail((640, 220)); og.paste(w, (600 - w.width // 2, 20), w)
og.save(OUT / "brand/og-image.jpg", quality=85)

# Plugin-Cover aus vorhandenen Plugin-Grafiken
P = ROOT / "Plugins"
save_webp(Image.open(P / "BleepDesigner/Assets/mech.jpg").convert("RGB"), OUT / "plugins/bleep-designer.webp", 900)
save_webp(Image.open(P / "Braincut/Assets/braincut_bg.jpg").convert("RGB"), OUT / "plugins/braincut.webp", 1200)
save_webp(Image.open(P / "Prizm/Assets/prizm_backdrop.jpg").convert("RGB"), OUT / "plugins/prizm.webp", 1200)
# Brainfuzz: Logo auf dunklem Synthwave-Grund
fz = Image.open(P / "BrainFuzz/Assets/logo.png").convert("RGBA")
cv = Image.new("RGBA", (1200, 800), (14, 8, 30, 255))
l = fz.copy(); l.thumbnail((900, 500)); cv.paste(l, (600 - l.width // 2, 400 - l.height // 2), l)
save_webp(cv.convert("RGB"), OUT / "plugins/brainfuzz.webp")
# Haas: Labor-Hintergrund
lab = Image.open(ROOT / "Assets/haas_lab.png").convert("RGBA")
bgc = Image.new("RGBA", lab.size, (8, 8, 14, 255)); bgc.alpha_composite(lab)
k = bug.copy(); k.thumbnail((420, 300)); bgc.alpha_composite(k, (990 - k.width // 2, 470 - k.height // 2))
save_webp(bgc.convert("RGB").crop((0, 0, 1980, 880)), OUT / "plugins/haas.webp", 1200)
