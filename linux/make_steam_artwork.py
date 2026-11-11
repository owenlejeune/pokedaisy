"""Generate Steam Grid artwork (vertical grid, horizontal grid, hero, logo) in
linux/steamdeck/artwork/ from the Android launcher flower PNG without third-party
imaging dependencies.

Outputs:
  - grid-vertical.png   (600x900)
  - grid-horizontal.png (920x430)
  - hero.png            (1920x620)
  - logo.png            (640x360, transparent)
"""
from pathlib import Path
import argparse
import struct
import zlib


def _paeth(a: int, b: int, c: int) -> int:
    p = a + b - c
    pa = abs(p - a)
    pb = abs(p - b)
    pc = abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    return b if pb <= pc else c


def read_rgba_png(path: Path) -> tuple[int, int, bytearray]:
    data = path.read_bytes()
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError(f'{path}: not a PNG')
    pos = 8
    w = h = 0
    idat = bytearray()
    while pos + 8 <= len(data):
        length, ctype = struct.unpack('>I4s', data[pos:pos + 8])
        chunk = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if ctype == b'IHDR':
            w, h, depth, col, comp, filt, interlace = struct.unpack('>IIBBBBB', chunk)
            if (depth, col, comp, filt, interlace) != (8, 6, 0, 0, 0):
                raise ValueError(f'{path}: expected non-interlaced 8-bit RGBA PNG')
        elif ctype == b'IDAT':
            idat.extend(chunk)
        elif ctype == b'IEND':
            break
    raw = zlib.decompress(bytes(idat))
    stride = w * 4
    out = bytearray(w * h * 4)
    prev = bytearray(stride)
    idx = 0
    for y in range(h):
        ftype = raw[idx]
        idx += 1
        row = bytearray(raw[idx:idx + stride])
        idx += stride
        for i in range(stride):
            left = row[i - 4] if i >= 4 else 0
            up = prev[i]
            upleft = prev[i - 4] if i >= 4 else 0
            if ftype == 1:
                row[i] = (row[i] + left) & 0xff
            elif ftype == 2:
                row[i] = (row[i] + up) & 0xff
            elif ftype == 3:
                row[i] = (row[i] + ((left + up) >> 1)) & 0xff
            elif ftype == 4:
                row[i] = (row[i] + _paeth(left, up, upleft)) & 0xff
        out[y * stride:(y + 1) * stride] = row
        prev = row
    return w, h, out


def _png_chunk(ctype: bytes, payload: bytes) -> bytes:
    crc = zlib.crc32(ctype + payload) & 0xffffffff
    return struct.pack('>I4s', len(payload), ctype) + payload + struct.pack('>I', crc)


def write_rgba_png(path: Path, w: int, h: int, rgba: bytes) -> None:
    stride = w * 4
    # Use Sub filter (type 1) or None (0); None is fast and clean with zlib.
    raw = bytearray((stride + 1) * h)
    for y in range(h):
        raw[y * (stride + 1)] = 0
        raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)] = rgba[y * stride:(y + 1) * stride]
    ihdr = struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)
    idat = zlib.compress(bytes(raw), level=6)
    png = b'\x89PNG\r\n\x1a\n' + _png_chunk(b'IHDR', ihdr) + _png_chunk(b'IDAT', idat) + _png_chunk(b'IEND', b'')
    path.write_bytes(png)


def crop_square(src: bytearray, sw: int, sh: int, x0: int, y0: int, side: int) -> bytearray:
    out = bytearray(side * side * 4)
    for y in range(side):
        sy = min(max(y0 + y, 0), sh - 1)
        s0 = (sy * sw + x0) * 4
        out[y * side * 4:(y + 1) * side * 4] = src[s0:s0 + side * 4]
    return out


def make_gradient_bg(w: int, h: int, top_rgb: tuple[int, int, int], bot_rgb: tuple[int, int, int]) -> bytearray:
    buf = bytearray(w * h * 4)
    row = bytearray(w * 4)
    denom = max(h - 1, 1)
    for y in range(h):
        r = top_rgb[0] + (bot_rgb[0] - top_rgb[0]) * y // denom
        g = top_rgb[1] + (bot_rgb[1] - top_rgb[1]) * y // denom
        b = top_rgb[2] + (bot_rgb[2] - top_rgb[2]) * y // denom
        pixel = bytes((r, g, b, 255))
        row[:] = pixel * w
        buf[y * w * 4:(y + 1) * w * 4] = row
    return buf


def blit_nearest_alpha(dst: bytearray, dw: int, dh: int,
                       src: bytearray, sw: int, sh: int,
                       dx0: int, dy0: int, out_w: int, out_h: int) -> None:
    for oy in range(out_h):
        dy = dy0 + oy
        if dy < 0 or dy >= dh:
            continue
        sy = oy * sh // out_h
        for ox in range(out_w):
            dx = dx0 + ox
            if dx < 0 or dx >= dw:
                continue
            sx = ox * sw // out_w
            si = (sy * sw + sx) * 4
            sa = src[si + 3]
            if sa == 0:
                continue
            di = (dy * dw + dx) * 4
            if sa == 255:
                dst[di:di + 4] = src[si:si + 4]
            else:
                inv = 255 - sa
                dst[di] = (src[si] * sa + dst[di] * inv) // 255
                dst[di + 1] = (src[si + 1] * sa + dst[di + 1] * inv) // 255
                dst[di + 2] = (src[si + 2] * sa + dst[di + 2] * inv) // 255
                dst[di + 3] = min(255, sa + dst[di + 3] * inv // 255)


def generate_artwork(out_dir: Path) -> list[Path]:
    repo = Path(__file__).resolve().parents[1]
    fg_path = repo / 'app/src/main/res/mipmap-xxxhdpi/ic_launcher_foreground.png'
    sw, sh, raw_fg = read_rgba_png(fg_path)
    # Center two thirds matches Android adaptive-icon foreground crop used by linux/make_icon.py.
    side = sw * 2 // 3
    off = (sw - side) // 2
    flower = crop_square(raw_fg, sw, sh, off, off, side)

    out_dir.mkdir(parents=True, exist_ok=True)
    specs = [
        ('grid-vertical.png', 600, 900, (0x33, 0x96, 0xff), (0x1a, 0x52, 0xb8), 420),
        ('grid-horizontal.png', 920, 430, (0x33, 0x96, 0xff), (0x1a, 0x52, 0xb8), 320),
        ('hero.png', 1920, 620, (0x1a, 0x4b, 0x9c), (0x0f, 0x2a, 0x62), 440),
        ('logo.png', 640, 360, None, None, 320),
    ]
    written = []
    for filename, w, h, top_rgb, bot_rgb, icon_size in specs:
        if top_rgb is not None and bot_rgb is not None:
            canvas = make_gradient_bg(w, h, top_rgb, bot_rgb)
        else:
            canvas = bytearray(w * h * 4)
        blit_nearest_alpha(canvas, w, h, flower, side, side,
                           (w - icon_size) // 2, (h - icon_size) // 2, icon_size, icon_size)
        target = out_dir / filename
        write_rgba_png(target, w, h, canvas)
        written.append(target)
    return written


def main() -> None:
    repo = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', nargs='?', type=Path, default=repo / 'linux/steamdeck/artwork')
    args = parser.parse_args()
    for p in generate_artwork(args.output):
        print(p)


if __name__ == '__main__':
    main()

