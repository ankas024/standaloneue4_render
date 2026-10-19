"""Generates the bitmap skin used by the Render Studio Slate style.

Only uses the Python standard library so it runs anywhere:
    python Scripts/generate_style_images.py

Output: Plugins/RenderStudio/Resources/Style/*.png (committed, so you only need
to re-run this when tweaking the look).
"""
import math
import os
import struct
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "Plugins", "RenderStudio", "Resources", "Style")


def write_png(path, w, h, pixels):
    """pixels: list of rows, each row a list of (r, g, b, a) tuples."""
    raw = b"".join(b"\x00" + bytes(c for px in row for c in px) for row in pixels)

    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def lerp(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(len(a)))


def canvas(w, h, fill=(0, 0, 0, 0)):
    return [[fill for _ in range(w)] for _ in range(h)]


def gradient(w, h, top, bottom, highlight=None, border=None, round_corners=False):
    img = canvas(w, h)
    for y in range(h):
        c = lerp(top, bottom, y / max(1, h - 1))
        for x in range(w):
            img[y][x] = c + (255,) if len(c) == 3 else c
    if highlight:
        for x in range(w):
            img[1 if border else 0][x] = highlight + (255,)
    if border:
        for x in range(w):
            img[0][x] = border + (255,)
            img[h - 1][x] = border + (255,)
        for y in range(h):
            img[y][0] = border + (255,)
            img[y][w - 1] = border + (255,)
    if round_corners:
        for (x, y) in ((0, 0), (w - 1, 0), (0, h - 1), (w - 1, h - 1)):
            img[y][x] = (0, 0, 0, 0)
    return img


def save(name, w, h, img):
    write_png(os.path.join(OUT, name + ".png"), w, h, img)


def icon(draw, size=16):
    img = canvas(size, size)

    def put(x, y, c):
        if 0 <= x < size and 0 <= y < size:
            img[y][x] = c

    draw(put)
    return img


def draw_cube(put):
    light, mid, dark = (220, 220, 225, 255), (160, 165, 175, 255), (105, 110, 120, 255)
    for y in range(3, 7):
        for x in range(2 + (6 - y), 14 - (y - 3) + 0):
            put(x, y, light)
    for y in range(7, 14):
        for x in range(2, 8):
            put(x, y, mid)
        for x in range(8, 14):
            put(x, y, dark)


def draw_sphere(put):
    for y in range(16):
        for x in range(16):
            dx, dy = x - 7.5, y - 7.5
            d = math.sqrt(dx * dx + dy * dy)
            if d <= 6.5:
                shade = max(0.25, 1.0 - math.sqrt((dx + 2.5) ** 2 + (dy + 2.5) ** 2) / 11.0)
                v = int(90 + 150 * shade)
                put(x, y, (v, v, min(255, v + 10), 255))


def draw_light(put):
    yellow = (255, 205, 70, 255)
    for y in range(16):
        for x in range(16):
            dx, dy = x - 7.5, y - 7.5
            if dx * dx + dy * dy <= 12:
                put(x, y, yellow)
    for a in range(8):
        ang = a * math.pi / 4
        for r in (5.5, 6.5):
            put(int(round(7.5 + math.cos(ang) * r)), int(round(7.5 + math.sin(ang) * r)), yellow)


def draw_sky(put):
    for y in range(2, 14):
        c = lerp((90, 150, 230), (200, 225, 250), (y - 2) / 11.0) + (255,)
        for x in range(1, 15):
            put(x, y, c)
    for x in range(1, 15):
        put(x, 12, (90, 110, 80, 255))
        put(x, 13, (90, 110, 80, 255))


def draw_camera(put):
    body = (200, 200, 205, 255)
    for y in range(5, 12):
        for x in range(1, 11):
            put(x, y, body)
    for i in range(4):
        for y in range(7 - i, 10 + i):
            put(11 + i, y, body)


