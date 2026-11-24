/* budget.c - Keep gifsicle's runs within the fuzzing budget.
   This file is part of gifsicle's fuzzing harness. See README.md.

   Command lines can legitimately ask for enormous outputs, such as
   `--resize 60000x60000`, or `--logical-screen 65535x65535 -O2`, whose
   optimizer allocates screen-sized buffers. Running such commands only
   measures the machine's memory and patience, and fuzzing services report
   them as out-of-memory or timeout bugs. The targets check their input
   GIFs against the budget before running (fuzz_within_budget), but the
   command line can grow the screen and the images in many ways.

   So gifsicle.c, compiled with GIFSICLE_FUZZ_HOOK_CALLS (see inprocess.h),
   calls these wrappers instead of the operations whose cost grows with the
   screen size. A wrapper whose operation would exceed the budget skips it
   and sets fuzz_over_budget, and runner.c ignores the run. Each wrapper
   first calls Gif_CalculateScreenSize(gfs, 0), as the operation it wraps
   does, so a run within the budget behaves exactly as it would otherwise. */

#include <config.h>
#include "runner.h"
#include "gifsicle.h"

int fuzz_over_budget;

/* Return 1 if a gfs->nimages-frame stream with a w x h screen is too
   expensive to resize, optimize, or unoptimize. Unoptimizing makes every
   frame screen-sized, and the optimizer and the resizer do work
   proportional to the screen size for every frame. */
static int over_budget(const Gif_Stream* gfs, unsigned long w, unsigned long h) {
    unsigned long long px = (unsigned long long) w * h;
    if (px > FUZZ_MAX_FRAME_PIXELS
        || px * (unsigned) gfs->nimages > FUZZ_MAX_SCREEN_WORK) {
        fuzz_over_budget = 1;
        return 1;
    }
    return 0;
}

int gifsicle_fuzz_resize_stream(Gif_Stream* gfs, double new_width, double new_height,
                                int flags, int method, int scale_colors,
                                int nthreads) {
    int w, h;
    Gif_CalculateScreenSize(gfs, 0);
    w = gfs->screen_width;
    h = gfs->screen_height;
    /* resize_stream reports results too large for a GIF itself */
    if (gfs->nimages > 0 && w > 0 && h > 0
        && resize_dimensions(&w, &h, new_width, new_height, flags) == 0
        && over_budget(gfs, w, h))
        return -1;
    return resize_stream(gfs, new_width, new_height, flags, method,
                         scale_colors, nthreads);
}

void gifsicle_fuzz_optimize_fragments(Gif_Stream* gfs, const Gt_OutputData* od,
                                      int huge_stream) {
    Gif_CalculateScreenSize(gfs, 0);
    if (!over_budget(gfs, gfs->screen_width, gfs->screen_height))
        optimize_fragments(gfs, od, huge_stream);
}

int gifsicle_fuzz_full_unoptimize(Gif_Stream* gfs, int flags) {
    Gif_CalculateScreenSize(gfs, 0);
    if (over_budget(gfs, gfs->screen_width, gfs->screen_height))
        return 1;
    return Gif_FullUnoptimize(gfs, flags);
}
