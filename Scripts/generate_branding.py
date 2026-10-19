"""Generates the standalone application's branding.

    python Scripts/generate_branding.py

Output (committed, re-run only to change the look):
  Build/Windows/Application.ico   icon of RenderStudio.exe (picked up by UnrealBuildTool)
  Content/Splash/EdSplash.bmp     startup splash screen (replaces the Unreal Editor splash)

Standard library only.
"""
import math
import os
import struct

from generate_style_images import ROOT, lerp, write_png

# 5x7 pixel font, just the letters we need
FONT = {
    "R": ["1110", "1001", "1001", "1110", "1010", "1001", "1001"],
    "E": ["1111", "1000", "1000", "1110", "1000", "1000", "1111"],
    "N": ["1001", "1101", "1101", "1011", "1011", "1001", "1001"],
    "D": ["1110", "1001", "1001", "1001", "1001", "1001", "1110"],
    "S": ["0111", "1000", "1000", "0110", "0001", "0001", "1110"],
    "T": ["11111", "00100", "00100", "00100", "00100", "00100", "00100"],
    "U": ["1001", "1001", "1001", "1001", "1001", "1001", "0110"],
    "I": ["111", "010", "010", "010", "010", "010", "111"],
    "O": ["0110", "1001", "1001", "1001", "1001", "1001", "0110"],
    " ": ["00", "00", "00", "00", "00", "00", "00"],
}


def shade_sphere(size, x, y):
    """The app mark: an orange-lit shader ball on a dark disc. Returns RGBA or None."""
    c = (size - 1) / 2.0
    radius = size * 0.47
    dx, dy = x - c, y - c
    d = math.sqrt(dx * dx + dy * dy)
    if d > radius:
        return None
    if d > radius * 0.93:
        return (25, 25, 28, 255)
    light = max(0.0, 1.0 - math.sqrt((dx + radius * 0.37) ** 2 + (dy + radius * 0.37) ** 2) / (radius * 1.65))
    return lerp((40, 40, 46), (235, 140, 50), light) + (255,)


def app_icon(size):
    return [[shade_sphere(size, x, y) or (0, 0, 0, 0) for x in range(size)] for y in range(size)]


def png_bytes(size, pixels):
    tmp = os.path.join(ROOT, "Intermediate", "_icon_tmp.png")
    os.makedirs(os.path.dirname(tmp), exist_ok=True)
    write_png(tmp, size, size, pixels)
    with open(tmp, "rb") as f:
        data = f.read()
    os.remove(tmp)
    return data


def write_ico(path, sizes):
    """ICO with PNG-compressed entries (supported since Windows Vista)."""
    images = [(s, png_bytes(s, app_icon(s))) for s in sizes]
    header = struct.pack("<HHH", 0, 1, len(images))
    offset = 6 + 16 * len(images)
    entries = b""
    for size, data in images:
        dim = 0 if size >= 256 else size
        entries += struct.pack("<BBBBHHII", dim, dim, 0, 0, 1, 32, len(data), offset)
        offset += len(data)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(header + entries + b"".join(data for _, data in images))


def write_bmp(path, w, h, pixels):
    """24-bit bottom-up BMP (what the Windows splash loader expects)."""
    row_pad = (4 - (w * 3) % 4) % 4
    data = b""
    for y in range(h - 1, -1, -1):
        row = b"".join(bytes((px[2], px[1], px[0])) for px in pixels[y])
        data += row + b"\x00" * row_pad
    header = struct.pack("<2sIHHI", b"BM", 54 + len(data), 0, 0, 54)
    info = struct.pack("<IiiHHIIiiII", 40, w, h, 1, 24, 0, len(data), 2835, 2835, 0, 0)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(header + info + data)


def draw_text(img, text, x0, y0, scale, color):
    x = x0
    for ch in text:
        glyph = FONT[ch]
        for gy, row in enumerate(glyph):
            for gx, bit in enumerate(row):
                if bit == "1":
                    for sy in range(scale):
                        for sx in range(scale):
                            px, py = x + gx * scale + sx, y0 + gy * scale + sy
                            if 0 <= py < len(img) and 0 <= px < len(img[0]):
                                img[py][px] = color
        x += (len(glyph[0]) + 1) * scale


def splash(w=600, h=220):
    img = [[lerp((70, 70, 76), (34, 34, 38), y / (h - 1)) for _ in range(w)] for y in range(h)]
    # 1px highlight and border, like the panels in the app
    for x in range(w):
        img[0][x] = (24, 24, 26)
        img[1][x] = (110, 110, 116)
        img[h - 1][x] = (24, 24, 26)
    for y in range(h):
        img[y][0] = (24, 24, 26)
        img[y][w - 1] = (24, 24, 26)
    # orange accent stripe
    for y in range(150, 153):
        for x in range(30, w - 30):
            img[y][x] = (232, 146, 56)

    ball = 110
    for y in range(ball):
        for x in range(ball):
            c = shade_sphere(ball, x, y)
            if c:
                img[25 + y][30 + x] = c[:3]

    draw_text(img, "RENDER STUDIO", 165, 52, 5, (235, 235, 238))
    return img


def main():
    write_ico(os.path.join(ROOT, "Build", "Windows", "Application.ico"), [16, 24, 32, 48, 64, 256])
    write_bmp(os.path.join(ROOT, "Content", "Splash", "EdSplash.bmp"), 600, 220, splash())
    print("Wrote Build/Windows/Application.ico and Content/Splash/EdSplash.bmp")


if __name__ == "__main__":
    main()
