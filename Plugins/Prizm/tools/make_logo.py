#!/usr/bin/env python3
"""Baut das Brain-Bugz-Logo fuer Prizm (Assets/prizm_logo.png).

Die Schrift kommt als Vektor-Umriss aus dem Brain-Bugz-3D-Kit (tools/logo_shapes.json,
ruhige Kanten). Daraus wird eine leicht gekippte Glas-Extrusion mit Tiefe gerechnet,
ohne Hintergrund: Seitenwaende mit Tiefenverlauf, Rueckseite schimmert durch die
halbtransparente Frontflaeche, Glanz, Prisma-Rand (kalt/warm) und dezenter Schein.
Aufruf: python3 tools/make_logo.py
"""
import json
import math
import os
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage as ndi

HERE = Path(__file__).resolve().parent
OUT = HERE.parent / "Assets" / "prizm_logo.png"

OUT_W = int(os.environ.get("PRIZM_LOGO_W", 1280))   # Breite der fertigen Datei
SS = int(os.environ.get("PRIZM_LOGO_SS", 3))         # Supersampling
UNIT = SS * OUT_W / 1280.0                           # Pixel-Einheit, damit der Look bei jeder Groesse gleich bleibt
YAW, PITCH = -16.0, 9.0   # leichte Schraegsicht (Grad)
DEPTH = 0.80        # Tiefe der Extrusion (Einheiten = Schriftkoordinaten / 1000)
CAM = 13.0          # Kameraabstand (Perspektive)
MARGIN = 0.07       # Rand fuer den Schein


def hexc(h):
    return np.array([(h >> 16) & 255, (h >> 8) & 255, h & 255], np.float32) / 255.0


def lerp(a, b, t):
    return a + (b - a) * t


def ramp(stops, t):
    """Farbverlauf: stops = [(pos, rgb)], t = Array 0..1 -> Array (...,3)."""
    t = np.clip(t, 0, 1)
    out = np.zeros(t.shape + (3,), np.float32)
    for (p0, c0), (p1, c1) in zip(stops[:-1], stops[1:]):
        m = (t >= p0) & (t <= p1)
        f = ((t - p0) / max(p1 - p0, 1e-6))[..., None]
        out = np.where(m[..., None], c0 + (c1 - c0) * f, out)
    return out


# ------------------------------------------------------------------ Geometrie
def load_shapes():
    d = json.loads((HERE / "logo_shapes.json").read_text())

    def conv(lst):
        res = []
        for shape in lst:
            rings = []
            for i, r in enumerate(shape):
                pts = np.array(r, np.float32).reshape(-1, 2) / 1000.0
                area = 0.5 * np.sum(pts[:, 0] * np.roll(pts[:, 1], -1) - np.roll(pts[:, 0], -1) * pts[:, 1])
                if (i == 0) != (area > 0):   # aussen gegen den Uhrzeigersinn, Loecher im Uhrzeigersinn
                    pts = pts[::-1]
                rings.append(pts)
            res.append(rings)
        return res

    return conv(d["letters"]), conv(d["dot"])


def rot():
    ya, pi = math.radians(YAW), math.radians(PITCH)
    ry = np.array([[math.cos(ya), 0, math.sin(ya)], [0, 1, 0], [-math.sin(ya), 0, math.cos(ya)]], np.float32)
    rx = np.array([[1, 0, 0], [0, math.cos(pi), -math.sin(pi)], [0, math.sin(pi), math.cos(pi)]], np.float32)
    return rx @ ry


class Cam:
    def __init__(self, shapes, scale, offset):
        self.R = rot()
        self.scale, self.offset = scale, offset

    def project(self, p):
        """p: (N,3) Weltpunkte -> (N,2) Pixel, (N,) Tiefe."""
        q = p @ self.R.T
        depth = CAM - q[:, 2]
        f = CAM
        x = q[:, 0] * f / depth
        y = q[:, 1] * f / depth
        return np.stack([x * self.scale + self.offset[0], -y * self.scale + self.offset[1]], 1), depth


