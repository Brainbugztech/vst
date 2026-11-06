# Baut das Gain-Lock-Logo aus Assets/logo_text.png:
# nur die Buchstaben (ohne Splatter/Blitze), glatte Kanten, Bernstein-Metall mit Tiefe.
# Aufruf: python3 make_logo.py <repo>/Assets/logo_text.png <out.png>
import sys
import numpy as np
from PIL import Image, ImageFilter
from scipy import ndimage as ndi

src, dst = sys.argv[1], sys.argv[2]
im = Image.open(src).convert("RGBA")
S = 3  # hochskalieren für glatte Kanten
im = im.resize((im.width * S, im.height * S), Image.LANCZOS)
a = np.asarray(im).astype(np.float32) / 255.0
alpha = a[..., 3]
# Buchstaben sind dicht und hell-grün; Splatter/Blitze dünn oder pink
rgb = a[..., :3]
green = (rgb[..., 1] > 0.55) & (rgb[..., 0] < rgb[..., 1] + 0.1)
mask = (alpha > 0.5) & green
# dünne Linien (Blitze) weg: Öffnen mit Kreis
r = 4 * S // 3 + 2
yy, xx = np.mgrid[-r:r + 1, -r:r + 1]
disk = (xx * xx + yy * yy) <= r * r
mask = ndi.binary_opening(mask, structure=disk)
mask = ndi.binary_closing(mask, structure=disk)
mask = ndi.binary_fill_holes(mask) & mask | ndi.binary_closing(mask, structure=disk)
# nur große zusammenhängende Teile (Buchstaben) behalten
lab, nlab = ndi.label(mask)
sizes = ndi.sum(mask, lab, range(1, nlab + 1))
keep = np.zeros(nlab + 1, bool)
keep[1:] = sizes > sizes.max() * 0.02
# Reste der Blitze über dem Schriftzug: Teile, die komplett im oberen Fünftel liegen
objs = ndi.find_objects(lab)
for i, sl in enumerate(objs, start=1):
    if sl is not None and sl[0].stop < mask.shape[0] * 0.24:
        keep[i] = False
mask = keep[lab]
# lange, schmale Schweife (Blitz-Ausläufer) an den Seiten: horizontale Ausdehnung beschneiden
ys, xs = np.nonzero(mask)
# weiche Kante
m = Image.fromarray((mask * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(1.2 * S))
m = np.asarray(m).astype(np.float32) / 255.0
m = np.clip((m - 0.5) * 2.2 + 0.5, 0, 1)

# Relief: Höhe aus Distanz zur Kante -> Licht von oben links
dist = ndi.distance_transform_edt(m > 0.5)
h = np.clip(dist / (6.0 * S), 0, 1) ** 0.6
gy, gx = np.gradient(ndi.gaussian_filter(h, 1.5 * S))
light = np.clip(0.5 - (gx * 0.7 + gy * 1.0) * 18.0, 0, 1)
# Bernstein-Metall: Verlauf oben hell -> unten tief
H, W = m.shape
v = np.linspace(0, 1, H)[:, None]
base = np.stack([1.00 - 0.10 * v, 0.72 - 0.30 * v, 0.20 - 0.14 * v], -1) * np.ones((H, W, 1))
col = base * (0.55 + 0.75 * light[..., None])
# feine Lichtkante oben, dunkle Kante unten
edge = np.clip(1 - dist / (1.5 * S), 0, 1) * (m > 0.5)
col = col * (1 - 0.35 * edge[..., None]) + 0.35 * edge[..., None] * np.array([1.0, 0.86, 0.55]) * (light[..., None] > 0.5)
col = np.clip(col, 0, 1)
out = np.dstack([col, m])
# zuschneiden
ys, xs = np.nonzero(m > 0.02)
pad = 6 * S
y0, y1 = max(ys.min() - pad, 0), min(ys.max() + pad, H)
x0, x1 = max(xs.min() - pad, 0), min(xs.max() + pad, W)
out = out[y0:y1, x0:x1]
img = Image.fromarray((out * 255).astype(np.uint8), "RGBA")
img = img.resize((img.width * 2 // S, img.height * 2 // S), Image.LANCZOS)  # 2x der Originalgröße
img.save(dst, optimize=True)
print(dst, img.size)
