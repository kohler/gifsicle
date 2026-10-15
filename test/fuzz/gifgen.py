"""gifgen.py - GIF generation, mutation, and strict checking for fuzzing.

This file is part of gifsicle's fuzzing harness. See README.md.

- `random_gif(rng)` generates a structured random GIF aimed at gifsicle's
  edge cases: frames partly or entirely outside the logical screen, every
  minimum code size, LZW streams that fill the code table, transparency,
  every disposal, local colormaps, interlacing, extensions, and so on.
- `special_gif(rng)` generates a GIF aimed at one specific edge case, such
  as a transparent index past the end of the colormap, duplicate colors,
  256 colors in use, an animation for the optimizer, or named frames.
  `concatenated_gif(rng)` returns several GIFs back to back.
- `mutate(data, rng)` applies byte-level mutations.
- `strict_check(data)` parses a GIF strictly (like test/fuzz/gifcheck.c) and
  returns None or a description of the first problem.
"""

import random
import struct

__all__ = ["random_gif", "special_gif", "concatenated_gif", "mutate", "strict_check",
           "lzw_encode", "make_gif"]


# ---------------------------------------------------------------------------
# LZW encoding

def lzw_encode(pixels, mcb, clear_every=None, defer_clear=False, early_change=False):
    """LZW-encode `pixels` with minimum code size `mcb`.

    By default this is a correct encoder. `clear_every` forces a CLEAR after
    that many codes; `defer_clear` keeps encoding after the table fills (a
    valid "deferred clear"); `early_change` grows the code width one code
    early, reproducing a common encoder bug.

    The encoder defines a table entry with every code it emits, one entry
    ahead of the decoder, so it widens codes when the table size *exceeds*
    a power of two. After the last data code, the decoder catches up by one
    entry, which can widen the EOI code."""
    clear, eoi = 1 << mcb, (1 << mcb) + 1
    out = bytearray()
    acc = nbits = 0
    width = mcb + 1

    def put(code):
        nonlocal acc, nbits
        acc |= code << nbits
        nbits += width
        while nbits >= 8:
            out.append(acc & 255)
            acc >>= 8
            nbits -= 8

    def reset():
        nonlocal width
        width = mcb + 1
        return {(i,): i for i in range(clear)}, clear + 2

    put(clear)
    table, nxt = reset()
    ncodes = 0
    cur = ()
    for p in pixels:
        p &= clear - 1
        s = cur + (p,)
        if s in table:
            cur = s
            continue
        put(table[cur])
        ncodes += 1
        if nxt < 4096:
            table[s] = nxt
            nxt += 1
            if (nxt >= (1 << width) if early_change else nxt > (1 << width)) and width < 12:
                width += 1
        if (nxt >= 4096 and not defer_clear) or (clear_every and ncodes >= clear_every):
            put(clear)
            table, nxt = reset()
            ncodes = 0
        cur = (p,)
    if cur:
        put(table[cur])
        # The decoder defines one last entry after the last data code, unless
        # that code was the first after CLEAR.
        if ncodes > 0 and nxt == (1 << width) and width < 12 and not early_change:
            width += 1
    put(eoi)
    if nbits:
        out.append(acc & 255)
    return bytes(out)


def subblocks(data):
    out = bytearray()
    for i in range(0, len(data), 255):
        chunk = data[i:i + 255]
        out.append(len(chunk))
        out += chunk
    out.append(0)
    return bytes(out)


# ---------------------------------------------------------------------------
# GIF assembly

def colortable(colors):
    """Pack a list of (r, g, b); pad to a power of two. Returns (size-bits, bytes)."""
    n = max(2, len(colors))
    bits = max(0, (n - 1).bit_length() - 1)
    padded = list(colors) + [(0, 0, 0)] * ((2 << bits) - len(colors))
    return bits, b"".join(bytes(c) for c in padded)