def draw_material(put):
    for y in range(16):
        for x in range(16):
            dx, dy = x - 7.5, y - 7.5
            if dx * dx + dy * dy <= 42:
                t = (x + y) / 30.0
                put(x, y, lerp((240, 140, 60), (120, 40, 20), t) + (255,))
    for (x, y) in ((5, 4), (4, 5), (5, 5), (6, 4)):
        put(x, y, (255, 230, 200, 255))


def draw_eye(put, open_eye=True):
    c = (210, 210, 210, 255) if open_eye else (100, 100, 100, 255)
    for x in range(1, 15):
        h = int(round(3.5 * math.sin(math.pi * (x - 0.5) / 14.0)))
        put(x, 8 - h, c)
        put(x, 8 + h, c)
    if open_eye:
        for y in range(6, 11):
            for x in range(6, 11):
                if (x - 8) ** 2 + (y - 8) ** 2 <= 4:
                    put(x, y, c)
    else:
        for i in range(2, 14):
            put(i, i, c)


def draw_app_icon():
    size = 128
    img = canvas(size, size)
    for y in range(size):
        for x in range(size):
            dx, dy = x - 63.5, y - 63.5
            d = math.sqrt(dx * dx + dy * dy)
            if d <= 60:
                shade = max(0.0, 1.0 - math.sqrt((dx + 22) ** 2 + (dy + 22) ** 2) / 100.0)
                base = lerp((40, 40, 46), (235, 140, 50), shade)
                if d > 56:
                    base = (25, 25, 28)
                img[y][x] = base + (255,)
    return img


def main():
    os.makedirs(OUT, exist_ok=True)

    # Panels / chrome. Classic 2013-2014 desktop DCC look: mid grey gradients,
    # 1px highlight on top, dark 1px border.
    save("PanelHeader", 8, 22, gradient(8, 22, (88, 88, 92), (60, 60, 64), highlight=(112, 112, 118), border=(30, 30, 32)))
    save("ToolbarBackground", 8, 34, gradient(8, 34, (78, 78, 82), (52, 52, 56), highlight=(100, 100, 106)))
    save("MenuBarBackground", 8, 22, gradient(8, 22, (70, 70, 74), (56, 56, 60)))
    save("StatusBar", 8, 20, gradient(8, 20, (52, 52, 56), (40, 40, 44), highlight=(70, 70, 74)))

    save("Button", 16, 16, gradient(16, 16, (104, 104, 110), (72, 72, 78), highlight=(132, 132, 138), border=(28, 28, 30), round_corners=True))
    save("Button_Hovered", 16, 16, gradient(16, 16, (122, 122, 130), (86, 86, 94), highlight=(150, 150, 158), border=(28, 28, 30), round_corners=True))
    save("Button_Pressed", 16, 16, gradient(16, 16, (50, 50, 54), (72, 72, 78), border=(20, 20, 22), round_corners=True))
    save("Button_Checked", 16, 16, gradient(16, 16, (236, 150, 60), (176, 96, 26), highlight=(255, 190, 110), border=(60, 32, 8), round_corners=True))

    save("Selection", 8, 18, gradient(8, 18, (240, 156, 64), (196, 108, 30)))
    save("RowHover", 8, 18, gradient(8, 18, (84, 84, 90), (70, 70, 76)))

    save("Icon_Mesh", 16, 16, icon(draw_cube))
    save("Icon_Sphere", 16, 16, icon(draw_sphere))
    save("Icon_Light", 16, 16, icon(draw_light))
    save("Icon_Sky", 16, 16, icon(draw_sky))
    save("Icon_Camera", 16, 16, icon(draw_camera))
    save("Icon_Material", 16, 16, icon(draw_material))
    save("Icon_Visible", 16, 16, icon(lambda put: draw_eye(put, True)))
    save("Icon_Hidden", 16, 16, icon(lambda put: draw_eye(put, False)))

    app = draw_app_icon()
    write_png(os.path.join(ROOT, "Plugins", "RenderStudio", "Resources", "Icon128.png"), 128, 128, app)
    print("Wrote style images to", OUT)


if __name__ == "__main__":
    main()
