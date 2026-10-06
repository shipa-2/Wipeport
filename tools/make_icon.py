"""Draws the Wipeport icon (a speed chevron in two neon colours on a dark tile) as .ico and .png.
Needs Pillow. The icon is the port's own drawing, not the game's artwork.
usage: make_icon.py launcher/wipeport.ico launcher/wipeport.png"""
import sys
from PIL import Image, ImageDraw, ImageFilter

S = 1024  # drawn large, scaled down for every icon size


def chevron(d, x, y, w, h, t, color):
    """A forward-pointing chevron: two thick strokes meeting at the right."""
    d.polygon([(x, y), (x + t, y), (x + w, y + h / 2), (x + t, y + h), (x, y + h),
               (x + w - t, y + h / 2)], fill=color)


def draw():
    tile = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    mask = Image.new('L', (S, S), 0)
    ImageDraw.Draw(mask).rounded_rectangle((24, 24, S - 24, S - 24), radius=200, fill=255)

    back = Image.new('RGBA', (S, S))
    bd = ImageDraw.Draw(back)
    for y in range(S):
        t = y / S
        bd.line((0, y, S, y), fill=(int(14 + 10 * t), int(16 + 6 * t), int(34 + 22 * t), 255))
    # A track running into the distance.
    bd.polygon([(S * 0.18, S), (S * 0.47, S * 0.52), (S * 0.53, S * 0.52), (S * 0.82, S)],
               fill=(30, 34, 66, 255))
    for i in range(6):
        y0 = S * (0.56 + i * i * 0.012 + i * 0.04)
        bd.line((S * 0.5, y0, S * 0.5, y0 + S * (0.015 + i * 0.01)), fill=(90, 110, 180, 255),
                width=int(6 + i * 3))

    glow = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    chevron(gd, S * 0.20, S * 0.24, S * 0.34, S * 0.40, S * 0.13, (0, 220, 255, 255))
    chevron(gd, S * 0.44, S * 0.24, S * 0.34, S * 0.40, S * 0.13, (255, 40, 170, 255))
    halo = glow.filter(ImageFilter.GaussianBlur(28))

    image = Image.alpha_composite(back, halo)
    image = Image.alpha_composite(image, glow)
    tile.paste(image, (0, 0), mask)
    return tile


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    icon = draw()
    sizes = [(s, s) for s in (16, 24, 32, 48, 64, 128, 256)]
    icon.resize((256, 256), Image.LANCZOS).save(sys.argv[1], sizes=sizes)
    icon.resize((256, 256), Image.LANCZOS).save(sys.argv[2])


if __name__ == '__main__':
    main()