def fit_camera(all_shapes):
    R = rot()
    pts = []
    for shapes in all_shapes:
        for rings in shapes:
            for r in rings:
                for z in (-DEPTH / 2, DEPTH / 2):
                    pts.append(np.column_stack([r, np.full(len(r), z, np.float32)]))
    pts = np.vstack(pts)
    cx, cy = (pts[:, 0].min() + pts[:, 0].max()) / 2, (pts[:, 1].min() + pts[:, 1].max()) / 2
    pts = pts - np.array([cx, cy, 0], np.float32)
    q = pts @ R.T
    depth = CAM - q[:, 2]
    px, py = q[:, 0] * CAM / depth, q[:, 1] * CAM / depth
    w, h = px.max() - px.min(), py.max() - py.min()
    W = OUT_W * SS
    scale = W * (1 - 2 * MARGIN) / w
    H = int(h * scale + 2 * MARGIN * W)
    mx, my = (px.max() + px.min()) / 2, (py.max() + py.min()) / 2
    cam = Cam(None, scale, (W / 2 - mx * scale, H / 2 + my * scale))
    cam.center = np.array([cx, cy, 0], np.float32)
    return cam, (W, H)


# ------------------------------------------------------------------ Zeichnen
LIGHT = np.array([-0.45, 0.75, 0.55], np.float32)
LIGHT /= np.linalg.norm(LIGHT)


def draw_walls(cam, shapes, size, pal, want_front):
    """Seitenwaende zeichnen. want_front=True: zur Kamera gewandte, False: abgewandte."""
    W, H = size
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    dr = ImageDraw.Draw(img)
    cam_pos = np.array([0, 0, CAM], np.float32)
    quads = []
    for rings in shapes:
        for r in rings:
            n = len(r)
            for i in range(n):
                a, b = r[i], r[(i + 1) % n]
                e = b - a
                ln = float(np.hypot(*e))
                if ln < 1e-5:
                    continue
                nrm = np.array([e[1], -e[0], 0], np.float32) / ln      # nach aussen
                mid = np.array([(a[0] + b[0]) / 2, (a[1] + b[1]) / 2, 0], np.float32) - cam.center
                nr = cam.R @ nrm
                midr = cam.R @ mid
                vis = float(nr @ (cam_pos - midr)) > 0
                if vis != want_front:
                    continue
                quads.append((a, b, nrm, nr, float((cam.R @ mid)[2])))
    quads.sort(key=lambda q: q[4])      # fern -> nah
    strips = 28
    for a, b, nrm, nr, _ in quads:
        lam = float(np.clip(nr @ LIGHT, -1, 1))
        up = float(nrm[1])
        for s in range(strips):
            t0, t1 = s / strips, (s + 1) / strips          # 0 = vorne, 1 = hinten
            z0, z1 = DEPTH / 2 - t0 * DEPTH, DEPTH / 2 - t1 * DEPTH
            p = np.array([[a[0], a[1], z0], [b[0], b[1], z0], [b[0], b[1], z1], [a[0], a[1], z1]], np.float32) - cam.center
            xy, _ = cam.project(p)
            tm = (t0 + t1) / 2
            base = ramp(pal["wall"], np.array(tm ** 0.8))
            col = base * (0.55 + 0.55 * max(lam, 0.0) + 0.25 * lam)
            col = col + pal["wall_up"] * max(up, 0) * 0.35 * (1 - tm) + pal["wall_dn"] * max(-up, 0) * 0.45 * (1 - tm)
            if s == 0:
                col = col + pal["edge"] * 0.55                 # helles Kantenlicht vorne
            if not want_front:
                col = col * 0.8
            col = np.clip(col, 0, 1)
            dr.polygon([tuple(v) for v in xy], fill=tuple(int(c * 255) for c in col) + (255,))
    return img


