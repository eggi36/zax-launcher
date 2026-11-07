"""Create the icon of Zax-Launcher.exe from the icon of Zax.exe.

The Zax logo (32 and 16 pixels, as in the original) gets a green start button in the bottom
right corner. The result is an .ico with 32-bit bitmaps (BMP instead of PNG so it also works on
Windows XP). Because the image comes from Zax.exe, it is generated at build time and is not
part of the repository.

Usage: python make_icon.py <Zax.exe> <target.ico>
"""
import io
import struct
import sys

import pefile
from PIL import Image, ImageDraw

RT_ICON, RT_GROUP_ICON = 3, 14

# size: (radius, centre x, centre y) of the start button in pixels
BADGE = {32: (6.5, 24.5, 24.5), 16: (4.0, 11.5, 11.5)}


def _resource(pe, entry):
    d = entry.directory.entries[0].data.struct
    return pe.get_data(d.OffsetToData, d.Size)


def exe_icons(path):
    """Images of the exe's first icon group as {size: RGBA image}."""
    pe = pefile.PE(path)
    res = {t.id: t for t in pe.DIRECTORY_ENTRY_RESOURCE.entries}
    group = _resource(pe, res[RT_GROUP_ICON].directory.entries[0])
    icons = {e.id: _resource(pe, e) for e in res[RT_ICON].directory.entries}
    count = struct.unpack_from('<H', group, 4)[0]
    entries, blobs = [], []
    for i in range(count):
        w, h, c, r, planes, bits, _size, iid = struct.unpack_from('<BBBBHHIH', group, 6 + 14 * i)
        entries.append((w, h, c, r, planes, bits, len(icons[iid])))
        blobs.append(icons[iid])
    # assemble as .ico and let Pillow read it
    ico = struct.pack('<HHH', 0, 1, count)
    offset = 6 + 16 * count
    for e in entries:
        ico += struct.pack('<BBBBHHII', *e, offset)
        offset += e[6]
    ico += b''.join(blobs)
    img = Image.open(io.BytesIO(ico))
    return {s[0]: img.ico.getimage(s).convert('RGBA') for s in img.ico.sizes()}


def add_badge(base, radius, cx, cy):
    """Green circle with a white triangle, drawn at 8x and scaled down (anti-aliased)."""
    size, s = base.width, 8
    big = Image.new('RGBA', (size * s, size * s), (0, 0, 0, 0))
    d = ImageDraw.Draw(big)
    r, x, y = radius * s, cx * s, cy * s
    d.ellipse([x - r - s, y - r - s, x + r + s, y + r + s], fill=(0, 0, 0, 255))
    d.ellipse([x - r, y - r, x + r, y + r], fill=(40, 170, 60, 255))
    t = r * 0.55
    d.polygon([(x - t * 0.6, y - t), (x - t * 0.6, y + t), (x + t, y)], fill=(255, 255, 255, 255))
    return Image.alpha_composite(base, big.resize((size, size), Image.LANCZOS))


def bmp_entry(img):
    """Icon image in BMP format: 32-bit BGRA bottom-up, plus the AND mask."""
    w, h = img.size
    header = struct.pack('<IiiHHIIiiII', 40, w, h * 2, 1, 32, 0, 0, 0, 0, 0, 0)
    px = img.load()
    rows = b''.join(bytes(c for xx in range(w) for c in (px[xx, yy][2], px[xx, yy][1], px[xx, yy][0], px[xx, yy][3]))
                    for yy in range(h - 1, -1, -1))
    mask_row = ((w + 31) // 32) * 4
    mask = b''
    for yy in range(h - 1, -1, -1):
        bits = bytearray(mask_row)
        for xx in range(w):
            if px[xx, yy][3] == 0:
                bits[xx // 8] |= 0x80 >> (xx % 8)
        mask += bytes(bits)
    return header + rows + mask


def write_ico(images, path):
    blobs = [bmp_entry(img) for img in images]
    out = struct.pack('<HHH', 0, 1, len(blobs))
    offset = 6 + 16 * len(blobs)
    for img, blob in zip(images, blobs):
        out += struct.pack('<BBBBHHII', img.width % 256, img.height % 256, 0, 0, 1, 32, len(blob), offset)
        offset += len(blob)
    with open(path, 'wb') as f:
        f.write(out + b''.join(blobs))


def main():
    src, dst = sys.argv[1], sys.argv[2]
    icons = exe_icons(src)
    images = [add_badge(icons[size], *BADGE[size]) for size in sorted(icons, reverse=True) if size in BADGE]
    if not images:
        sys.exit('Zax.exe contains no icon with 32 or 16 pixels')
    write_ico(images, dst)


if __name__ == '__main__':
    main()
