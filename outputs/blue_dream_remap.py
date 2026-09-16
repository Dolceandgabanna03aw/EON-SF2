#!/usr/bin/env python3
"""Apply the approved Blue Dream palette to the Aoi YUME editor sources.

Every mapped literal is a measured value from
outputs/aoi_yume_blue_dream_hero_refined_20260906.png: deep navy/teal
chassis, steel-blue accents, warm cream keys, amber/red signal colours
kept for warning semantics.  The map is exact-string replacement so the
script is idempotent and auditable.
"""

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FILES = [
    ROOT / "plugins/rompler/Source/PluginEditor.h",
    ROOT / "plugins/rompler/Source/PluginEditor.cpp",
    ROOT / "plugins/rompler/Source/PresetBrowserOverlay.cpp",
]

MAP = {
    # theme header
    "0xff1b2422": "0xff171e26",
    "0xff37423e": "0xff333f4a",
    "0xff8e9a8e": "0xff88929e",
    "0xff3a4742": "0xff35414d",
    "0xff748178": "0xff6f7d8a",
    "0xffd0d8ca": "0xffcdd6da",
    "0xff69d6b4": "0xff5ad2e6",
    "0xff2f977e": "0xff1f7fa8",
    "0xffbcf1d6": "0xffc9ecf6",
    "0xffabb2a6": "0xffa4adb4",
    "0xff1b2622": "0xff182129",
    "0xff26312d": "0xff232e38",
    "0xff718279": "0xff6c7c88",
    "0xffa0afa4": "0xff9aa6b0",
    "0xff72dfbc": "0xff6cd4ee",
    "0xff121918": "0xff101722",
    "0xff071716": "0xff071320",
    "0xff1d4038": "0xff1d3c50",
    "0xff9be9ce": "0xff9adff2",
    # editor cpp materials
    "0xffb8bdb3": "0xffb4bac0",
    "0xff3c4741": "0xff3a434b",
    "0xff18211e": "0xff161e26",
    "0xff25302c": "0xff232c35",
    "0xffb9c1b4": "0xffb2bcc4",
    "0xff17201e": "0xff151e27",
    "0xff26332f": "0xff24303b",
    "0xff58675f": "0xff546472",
    "0xff6e7d74": "0xff6a7a88",
    "0xff101716": "0xff0f151d",
    "0xff65c7aa": "0xff5fc2e2",
    "0xff246b59": "0xff22688c",
    "0xff2e7865": "0xff2b749c",
    "0xff0c2d25": "0xff0b2c40",
    "0xff2a302e": "0xff28313a",
    "0xff0b1010": "0xff0a0f14",
    "0xff949c92": "0xff8f99a2",
    "0xff46514b": "0xff424e59",
    "0xff455049": "0xff414d58",
    "0xff080d0d": "0xff070c11",
    "0xff465049": "0xff424e58",
    "0xff080c0c": "0xff070b10",
    "0xff111716": "0xff10161d",
    "0xffcce5d9": "0xffc9e6f2",
    "0xff9ca39a": "0xff97a1aa",
    "0xff9da39a": "0xff98a2ab",
    "0xff3f8f7c": "0xff3b87b0",
    "0xff0d332a": "0xff0c3148",
    "0xff424b46": "0xff3e4954",
    "0xff06231b": "0xff062330",
    "0xff346f5e": "0xff31688e",
    "0xff12372e": "0xff11364a",
    "0xff111817": "0xff10171e",
    "0xff0b1110": "0xff0a1015",
    "0xff515c54": "0xff4d5863",
    "0xff070c0c": "0xff060b10",
    "0xff59665d": "0xff55636e",
    "0xff1b2421": "0xff181f28",
    "0xff6b7a70": "0xff677787",
    "0xff0c1412": "0xff0b131a",
    "0xff8b998e": "0xff86939f",
    "0xff0c1211": "0xff0b1116",
    "0xff4a5750": "0xff46535f",
    "0xff9fd9c4": "0xff9bd9ea",
    "0xff347d68": "0xff317a9e",
    # overlay
    "0xf217211f": "0xf2151e27",
    "0xff34413c": "0xff313e4a",
    "0xffb9c9bd": "0xffb3c2cc",
    "0xff18211f": "0xff171f28",
}


def main() -> int:
    for path in FILES:
        text = path.read_text(encoding="utf-8")
        original = text
        counts = {}
        for old, new in MAP.items():
            n = text.count(old)
            if n:
                counts[old] = n
                text = text.replace(old, new)
        if text != original:
            path.write_text(text, encoding="utf-8")
        total = sum(counts.values())
        print(f"{path.name}: {total} literals remapped")
        for old, n in sorted(counts.items()):
            print(f"  {old} x{n}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
