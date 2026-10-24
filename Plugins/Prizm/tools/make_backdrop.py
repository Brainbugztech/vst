"""Erzeugt den Prizm-Hintergrund (nasse, dunkle Tech-Oberfläche mit Bokeh).

Aufruf: python make_backdrop.py ../Assets/prizm_backdrop.jpg
Braucht numpy und Pillow. Ausgabe: 1600 x 1080 (2x der GUI-Größe 800 x 540).
"""
import sys
import numpy as np
from PIL import Image, ImageDraw, ImageFilter
from scipy.ndimage import gaussian_filter

W, H = 1600, 1080
rng = np.random.default_rng(1729)


def blur(a, r):
    return gaussian_filter(a.astype(np.float32), r)


def blur_rgb(a, r):
    return np.stack([blur(a[..., i], r) for i in range(3)], axis=-1)


y = np.linspace(0, 1, H)[:, None]
x = np.linspace(0, 1, W)[None, :]

# 1) base: deep navy, a bit lighter at the top
top = np.array([0.045, 0.075, 0.125])
mid = np.array([0.020, 0.035, 0.062])
bot = np.array([0.012, 0.022, 0.040])
t = y[..., None]
img = np.where(t < 0.5, top + (mid - top) * (t / 0.5), mid + (bot - mid) * ((t - 0.5) / 0.5))
img = np.broadcast_to(img, (H, W, 3)).copy()

# 2) wet metal: height field -> specular sheen from the upper left
field = np.zeros((H, W), np.float32)
for scale, amp in [(160, 1.0), (60, 0.30)]:
    f = blur(rng.random((H, W)).astype(np.float32), scale)
    field += (f - f.mean()) / (f.std() + 1e-9) * amp
field = (field - field.min()) / (field.max() - field.min())
gy, gx = np.gradient(field * 260.0)
nz = 1.0 / np.sqrt(gx * gx + gy * gy + 1.0)
nx, ny = -gx * nz, -gy * nz
light = np.array([-0.45, -0.55, 0.70]); light /= np.linalg.norm(light)
half = light + np.array([0, 0, 1.0]); half /= np.linalg.norm(half)
spec = np.clip(nx * half[0] + ny * half[1] + nz * half[2], 0, 1) ** 45
sheen_col = np.array([0.35, 0.60, 1.0])
img += spec[..., None] * sheen_col * 0.20
img *= np.clip(0.94 + 0.06 * field, 0.8, 1.1)[..., None]

# 3) bokeh lights (blurred background city lights), mostly at the top and in the floor
bokeh = np.zeros((H, W, 3), np.float32)
palette = [((0.25, 0.65, 1.0), 12), ((0.20, 0.35, 0.95), 9), ((0.30, 0.95, 0.95), 4),
           ((1.0, 0.65, 0.25), 0.5), ((0.70, 1.0, 0.20), 0.5), ((1.0, 0.25, 0.60), 0.4)]
cols = [np.array(c) for c, _ in palette]
weights = np.array([w for _, w in palette]); weights /= weights.sum()
layer = Image.new("RGB", (W, H))
d = ImageDraw.Draw(layer)
for _ in range(130):
    band = rng.random()
    if band < 0.75:
        cy = rng.uniform(0, H * 0.22)
    else:
        cy = rng.uniform(H * 0.84, H)
    cx = rng.uniform(0, W)
    r = rng.uniform(4, 34) if cy < H * 0.4 else rng.uniform(2, 14)
    c = cols[rng.choice(len(cols), p=weights)] * rng.uniform(0.15, 0.6)
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=tuple(int(v * 255) for v in c))
raw = np.asarray(layer, dtype=np.float32) / 255.0
bokeh = np.stack([blur(raw[..., i], 7.0) for i in range(3)], -1)
img += bokeh * 0.38

# wet floor: the top lights reflect as long vertical smears in the lower part
refl = raw[::-1].copy()
refl = np.stack([gaussian_filter(refl[..., i], (38, 4)) for i in range(3)], -1)
fade = np.clip((y - 0.55) / 0.45, 0, 1) ** 1.5
img += refl * fade[..., None] * 0.55