def make_gif(screen, gct, frames, version=b"GIF89a", background=0,
             loopcount=None, comment=None, trailer=b";"):
    """Assemble a GIF. `frames` is a list of dicts with keys left, top,
    width, height, pixels, and optionally local (color list), interlace,
    transparent, disposal, delay, mcb, lzw (raw LZW bytes), comment."""
    d = bytearray(version)
    flags = 0
    gct_bytes = b""
    if gct is not None:
        bits, gct_bytes = colortable(gct)
        flags = 0x80 | 0x70 | bits
    d += struct.pack("<HHBBB", screen[0], screen[1], flags, background & 255, 0)
    d += gct_bytes
    if loopcount is not None:
        d += b"\x21\xff\x0bNETSCAPE2.0\x03\x01" + struct.pack("<H", loopcount) + b"\x00"
    if comment is not None:
        d += b"\x21\xfe" + subblocks(comment)
    for f in frames:
        if f.get("comment") is not None:
            d += b"\x21\xfe" + subblocks(f["comment"])
        if any(k in f for k in ("transparent", "disposal", "delay")):
            t = f.get("transparent")
            packed = ((f.get("disposal", 0) & 7) << 2) | (1 if t is not None else 0)
            d += b"\x21\xf9\x04" + struct.pack("<BHB", packed, f.get("delay", 0) & 0xFFFF,
                                               (t or 0) & 255) + b"\x00"
        iflags = 0x40 if f.get("interlace") else 0
        lct = b""
        if f.get("local") is not None:
            bits, lct = colortable(f["local"])
            iflags |= 0x80 | bits
        d += b"\x2c" + struct.pack("<HHHHB", f["left"], f["top"], f["width"], f["height"], iflags)
        d += lct
        mcb = f.get("mcb", 8)
        d.append(mcb & 255)
        lzw = f.get("lzw")
        if lzw is None:
            px = f["pixels"]
            if f.get("interlace"):
                px = interlace_rows(px, f["width"], f["height"])
            lzw = lzw_encode(px, max(1, min(mcb, 11)))
        d += subblocks(lzw)
    d += trailer
    return bytes(d)


def interlace_order(height):
    return (list(range(0, height, 8)) + list(range(4, height, 8))
            + list(range(2, height, 4)) + list(range(1, height, 2)))


def interlace_rows(pixels, width, height):
    rows = [pixels[y * width:(y + 1) * width] for y in range(height)]
    return [p for y in interlace_order(height) for p in rows[y]]


# ---------------------------------------------------------------------------
# Random GIFs

def random_colors(rng, n):
    return [(rng.randrange(256), rng.randrange(256), rng.randrange(256)) for _ in range(n)]


