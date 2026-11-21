# Erzeugt die dynamischen GUI-Bausteine aus Benetts freigegebenem GUI-Kit (BrainBugz Touch Me!).
# Aufruf: python make_assets.py <Pfad zum Kit-Ordner> <Ausgabeordner>
import sys, numpy as np
from PIL import Image, ImageFilter

kit, out = sys.argv[1], sys.argv[2]
def load(p, mode="RGB"): return np.asarray(Image.open(f"{kit}/{p}").convert(mode)).astype(np.float32)
def save(a, name, mode="RGB"): Image.fromarray(np.clip(a, 0, 255).astype(np.uint8), mode).save(f"{out}/{name}")

bg = load("01_PIXELGETREUE_BASIS/Background_Full_1536x1024.png")
save(bg, "bg_full.png")

# ---------- XY-Pad ohne Fadenkreuz ----------
pad = load("04_NUTZBARE_DISPLAY_UNTERGRUENDE/XY_Pad_Without_Cursor_604x499.png")
orig = pad.copy()
VX, HY = 371, 285
clean = pad.copy()
w = 11
def smooth(v, k=15):
    ker = np.ones(k) / k
    return np.stack([np.convolve(np.pad(v[:, c], k, mode='edge'), ker, 'same')[k:-k] for c in range(v.shape[1])], axis=1)
# senkrechte Linie durch Rasterlinie bei 328 ersetzen, Helligkeit zeilenweise an die Umgebung angleichen
src = pad[:, VX-43-w:VX-43+w+1]
nb_t = np.concatenate([pad[:, VX-w-4:VX-w-1], pad[:, VX+w+2:VX+w+5]], axis=1).mean(axis=1)
nb_s = np.concatenate([pad[:, VX-43-w-4:VX-43-w-1], pad[:, VX-43+w+2:VX-43+w+5]], axis=1).mean(axis=1)
gain = smooth(nb_t / np.maximum(nb_s, 1))
clean[:, VX-w:VX+w+1] = src * gain[:, None, :]
tmp = clean.copy()
# waagerechte Linie durch Rasterlinie bei 244 ersetzen, spaltenweise angleichen
src = tmp[HY-41-w:HY-41+w+1, :]
nb_t = np.concatenate([tmp[HY-w-4:HY-w-1], tmp[HY+w+2:HY+w+5]], axis=0).mean(axis=0)
nb_s = np.concatenate([tmp[HY-41-w-4:HY-41-w-1], tmp[HY-41+w+2:HY-41+w+5]], axis=0).mean(axis=0)
gain = smooth(nb_t / np.maximum(nb_s, 1))
clean[HY-w:HY+w+1, :] = src * gain[None, :, :]
# Glow-Fleck in der Mitte (Radius ~72) durch saubere Rasterkreuzungen ersetzen, Helligkeit angeglichen
cx, cy, R = 371, 286, 86
yy, xx = np.mgrid[-R:R, -R:R]
rr = np.sqrt(xx**2 + yy**2)
srcs = [clean[sy-R:sy+R, sx-R:sx+R] for sx, sy in [(191, 115), (145, 286), (191, 459 - 2*R + 84)]]
srcs = [a for a in srcs if a.shape[:2] == (2*R, 2*R)]
src = np.mean(srcs, axis=0)
ring = (rr > 76) & (rr < 86) & (np.abs(xx) > 10) & (np.abs(yy) > 10)
tgt = clean[cy-R:cy+R, cx-R:cx+R]
gain = np.median(tgt[ring], axis=0) / np.maximum(np.median(src[ring], axis=0), 1)
src = src * gain
m = np.clip((84 - rr) / 10.0, 0, 1)[..., None]
clean[cy-R:cy+R, cx-R:cx+R] = tgt * (1-m) + src * m
save(clean, "xy_pad_clean.png")

# Linienprofil (Alpha über Cyan) aus dem Original messen
def profile(diffband, base):
    C = np.array([150, 240, 255], np.float32)
    a = ((diffband - base) / np.maximum(C - base, 1)).mean(axis=2)
    return np.clip(a, 0, 1)
