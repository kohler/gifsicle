/* gifcheck.c - Strict, independent GIF parser for checking gifsicle output.
   This file is part of gifsicle's fuzzing harness. See gifcheck.h. */

#include "gifcheck.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    const uint8_t* p;
    const uint8_t* end;
} reader;

static int rd_byte(reader* r) {
    return r->p < r->end ? *r->p++ : -1;
}

static int rd_u16(reader* r) {
    int lo = rd_byte(r), hi = rd_byte(r);
    return lo < 0 || hi < 0 ? -1 : lo | (hi << 8);
}

/* Skip a sequence of data sub-blocks, including the zero terminator. */
static int skip_subblocks(reader* r) {
    int n;
    while ((n = rd_byte(r)) > 0) {
        if (r->end - r->p < n)
            return 0;
        r->p += n;
    }
    return n == 0;
}

/* Row `r` of an interlaced image is stored in output row interlace_row(r). */
static int interlace_row(int r, int height) {
    static const int start[4] = {0, 4, 2, 1}, step[4] = {8, 8, 4, 2};
    int pass;
    for (pass = 0; pass < 4; ++pass) {
        int n = height > start[pass]
            ? (height - start[pass] + step[pass] - 1) / step[pass] : 0;
        if (r < n)
            return start[pass] + r * step[pass];
        r -= n;
    }
    return -1;
}

/* Strictly decode an LZW stream. Code width grows as soon as the table
   reaches a power of two (as in giflib and browsers). The stream must start
   with CLEAR, must end with a complete EOI, must decode to exactly `npixels`
   pixels, and may have fewer than 8 bits of padding after EOI. */
static const char* lzw_decode(const uint8_t* data, size_t len, int mcb,
                              uint8_t* out, size_t npixels) {
    uint16_t prefix[4096];
    uint8_t suffix[4096], first[4096];
    uint16_t length[4096];
    int clear = 1 << mcb, eoi = clear + 1;
    int width = mcb + 1, next = clear + 2, prev = -1, ncodes = 0, i;
    size_t bitpos = 0, nbits = len * 8, pos = 0;

    for (i = 0; i < clear; ++i) {
        prefix[i] = 0;
        suffix[i] = first[i] = (uint8_t) i;
        length[i] = 1;
    }

    for (;;) {
        int code = 0, b, cur;
        if (bitpos + width > nbits)
            return "LZW data ends before EOI (or EOI is truncated)";
        for (b = 0; b < width; ++b, ++bitpos)
            code |= ((data[bitpos >> 3] >> (bitpos & 7)) & 1) << b;
        ++ncodes;

        if (ncodes == 1 && code != clear)
            return "LZW data does not start with CLEAR";
        if (code == clear) {
            width = mcb + 1;
            next = clear + 2;
            prev = -1;
            continue;
        }
        if (code == eoi)
            break;
        if (prev < 0) {
            if (code > clear)
                return "first code after CLEAR is not a literal";
            cur = code;
        } else if (code < next) {
            cur = code;
        } else if (code == next && next < 4096) {
            cur = -1;           /* KwKwK */
        } else
            return "LZW code beyond end of table";

        if (prev >= 0 && next < 4096) {
            prefix[next] = (uint16_t) prev;
            first[next] = first[prev];
            suffix[next] = cur < 0 ? first[prev] : first[cur];
            length[next] = (uint16_t) (length[prev] + 1);
            if (cur < 0)
                cur = next;
            ++next;
            if (next == (1 << width) && width < 12)
                ++width;
        }

        /* emit string for `cur` */
        if (npixels - pos < length[cur])
            return "LZW data decodes to too many pixels";
        {
            int c = cur;
            size_t k = pos + length[cur];
            while (k > pos) {
                out[--k] = suffix[c];
                c = prefix[c];
            }
            pos += length[cur];
        }
        prev = code;
    }

    if (pos != npixels)
        return "LZW data decodes to too few pixels";
    if (nbits - bitpos >= 8)
        return "unexpected data after EOI";
    return NULL;
}

