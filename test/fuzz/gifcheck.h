/* gifcheck.h - Strict, independent GIF parser for checking gifsicle output.
   This file is part of gifsicle's fuzzing harness.

   gifsicle's own reader is deliberately lenient, so it cannot be trusted to
   find bugs in gifsicle's own writer. This parser shares no code with
   gifsicle. It accepts only well-formed GIFs that a strict decoder (giflib,
   browsers, Wuffs) would decode exactly. */

#ifndef GIFSICLE_FUZZ_GIFCHECK_H
#define GIFSICLE_FUZZ_GIFCHECK_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int left, top, width, height;
    int interlace;
    int min_code_size;
    int has_local_colormap;
    int local_colormap_size;    /* entries, 0 if no local colormap */
    const uint8_t* local_colormap; /* RGB triples, pointing into the data */
    int transparent;            /* -1 if none */
    int disposal;
    int delay;
    uint8_t* pixels;            /* width*height, row-major, deinterlaced */
} gc_image;

typedef struct {
    int screen_width, screen_height;
    int global_colormap_size;   /* entries, 0 if no global colormap */
    const uint8_t* global_colormap; /* RGB triples, pointing into the data */
    int loopcount;              /* from a NETSCAPE2.0 extension; -1 if none */
    int nimages;
    gc_image* images;
} gc_file;

/* Parse and fully decode `data`. Returns NULL on success, or a static
   description of the first problem found. Either way, call gc_free(). */
const char* gc_parse(const uint8_t* data, size_t len, gc_file* gf);
void gc_free(gc_file* gf);

#ifdef __cplusplus
}
#endif
#endif