rowsel = np.r_[20:220, 360:480]
vb = orig[rowsel, VX-w:VX+w+1]; vc = clean[rowsel, VX-w:VX+w+1]
va = profile(vb, vc).mean(axis=0)
colsel = np.r_[20:300, 450:590]
hb = orig[HY-w:HY+w+1][:, colsel]; hc = clean[HY-w:HY+w+1][:, colsel]
ha = profile(hb.transpose(1,0,2), hc.transpose(1,0,2)).mean(axis=0)
print("v-profil", np.round(va, 2)); print("h-profil", np.round(ha, 2))
def strip(alpha, vertical):
    n = len(alpha)
    img = np.zeros((1, n, 4) if vertical else (n, 1, 4), np.float32)
    for i, a in enumerate(alpha):
        px = [150, 240, 255, 255 * a]
        if vertical: img[0, i] = px
        else: img[i, 0] = px
    return img
# Linie direkt nach den gemessenen Original-Pixeln (Kern, Nachbarn, dunkler Saum, leichter Glow)
LINE = [(0, 200, 230, 0.05), (0, 200, 230, 0.07), (0, 22, 34, 0.35), (11, 84, 100, 0.85),
        (50, 218, 236, 1.0), (26, 148, 170, 0.9), (0, 24, 34, 0.35), (0, 200, 230, 0.07), (0, 200, 230, 0.05)]
def line_img(vertical):
    n = len(LINE); img = np.zeros((1, n, 4) if vertical else (n, 1, 4), np.float32)
    for i, (r, g_, b_, a_) in enumerate(LINE):
        if vertical: img[0, i] = (r, g_, b_, 255 * a_)
        else: img[i, 0] = (r, g_, b_, 255 * a_)
    return img
save(line_img(True), "xy_line_v.png", "RGBA")
save(line_img(False), "xy_line_h.png", "RGBA")

# ---------- Hilfen ----------
def crop(a, l, t, r, b): return a[t:b, l:r].copy()
def row_median_inpaint(img, l, t, r, b, mask_thr=None):
    """Text in einer Fläche entfernen: helle Pixel durch den Zeilen-Median der dunklen Pixel ersetzen."""
    a = img.copy()
    reg = a[t:b, l:r]
    lum = reg.mean(axis=2)
    for y in range(reg.shape[0]):
        row = reg[y]; dark = lum[y] < (mask_thr if mask_thr else np.percentile(lum[y], 40) + 12)
        if dark.sum() < 3: continue
        med = np.median(row[dark], axis=0)
        bright = ~dark
        # Ränder des Textes weich mitnehmen
        bright = np.convolve(bright.astype(float), np.ones(5), 'same') > 0
        row[bright] = med
    return a

# ---------- FX-Buttons (Text entfernen, auf 36 px Höhe bringen) ----------
def blank_button(l, t, r, b, text_l, text_r, plain_l, plain_r):
    a = crop(bg, l, t, r, b)
    h = a.shape[0]
    tl, tr = text_l - l, text_r - l
    pl, pr = plain_l - l, plain_r - l
    pw = pr - pl
    for y in range(8, h - 8):
        for x in range(tl, tr):
            a[y, x] = a[y, pl + (x - tl) % pw]
    return a
def squash(a, H, keep=13):
    h = a.shape[0]
    top, bot = a[:keep], a[h-keep:]
    mid_src = a[keep:h-keep]
    n = H - 2*keep
    idx = np.linspace(0, mid_src.shape[0]-1, n).round().astype(int)
    return np.concatenate([top, mid_src[idx], bot], axis=0)
BTN_H = 33
btn_norm = blank_button(35, 336, 290, 410, 58, 202, 205, 262)
btn_act  = blank_button(35, 254, 290, 334, 58, 226, 228, 262)
def shrink(a, H):  # ganze Taste gleichmäßig in der Höhe verkleinern (Rahmen bleibt proportional)
    return np.asarray(Image.fromarray(np.clip(a, 0, 255).astype(np.uint8)).resize((a.shape[1], H), Image.LANCZOS)).astype(np.float32)
save(shrink(btn_norm, BTN_H), "btn_normal.png")
save(shrink(btn_act, BTN_H), "btn_active.png")
save(btn_norm, "btn_normal_full.png")

