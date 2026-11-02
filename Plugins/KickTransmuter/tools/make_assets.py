#!/usr/bin/env python3
# Bereitet Benetts GUI-Bild für das Plugin auf:
#  - entfernt die fest eingezeichneten Zeiger und leuchtenden Ringe der Regler
#  - dunkelt die beiden Leuchtbalken ab (die Anzeigen zeichnet das Plugin selbst)
#  - schneidet das Gehirn als helle Ebene aus (Puls bei jeder Kick)
# Aufruf: make_assets.py <Originalbild.png> <Assets-Ordner>
import sys
import cv2
import numpy as np

src, out = sys.argv[1], sys.argv[2]
im = cv2.imread(src)
H, W = im.shape[:2]
gray = cv2.cvtColor(im, cv2.COLOR_BGR2GRAY)

def refine(cx, cy, r, span=14):
    # Felgen-Kreis der Kappe genau finden
    x0, y0 = max(cx - r - 30, 0), max(cy - r - 30, 0)
    roi = gray[y0:cy + r + 30, x0:cx + r + 30]
    best = (-1.0, cx, cy, r)
    grad = cv2.magnitude(cv2.Sobel(roi, cv2.CV_32F, 1, 0), cv2.Sobel(roi, cv2.CV_32F, 0, 1))
    th = np.linspace(0, 2 * np.pi, 180, endpoint=False)
    for dx in range(-span, span + 1):
        for dy in range(-span, span + 1):
            for rr in range(r - 6, r + 7):
                xs = np.int32(cx + dx + rr * np.cos(th)) - x0
                ys = np.int32(cy + dy + rr * np.sin(th)) - y0
                v = float(np.median(grad[ys, xs]))
                if v > best[0]:
                    best = (v, cx + dx, cy + dy, rr)
    return best[1], best[2], best[3]

knobs = {
    "attack":  (170, 528, 72),
    "sustain": (438, 528, 72),
    "punch":   (705, 528, 72),
    "sat":     (966, 528, 72),
    "low":     (1233, 528, 72),
    "mix":     (1498, 528, 72),
    "output":  (1536, 795, 42),
}
geo = {k: refine(*v) for k, v in knobs.items()}

# Ring-Mitte (Skala und Bogen). Im Bild sitzen die Kappen leicht versetzt, im Plugin sitzt alles konzentrisch.
rings = {
    "attack":  (178.0, 529.0, 103.0),
    "sustain": (445.5, 530.0, 103.0),
    "punch":   (708.0, 530.0, 103.0),
    "sat":     (971.0, 530.0, 103.0),
    "low":     (1233.5, 529.5, 103.0),
    "mix":     (1496.5, 529.5, 103.0),
    "output":  (1529.0, 793.5, 61.0),
}

yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
res = im.astype(np.float32)

def ramp(a, lo, hi):
    return np.clip((a - lo) / max(hi - lo, 1e-3), 0, 1)

