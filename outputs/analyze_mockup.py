#!/usr/bin/env python3
"""Extract UI structure from the Blue Dream mockup.

Outputs:
  - OCR text boxes (label inventory with positions)
  - Hough circle detections (knob/LED inventory)
  - row-band mean colours (section boundaries)
"""

import sys

import cv2
import numpy as np


def main(path: str) -> int:
    img = cv2.imread(path)
    h, w = img.shape[:2]
    print(f"== {path} {w}x{h}")

    # --- OCR at 2x for small legends ---
    up = cv2.resize(img, (w * 2, h * 2), interpolation=cv2.INTER_CUBIC)
    gray = cv2.cvtColor(up, cv2.COLOR_BGR2GRAY)
    gray = cv2.convertScaleAbs(gray, alpha=1.6, beta=-40)
    data = cv2.cvtColor(cv2.cvtColor(gray, cv2.COLOR_GRAY2BGR), cv2.COLOR_BGR2GRAY)
    import subprocess, tempfile, os
    tmp = tempfile.NamedTemporaryFile(suffix=".png", delete=False)
    cv2.imwrite(tmp.name, data)
    tsv = subprocess.run(
        ["tesseract", tmp.name, "stdout", "--psm", "11", "tsv"],
        capture_output=True, text=True).stdout
    os.unlink(tmp.name)
    print("== OCR (conf>55, text len>=2) x,y are full-res mockup coords")
    print("x\ty\tw\th\tconf\ttext")
    for line in tsv.splitlines()[1:]:
        parts = line.split("\t")
        if len(parts) != 12:
            continue
        try:
            conf = float(parts[10])
        except ValueError:
            continue
        text = parts[11].strip()
        if conf < 55 or len(text) < 2:
            continue
        x, y, ww, hh = (int(round(int(p) / 2)) for p in parts[6:10])
        print(f"{x}\t{y}\t{ww}\t{hh}\t{conf:.0f}\t{text}")

    # --- circles: knobs are 25-70px radius at this 1492px width ---
    g2 = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
    g2 = cv2.medianBlur(g2, 5)
    circles = cv2.HoughCircles(g2, cv2.HOUGH_GRADIENT, dp=1.2, minDist=28,
                               param1=110, param2=42, minRadius=14, maxRadius=80)
    print("== circles (x,y,r)")
    if circles is not None:
        pts = sorted(circles[0].tolist(), key=lambda c: (round(c[1] / 40), c[0]))
        print(len(pts), "circles")
        for x, y, r in pts:
            print(f"{x:.0f}\t{y:.0f}\t{r:.0f}")
    else:
        print("none")

    # --- row bands: 20px mean colour to reveal section boundaries ---
    print("== row bands y, meanBGR")
    for y in range(0, h - 20, 20):
        band = img[y:y + 20].reshape(-1, 3).mean(axis=0).astype(int)
        print(f"{y}\t{band[0]}\t{band[1]}\t{band[2]}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