# Panel-Fläche hinter den Buttons (gebürstetes Metall, Streifen aus dem Bereich unter den Buttons gespiegelt gekachelt)
PL, PT, PR, PB = 30, 248, 296, 745
strip_ = crop(bg, PL, 722, PR, 742)
rows = []
for i in range(PB - PT):
    k = i % (2 * strip_.shape[0])
    rows.append(strip_[k if k < strip_.shape[0] else 2*strip_.shape[0]-1-k])
panel = np.stack(rows, axis=0)
save(panel, "fx_panel_fill.png")

# ---------- Preset-Liste ----------
lst = load("04_NUTZBARE_DISPLAY_UNTERGRUENDE/Preset_List_Blank.png")
lst[10:50, 4:24] = lst[50:90, 4:24]                       # cyan Rest der Markierung entfernen
thumb = crop(lst, 436, 56, 469, 140)
save(thumb, "scroll_thumb.png")
lst[54:144, 436:469] = lst[180:181, 436:469]               # Schiebereglerbahn frei machen
save(lst, "preset_list_blank.png")

# ---------- Kategorie-Auswahl ohne Text ----------
def column_copy(a, l, t, r, b, sl, sr):
    a = a.copy(); w = sr - sl
    for x in range(l, r): a[t:b, x] = a[t:b, sl + (x - l) % w]
    return a
cat = crop(bg, 1028, 236, 1504, 290)
cat = column_copy(cat, 1052 - 1028, 248 - 236, 1272 - 1028, 280 - 236, 1290 - 1028, 1400 - 1028)
save(cat, "category_blank.png")

# ---------- Dry/Wet: Prozentanzeige leer, Bahn leer/voll ----------
pct = crop(bg, 860, 906, 962, 956)
pct = column_copy(pct, 880 - 860, 913 - 906, 952 - 860, 948 - 906, 867 - 860, 875 - 860)
save(pct, "pct_blank.png")
trk = crop(bg, 478, 916, 853, 956)
TW = trk.shape[1]
thumb_l, thumb_r = 715 - 478, 765 - 478
cyan_col = trk[:, 150:151]; dark_col = trk[:, 320:321]
full = trk.copy(); empty = trk.copy()
capw = 14
for x in range(capw, TW - capw):
    full[:, x] = cyan_col[:, 0]; empty[:, x] = dark_col[:, 0]
full[:, TW-capw:] = trk[:, :capw][:, ::-1]                  # rechte Kappe: gespiegelte linke (cyan)
empty[:, :capw] = trk[:, TW-capw:][:, ::-1]                 # linke Kappe: gespiegelte rechte (dunkel)
save(full, "track_full.png"); save(empty, "track_empty.png")
save(load("03_TRANSPARENTE_SPRITES/controls/Dry_Wet_Slider_Thumb.png", "RGBA"), "slider_thumb.png", "RGBA")
save(load("03_TRANSPARENTE_SPRITES/controls/XY_Touch_Cursor_Extracted.png", "RGBA"), "xy_cursor.png", "RGBA")

# ---------- weitere leere Displays 1:1 übernehmen ----------
for src, dst in [("Preset_Top_Display_Blank.png", "top_display_blank.png"),
                 ("Waveform_Lcd_Blank.png", "wave_lcd_blank.png")]:
    save(load("04_NUTZBARE_DISPLAY_UNTERGRUENDE/" + src), dst)
# X/Y-LCDs: Beschriftungszeile (grau) und Wertefeld getrennt freistellen, Trennlinie bleibt erhalten
xl = crop(bg, 339, 748, 648, 864)
xl = column_copy(xl, 20, 17, 200, 46, 230, 290)
xl = column_copy(xl, 46, 54, 294, 110, 14, 44)
save(xl, "x_lcd_blank.png")
yl = crop(bg, 659, 748, 969, 864)
yl = column_copy(yl, 20, 17, 235, 46, 240, 296)
yl = column_copy(yl, 46, 54, 294, 110, 14, 44)
save(yl, "y_lcd_blank.png")
# Dry/Wet-Feld ohne eingebackenen Regler-Knopf (der wird live gezeichnet)
dw = crop(bg, 470, 898, 862, 968)
dw = column_copy(dw, 708 - 470, 0, 772 - 470, 70, 630 - 470, 694 - 470)
save(dw, "drywet_clean.png")
print("fertig")
