#!/usr/bin/env python3
"""Generate original indexed PNG assets; no image-library dependency."""
from pathlib import Path
import struct
import zlib


def png(path, width, height):
    palette = b"".join(bytes((20 + i * 14 // 63, 26 + i * 15 // 63, 36 + i * 22 // 63))
                       for i in range(64)) + bytes((246, 195, 63))
    rows = []
    for y in range(height):
        row = bytearray([0])
        for x in range(width):
            cx, cy = x - width // 2, y - height // 2
            trophy = ((-width // 5 < cx < width // 5 and -height // 4 < cy < 0) or
                      (-width // 18 < cx < width // 18 and 0 <= cy < height // 5) or
                      (-width // 4 < cx < width // 4 and height // 5 <= cy < height // 5 + height // 18))
            row.append(64 if trophy else min(63, (x * 32 // width + y * 32 // height)))
        rows.append(bytes(row))
    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xffffffff)
    path.write_bytes(b"\x89PNG\r\n\x1a\n" +
                     chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 3, 0, 0, 0)) +
                     chunk(b"PLTE", palette) + chunk(b"IDAT", zlib.compress(b"".join(rows), 9)) +
                     chunk(b"IEND", b""))


if __name__ == "__main__":
    root = Path(__file__).resolve().parents[1] / "sce_sys"
    for name, width, height in [("icon0.png", 128, 128), ("livearea/contents/bg.png", 840, 500),
                                ("livearea/contents/startup.png", 280, 158)]:
        png(root / name, width, height)