static const char* parse_image(reader* r, gc_file* gf, const int gce[3]) {
    gc_image* gi;
    int flags, mcb, n;
    size_t npixels, datalen = 0, datacap = 256;
    uint8_t* data;
    uint8_t* raw;
    const char* err;
    int y;

    gf->images = (gc_image*) realloc(gf->images,
                                     sizeof(gc_image) * (gf->nimages + 1));
    gi = &gf->images[gf->nimages];
    memset(gi, 0, sizeof(*gi));
    gi->left = rd_u16(r);
    gi->top = rd_u16(r);
    gi->width = rd_u16(r);
    gi->height = rd_u16(r);
    flags = rd_byte(r);
    if (flags < 0)
        return "truncated image descriptor";
    gi->interlace = (flags & 0x40) != 0;
    if (flags & 0x80) {
        gi->has_local_colormap = 1;
        gi->local_colormap_size = 2 << (flags & 7);
        if (r->end - r->p < 3 * gi->local_colormap_size)
            return "truncated local colormap";
        gi->local_colormap = r->p;
        r->p += 3 * gi->local_colormap_size;
    }
    gi->transparent = gce[0];
    gi->disposal = gce[1];
    gi->delay = gce[2];
    ++gf->nimages;              /* gc_free frees gi->pixels from here on */

    if (gi->left + gi->width > 65535 || gi->top + gi->height > 65535)
        return "image extends beyond 65535x65535";

    mcb = rd_byte(r);
    if (mcb < 2 || mcb > 8)
        return "bad LZW minimum code size";
    gi->min_code_size = mcb;

    data = (uint8_t*) malloc(datacap);
    while ((n = rd_byte(r)) > 0) {
        if (r->end - r->p < n) {
            free(data);
            return "truncated image data sub-block";
        }
        if (datalen + n > datacap) {
            while (datalen + n > datacap)
                datacap *= 2;
            data = (uint8_t*) realloc(data, datacap);
        }
        memcpy(data + datalen, r->p, n);
        datalen += n;
        r->p += n;
    }
    if (n < 0) {
        free(data);
        return "missing image data terminator";
    }

    npixels = (size_t) gi->width * gi->height;
    raw = (uint8_t*) malloc(npixels ? npixels : 1);
    err = lzw_decode(data, datalen, mcb, raw, npixels);
    free(data);
    if (err) {
        free(raw);
        return err;
    }

    if (!gi->interlace || gi->height <= 1)
        gi->pixels = raw;
    else {
        gi->pixels = (uint8_t*) malloc(npixels);
        for (y = 0; y < gi->height; ++y)
            memcpy(gi->pixels + (size_t) interlace_row(y, gi->height) * gi->width,
                   raw + (size_t) y * gi->width, gi->width);
        free(raw);
    }
    return NULL;
}

const char* gc_parse(const uint8_t* data, size_t len, gc_file* gf) {
    reader r;
    int flags, t;
    int gce[3] = {-1, 0, 0};    /* transparent, disposal, delay */
    const char* err;

    memset(gf, 0, sizeof(*gf));
    gf->loopcount = -1;
    r.p = data;
    r.end = data + len;

    if (len < 13 || (memcmp(data, "GIF87a", 6) != 0
                     && memcmp(data, "GIF89a", 6) != 0))
        return "bad GIF header";
    r.p += 6;
    gf->screen_width = rd_u16(&r);
    gf->screen_height = rd_u16(&r);
    flags = rd_byte(&r);
    r.p += 2;                   /* background, aspect ratio */
    if (flags & 0x80) {
        gf->global_colormap_size = 2 << (flags & 7);
        if (r.end - r.p < 3 * gf->global_colormap_size)
            return "truncated global colormap";
        gf->global_colormap = r.p;
        r.p += 3 * gf->global_colormap_size;
    }

    while ((t = rd_byte(&r)) != 0x3B) {
        if (t == 0x2C) {
            if ((err = parse_image(&r, gf, gce)))
                return err;
            gce[0] = -1;
            gce[1] = gce[2] = 0;
        } else if (t == 0x21) {
            int label = rd_byte(&r);
            if (label == 0xF9) {
                /* graphic control extension: exactly one 4-byte block */
                int size = rd_byte(&r), packed = rd_byte(&r),
                    delay = rd_u16(&r), transp = rd_byte(&r);
                if (size != 4 || transp < 0 || rd_byte(&r) != 0)
                    return "malformed graphic control extension";
                gce[0] = (packed & 1) ? transp : -1;
                gce[1] = (packed >> 2) & 7;
                gce[2] = delay;
            } else if (label == 0xFF && r.end - r.p >= 12 && r.p[0] == 11
                       && memcmp(r.p + 1, "NETSCAPE2.0", 11) == 0) {
                /* loop count: a 3-byte sub-block {1, count (2 bytes)} */
                r.p += 12;
                if (r.end - r.p >= 4 && r.p[0] == 3 && r.p[1] == 1)
                    gf->loopcount = r.p[2] | (r.p[3] << 8);
                if (!skip_subblocks(&r))
                    return "truncated extension";
            } else if (label < 0 || !skip_subblocks(&r))
                return "truncated extension";
        } else if (t < 0)
            return "missing GIF trailer";
        else
            return "unknown block type";
    }
    if (r.p != r.end)
        return "data after GIF trailer";
    return NULL;
}

void gc_free(gc_file* gf) {
    int i;
    for (i = 0; i < gf->nimages; ++i)
        free(gf->images[i].pixels);
    free(gf->images);
    memset(gf, 0, sizeof(*gf));
}
