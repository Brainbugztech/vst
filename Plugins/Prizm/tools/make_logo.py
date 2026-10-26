#!/usr/bin/env python3
"""Baut das Brain-Bugz-Logo fuer Prizm (Assets/prizm_logo.png) aus dem 3D-Kit.

Eingang: tools/logo_src/ (Schrift- und Highlight-Ebene, freigestellt, gleiche Zeichenflaeche)
Ausgang: Assets/prizm_logo.png
Look: Glasig-leuchtende Giftgruen-Schrift mit Kantenlicht, Glanz, dunkler Kontur und
weichem Glow, Blitze und Magenta-Akzente dezent darueber.
"""
import sys
from pathlib import Path

import numpy as np
from PIL import Image
from scipy import ndimage as ndi

HERE = Path(__file__).resolve().parent
SRC = HERE / "logo_src"
OUT = HERE.parent / "Assets" / "prizm_logo.png"
PAD = 70          # Rand fuer den Glow
OUT_W = 1280      # Breite der fertigen Datei


def rgb(h):
    return np.array([(h >> 16) & 255, (h >> 8) & 255, h & 255], np.float32) / 255.0


def blur(a, s):
    return ndi.gaussian_filter(a, s) if a.ndim == 2 else np.dstack([ndi.gaussian_filter(a[..., i], s) for i in range(a.shape[2])])


def over(dst, src_rgb, src_a):
    """Normales Alpha-Compositing (premultiplied dst: rgb, a)."""
    d_rgb, d_a = dst
    a = src_a[..., None]
    out_rgb = src_rgb * a + d_rgb * (1 - a)
    out_a = src_a + d_a * (1 - src_a)
    return out_rgb, out_a


def add_light(dst, col, a):
    """Additives Licht (Glow) auf Premultiplied-Ziel."""
    d_rgb, d_a = dst
    return d_rgb + col * a[..., None], np.clip(d_a + a * (1 - d_a), 0, 1)


def main():
    txt = Image.open(SRC / "BrainBugz_Schrift_Original_Alpha.png").convert("RGBA")
    hil = Image.open(SRC / "BrainBugz_Highlights_Original_Alpha.png").convert("RGBA")
    w, h = txt.size
    W, H = w + 2 * PAD, h + 2 * PAD

    def canvas(im):
        c = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        c.paste(im, (PAD, PAD))
        return np.asarray(c, np.float32) / 255.0

    T, Hl = canvas(txt), canvas(hil)
    a = T[..., 3]
    # Maske leicht glaetten, damit die Kanten ruhig wirken
    a = np.clip((blur(a, 0.8) - 0.5) * 2.2 + 0.5, 0, 1)

    # --- Flaeche: Verlauf mit der Originaltextur als feiner Struktur ---
    lum = T[..., :3] @ np.array([0.3, 0.59, 0.11], np.float32)
    lum_n = np.clip((lum - 0.45) / 0.5, 0, 1)
    yy = (np.arange(H, dtype=np.float32) - PAD) / h
    yy = np.clip(yy, 0, 1)[:, None] * np.ones((1, W), np.float32)
    top, mid, bot = rgb(0xe4ff5a), rgb(0x9cf21c), rgb(0x1fb01a)
    t = yy[..., None]
    grad = np.where(t < 0.45, top + (mid - top) * (t / 0.45), mid + (bot - mid) * ((t - 0.45) / 0.55))
    tex = (0.82 + 0.30 * lum_n)[..., None]
    face = np.clip(grad * tex, 0, 1)

    # --- Fase: Licht von oben links, Schatten unten rechts ---
    h1 = blur(a, 2.2)
    gy, gx = np.gradient(h1)
    shade = np.clip(-(gx * -0.7 + gy * -0.7) * 9.0, -1, 1)
    face = face + np.clip(shade, 0, 1)[..., None] * 0.55 * rgb(0xf4ffc0)
    face = face * (1 - np.clip(-shade, 0, 1)[..., None] * 0.45)

    # --- Glasglanz: weiche helle Zone im oberen Drittel jeder Buchstabenflaeche ---
    gloss = np.clip(1.0 - np.abs(yy - 0.24) / 0.22, 0, 1) ** 1.6
    face = face + (gloss * 0.22)[..., None] * rgb(0xeaffff)

    # --- Kantenlicht (duenner heller Rand innen) ---
    inner = np.clip(a - ndi.minimum_filter(a, size=5), 0, 1)
    inner = blur(inner, 0.7)
    face = face + inner[..., None] * 0.38 * rgb(0xf0ffc0)
    face = np.clip(face, 0, 1)

    # --- Aufbau: Glow -> dunkle Kontur -> Flaeche -> Licht -> Highlights ---
    dst = (np.zeros((H, W, 3), np.float32), np.zeros((H, W), np.float32))

    glow_green = rgb(0x6bff1a)
    glow_cyan = rgb(0x22e6ff)
    for s, k in ((34, 0.50), (16, 0.55), (7, 0.55)):
        dst = add_light(dst, glow_green, np.clip(blur(a, s) * k, 0, 1))
    dst = add_light(dst, glow_cyan, np.clip(blur(np.roll(a, 10, axis=0), 26) * 0.14, 0, 1))  # kuehler Schein von unten

    outline = np.clip(ndi.maximum_filter(a, size=5), 0, 1)
    outline = blur(outline, 0.8)
    dst = over(dst, rgb(0x03140a), np.clip(outline * 0.78, 0, 1))
    dst = over(dst, face, a)

    # Highlights: Blitze und Magenta, dezent und mit eigenem Schein
    ha = Hl[..., 3]
    hrgb = Hl[..., :3]
    dst = add_light(dst, rgb(0xff3bd4), np.clip(blur(ha * (hrgb[..., 0] > 0.7) * (hrgb[..., 1] < 0.6), 5) * 0.7, 0, 1))
    dst = over(dst, hrgb, np.clip(ha * 0.85, 0, 1))

    rgb_img, alpha = dst
    alpha = np.clip(alpha, 0, 1)
    straight = np.where(alpha[..., None] > 1e-4, rgb_img / np.maximum(alpha[..., None], 1e-4), 0)
    out = np.dstack([np.clip(straight, 0, 1), alpha])
    img = Image.fromarray((out * 255 + 0.5).astype(np.uint8), "RGBA")
    img = img.resize((OUT_W, round(OUT_W * H / W)), Image.LANCZOS)
    img.save(OUT, optimize=True)
    print(f"{OUT} {img.size}")


if __name__ == "__main__":
    sys.exit(main())