# 3b) faint circuit traces etched into the plate
traces = Image.new("L", (W, H))
td = ImageDraw.Draw(traces)
for _ in range(70):
    px, py = rng.uniform(0, W), rng.uniform(H * 0.25, H)
    pts = [(px, py)]
    for _ in range(int(rng.integers(2, 5))):
        dx, dy = [(1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (1, -1), (-1, 1), (-1, -1)][int(rng.integers(0, 8))]
        ln = rng.uniform(30, 160)
        px, py = px + dx * ln, py + dy * ln
        pts.append((px, py))
    td.line(pts, fill=255, width=2, joint="curve")
    for (qx, qy) in (pts[0], pts[-1]):
        td.ellipse([qx - 4, qy - 4, qx + 4, qy + 4], outline=255, width=2)
tr = blur(np.asarray(traces, np.float32) / 255.0, 0.7)
img *= (1.0 - 0.35 * tr)[..., None]
img += np.roll(tr, (1, 1), (0, 1))[..., None] * np.array([0.20, 0.45, 0.85]) * 0.10

# 4) panel seams of the tech plate (line up with the GUI layout at 2x)
seams = np.zeros((H, W), np.float32)
hl = np.zeros((H, W), np.float32)
for yy in [200, 592]:
    seams[yy:yy + 3, :] = 1.0
    hl[yy + 3:yy + 4, :] = 1.0
for xx in [400, 1210]:
    seams[:200, xx:xx + 3] = 1.0
    hl[:200, xx + 3:xx + 4] = 1.0
img *= (1.0 - 0.55 * blur(seams, 1.0))[..., None]
img += blur(hl, 0.8)[..., None] * np.array([0.25, 0.45, 0.8]) * 0.35

# small bolts in the plate corners
bolt = Image.new("L", (W, H))
bd = ImageDraw.Draw(bolt)
for bx, by in [(30, 30), (W - 30, 30), (30, H - 30), (W - 30, H - 30), (420, 30), (1190, 30)]:
    bd.ellipse([bx - 7, by - 7, bx + 7, by + 7], fill=110)
    bd.ellipse([bx - 3, by - 6, bx + 2, by - 1], fill=255)
img += (np.asarray(bolt.filter(ImageFilter.GaussianBlur(0.8)), np.float32) / 255.0)[..., None] * np.array([0.25, 0.35, 0.5]) * 0.5

# 5) water droplets: dark body, bright cyan-white highlight upper left, glint lower right
drops_dark = Image.new("L", (W, H))
drops_hi = Image.new("L", (W, H))
dd, dh = ImageDraw.Draw(drops_dark), ImageDraw.Draw(drops_hi)
for _ in range(520):
    cy = H * (rng.random() ** 0.6)  # more drops towards the bottom
    cx = rng.uniform(0, W)
    r = rng.uniform(2.0, 4.0) if rng.random() < 0.7 else rng.uniform(4.0, 11.0)
    dd.ellipse([cx - r, cy - r, cx + r, cy + r], fill=int(rng.uniform(60, 130)))
    hr = max(0.8, r * 0.38)
    hx, hy = cx - r * 0.35, cy - r * 0.35
    dh.ellipse([hx - hr, hy - hr, hx + hr, hy + hr], fill=int(rng.uniform(120, 255)))
    if r > 3:
        dh.arc([cx - r * 0.8, cy - r * 0.8, cx + r * 0.8, cy + r * 0.8], 20, 110, fill=90, width=1)
dark = np.asarray(drops_dark.filter(ImageFilter.GaussianBlur(0.9)), np.float32) / 255.0
hi = np.asarray(drops_hi.filter(ImageFilter.GaussianBlur(0.6)), np.float32) / 255.0
img *= (1.0 - 0.50 * dark)[..., None]
img += hi[..., None] * np.array([0.55, 0.80, 1.0]) * 0.45

# a few drip streaks
streaks = np.zeros((H, W), np.float32)
for _ in range(14):
    sx = int(rng.uniform(20, W - 20))
    sy = int(rng.uniform(0, H * 0.6))
    ln = int(rng.uniform(60, 260))
    streaks[sy:sy + ln, sx] = np.linspace(0.9, 0.0, len(streaks[sy:sy + ln, sx]))
img += blur(streaks, 0.9)[..., None] * np.array([0.3, 0.55, 0.9]) * 0.5

# 6) glowing floor line at the bottom (like a holo projector base)
glow = np.exp(-((y - 0.985) / 0.012) ** 2) * np.exp(-((x - 0.5) / 0.38) ** 2)
img += glow[..., None] * np.array([0.15, 0.55, 1.0]) * 0.35

# 7) fine film grain + vignette
img += (rng.random((H, W, 1)).astype(np.float32) - 0.5) * 0.018
vig = 1.0 - 0.55 * (((x - 0.5) / 0.75) ** 2 + ((y - 0.45) / 0.85) ** 2)
img *= np.clip(vig, 0.35, 1.0)[..., None]

out = Image.fromarray((np.clip(img, 0, 1) ** (1 / 1.05) * 255).astype(np.uint8))
out.save(sys.argv[1] if len(sys.argv) > 1 else "prizm_backdrop.jpg", quality=90, optimize=True)
print("ok", out.size)