def cap_mask(cam, shapes, size, z):
    W, H = size
    m = Image.new("L", (W, H), 0)
    dr = ImageDraw.Draw(m)
    for rings in shapes:
        for i, r in enumerate(rings):
            p = np.column_stack([r, np.full(len(r), z, np.float32)]) - cam.center
            xy, _ = cam.project(p)
            dr.polygon([tuple(v) for v in xy], fill=255 if i == 0 else 0)
    return np.asarray(m, np.float32) / 255.0


def front_cap(mask, pal):
    """Glasflaeche: Verlauf, Randlicht (Prisma kalt/warm), Glanz je Buchstabe."""
    H, W = mask.shape
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    bin_ = mask > 0.5
    ys = np.where(bin_.any(1))[0]
    v = np.clip((yy - ys.min()) / max(ys.max() - ys.min(), 1), 0, 1)
    u = np.clip((xx - np.where(bin_.any(0))[0].min()) / max(np.ptp(np.where(bin_.any(0))[0]), 1), 0, 1)
    vd = np.clip(v + (u - 0.5) * 0.12, 0, 1)                     # leicht diagonal
    col = ramp(pal["face"], vd)

    dist = ndi.distance_transform_edt(bin_).astype(np.float32)
    px = UNIT * 1.0
    rim = np.exp(-dist / (px * 2.2))
    rim2 = np.exp(-dist / (px * 7.0))

    # Randrichtung: zur Lichtseite kalt (cyan), abgewandt warm (magenta)
    gy, gx = np.gradient(ndi.gaussian_filter(dist, px * 1.2))
    facing = np.clip((-gx * LIGHT[0] + gy * LIGHT[1]) / 0.6 * -1 + 0.5, 0, 1)   # 1 = lichtzugewandt
    rim_col = pal["rim_cold"] * facing[..., None] + pal["rim_warm"] * (1 - facing)[..., None]
    col = col * (0.80 + 0.20 * (1 - rim2))[..., None]            # Glasdicke: Mitte minimal dunkler
    col = col + rim_col * (rim * 0.85)[..., None] + pal["edge"] * (rim * 0.25)[..., None]
    col = col + pal["rim_cold"] * (rim2 * 0.10)[..., None]

    # Glanz: je Buchstabe weicher Schimmer oben + zwei schraege Lichtstreifen
    lab, nlab = ndi.label(bin_)
    gloss = np.zeros_like(v)
    if nlab:
        objs = ndi.find_objects(lab)
        for i, sl in enumerate(objs, 1):
            if sl is None:
                continue
            sub = (lab[sl] == i)
            hh = sl[0].stop - sl[0].start
            ly = (yy[sl] - sl[0].start) / max(hh, 1)
            g = np.clip(1.0 - ly / 0.55, 0, 1) ** 1.4
            gloss[sl] = np.maximum(gloss[sl], g * sub)
    col = col + gloss[..., None] * 0.20 * pal["white"]
    diag = (xx * 0.84 + yy * 0.54) / (UNIT * 100.0)
    band = lambda c, w: np.clip(1 - np.abs(diag - c) / w, 0, 1) ** 2
    sheen = band(np.mean(diag[bin_]) - 1.9, 0.22) * 0.9 + band(np.mean(diag[bin_]) - 0.3, 0.10) * 0.55
    col = col + (sheen * 0.16)[..., None] * pal["white"]
    # feine Innenkante wie bei geschliffenem Glas
    lip = np.exp(-(((dist - px * 11.0) / (px * 1.3)) ** 2)) * bin_
    col = col + lip[..., None] * (0.055 * pal["rim_cold"] + 0.025 * pal["white"])

    alpha = mask * (0.70 + 0.30 * np.clip(rim * 1.4 + lip * 0.5, 0, 1))
    return np.clip(col, 0, 1), alpha


def over(dst_rgb, dst_a, src_rgb, src_a):
    a = src_a[..., None]
    out_a = src_a + dst_a * (1 - src_a)
    out_rgb = (src_rgb * a + dst_rgb * dst_a[..., None] * (1 - a)) / np.maximum(out_a[..., None], 1e-6)
    return out_rgb, out_a


