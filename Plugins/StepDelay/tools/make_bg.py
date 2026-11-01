# Erzeugt den Hintergrund aus Benetts Vorlage: alle beweglichen Teile (Balken, Texte mit Werten,
# Regler-Ringe) werden entfernt, damit das Plugin sie live darüber zeichnen kann.
# Aufruf: python make_bg.py vorlage.png ../Assets/stepdelay_bg.jpg
import sys
import cv2
import numpy as np

src, dst = sys.argv[1], sys.argv[2]
img = cv2.imread(src)
h, w = img.shape[:2]
assert (w, h) == (1774, 887), "Vorlage hat sich geändert, Koordinaten prüfen"

mask = np.zeros((h, w), np.uint8)

def rect(x0, y0, x1, y1):
    cv2.rectangle(mask, (x0, y0), (x1, y1), 255, -1)

# Texte mit wechselndem Inhalt
rect(122, 140, 335, 176)     # "16 STEP PATTERN"
rect(1522, 140, 1642, 172)   # "1/16 GRID"
rect(122, 492, 342, 528)     # "PRESET 01"
knobs = [268, 523, 772, 1020, 1270, 1525]
for cx in knobs:
    rect(cx - 44, 797, cx + 44, 829)   # Werte unter den Reglern

clean = cv2.inpaint(img, mask, 6, cv2.INPAINT_TELEA)

# Balkenfeld: leer, dunkel (die Spalten zeichnet das Plugin)
field = (46, 178, 1738, 438)
clean[field[1]:field[3], field[0]:field[2]] = (27, 10, 18)  # BGR, dunkles Lila

# Regler: Ring und Knopf raus, Skalen-Striche außen bleiben
for cx in knobs:
    cv2.circle(clean, (cx, 712), 85, (20, 7, 13), -1, lineType=cv2.LINE_AA)
    # Beschriftung über dem Regler wiederherstellen
    clean[600:629, cx - 70:cx + 70] = img[600:629, cx - 70:cx + 70]

cv2.imwrite(dst, clean, [cv2.IMWRITE_JPEG_QUALITY, 90])
print("ok", dst)
