#!/usr/bin/env python3
import struct
import zlib

SPRITE = [
    "         WW         ",
    "        WCCC        ",
    "      CCCCCCCC      ",
    "     CCCCCCCCCC     ",
    "    CCMMCCCCMMCC    ",
    "   CCCCCCCCCCCCCC   ",
    "    TTTTTTTTTTTT    ",
    "     DDDDDDDDDD     ",
    "      DDDDDD        ",
    "       DDDD         ",
]
HEX = {"M": "#d633d6", "W": "#ffffff", "C": "#26c6da", "T": "#4dd0e1", "D": "#006064"}
RGB = {"M": (214, 51, 214), "W": (255, 255, 255), "C": (38, 198, 218), "T": (77, 208, 225), "D": (0, 96, 100)}
BG = (13, 17, 23)
SCALE = 4
W, H = 20 * SCALE, 10 * SCALE
rows = []
svg = [
    f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" shape-rendering="crispEdges">'
]
for y, line in enumerate(SPRITE):
    row = []
    for x, ch in enumerate(line):
        if ch != " ":
            svg.append(
                f'<rect x="{x * SCALE}" y="{y * SCALE}" width="{SCALE}" height="{SCALE}" fill="{HEX[ch]}"/>'
            )
        rgb = RGB.get(ch, BG)
        for _ in range(SCALE):
            row.extend(rgb)
    for _ in range(SCALE):
        rows.append(bytes(row))
svg.append("</svg>")

raw = b"".join(b"\x00" + r for r in rows)

def chunk(tag, data):
    return (
        struct.pack(">I", len(data))
        + tag
        + data
        + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    )

ihdr = struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0)
png = (
    b"\x89PNG\r\n\x1a\n"
    + chunk(b"IHDR", ihdr)
    + chunk(b"IDAT", zlib.compress(raw, 9))
    + chunk(b"IEND", b"")
)
open("assets/moontail-logo.png", "wb").write(png)
open("assets/moontail-logo.svg", "w", encoding="utf-8", newline="\n").write("\n".join(svg))