PAL_GLASS = dict(
    face=[(0.0, hexc(0xc9f1ff)), (0.28, hexc(0x62cbff)), (0.62, hexc(0x5a6cff)), (1.0, hexc(0xa046ff))],
    wall=[(0.0, hexc(0x62e8ff)), (0.45, hexc(0x3a7ae0)), (1.0, hexc(0x2a1a78))],
    wall_up=hexc(0xbff6ff), wall_dn=hexc(0xc24bff), edge=hexc(0xa8f4ff),
    rim_cold=hexc(0x8ff2ff), rim_warm=hexc(0xff7ae6), white=hexc(0xffffff))
PAL_DOT = dict(
    face=[(0.0, hexc(0xfff3d0)), (0.5, hexc(0xffb25c)), (1.0, hexc(0xff4fb0))],
    wall=[(0.0, hexc(0xffc470)), (0.5, hexc(0xe0508a)), (1.0, hexc(0x5a1a60))],
    wall_up=hexc(0xffe9b8), wall_dn=hexc(0xff4fd0), edge=hexc(0xffe2a0),
    rim_cold=hexc(0xfff0b0), rim_warm=hexc(0xff6ad0), white=hexc(0xffffff))


def render_part(cam, shapes, size, pal):
    """Gibt (rgb, alpha) eines Teils zurueck (Rueckseite -> sichtbare Waende -> Glasfront)."""
    W, H = size
    back_walls = np.asarray(draw_walls(cam, shapes, size, pal, False), np.float32) / 255.0
    side_walls = np.asarray(draw_walls(cam, shapes, size, pal, True), np.float32) / 255.0
    back_cap = cap_mask(cam, shapes, size, -DEPTH / 2)
    front = cap_mask(cam, shapes, size, DEPTH / 2)

    rgb = np.zeros((H, W, 3), np.float32)
    a = np.zeros((H, W), np.float32)
    # Rueckseite (Waende innen + Rueckflaeche) schimmert gedaempft durch
    back_rgb = back_walls[..., :3]
    back_a = back_walls[..., 3] * 0.70
    rgb, a = over(rgb, a, ramp(pal["wall"], np.full((H, W), 0.9, np.float32)) * 0.7, back_cap * 0.35)
    rgb, a = over(rgb, a, back_rgb, back_a)
    rgb, a = over(rgb, a, side_walls[..., :3], side_walls[..., 3])
    f_rgb, f_a = front_cap(front, pal)
    rgb, a = over(rgb, a, f_rgb, f_a)
    return rgb, a


def main():
    letters, dot = load_shapes()
    cam, size = fit_camera([letters, dot])
    W, H = size
    rgb, a = render_part(cam, letters, size, PAL_GLASS)
    d_rgb, d_a = render_part(cam, dot, size, PAL_DOT)
    rgb, a = over(rgb, a, d_rgb, d_a)

    # dezenter Schein: kuehles Cyan/Violett, kein Gruen, keine Splitter
    glow = np.zeros((H, W, 3), np.float32)
    for s, k, c in ((UNIT * 22, 0.42, hexc(0x5fd0ff)), (UNIT * 9, 0.40, hexc(0x8f7bff))):
        g = ndi.gaussian_filter(a, s) * k
        glow += c * g[..., None]
    glow_a = np.clip(ndi.gaussian_filter(a, UNIT * 14) * 0.55, 0, 0.5)
    out_rgb, out_a = over(np.clip(glow / np.maximum(glow.max(), 1e-6) * 1.0, 0, 1) * 0 + hexc(0x6fb8ff) * 0.9 + 0 * glow, glow_a * 0.55,
                          rgb, a)

    img = np.dstack([np.clip(out_rgb, 0, 1), np.clip(out_a, 0, 1)])
    im = Image.fromarray((img * 255 + 0.5).astype(np.uint8), "RGBA")
    im = im.resize((OUT_W, round(OUT_W * H / W)), Image.LANCZOS)
    out = Path(os.environ.get("PRIZM_LOGO_OUT", OUT))
    im.save(out, optimize=True)
    print(f"{out} {im.size}")


if __name__ == "__main__":
    main()
