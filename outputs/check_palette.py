#!/usr/bin/env python3
"""Report dominant colours of a UI snapshot by region."""

import sys
from collections import Counter
from pathlib import Path

from PIL import Image


def dominant(crop, n=5):
    crop.thumbnail((120, 120))
    cnt = Counter((p[0] // 12 * 12, p[1] // 12 * 12, p[2] // 12 * 12) for p in list(crop.getdata()))
    total = sum(cnt.values())
    return [("#%02x%02x%02x" % c, round(v / total * 100, 1)) for c, v in cnt.most_common(n)]


def main(path: str) -> int:
    im = Image.open(path).convert("RGB")
    w, h = im.size
    print("size", im.size)
    regions = {
        "header": (0, 0, w, h // 8),
        "panel": (0, h // 8, w, h // 2),
        "keys": (0, int(h * 0.8), w, h),
    }
    for name, box in regions.items():
        print(name, dominant(im.crop(box)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