def random_pixels(rng, w, h, ncolors):
    n = w * h
    ncolors = max(1, ncolors)
    kind = rng.choice(["noise", "runs", "gradient", "stripes", "const", "mixed", "repeat"])
    if kind == "noise":
        return [rng.randrange(ncolors) for _ in range(n)]
    if kind == "const":
        return [rng.randrange(ncolors)] * n
    if kind == "gradient":
        return [((i % max(w, 1)) * ncolors // max(w, 1) + i // max(w, 1)) % ncolors for i in range(n)]
    if kind == "stripes":
        k = rng.randint(1, 9)
        return [(i // k) % ncolors for i in range(n)]
    if kind == "repeat":
        pat = [rng.randrange(ncolors) for _ in range(rng.randint(1, 40))]
        return [pat[i % len(pat)] for i in range(n)]
    out = []
    while len(out) < n:
        if kind == "runs" or rng.random() < .5:
            out += [rng.randrange(ncolors)] * rng.choice([1, 2, 3, 7, 50, 300, 3000])
        else:
            out += [rng.randrange(ncolors) for _ in range(rng.randint(1, 2000))]
    return out[:n]


def random_gif(rng, max_screen=64, max_frames=5):
    """Return a random GIF as bytes. The first frame (which can grow the
    logical screen) stays near the origin, so the result never asks gifsicle
    for an enormous screen, but later frames may lie anywhere."""
    sw, sh = rng.randint(0 if rng.random() < .03 else 1, max_screen), rng.randint(1, max_screen)
    gct_n = rng.choice([None, 1, 2, 3, 4, 16, 17, 100, 256])
    gct = random_colors(rng, gct_n) if gct_n else None
    frames = []
    for k in range(rng.randint(1, max_frames)):
        w, h = rng.randint(1, max_screen), rng.randint(1, max_screen)
        if rng.random() < .03:
            w = 0
        pick = rng.random()
        if k == 0 or pick < .5:
            left, top = rng.randint(0, sw + 8), rng.randint(0, sh + 8)
        elif pick < .8:
            left, top = rng.randint(0, 4 * sw + 4), rng.randint(0, 4 * sh + 4)
        else:
            left, top = rng.randint(0, 65535 - w), rng.randint(0, 65535 - h)
        f = {"left": left, "top": top, "width": w, "height": h}
        local_n = rng.choice([None, None, None, 2, 5, 64, 256])
        if local_n or gct is None and rng.random() < .7:
            f["local"] = random_colors(rng, local_n or 4)
        ncolors = len(f.get("local") or gct or [0, 0])
        # occasionally use pixel values beyond the colormap
        f["pixels"] = random_pixels(rng, w, h, ncolors + (rng.randint(1, 8) if rng.random() < .1 else 0))
        need = max(f["pixels"] or [0]) + 1
        f["mcb"] = max(2, (need - 1).bit_length(), rng.choice([2, 2, 3, 8]))
        f["mcb"] = min(f["mcb"], 8) if rng.random() < .97 else rng.choice([1, 9, 11, 12])
        if rng.random() < .6:
            f["disposal"] = rng.randrange(8)
            f["delay"] = rng.choice([0, 0, 1, 10, 65535])
            if rng.random() < .6:
                f["transparent"] = rng.randrange(ncolors + 2 if rng.random() < .8 else 256)
        if rng.random() < .2:
            f["interlace"] = True
        if rng.random() < .05:
            f["comment"] = bytes(rng.randrange(256) for _ in range(rng.randint(0, 300)))
        if rng.random() < .1:
            f["lzw"] = lzw_encode([p & ((1 << max(f["mcb"], 1)) - 1) for p in f["pixels"]],
                                  max(1, min(f["mcb"], 11)),
                                  clear_every=rng.choice([None, 1, 5, 100]),
                                  defer_clear=rng.random() < .5,
                                  early_change=rng.random() < .3)
        frames.append(f)
    data = make_gif((sw, sh), gct, frames,
                    version=rng.choice([b"GIF89a", b"GIF89a", b"GIF87a"]),
                    background=rng.randrange(260),
                    loopcount=rng.choice([None, None, 0, 1, 65535]),
                    comment=b"fuzz" if rng.random() < .1 else None,
                    trailer=b";" if rng.random() < .9 else rng.choice([b"", b";garbage", b";;"]))
    # occasionally truncate the GIF
    if rng.random() < .08:
        data = data[:rng.randint(13, len(data) - 1)]
    return data


def extension(label, payload):
    return b"\x21" + bytes([label]) + subblocks(payload)


def special_gif(rng):
    """Return a GIF aimed at one of gifsicle's colormap, transparency,
    optimization, or extension edge cases."""
    kind = rng.choice(["transparent-past-end", "duplicates", "full-256",
                       "animation", "names", "extensions", "tiny-interlaced",
                       "identical-frames", "many-colors", "all-transparent"])
    sw, sh = rng.randint(1, 48), rng.randint(1, 48)
    nframes = rng.randint(1, 6)

    if kind == "transparent-past-end":
        # power-of-two colormap, every color used, transparent index at or
        # past its end
        n = rng.choice([2, 4, 8, 16, 256])
        gct = random_colors(rng, n)
        frames = []
        for _ in range(nframes):
            w, h = rng.randint(1, sw), rng.randint(1, sh)
            px = random_pixels(rng, w, h, n)
            px[:n] = list(range(n))[:len(px)]
            f = {"left": rng.randint(0, sw - w), "top": rng.randint(0, sh - h),
                 "width": w, "height": h, "mcb": max(2, (n - 1).bit_length())}
            f["transparent"] = rng.choice([n, n, n + 1, 255])
            f["pixels"] = px
            f["disposal"] = rng.randrange(4)
            frames.append(f)
        return make_gif((sw, sh), gct, frames, background=rng.choice([0, n, 255]))

    if kind == "duplicates":
        # colormaps with repeated colors, including the transparent slot
        base = random_colors(rng, rng.randint(1, 6))
        gct = [rng.choice(base) for _ in range(rng.choice([4, 8, 16]))]
        frames = []
        for _ in range(nframes):
            w, h = rng.randint(1, sw), rng.randint(1, sh)
            f = {"left": rng.randint(0, sw - w), "top": rng.randint(0, sh - h),
                 "width": w, "height": h, "pixels": random_pixels(rng, w, h, len(gct))}
            if rng.random() < .7:
                f["transparent"] = rng.randrange(len(gct))
                f["disposal"] = rng.randrange(4)
            if rng.random() < .3:
                f["local"] = [rng.choice(base) for _ in range(len(gct))]
            frames.append(f)
        return make_gif((sw, sh), gct, frames)

    if kind == "full-256":
        # 256 distinct colors in use, plus transparency: unoptimizing or
        # merging may need 257 colors
        gct = random_colors(rng, 256)
        frames = []
        for k in range(nframes):
            w, h = rng.randint(16, 48), rng.randint(16, 48)
            px = list(range(256)) * (w * h // 256 + 1)
            rng.shuffle(px)
            f = {"left": 0, "top": 0, "width": w, "height": h, "pixels": px[:w * h]}
            if k > 0 or rng.random() < .5:
                f["transparent"] = rng.randrange(256)
                f["disposal"] = rng.choice([0, 1, 2, 3])
            if rng.random() < .3:
                f["local"] = random_colors(rng, 256)
            frames.append(f)
        return make_gif((48, 48), gct, frames)

    if kind == "animation":
        # a sprite moving over a background, which the optimizer can crop
        # and make transparent
        gct = random_colors(rng, rng.choice([4, 16, 64]))
        n = len(gct)
        bg = random_pixels(rng, sw, sh, n)
        sprite_w, sprite_h = rng.randint(1, max(1, sw // 2)), rng.randint(1, max(1, sh // 2))
        sprite = random_pixels(rng, sprite_w, sprite_h, n)
        frames = []
        x, y = rng.randint(0, sw - sprite_w), rng.randint(0, sh - sprite_h)
        disposal = rng.choice([None, 0, 1, 2, 3])
        for k in range(rng.randint(2, 12)):
            px = list(bg)
            for j in range(sprite_h):
                for i in range(sprite_w):
                    if 0 <= x + i < sw and 0 <= y + j < sh:
                        px[(y + j) * sw + x + i] = sprite[j * sprite_w + i]
            f = {"left": 0, "top": 0, "width": sw, "height": sh, "pixels": px,
                 "delay": rng.choice([0, 2, 10])}
            if disposal is not None:
                f["disposal"] = disposal
            frames.append(f)
            x = max(0, min(sw - sprite_w, x + rng.randint(-3, 3)))
            y = max(0, min(sh - sprite_h, y + rng.randint(-3, 3)))
        return make_gif((sw, sh), gct, frames, loopcount=rng.choice([None, 0, 3]))

    if kind == "names":
        # named frames, for '#name' frame selections and --explode-by-name
        gct = random_colors(rng, 4)
        d = bytearray(make_gif((sw, sh), gct, [], trailer=b""))
        for k in range(nframes):
            w, h = rng.randint(1, sw), rng.randint(1, sh)
            name = rng.choice([b"a", b"b", b"frame%d" % k, b"", b"x" * 300, b"1", b"#2"])
            d += extension(0xCE, name)
            d += make_gif((sw, sh), None, [{"left": 0, "top": 0, "width": w, "height": h,
                                             "pixels": random_pixels(rng, w, h, 4)}],
                          trailer=b"")[13:]
        return bytes(d + b";")

    if kind == "extensions":
        # unusual extensions around and between frames
        gct = random_colors(rng, 4)
        d = bytearray(make_gif((sw, sh), gct, [], trailer=b""))
        for k in range(nframes):
            for _ in range(rng.randint(0, 3)):
                d += rng.choice([
                    extension(0xFF, b"NETSCAPE2.0" + b"\x01\x05\x00" + b"\x02\x00"),
                    b"\x21\xff\x0bNETSCAPE2.0\x05\x01\x00\x00\x00\x00\x00",
                    b"\x21\xff\x0bANIMEXTS1.0\x03\x01\x00\x00\x00",
                    b"\x21\xff\x0bXMP DataXMP" + subblocks(bytes(rng.randrange(256) for _ in range(40))),
                    b"\x21\xff\x08SHORTAPP\x00",
                    extension(rng.choice([0x01, 0x02, 0xCE, 0xFE, 0x99]),
                              bytes(rng.randrange(256) for _ in range(rng.randint(0, 600)))),
                    b"\x21\xf9\x04\x09\x00\x00\x02\x00",      # bare GCE
                    b"\x21\xf9\x05\x01\x00\x00\x00\x00\x00",  # long GCE
                ])
            w, h = rng.randint(1, sw), rng.randint(1, sh)
            d += make_gif((sw, sh), None, [{"left": 0, "top": 0, "width": w, "height": h,
                                             "pixels": random_pixels(rng, w, h, 4)}],
                          trailer=b"")[13:]
        return bytes(d + b";")

    if kind == "many-colors":
        # overlapping frames with different 256-color local colormaps, so
        # the optimizer's frames can need more than 256 colors
        sw, sh = rng.randint(10, 24), rng.randint(10, 24)
        frames = []
        for _ in range(rng.randint(2, 5)):
            w, h = rng.randint(1, sw), rng.randint(1, sh)
            px = random_pixels(rng, w, h, 256)
            px[:256] = list(range(256))[:len(px)]
            f = {"left": rng.randint(0, sw - w), "top": rng.randint(0, sh - h),
                 "width": w, "height": h, "pixels": px, "local": random_colors(rng, 256),
                 "disposal": rng.choice([1, 2, 2, 3]), "delay": 5}
            if rng.random() < .6:
                f["transparent"] = rng.randrange(256)
            frames.append(f)
        return make_gif((sw, sh), None if rng.random() < .7 else random_colors(rng, 4), frames)

    if kind == "all-transparent":
        # frames whose every pixel is transparent, so no color is used
        gct = random_colors(rng, rng.choice([2, 4]))
        frames = []
        for _ in range(nframes):
            w, h = rng.randint(1, sw), rng.randint(1, sh)
            t = rng.randrange(len(gct))
            frames.append({"left": rng.randint(0, sw - w), "top": rng.randint(0, sh - h),
                           "width": w, "height": h, "pixels": [t] * (w * h),
                           "transparent": t, "disposal": rng.randrange(4)})
        return make_gif((sw, sh), gct, frames, background=rng.randrange(len(gct)))

    if kind == "tiny-interlaced":
        gct = random_colors(rng, 16)
        frames = []
        for _ in range(nframes):
            w, h = rng.randint(1, 9), rng.randint(1, 9)
            frames.append({"left": rng.randint(0, 4), "top": rng.randint(0, 4),
                           "width": w, "height": h, "interlace": True,
                           "pixels": random_pixels(rng, w, h, 16)})
        return make_gif((12, 12), gct, frames)

    # identical-frames: repeated frames, which -O makes empty
    gct = random_colors(rng, 8)
    px = random_pixels(rng, sw, sh, 8)
    frames = []
    for k in range(rng.randint(2, 8)):
        f = {"left": 0, "top": 0, "width": sw, "height": sh, "pixels": px,
             "delay": rng.choice([0, 5, 10]), "disposal": rng.choice([0, 1, 2, 3])}
        if rng.random() < .4:
            f["transparent"] = rng.randrange(9)
        frames.append(f)
    return make_gif((sw, sh), gct, frames)


def concatenated_gif(rng):
    """Return two or three GIFs back to back, for --multifile."""
    return b"".join(rng.choice([random_gif, special_gif])(rng) for _ in range(rng.randint(2, 3)))


# ---------------------------------------------------------------------------
# Mutation

INTERESTING16 = [0, 1, 2, 3, 4, 7, 8, 15, 16, 255, 256, 4095, 4096, 32767, 32768, 65534, 65535]


def mutate(data, rng, nmut=None):
    d = bytearray(data)
    for _ in range(nmut or rng.randint(1, 6)):
        pos = rng.randrange(len(d)) if d else 0
        op = rng.randrange(8)
        if op == 0 and d:
            d[pos] ^= 1 << rng.randrange(8)
        elif op == 1 and d:
            d[pos] = rng.choice([0, 1, 2, 7, 8, 11, 12, 0x7f, 0x80, 0xff])
        elif op == 2 and len(d) >= 2:
            pos = rng.randrange(len(d) - 1)
            d[pos:pos + 2] = struct.pack("<H", rng.choice(INTERESTING16))
        elif op == 3 and d:
            d[pos] = rng.randrange(256)
        elif op == 4 and d:
            n = rng.randint(1, 64)
            del d[pos:pos + n]
        elif op == 5:
            n = rng.randint(1, 64)
            d[pos:pos] = d[pos:pos + n]
        elif op == 6 and len(d) > 13:
            d = d[:rng.randint(13, len(d))]
        elif op == 7:
            d[pos:pos] = bytes(rng.randrange(256) for _ in range(rng.randint(1, 16)))
    return bytes(d)


# ---------------------------------------------------------------------------
# Strict checking (a port of gifcheck.c)

def _lzw_decode(data, mcb, npixels):
    clear, eoi = 1 << mcb, (1 << mcb) + 1
    bits = int.from_bytes(data, "little")
    nbits = len(data) * 8
    pos = 0
    width, nxt, prev = mcb + 1, clear + 2, None
    table = {}
    out = []
    first = True
    while True:
        if pos + width > nbits:
            return None, "LZW data ends before EOI (or EOI is truncated)"
        code = (bits >> pos) & ((1 << width) - 1)
        pos += width
        if first and code != clear:
            return None, "LZW data does not start with CLEAR"
        first = False
        if code == clear:
            width, nxt, prev, table = mcb + 1, clear + 2, None, {}
            continue
        if code == eoi:
            break

        def string(c):
            return [c] if c < clear else table[c]
        if prev is None:
            if code > clear:
                return None, "first code after CLEAR is not a literal"
            cur = [code]
        elif code < nxt:
            cur = string(code)
        elif code == nxt and nxt < 4096:
            cur = string(prev) + [string(prev)[0]]
        else:
            return None, "LZW code beyond end of table"
        if prev is not None and nxt < 4096:
            table[nxt] = string(prev) + [cur[0]]
            nxt += 1
            if nxt == (1 << width) and width < 12:
                width += 1
        out += cur
        if len(out) > npixels:
            return None, "LZW data decodes to too many pixels"
        prev = code
    if len(out) != npixels:
        return None, "LZW data decodes to too few pixels"
    if nbits - pos >= 8:
        return None, "unexpected data after EOI"
    return out, None


def strict_check(data):
    """Return None if `data` is a well-formed GIF that strict decoders decode
    exactly, otherwise a description of the first problem."""
    d = data
    if len(d) < 13 or d[:6] not in (b"GIF87a", b"GIF89a"):
        return "bad GIF header"
    p = 13
    if d[10] & 0x80:
        p += 3 * (2 << (d[10] & 7))
    nimage = 0
    try:
        while True:
            t = d[p]
            if t == 0x3B:
                p += 1
                break
            if t == 0x21:
                label = d[p + 1]
                p += 2
                if label == 0xF9 and d[p] != 4:
                    return "malformed graphic control extension"
                while d[p]:
                    p += 1 + d[p]
                p += 1
            elif t == 0x2C:
                w, h = struct.unpack("<HH", d[p + 5:p + 9])
                left, top = struct.unpack("<HH", d[p + 1:p + 5])
                flags = d[p + 9]
                p += 10
                if flags & 0x80:
                    p += 3 * (2 << (flags & 7))
                if left + w > 65535 or top + h > 65535:
                    return "image #%d extends beyond 65535x65535" % nimage
                mcb = d[p]
                p += 1
                if not 2 <= mcb <= 8:
                    return "image #%d: bad LZW minimum code size %d" % (nimage, mcb)
                lzw = bytearray()
                while d[p]:
                    lzw += d[p + 1:p + 1 + d[p]]
                    if p + 1 + d[p] > len(d):
                        return "image #%d: truncated image data" % nimage
                    p += 1 + d[p]
                p += 1
                _, err = _lzw_decode(bytes(lzw), mcb, w * h)
                if err:
                    return "image #%d (%dx%d): %s" % (nimage, w, h, err)
                nimage += 1
            else:
                return "unknown block type 0x%02x" % t
    except IndexError:
        return "missing GIF trailer"
    if p != len(d):
        return "data after GIF trailer"
    return None


if __name__ == "__main__":
    # self-test: generated GIFs with correct LZW must pass the strict check
    rng = random.Random(1)
    good = 0
    for i in range(200):
        r = random.Random(i)
        g = random_gif(r)
        if strict_check(g) is None:
            good += 1
    print("%d of 200 random GIFs pass the strict check (some are deliberately malformed)" % good)
    px = [rng.randrange(4) for _ in range(5000)]
    assert strict_check(make_gif((50, 100), [(0, 0, 0)] * 4,
                                 [{"left": 0, "top": 0, "width": 50, "height": 100,
                                   "pixels": px, "mcb": 2}])) is None
    bad = lzw_encode(px, 2, early_change=True)
    gif = make_gif((50, 100), [(0, 0, 0)] * 4,
                   [{"left": 0, "top": 0, "width": 50, "height": 100, "pixels": px,
                     "mcb": 2, "lzw": bad}])
    assert strict_check(gif) is not None
    print("self-test passed")
