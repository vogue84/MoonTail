#!/usr/bin/env python3
"""Generate MoonTail logo PNG/SVG from the shared pixel-comet sprite."""
import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# Horizontal comet: warm core head (left), cyan/teal tail tapering right.
SPRITE = [
    "                                ",
    "                  BBBBBBBB      ",
    "              BBBBBBBBBBBBBB    ",
    "          BBBBBBBBBBBBBBBBBBBB  ",
    "      DDDDDDDDDDDDDDDDDDDDDDDD  ",
    "  TTTTTTTTTTTTTTTTTTTTTTTTTTTTT ",
    " TTCCCCCCCCCCCCCCCCCCCCCCCCTTT  ",
    "CCOOYYYYWWWWYYYYOOCCCCCCCCCCTT  ",
    "CCOOYYYOOWWOOYYYYOOCCCCCCCCCT   ",
    " TTCCCCCCCCCCCCCCCCCCCCCCCCT    ",
    "    TTTTTTTTTTTTTTTTTTTTT       ",
    "        DDDDDDDDDDDD            ",
    "             BB                 ",
]

PALETTE = {
    "W": ("#fffef5", (255, 254, 245)),
    "Y": ("#ffd54f", (255, 213, 79)),
    "O": ("#ff9800", (255, 152, 0)),
    "C": ("#4dd0e1", (77, 208, 225)),
    "T": ("#26c6da", (38, 198, 218)),
    "D": ("#00838f", (0, 131, 143)),
    "B": ("#004d56", (0, 77, 86)),
}
BG = (13, 17, 23)
SCALE = 4


def main() -> None:
    w, h = len(SPRITE[0]) * SCALE, len(SPRITE) * SCALE
    assert all(len(row) == len(SPRITE[0]) for row in SPRITE), "sprite rows must be equal width"

    rows = []
    svg = [
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" shape-rendering="crispEdges">'
    ]
    for y, line in enumerate(SPRITE):
        row = []
        for x, ch in enumerate(line):
            if ch != " ":
                hx, rgb = PALETTE[ch]
                svg.append(
                    f'<rect x="{x * SCALE}" y="{y * SCALE}" width="{SCALE}" height="{SCALE}" fill="{hx}"/>'
                )
                for _ in range(SCALE):
                    row.extend(rgb)
            else:
                for _ in range(SCALE):
                    row.extend(BG)
        for _ in range(SCALE):
            rows.append(bytes(row))
    svg.append("</svg>")

    raw = b"".join(b"\x00" + r for r in rows)

    def chunk(tag: bytes, data: bytes) -> bytes:
        return (
            struct.pack(">I", len(data))
            + tag
            + data
            + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
        )

    ihdr = struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)
    png = (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", ihdr)
        + chunk(b"IDAT", zlib.compress(raw, 9))
        + chunk(b"IEND", b"")
    )
    (ROOT / "assets/moontail-logo.png").write_bytes(png)
    (ROOT / "assets/moontail-logo.svg").write_text("\n".join(svg) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