for name, (cx, cy, rc) in geo.items():
    rx, ry, ra = rings[name]
    d = np.hypot(xx - cx, yy - cy)

    # 1) Zeiger aus der Kappe: die gebürstete Kappe ist fast punktsymmetrisch, also von gegenüber ersetzen
    Rot = cv2.getRotationMatrix2D((float(cx), float(cy)), 180, 1.0)
    rot = cv2.warpAffine(im.astype(np.float32), Rot, (W, H), flags=cv2.INTER_LINEAR)
    src_ = im.astype(np.float32)
    g, grot = src_.mean(axis=2), rot.mean(axis=2)
    sat = src_.max(axis=2) - src_.min(axis=2)
    ptr = (sat > 40) & (d > rc * 0.12) & (d < rc * 0.965)
    if name == "output":
        # Zusätzlicher weißer Strich oben auf der Output-Kappe
        capAng = np.degrees(np.arctan2(xx - cx, -(yy - cy)))
        ptr |= (np.abs(capAng) < 9) & (d > rc * 0.4) & (d < rc * 0.95)
    ptr = cv2.dilate(ptr.astype(np.uint8), np.ones((7, 7), np.uint8))
    ptr = (cv2.GaussianBlur(ptr.astype(np.float32), (0, 0), 1.2) * (d < rc * 0.975))[..., None]
    cap = src_ * (1 - ptr) + rot * ptr
    capG = cap.mean(axis=2)
    capG = np.clip(capG * 1.1, 0, 255)
    capImg = np.stack([capG, capG, capG], axis=2)

    # 2) Kappe auf die Ring-Mitte schieben
    M = np.float32([[1, 0, rx - cx], [0, 1, ry - cy]])
    capShift = cv2.warpAffine(capImg, M, (W, H), flags=cv2.INTER_LINEAR)
    dr = np.hypot(xx - rx, yy - ry)
    ang = np.degrees(np.arctan2(xx - rx, -(yy - ry)))

    # 3) Schacht: dunkel, mit Rändel um die Kappe und einer schmalen Spur für den Leuchtbogen
    s = ra / 103.0
    well = 9.0 + 7.0 * (1 - ramp(dr, rc * 1.0, ra)) * 0.0 + 0 * dr
    knurl = (1 - ramp(dr, rc * 1.13, rc * 1.17)) * ramp(dr, rc * 0.98, rc * 1.0)
    ribs = 0.5 + 0.5 * np.cos(np.radians(ang) * (150 if name != "output" else 90))
    lightDir = 0.55 + 0.45 * np.cos(np.radians(ang + 40))
    well = well + knurl * (10 + 44 * ribs * lightDir)
    bandW = 7.0 * s
    band = ramp(dr, ra - bandW - 1, ra - bandW + 1) * (1 - ramp(dr, ra + bandW - 1, ra + bandW + 1)) * (np.abs(ang) < 146)
    well = well + band * 19
    wellImg = np.stack([well * 0.96, well, well * 0.97], axis=2)

    outerR = ra + 9.5 * s
    a = (1 - ramp(dr, outerR - 1.5, outerR + 2))[..., None]
    res = res * (1 - a) + wellImg * a

    capA = (1 - ramp(dr, rc - 0.8, rc + 0.8))[..., None]
    res = res * (1 - capA) + capShift * capA

res = np.clip(res, 0, 255)

# Leuchtbalken abdunkeln, Flächen merken
def darken_bar(x0, y0, x1, y1, keep=0.14):
    roi = res[y0:y1, x0:x1]
    sat = roi.max(axis=2) - roi.min(axis=2)
    m = cv2.GaussianBlur((sat > 28).astype(np.float32), (0, 0), 1.5)[..., None]
    g = roi.mean(axis=2, keepdims=True) * keep + 6
    res[y0:y1, x0:x1] = roi * (1 - m) + np.repeat(g, 3, axis=2) * m

darken_bar(128, 770, 345, 818)
darken_bar(596, 722, 1072, 762)

cv2.imwrite(f"{out}/kick_bg.jpg", np.uint8(res), [cv2.IMWRITE_JPEG_QUALITY, 93])

# Gehirn + Kabel als helle Ebene mit weicher Maske
x0, y0, x1, y1 = 560, 0, 1180, 360
crop = im[y0:y1, x0:x1].astype(np.float32)
hsv = cv2.cvtColor(im[y0:y1, x0:x1], cv2.COLOR_BGR2HSV).astype(np.float32)
h, s_, v = hsv[..., 0], hsv[..., 1], hsv[..., 2]
pink = ((h > 150) | (h < 4)) & (s_ > 90) & (v > 70)
pink = cv2.morphologyEx(pink.astype(np.uint8), cv2.MORPH_CLOSE, cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (25, 25)))
n, lab, stats, _ = cv2.connectedComponentsWithStats(pink)
big = np.argmax(stats[1:, cv2.CC_STAT_AREA]) + 1
brain = (lab == big).astype(np.uint8)
cnt, _ = cv2.findContours(brain, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
cv2.drawContours(brain, cnt, -1, 1, cv2.FILLED)
# Kabel: gesättigtes Gelbgrün nah am Gehirn, oberer Bereich
cable = ((h > 25) & (h < 50) & (s_ > 110) & (v > 90)).astype(np.uint8)
near = cv2.dilate(brain, cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (41, 41)))
cable = cable & near
cable[int(0.66 * (y1 - y0)):] = 0
aB = cv2.GaussianBlur(brain.astype(np.float32), (0, 0), 1.5)
aC = cv2.GaussianBlur(cable.astype(np.float32), (0, 0), 1.0)
alpha = np.clip(np.maximum(aB, aC), 0, 1)
lit = np.clip(crop * 1.32 + 12, 0, 255)
rgba = np.dstack([lit, alpha * 255])
cv2.imwrite(f"{out}/kick_brain.png", np.uint8(rgba), [cv2.IMWRITE_PNG_COMPRESSION, 9])
