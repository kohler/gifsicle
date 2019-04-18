/* quantize.c - Histograms and quantization for gifsicle.
   Copyright (C) 1997-2019 Eddie Kohler, ekohler@gmail.com
   This file is part of gifsicle.

   Gifsicle is free software. It is distributed under the GNU Public License,
   version 2; you can copy, distribute, or alter it at will, as long
   as this notice is kept intact and this source code is made available. There
   is no warranty, express or implied. */

#include <config.h>
#include "gifsicle.h"
#include "kcolor.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <math.h>

/* Invariant: (0<=x<256) ==> (srgb_revgamma[srgb_gamma[x] >> 7] <= x). */

static const uint16_t srgb_gamma_table_256[256] = {
    0, 10, 20, 30, 40, 50, 60, 70,
    80, 90, 99, 110, 120, 132, 144, 157,
    170, 184, 198, 213, 229, 246, 263, 281,
    299, 319, 338, 359, 380, 403, 425, 449,
    473, 498, 524, 551, 578, 606, 635, 665,
    695, 727, 759, 792, 825, 860, 895, 931,
    968, 1006, 1045, 1085, 1125, 1167, 1209, 1252,
    1296, 1341, 1386, 1433, 1481, 1529, 1578, 1629,
    1680, 1732, 1785, 1839, 1894, 1950, 2007, 2065,
    2123, 2183, 2244, 2305, 2368, 2432, 2496, 2562,
    2629, 2696, 2765, 2834, 2905, 2977, 3049, 3123,
    3198, 3273, 3350, 3428, 3507, 3587, 3668, 3750,
    3833, 3917, 4002, 4088, 4176, 4264, 4354, 4444,
    4536, 4629, 4723, 4818, 4914, 5011, 5109, 5209,
    5309, 5411, 5514, 5618, 5723, 5829, 5936, 6045,
    6154, 6265, 6377, 6490, 6604, 6720, 6836, 6954,
    7073, 7193, 7315, 7437, 7561, 7686, 7812, 7939,
    8067, 8197, 8328, 8460, 8593, 8728, 8863, 9000,
    9139, 9278, 9419, 9560, 9704, 9848, 9994, 10140,
    10288, 10438, 10588, 10740, 10893, 11048, 11204, 11360,
    11519, 11678, 11839, 12001, 12164, 12329, 12495, 12662,
    12831, 13000, 13172, 13344, 13518, 13693, 13869, 14047,
    14226, 14406, 14588, 14771, 14955, 15141, 15328, 15516,
    15706, 15897, 16089, 16283, 16478, 16675, 16872, 17071,
    17272, 17474, 17677, 17882, 18088, 18295, 18504, 18714,
    18926, 19138, 19353, 19569, 19786, 20004, 20224, 20445,
    20668, 20892, 21118, 21345, 21573, 21803, 22034, 22267,
    22501, 22736, 22973, 23211, 23451, 23692, 23935, 24179,
    24425, 24672, 24920, 25170, 25421, 25674, 25928, 26184,
    26441, 26700, 26960, 27222, 27485, 27749, 28016, 28283,
    28552, 28823, 29095, 29368, 29643, 29920, 30197, 30477,
    30758, 31040, 31324, 31610, 31897, 32185, 32475, 32767
};

static const uint16_t srgb_revgamma_table_256[256] = {
    0, 1628, 2776, 3619, 4309, 4904, 5434, 5914,
    6355, 6765, 7150, 7513, 7856, 8184, 8497, 8798,
    9086, 9365, 9634, 9895, 10147, 10393, 10631, 10864,
    11091, 11312, 11528, 11739, 11946, 12148, 12347, 12541,
    12732, 12920, 13104, 13285, 13463, 13639, 13811, 13981,
    14149, 14314, 14476, 14637, 14795, 14951, 15105, 15257,
    15408, 15556, 15703, 15848, 15991, 16133, 16273, 16412,
    16549, 16685, 16819, 16953, 17084, 17215, 17344, 17472,
    17599, 17725, 17849, 17973, 18095, 18217, 18337, 18457,
    18575, 18692, 18809, 18925, 19039, 19153, 19266, 19378,
    19489, 19600, 19710, 19819, 19927, 20034, 20141, 20247,
    20352, 20457, 20560, 20664, 20766, 20868, 20969, 21070,
    21170, 21269, 21368, 21466, 21564, 21661, 21758, 21854,
    21949, 22044, 22138, 22232, 22326, 22418, 22511, 22603,
    22694, 22785, 22875, 22965, 23055, 23144, 23232, 23321,
    23408, 23496, 23583, 23669, 23755, 23841, 23926, 24011,
    24095, 24180, 24263, 24347, 24430, 24512, 24595, 24676,
    24758, 24839, 24920, 25001, 25081, 25161, 25240, 25319,
    25398, 25477, 25555, 25633, 25710, 25788, 25865, 25941,
    26018, 26094, 26170, 26245, 26321, 26396, 26470, 26545,
    26619, 26693, 26766, 26840, 26913, 26986, 27058, 27130,
    27202, 27274, 27346, 27417, 27488, 27559, 27630, 27700,
    27770, 27840, 27910, 27979, 28048, 28117, 28186, 28255,
    28323, 28391, 28459, 28527, 28594, 28661, 28728, 28795,
    28862, 28928, 28995, 29061, 29127, 29192, 29258, 29323,
    29388, 29453, 29518, 29582, 29646, 29711, 29775, 29838,
    29902, 29965, 30029, 30092, 30155, 30217, 30280, 30342,
    30404, 30466, 30528, 30590, 30652, 30713, 30774, 30835,
    30896, 30957, 31017, 31078, 31138, 31198, 31258, 31318,
    31378, 31437, 31497, 31556, 31615, 31674, 31733, 31791,
    31850, 31908, 31966, 32024, 32082, 32140, 32198, 32255,
    32313, 32370, 32427, 32484, 32541, 32598, 32654, 32711
};

uint16_t* gamma_tables[2] = {
    (uint16_t*) srgb_gamma_table_256,
    (uint16_t*) srgb_revgamma_table_256
};

#if ENABLE_THREADS
pthread_mutex_t kd3_sort_lock;
#endif


const char* kc_debug_str(kcolor x) {
    static int whichbuf = 0;
    static char buf[4][32];
    whichbuf = (whichbuf + 1) % 4;
    if (x.a[0] >= 0 && x.a[1] >= 0 && x.a[2] >= 0) {
        kc_revgamma_transform(&x);
        sprintf(buf[whichbuf], "#%02X%02X%02X",
                x.a[0] >> 7, x.a[1] >> 7, x.a[2] >> 7);
    } else
        sprintf(buf[whichbuf], "<%d,%d,%d>", x.a[0], x.a[1], x.a[2]);
    return buf[whichbuf];
}

void kc_set_gamma(int type, double gamma) {
#if HAVE_POW
    static int cur_type = KC_GAMMA_SRGB;
    static double cur_gamma = 2.2;
    int i, j;
    if (type == cur_type && (type != KC_GAMMA_NUMERIC || gamma == cur_gamma))
        return;
    if (type == KC_GAMMA_SRGB) {
        if (gamma_tables[0] != srgb_gamma_table_256) {
            Gif_DeleteArray(gamma_tables[0]);
            Gif_DeleteArray(gamma_tables[1]);
        }
        gamma_tables[0] = (uint16_t*) srgb_gamma_table_256;
        gamma_tables[1] = (uint16_t*) srgb_revgamma_table_256;
    } else {
        if (gamma_tables[0] == srgb_gamma_table_256) {
            gamma_tables[0] = Gif_NewArray(uint16_t, 256);
            gamma_tables[1] = Gif_NewArray(uint16_t, 256);
        }
        for (j = 0; j != 256; ++j) {
            gamma_tables[0][j] = (int) (pow(j/255.0, gamma) * 32767 + 0.5);
            gamma_tables[1][j] = (int) (pow(j/256.0, 1/gamma) * 32767 + 0.5);
            /* The ++gamma_tables[][] ensures that round-trip gamma correction
               always preserve the input colors. Without it, one might have,
               for example, input values 0, 1, and 2 all mapping to
               gamma-corrected value 0. Then a round-trip through gamma
               correction loses information.
            */
            for (i = 0; i != 2; ++i)
                while (j && gamma_tables[i][j] <= gamma_tables[i][j-1]
                       && gamma_tables[i][j] < 32767)
                    ++gamma_tables[i][j];
        }
    }
    cur_type = type;
    cur_gamma = gamma;
#else
    (void) type, (void) gamma;
#endif
}

void kc_revgamma_transform(kcolor* x) {
    int d;
    for (d = 0; d != 3; ++d) {
        int c = gamma_tables[1][x->a[d] >> 7];
        while (c < 0x7F80 && x->a[d] >= gamma_tables[0][(c + 0x80) >> 7])
            c += 0x80;
        x->a[d] = c;
    }
}

#if 0
static void kc_test_gamma() {
    int x, y, z;
    for (x = 0; x != 256; ++x)
        for (y = 0; y != 256; ++y)
            for (z = 0; z != 256; ++z) {
                kcolor k;
                kc_set8g(&k, x, y, z);
                kc_revgamma_transform(&k);
                if ((k.a[0] >> 7) != x || (k.a[1] >> 7) != y
                    || (k.a[2] >> 7) != z) {
                    kcolor kg;
                    kc_set8g(&kg, x, y, z);
                    fprintf(stderr, "#%02X%02X%02X ->g #%04X%04X%04X ->revg #%02X%02X%02X!\n",
                            x, y, z, kg.a[0], kg.a[1], kg.a[2],
                            k.a[0] >> 7, k.a[1] >> 7, k.a[2] >> 7);
                    assert(0);
                }
            }
}
#endif


static int kchist_sizes[] = {
    4093, 16381, 65521, 262139, 1048571, 4194301, 16777213,
    67108859, 268435459, 1073741839
};

static void kchist_grow(kchist* kch);

void kchist_init(kchist* kch) {
    int i;
    kch->h = Gif_NewArray(kchistitem, kchist_sizes[0]);
    kch->n = 0;
    kch->capacity = kchist_sizes[0];
    for (i = 0; i != kch->capacity; ++i)
        kch->h[i].count = 0;
}

void kchist_cleanup(kchist* kch) {
    Gif_DeleteArray(kch->h);
    kch->h = NULL;
}

kchistitem* kchist_add(kchist* kch, kcolor k, kchist_count_t count) {
    unsigned hash1, hash2 = 0;
    kacolor ka;
    kchistitem *khi;
    ka.k = k;
    ka.a[3] = 0;

    if (!kch->capacity
        || kch->n > ((kch->capacity * 3) >> 4))
        kchist_grow(kch);
    hash1 = (((ka.a[0] & 0x7FE0) << 15)
             | ((ka.a[1] & 0x7FE0) << 5)
             | ((ka.a[2] & 0x7FE0) >> 5)) % kch->capacity;

    while (1) {
        khi = &kch->h[hash1];
        if (!khi->count
            || memcmp(&khi->ka, &ka, sizeof(ka)) == 0)
            break;
        if (!hash2) {
            hash2 = (((ka.a[0] & 0x03FF) << 20)
                     | ((ka.a[1] & 0x03FF) << 10)
                     | (ka.a[2] & 0x03FF)) % kch->capacity;
            hash2 = hash2 ? hash2 : 1;
        }
        hash1 += hash2;
        if (hash1 >= (unsigned) kch->capacity)
            hash1 -= kch->capacity;
    }

    if (!khi->count) {
        khi->ka = ka;
        ++kch->n;
    }
    khi->count += count;
    if (khi->count < count)
        khi->count = (kchist_count_t) -1;
    return khi;
}

static void kchist_grow(kchist* kch) {
    kchistitem* oldh = kch->h;
    int i, oldcapacity = kch->capacity ? kch->capacity : kch->n;
    for (i = 0; kchist_sizes[i] <= oldcapacity; ++i)
        /* do nothing */;
    kch->capacity = kchist_sizes[i];
    kch->h = Gif_NewArray(kchistitem, kch->capacity);
    kch->n = 0;
    for (i = 0; i != kch->capacity; ++i)
        kch->h[i].count = 0;
    for (i = 0; i != oldcapacity; ++i)
        if (oldh[i].count)
            kchist_add(kch, oldh[i].ka.k, oldh[i].count);
    Gif_DeleteArray(oldh);
}

void kchist_compress(kchist* kch) {
    int i, j;
    for (i = 0, j = kch->n; i != kch->n; )
        if (kch->h[i].count)
            ++i;
        else if (kch->h[j].count) {
            kch->h[i] = kch->h[j];
            ++i, ++j;
        } else
            ++j;
    kch->capacity = 0;
}

void kchist_make(kchist* kch, Gif_Stream* gfs, uint32_t* ntransp_store) {
    uint32_t gcount[256], lcount[256];
    uint32_t nbackground = 0, ntransparent = 0;
    int x, y, i, imagei;
    kchist_init(kch);

    for (i = 0; i != 256; ++i)
        gcount[i] = 0;

    /* Count pixels */
    for (imagei = 0; imagei < gfs->nimages; ++imagei) {
        Gif_Image* gfi = gfs->images[imagei];
        Gif_Colormap* gfcm = gfi->local ? gfi->local : gfs->global;
        uint32_t* count = gfi->local ? lcount : gcount;
        uint32_t old_transparent_count = 0;
        int only_compressed = (gfi->img == 0);
        if (!gfcm)
            continue;
        if (count == lcount)
            for (i = 0; i != 256; ++i)
                count[i] = 0;
        if (gfi->transparent >= 0)
            old_transparent_count = count[gfi->transparent];

        /* unoptimize the image if necessary */
        if (only_compressed)
            Gif_UncompressImage(gfs, gfi);

        /* sweep over the image data, counting pixels */
        for (y = 0; y < gfi->height; ++y) {
            const uint8_t* data = gfi->img[y];
            for (x = 0; x < gfi->width; ++x, ++data)
                ++count[*data];
        }

        /* add counted colors to global histogram (local only) */
        if (gfi->local)
            for (i = 0; i != gfcm->ncol; ++i)
                if (count[i] && i != gfi->transparent)
                    kchist_add(kch, kc_makegfcg(&gfcm->col[i]), count[i]);
        if (gfi->transparent >= 0
            && count[gfi->transparent] != old_transparent_count) {
            ntransparent += count[gfi->transparent] - old_transparent_count;
            count[gfi->transparent] = old_transparent_count;
        }

        /* if this image has background disposal, count its size towards the
           background's pixel count */
        if (gfi->disposal == GIF_DISPOSAL_BACKGROUND)
            nbackground += (unsigned) gfi->width * (unsigned) gfi->height;

        /* throw out compressed image if necessary */
        if (only_compressed)
            Gif_ReleaseUncompressedImage(gfi);
    }

    if (gfs->images[0]->transparent < 0
        && gfs->global && gfs->background < gfs->global->ncol)
        gcount[gfs->background] += nbackground;
    else
        ntransparent += nbackground;

    if (gfs->global)
        for (i = 0; i != gfs->global->ncol; ++i)
            if (gcount[i])
                kchist_add(kch, kc_makegfcg(&gfs->global->col[i]), gcount[i]);

    /* now, make the linear histogram from the hashed histogram */
    kchist_compress(kch);
    *ntransp_store = ntransparent;
}

static int red_kchistitem_compare(const void* va, const void* vb) {
    const kchistitem* a = (const kchistitem*) va;
    const kchistitem* b = (const kchistitem*) vb;
    return a->ka.a[0] - b->ka.a[0];
}

static int green_kchistitem_compare(const void* va, const void* vb) {
    const kchistitem* a = (const kchistitem*) va;
    const kchistitem* b = (const kchistitem*) vb;
    return a->ka.a[1] - b->ka.a[1];
}

static int blue_kchistitem_compare(const void* va, const void* vb) {
    const kchistitem* a = (const kchistitem*) va;
    const kchistitem* b = (const kchistitem*) vb;
    return a->ka.a[2] - b->ka.a[2];
}

static int popularity_kchistitem_compare(const void* va, const void* vb) {
    const kchistitem* a = (const kchistitem*) va;
    const kchistitem* b = (const kchistitem*) vb;
    return a->count > b->count ? -1 : a->count != b->count;
}

static int popularity_sort_compare(const void* va, const void* vb) {
    const Gif_Color* a = (const Gif_Color*) va;
    const Gif_Color* b = (const Gif_Color*) vb;
    return a->pixel > b->pixel ? -1 : a->pixel != b->pixel;
}


/* COLORMAP FUNCTIONS return a palette (a vector of Gif_Colors). The
   pixel fields are undefined; the haspixel fields are all 0. */

typedef struct {
    int first;
    int size;
    uint32_t pixel;
} adaptive_slot;

Gif_Colormap* colormap_median_cut(kchist* kch, Gt_OutputData* od)
{
    int adapt_size = od->colormap_size;
    adaptive_slot *slots = Gif_NewArray(adaptive_slot, adapt_size);
    Gif_Colormap *gfcm = Gif_NewFullColormap(adapt_size, 256);
    Gif_Color *adapt = gfcm->col;
    int nadapt;
    int i, j, k;

    /* This code was written with reference to ppmquant by Jef Poskanzer,
       part of the pbmplus package. */

    if (adapt_size < 2 || adapt_size > 256)
        fatal_error("adaptive palette size must be between 2 and 256");
    if (adapt_size >= kch->n && !od->colormap_fixed)
        warning(1, "trivial adaptive palette (only %d %s in source)",
                kch->n, kch->n == 1 ? "color" : "colors");
    if (adapt_size >= kch->n)
        adapt_size = kch->n;

    /* 0. remove any transparent color from consideration; reduce adaptive
       palette size to accommodate transparency if it looks like that'll be
       necessary */
    if (adapt_size > 2 && adapt_size < kch->n && kch->n <= 265
        && od->colormap_needs_transparency)
        adapt_size--;

    /* 1. set up the first slot, containing all pixels. */
    slots[0].first = 0;
    slots[0].size = kch->n;
    slots[0].pixel = 0;
    for (i = 0; i < kch->n; i++)
        slots[0].pixel += kch->h[i].count;

    /* 2. split slots until we have enough. */
    for (nadapt = 1; nadapt < adapt_size; nadapt++) {
        adaptive_slot *split = 0;
        kcolor minc, maxc;
        kchistitem *slice;

        /* 2.1. pick the slot to split. */
        {
            uint32_t split_pixel = 0;
            for (i = 0; i < nadapt; i++)
                if (slots[i].size >= 2 && slots[i].pixel > split_pixel) {
                    split = &slots[i];
                    split_pixel = slots[i].pixel;
                }
            if (!split)
                break;
        }
        slice = &kch->h[split->first];

        /* 2.2. find its extent. */
        {
            kchistitem *trav = slice;
            minc = maxc = trav->ka.k;
            for (i = 1, trav++; i < split->size; i++, trav++)
                for (k = 0; k != 3; ++k) {
                    minc.a[k] = min(minc.a[k], trav->ka.a[k]);
                    maxc.a[k] = max(maxc.a[k], trav->ka.a[k]);
                }
        }

        /* 2.3. decide how to split it. use the luminance method. also sort
           the colors. */
        {
            double red_diff = 0.299 * (maxc.a[0] - minc.a[0]);
            double green_diff = 0.587 * (maxc.a[1] - minc.a[1]);
            double blue_diff = 0.114 * (maxc.a[2] - minc.a[2]);
            if (red_diff >= green_diff && red_diff >= blue_diff)
                qsort(slice, split->size, sizeof(kchistitem), red_kchistitem_compare);
            else if (green_diff >= blue_diff)
                qsort(slice, split->size, sizeof(kchistitem), green_kchistitem_compare);
            else
                qsort(slice, split->size, sizeof(kchistitem), blue_kchistitem_compare);
        }

        /* 2.4. decide where to split the slot and split it there. */
        {
            uint32_t half_pixels = split->pixel / 2;
            uint32_t pixel_accum = slice[0].count;
            uint32_t diff1, diff2;
            for (i = 1; i < split->size - 1 && pixel_accum < half_pixels; i++)
                pixel_accum += slice[i].count;

            /* We know the area before the split has more pixels than the
               area after, possibly by a large margin (bad news). If it
               would shrink the margin, change the split. */
            diff1 = 2*pixel_accum - split->pixel;
            diff2 = split->pixel - 2*(pixel_accum - slice[i-1].count);
            if (diff2 < diff1 && i > 1) {
                i--;
                pixel_accum -= slice[i].count;
            }

            slots[nadapt].first = split->first + i;
            slots[nadapt].size = split->size - i;
            slots[nadapt].pixel = split->pixel - pixel_accum;
            split->size = i;
            split->pixel = pixel_accum;
        }
    }

    /* 3. make the new palette by choosing one color from each slot. */
    for (i = 0; i < nadapt; i++) {
        double px[3];
        kchistitem* slice = &kch->h[ slots[i].first ];
        kcolor kc;
        px[0] = px[1] = px[2] = 0;
        for (j = 0; j != slots[i].size; ++j)
            for (k = 0; k != 3; ++k)
                px[k] += slice[j].ka.a[k] * (double) slice[j].count;
        kc.a[0] = (int) (px[0] / slots[i].pixel);
        kc.a[1] = (int) (px[1] / slots[i].pixel);
        kc.a[2] = (int) (px[2] / slots[i].pixel);
        adapt[i] = kc_togfcg(&kc);
    }

    Gif_DeleteArray(slots);
    gfcm->ncol = nadapt;
    return gfcm;
}


void kcdiversity_init(kcdiversity* div, kchist* kch, int dodither) {
    int i;
    div->kch = kch;
    qsort(kch->h, kch->n, sizeof(kchistitem), popularity_kchistitem_compare);
    div->closest = Gif_NewArray(int, kch->n);
    div->min_dist = Gif_NewArray(uint32_t, kch->n);
    for (i = 0; i != kch->n; ++i)
        div->min_dist[i] = (uint32_t) -1;
    if (dodither) {
        div->min_dither_dist = Gif_NewArray(uint32_t, kch->n);
        for (i = 0; i != kch->n; ++i)
            div->min_dither_dist[i] = (uint32_t) -1;
    } else
        div->min_dither_dist = NULL;
    div->chosen = Gif_NewArray(int, kch->n);
    div->nchosen = 0;
}

void kcdiversity_cleanup(kcdiversity* div) {
    Gif_DeleteArray(div->closest);
    Gif_DeleteArray(div->min_dist);
    Gif_DeleteArray(div->min_dither_dist);
    Gif_DeleteArray(div->chosen);
}

int kcdiversity_find_popular(kcdiversity* div) {
    int i, n = div->kch->n;
    for (i = 0; i != n && div->min_dist[i] == 0; ++i)
        /* spin */;
    return i;
}

int kcdiversity_find_diverse(kcdiversity* div, double ditherweight) {
    int i, n = div->kch->n, chosen = kcdiversity_find_popular(div);
    if (chosen == n)
        /* skip */;
    else if (!ditherweight || !div->min_dither_dist) {
        for (i = chosen + 1; i != n; ++i)
            if (div->min_dist[i] > div->min_dist[chosen])
                chosen = i;
    } else {
        double max_dist = div->min_dist[chosen] + ditherweight * div->min_dither_dist[chosen];
        for (i = chosen + 1; i != n; ++i)
            if (div->min_dist[i] != 0) {
                double dist = div->min_dist[i] + ditherweight * div->min_dither_dist[i];
                if (dist > max_dist) {
                    chosen = i;
                    max_dist = dist;
                }
            }
    }
    return chosen;
}

int kcdiversity_choose(kcdiversity* div, int chosen, int dodither) {
    int i, j, k, n = div->kch->n;
    kchistitem* hist = div->kch->h;

    div->min_dist[chosen] = 0;
    if (div->min_dither_dist)
        div->min_dither_dist[chosen] = 0;
    div->closest[chosen] = chosen;

    /* adjust the min_dist array */
    for (i = 0; i != n; ++i)
        if (div->min_dist[i]) {
            uint32_t dist = kc_distance(&hist[i].ka.k, &hist[chosen].ka.k);
            if (dist < div->min_dist[i]) {
                div->min_dist[i] = dist;
                div->closest[i] = chosen;
            }
        }

    /* also account for dither distances */
    if (dodither && div->min_dither_dist)
        for (i = 0; i != div->nchosen; ++i) {
            kcolor x = hist[chosen].ka.k, *y = &hist[div->chosen[i]].ka.k;
            /* penalize combinations with large luminance difference */
            double dL = abs(kc_luminance(&x) - kc_luminance(y));
            dL = (dL > 8192 ? dL * 4 / 32767. : 1);
            /* create combination */
            for (k = 0; k != 3; ++k)
                x.a[k] = (x.a[k] + y->a[k]) >> 1;
            /* track closeness of combination to other colors */
            for (j = 0; j != n; ++j)
                if (div->min_dist[j]) {
                    double dist = kc_distance(&hist[j].ka.k, &x) * dL;
                    if (dist < div->min_dither_dist[j])
                        div->min_dither_dist[j] = (uint32_t) dist;
                }
        }

    div->chosen[div->nchosen] = chosen;
    ++div->nchosen;
    return chosen;
}

static void colormap_diversity_do_blend(kcdiversity* div) {
    int i, j, k, n = div->kch->n;
    kchistitem* hist = div->kch->h;
    int* chosenmap = Gif_NewArray(int, n);
    scale_color* di = Gif_NewArray(scale_color, div->nchosen);
    for (i = 0; i != div->nchosen; ++i)
        for (k = 0; k != 4; ++k)
            di[i].a[k] = 0;
    for (i = 0; i != div->nchosen; ++i)
        chosenmap[div->chosen[i]] = i;
    for (i = 0; i != n; ++i) {
        double count = hist[i].count;
        if (div->closest[i] == i)
            count *= 3;
        j = chosenmap[div->closest[i]];
        for (k = 0; k != 3; ++k)
            di[j].a[k] += hist[i].ka.a[k] * count;
        di[j].a[3] += count;
    }
    for (i = 0; i != div->nchosen; ++i) {
        int match = div->chosen[i];
        if (di[i].a[3] >= 5 * hist[match].count)
            for (k = 0; k != 3; ++k)
                hist[match].ka.a[k] = (int) (di[i].a[k] / di[i].a[3]);
    }
    Gif_DeleteArray(chosenmap);
    Gif_DeleteArray(di);
}


static Gif_Colormap *
colormap_diversity(kchist* kch, Gt_OutputData* od, int blend)
{
    int adapt_size = od->colormap_size;
    kcdiversity div;
    Gif_Colormap* gfcm = Gif_NewFullColormap(adapt_size, 256);
    int nadapt = 0;
    int chosen;

    /* This code was uses XV's modified diversity algorithm, and was written
       with reference to XV's implementation of that algorithm by John Bradley
       <bradley@cis.upenn.edu> and Tom Lane <Tom.Lane@g.gp.cs.cmu.edu>. */

    if (adapt_size < 2 || adapt_size > 256)
        fatal_error("adaptive palette size must be between 2 and 256");
    if (adapt_size > kch->n && !od->colormap_fixed)
        warning(1, "trivial adaptive palette (only %d colors in source)", kch->n);
    if (adapt_size > kch->n)
        adapt_size = kch->n;

    /* 0. remove any transparent color from consideration; reduce adaptive
       palette size to accommodate transparency if it looks like that'll be
       necessary */
    /* It will be necessary to accommodate transparency if (1) there is
       transparency in the image; (2) the adaptive palette isn't trivial; and
       (3) there are a small number of colors in the image (arbitrary constant:
       <= 265), so it's likely that most images will use most of the slots, so
       it's likely there won't be unused slots. */
    if (adapt_size > 2 && adapt_size < kch->n && kch->n <= 265
        && od->colormap_needs_transparency)
        adapt_size--;

    /* blending has bad effects when there are very few colors */
    if (adapt_size < 4)
        blend = 0;

    /* 1. initialize min_dist and sort the colors in order of popularity. */
    kcdiversity_init(&div, kch, od->dither_type != dither_none);

    /* 2. choose colors one at a time */
    for (nadapt = 0; nadapt < adapt_size; nadapt++) {
        /* 2.1. choose the color to be added */
        if (nadapt == 0 || (nadapt >= 10 && nadapt % 2 == 0))
            /* 2.1a. want most popular unchosen color */
            chosen = kcdiversity_find_popular(&div);
        else if (od->dither_type == dither_none)
            /* 2.1b. choose based on diversity from unchosen colors */
            chosen = kcdiversity_find_diverse(&div, 0);
        else {
            /* 2.1c. choose based on diversity from unchosen colors, but allow
               dithered combinations to stand in for colors, particularly early
               on in the color finding process */
            /* Weight assigned to dithered combinations drops as we proceed. */
#if HAVE_POW
            double ditherweight = 0.05 + pow(0.25, 1 + (nadapt - 1) / 3.);
#else
            double ditherweight = nadapt < 4 ? 0.25 : 0.125;
#endif
            chosen = kcdiversity_find_diverse(&div, ditherweight);
        }

        kcdiversity_choose(&div, chosen,
                           od->dither_type != dither_none
                           && nadapt > 0 && nadapt < 64);
    }

    /* 3. make the new palette by choosing one color from each slot. */
    if (blend)
        colormap_diversity_do_blend(&div);

    for (nadapt = 0; nadapt != div.nchosen; ++nadapt)
        gfcm->col[nadapt] = kc_togfcg(&kch->h[div.chosen[nadapt]].ka.k);
    gfcm->ncol = nadapt;

    kcdiversity_cleanup(&div);
    return gfcm;
}


Gif_Colormap* colormap_blend_diversity(kchist* kch, Gt_OutputData* od) {
    return colormap_diversity(kch, od, 1);
}

Gif_Colormap* colormap_flat_diversity(kchist* kch, Gt_OutputData* od) {
    return colormap_diversity(kch, od, 0);
}


/*****
 * kd_tree allocation and deallocation
 **/

struct kd3_treepos {
    int pivot;
    int offset;
};

void kd3_init(kd3_tree* kd3, void (*transform)(kcolor*)) {
    kd3->tree = NULL;
    kd3->ks = Gif_NewArray(kcolor, 256);
    kd3->nitems = 0;
    kd3->items_cap = 256;
    kd3->transform = transform;
    kd3->xradius = NULL;
    kd3->disabled = -1;
}

void kd3_cleanup(kd3_tree* kd3) {
    Gif_DeleteArray(kd3->tree);
    Gif_DeleteArray(kd3->ks);
    Gif_DeleteArray(kd3->xradius);
}

void kd3_add_transformed(kd3_tree* kd3, const kcolor* k) {
    if (kd3->nitems == kd3->items_cap) {
        kd3->items_cap *= 2;
        Gif_ReArray(kd3->ks, kcolor, kd3->items_cap);
    }
    kd3->ks[kd3->nitems] = *k;
    ++kd3->nitems;
    if (kd3->tree) {
        Gif_DeleteArray(kd3->tree);
        Gif_DeleteArray(kd3->xradius);
        kd3->tree = NULL;
        kd3->xradius = NULL;
    }
}

void kd3_add8g(kd3_tree* kd3, int a0, int a1, int a2) {
    kcolor k;
    kc_set8g(&k, a0, a1, a2);
    if (kd3->transform)
        kd3->transform(&k);
    kd3_add_transformed(kd3, &k);
}

static kd3_tree* kd3_sorter;

static int kd3_item_compar_0(const void* a, const void* b) {
    const int* aa = (const int*) a, *bb = (const int*) b;
    return kd3_sorter->ks[*aa].a[0] - kd3_sorter->ks[*bb].a[0];
}

static int kd3_item_compar_1(const void* a, const void* b) {
    const int* aa = (const int*) a, *bb = (const int*) b;
    return kd3_sorter->ks[*aa].a[1] - kd3_sorter->ks[*bb].a[1];
}

static int kd3_item_compar_2(const void* a, const void* b) {
    const int* aa = (const int*) a, *bb = (const int*) b;
    return kd3_sorter->ks[*aa].a[2] - kd3_sorter->ks[*bb].a[2];
}

static int (*kd3_item_compars[])(const void*, const void*) = {
    &kd3_item_compar_0, &kd3_item_compar_1, &kd3_item_compar_2
};

static int kd3_item_all_compar(const void* a, const void* b) {
    const int* aa = (const int*) a, *bb = (const int*) b;
    return memcmp(&kd3_sorter->ks[*aa], &kd3_sorter->ks[*bb], sizeof(kcolor));
}

static int kd3_build_range(int* perm, int nperm, int n, int depth) {
    kd3_tree* kd3 = kd3_sorter;
    int m, nl, nr, aindex = depth % 3;
    if (depth > kd3->maxdepth)
        kd3->maxdepth = depth;
    while (n >= kd3->ntree) {
        kd3->ntree *= 2;
        Gif_ReArray(kd3->tree, kd3_treepos, kd3->ntree);
    }
    if (nperm <= 1) {
        kd3->tree[n].pivot = (nperm == 0 ? -1 : perm[0]);
        kd3->tree[n].offset = -1;
        return 2;
    }

    qsort(perm, nperm, sizeof(int), kd3_item_compars[aindex]);

    /* pick pivot: a color component to split */
    m = nperm >> 1;
    while (m > 0
           && kd3->ks[perm[m]].a[aindex] == kd3->ks[perm[m-1]].a[aindex])
        --m;
    if (m == 0) { /* don't split entirely to the right (infinite loop) */
        m = nperm >> 1;
        while (m < nperm - 1 /* also, don't split entirely to the left */
               && kd3->ks[perm[m]].a[aindex] == kd3->ks[perm[m-1]].a[aindex])
            ++m;
    }
    if (m == 0)
        kd3->tree[n].pivot = kd3->ks[perm[m]].a[aindex];
    else
        kd3->tree[n].pivot = kd3->ks[perm[m-1]].a[aindex]
            + ((kd3->ks[perm[m]].a[aindex] - kd3->ks[perm[m-1]].a[aindex]) >> 1);

    /* recurse */
    nl = kd3_build_range(perm, m, n+1, depth+1);
    kd3->tree[n].offset = 1+nl;
    nr = kd3_build_range(&perm[m], nperm - m, n+1+nl, depth+1);
    return 1+nl+nr;
}

#if 0
static void kd3_print_depth(kd3_tree* kd3, int depth, kd3_treepos* p,
                            int* a, int* b) {
    int i;
    char x[6][10];
    for (i = 0; i != 3; ++i) {
        if (a[i] == INT_MIN)
            sprintf(x[2*i], "*");
        else
            sprintf(x[2*i], "%d", a[i]);
        if (b[i] == INT_MAX)
            sprintf(x[2*i+1], "*");
        else
            sprintf(x[2*i+1], "%d", b[i]);
    }
    printf("%*s<%s:%s,%s:%s,%s:%s>", depth*3, "",
           x[0], x[1], x[2], x[3], x[4], x[5]);
    if (p->offset < 0) {
        if (p->pivot >= 0) {
            assert(kd3->ks[p->pivot].a[0] >= a[0]);
            assert(kd3->ks[p->pivot].a[1] >= a[1]);
            assert(kd3->ks[p->pivot].a[2] >= a[2]);
            assert(kd3->ks[p->pivot].a[0] < b[0]);
            assert(kd3->ks[p->pivot].a[1] < b[1]);
            assert(kd3->ks[p->pivot].a[2] < b[2]);
            printf(" ** @%d: <%d,%d,%d>\n", p->pivot, kd3->ks[p->pivot].a[0], kd3->ks[p->pivot].a[1], kd3->ks[p->pivot].a[2]);
        }
    } else {
        int aindex = depth % 3, x[3];
        assert(p->pivot >= a[aindex]);
        assert(p->pivot < b[aindex]);
        printf((aindex == 0 ? " | <%d,_,_>\n" :
                aindex == 1 ? " | <_,%d,_>\n" : " | <_,_,%d>\n"), p->pivot);
        memcpy(x, b, sizeof(int) * 3);
        x[aindex] = p->pivot;
        kd3_print_depth(kd3, depth + 1, p + 1, a, x);
        memcpy(x, a, sizeof(int) * 3);
        x[aindex] = p->pivot;
        kd3_print_depth(kd3, depth + 1, p + p->offset, x, b);
    }
}

static void kd3_print(kd3_tree* kd3) {
    int a[3], b[3];
    a[0] = a[1] = a[2] = INT_MIN;
    b[0] = b[1] = b[2] = INT_MAX;
    kd3_print_depth(kd3, 0, kd3->tree, a, b);
}
#endif

void kd3_build_xradius(kd3_tree* kd3) {
    int i, j;
    /* create xradius */
    if (kd3->xradius)
        return;
    kd3->xradius = Gif_NewArray(unsigned, kd3->nitems);
    for (i = 0; i != kd3->nitems; ++i)
        kd3->xradius[i] = (unsigned) -1;
    for (i = 0; i != kd3->nitems; ++i)
        for (j = i + 1; j != kd3->nitems; ++j) {
            unsigned dist = kc_distance(&kd3->ks[i], &kd3->ks[j]);
            unsigned radius = dist / 4;
            if (radius < kd3->xradius[i])
                kd3->xradius[i] = radius;
            if (radius < kd3->xradius[j])
                kd3->xradius[j] = radius;
        }
}

void kd3_build(kd3_tree* kd3) {
    int i, delta, *perm;
    assert(!kd3->tree);

    /* create tree */
    kd3->tree = Gif_NewArray(kd3_treepos, 256);
    kd3->ntree = 256;
    kd3->maxdepth = 0;

    /* create copy of items; remove duplicates */
    perm = Gif_NewArray(int, kd3->nitems);
    for (i = 0; i != kd3->nitems; ++i)
        perm[i] = i;
#if ENABLE_THREADS
    /*
     * Because kd3_sorter is a static global used in some
     * sorting comparators, put a mutex around this
     * code block to avoid an utter catastrophe.
     */
    pthread_mutex_lock(&kd3_sort_lock);
#endif

    kd3_sorter = kd3;
    qsort(perm, kd3->nitems, sizeof(int), kd3_item_all_compar);
    for (i = 0, delta = 1; i + delta < kd3->nitems; ++i)
        if (memcmp(&kd3->ks[perm[i]], &kd3->ks[perm[i+delta]],
                   sizeof(kcolor)) == 0)
            ++delta, --i;
        else if (delta > 1)
            perm[i+1] = perm[i+delta];

    kd3_build_range(perm, kd3->nitems - (delta - 1), 0, 0);
    assert(kd3->maxdepth < 32);

#if ENABLE_THREADS
    pthread_mutex_unlock(&kd3_sort_lock);
#endif
    Gif_DeleteArray(perm);
}

void kd3_init_build(kd3_tree* kd3, void (*transform)(kcolor*),
                    const Gif_Colormap* gfcm) {
    int i;
    kd3_init(kd3, transform);
    for (i = 0; i < gfcm->ncol; ++i)
        kd3_add8g(kd3, gfcm->col[i].gfc_red, gfcm->col[i].gfc_green,
                  gfcm->col[i].gfc_blue);
    kd3_build(kd3);
}

int kd3_closest_transformed(kd3_tree* kd3, const kcolor* k,
                            unsigned* dist_store) {
    const kd3_treepos* stack[32];
    uint8_t state[32];
    int stackpos = 0;
    int result = -1;
    unsigned mindist = (unsigned) -1;

    if (!kd3->tree)
        kd3_build(kd3);

    stack[0] = kd3->tree;
    state[0] = 0;

    while (stackpos >= 0) {
        const kd3_treepos* p;
        assert(stackpos < 32);
        p = stack[stackpos];

        if (p->offset < 0) {
            if (p->pivot >= 0 && kd3->disabled != p->pivot) {
                unsigned dist = kc_distance(&kd3->ks[p->pivot], k);
                if (dist < mindist) {
                    mindist = dist;
                    result = p->pivot;
                }
            }
            if (--stackpos >= 0)
                ++state[stackpos];
        } else if (state[stackpos] == 0) {
            if (k->a[stackpos % 3] < p->pivot)
                stack[stackpos + 1] = p + 1;
            else
                stack[stackpos + 1] = p + p->offset;
            ++stackpos;
            state[stackpos] = 0;
        } else {
            int delta = k->a[stackpos % 3] - p->pivot;
            if (state[stackpos] == 1
                && (unsigned) delta * (unsigned) delta < mindist) {
                if (delta < 0)
                    stack[stackpos + 1] = p + p->offset;
                else
                    stack[stackpos + 1] = p + 1;
                ++stackpos;
                state[stackpos] = 0;
            } else if (--stackpos >= 0)
                ++state[stackpos];
        }
    }

    if (dist_store)
        *dist_store = mindist;
    return result;
}

int kd3_closest8g(kd3_tree* kd3, int a0, int a1, int a2) {
    kcolor k;
    kc_set8g(&k, a0, a1, a2);
    if (kd3->transform)
        kd3->transform(&k);
    return kd3_closest_transformed(kd3, &k, NULL);
}


void
colormap_image_posterize(Gif_Image *gfi, uint8_t *new_data,
                         Gif_Colormap *old_cm, kd3_tree* kd3,
                         uint32_t *histogram)
{
  int ncol = old_cm->ncol;
  Gif_Color *col = old_cm->col;
  int map[256];
  int i, j;
  int transparent = gfi->transparent;

  /* find closest colors in new colormap */
  for (i = 0; i < ncol; i++) {
      map[i] = col[i].pixel = kd3_closest8g(kd3, col[i].gfc_red, col[i].gfc_green, col[i].gfc_blue);
      col[i].haspixel = 1;
  }

  /* map image */
  for (j = 0; j < gfi->height; j++) {
    uint8_t *data = gfi->img[j];
    for (i = 0; i < gfi->width; i++, data++, new_data++)
      if (*data != transparent) {
        *new_data = map[*data];
        histogram[*new_data]++;
      }
  }
}


#define DITHER_SCALE    1024
#define DITHER_SHIFT    10
#define DITHER_SCALE_M1 (DITHER_SCALE-1)
#define DITHER_ITEM2ERR (1<<(DITHER_SHIFT-7))
#define N_RANDOM_VALUES 512

void
colormap_image_floyd_steinberg(Gif_Image *gfi, uint8_t *all_new_data,
                               Gif_Colormap *old_cm, kd3_tree* kd3,
                               uint32_t *histogram)
{
  static int32_t *random_values = 0;

  int width = gfi->width;
  int dither_direction = 0;
  int transparent = gfi->transparent;
  int i, j, k;
  wkcolor *err, *err1;

  /* Initialize distances */
  for (i = 0; i < old_cm->ncol; ++i) {
      Gif_Color* c = &old_cm->col[i];
      c->pixel = kd3_closest8g(kd3, c->gfc_red, c->gfc_green, c->gfc_blue);
      c->haspixel = 1;
  }

  /* This code was written with reference to ppmquant by Jef Poskanzer, part
     of the pbmplus package. */

  /* Initialize Floyd-Steinberg error vectors to small random values, so we
     don't get artifacts on the top row */
  err = Gif_NewArray(wkcolor, width + 2);
  err1 = Gif_NewArray(wkcolor, width + 2);
  /* Use the same random values on each call in an attempt to minimize
     "jumping dithering" effects on animations */
  if (!random_values) {
    random_values = Gif_NewArray(int32_t, N_RANDOM_VALUES);
    for (i = 0; i < N_RANDOM_VALUES; i++)
      random_values[i] = RANDOM() % (DITHER_SCALE_M1 * 2) - DITHER_SCALE_M1;
  }
  for (i = 0; i < gfi->width + 2; i++) {
    j = (i + gfi->left) * 3;
    for (k = 0; k < 3; ++k)
        err[i].a[k] = random_values[ (j + k) % N_RANDOM_VALUES ];
  }
  /* err1 initialized below */

  kd3_build_xradius(kd3);

  /* Do the image! */
  for (j = 0; j < gfi->height; j++) {
    int d0, d1, d2, d3;         /* used for error diffusion */
    uint8_t *data, *new_data;
    int x;

    if (dither_direction) {
      x = width - 1;
      d0 = 0, d1 = 2, d2 = 1, d3 = 0;
    } else {
      x = 0;
      d0 = 2, d1 = 0, d2 = 1, d3 = 2;
    }
    data = &gfi->img[j][x];
    new_data = all_new_data + j * (unsigned) width + x;

    for (i = 0; i < width + 2; i++)
      err1[i].a[0] = err1[i].a[1] = err1[i].a[2] = 0;

    /* Do a single row */
    while (x >= 0 && x < width) {
      int e;
      kcolor use;

      /* the transparent color never gets adjusted */
      if (*data == transparent)
        goto next;

      /* find desired new color */
      kc_set8g(&use, old_cm->col[*data].gfc_red, old_cm->col[*data].gfc_green,
               old_cm->col[*data].gfc_blue);
      if (kd3->transform)
          kd3->transform(&use);
      /* use Floyd-Steinberg errors to adjust */
      for (k = 0; k < 3; ++k) {
          int v = use.a[k]
              + (err[x+1].a[k] & ~(DITHER_ITEM2ERR-1)) / DITHER_ITEM2ERR;
          use.a[k] = KC_CLAMPV(v);
      }

      e = old_cm->col[*data].pixel;
      if (kc_distance(&kd3->ks[e], &use) < kd3->xradius[e])
          *new_data = e;
      else
          *new_data = kd3_closest_transformed(kd3, &use, NULL);
      histogram[*new_data]++;

      /* calculate and propagate the error between desired and selected color.
         Assume that, with a large scale (1024), we don't need to worry about
         image artifacts caused by error accumulation (the fact that the
         error terms might not sum to the error). */
      for (k = 0; k < 3; ++k) {
          e = (use.a[k] - kd3->ks[*new_data].a[k]) * DITHER_ITEM2ERR;
          if (e) {
              err [x+d0].a[k] += ((e * 7) & ~15) / 16;
              err1[x+d1].a[k] += ((e * 3) & ~15) / 16;
              err1[x+d2].a[k] += ((e * 5) & ~15) / 16;
              err1[x+d3].a[k] += ( e      & ~15) / 16;
          }
      }

     next:
      if (dither_direction)
        x--, data--, new_data--;
      else
        x++, data++, new_data++;
    }
    /* Did a single row */

    /* change dithering directions */
    {
      wkcolor *temp = err1;
      err1 = err;
      err = temp;
      dither_direction = !dither_direction;
    }
  }

  /* delete temporary storage */
  Gif_DeleteArray(err);
  Gif_DeleteArray(err1);
}


typedef struct odselect_planitem {
    uint8_t plan;
    uint16_t frac;
} odselect_planitem;

static int* ordered_dither_lum;

static void plan_from_cplan(uint8_t* plan, int nplan,
                            const odselect_planitem* cp, int ncp, int whole) {
    int i, cfrac_subt = 0, planpos = 0, end_planpos;
    for (i = 0; i != ncp; ++i) {
        cfrac_subt += cp[i].frac;
        end_planpos = cfrac_subt * nplan / whole;
        while (planpos != end_planpos)
            plan[planpos++] = cp[i].plan;
    }
    assert(planpos == nplan);
}

static int ordered_dither_plan_compare(const void* xa, const void* xb) {
    const uint8_t* a = (const uint8_t*) xa;
    const uint8_t* b = (const uint8_t*) xb;
    if (ordered_dither_lum[*a] != ordered_dither_lum[*b])
        return ordered_dither_lum[*a] - ordered_dither_lum[*b];
    else
        return *a - *b;
}

static int kc_line_closest(const kcolor* p0, const kcolor* p1,
                           const kcolor* ref, double* t, unsigned* dist) {
    wkcolor p01, p0ref;
    kcolor online;
    unsigned den;
    int d;
    for (d = 0; d != 3; ++d) {
        p01.a[d] = p1->a[d] - p0->a[d];
        p0ref.a[d] = ref->a[d] - p0->a[d];
    }
    den = (unsigned)
        (p01.a[0]*p01.a[0] + p01.a[1]*p01.a[1] + p01.a[2]*p01.a[2]);
    if (den == 0)
        return 0;
    /* NB: We've run out of bits of precision. We can calculate the
       denominator in unsigned arithmetic, but the numerator might
       be negative, or it might be so large that it is unsigned.
       Calculate the numerator as a double. */
    *t = ((double) p01.a[0]*p0ref.a[0] + p01.a[1]*p0ref.a[1]
          + p01.a[2]*p0ref.a[2]) / den;
    if (*t < 0 || *t > 1)
        return 0;
    for (d = 0; d != 3; ++d) {
        int v = (int) (p01.a[d] * *t) + p0->a[d];
        online.a[d] = KC_CLAMPV(v);
    }
    *dist = kc_distance(&online, ref);
    return 1;
}

static int kc_plane_closest(const kcolor* p0, const kcolor* p1,
                            const kcolor* p2, const kcolor* ref,
                            double* t, unsigned* dist) {
    wkcolor p0ref, p01, p02;
    double n[3], pvec[3], det, qvec[3], u, v;
    int d;

    /* Calculate the non-unit normal of the plane determined by the input
       colors (p0-p2) */
    for (d = 0; d != 3; ++d) {
        p0ref.a[d] = ref->a[d] - p0->a[d];
        p01.a[d] = p1->a[d] - p0->a[d];
        p02.a[d] = p2->a[d] - p0->a[d];
    }
    n[0] = p01.a[1]*p02.a[2] - p01.a[2]*p02.a[1];
    n[1] = p01.a[2]*p02.a[0] - p01.a[0]*p02.a[2];
    n[2] = p01.a[0]*p02.a[1] - p01.a[1]*p02.a[0];

    /* Moeller-Trumbore ray tracing algorithm: trace a ray from `ref` along
       normal `n`; convert to barycentric coordinates to see if the ray
       intersects with the triangle. */
    pvec[0] = n[1]*p02.a[2] - n[2]*p02.a[1];
    pvec[1] = n[2]*p02.a[0] - n[0]*p02.a[2];
    pvec[2] = n[0]*p02.a[1] - n[1]*p02.a[0];

    det = pvec[0]*p01.a[0] + pvec[1]*p01.a[1] + pvec[2]*p01.a[2];
    if (fabs(det) <= 0.0001220703125) /* optimizer will take care of that */
        return 0;
    det = 1 / det;

    u = (p0ref.a[0]*pvec[0] + p0ref.a[1]*pvec[1] + p0ref.a[2]*pvec[2]) * det;
    if (u < 0 || u > 1)
        return 0;

    qvec[0] = p0ref.a[1]*p01.a[2] - p0ref.a[2]*p01.a[1];
    qvec[1] = p0ref.a[2]*p01.a[0] - p0ref.a[0]*p01.a[2];
    qvec[2] = p0ref.a[0]*p01.a[1] - p0ref.a[1]*p01.a[0];

    v = (n[0]*qvec[0] + n[1]*qvec[1] + n[2]*qvec[2]) * det;
    if (v < 0 || v > 1 || u + v > 1)
        return 0;

    /* Now we know at there is a point in the triangle that is closer to
       `ref` than any point along its edges. Return the barycentric
       coordinates for that point and the distance to that point. */
    t[0] = u;
    t[1] = v;
    v = (p02.a[0]*qvec[0] + p02.a[1]*qvec[1] + p02.a[2]*qvec[2]) * det;
    *dist = (unsigned) (v * v * (n[0]*n[0] + n[1]*n[1] + n[2]*n[2]) + 0.5);
    return 1;
}

static void limit_ordered_dither_plan(uint8_t* plan, int nplan, int nc,
                                      const kcolor* want, kd3_tree* kd3) {
    unsigned mindist, dist;
    int ncp = 0, nbestcp = 0, i, j, k;
    double t[2];
    odselect_planitem cp[256], bestcp[16];
    nc = nc <= 16 ? nc : 16;

    /* sort colors */
    cp[0].plan = plan[0];
    cp[0].frac = 1;
    for (ncp = i = 1; i != nplan; ++i)
        if (plan[i - 1] == plan[i])
            ++cp[ncp - 1].frac;
        else {
            cp[ncp].plan = plan[i];
            cp[ncp].frac = 1;
            ++ncp;
        }

    /* calculate plan */
    mindist = (unsigned) -1;
    for (i = 0; i != ncp; ++i) {
        /* check for closest single color */
        dist = kc_distance(&kd3->ks[cp[i].plan], want);
        if (dist < mindist) {
            bestcp[0].plan = cp[i].plan;
            bestcp[0].frac = KC_WHOLE;
            nbestcp = 1;
            mindist = dist;
        }

        for (j = i + 1; nc >= 2 && j < ncp; ++j) {
            /* check for closest blend of two colors */
            if (kc_line_closest(&kd3->ks[cp[i].plan],
                                &kd3->ks[cp[j].plan],
                                want, &t[0], &dist)
                && dist < mindist) {
                bestcp[0].plan = cp[i].plan;
                bestcp[1].plan = cp[j].plan;
                bestcp[1].frac = (int) (KC_WHOLE * t[0]);
                bestcp[0].frac = KC_WHOLE - bestcp[1].frac;
                nbestcp = 2;
                mindist = dist;
            }

            for (k = j + 1; nc >= 3 && k < ncp; ++k)
                /* check for closest blend of three colors */
                if (kc_plane_closest(&kd3->ks[cp[i].plan],
                                     &kd3->ks[cp[j].plan],
                                     &kd3->ks[cp[k].plan],
                                     want, &t[0], &dist)
                    && dist < mindist) {
                    bestcp[0].plan = cp[i].plan;
                    bestcp[1].plan = cp[j].plan;
                    bestcp[1].frac = (int) (KC_WHOLE * t[0]);
                    bestcp[2].plan = cp[k].plan;
                    bestcp[2].frac = (int) (KC_WHOLE * t[1]);
                    bestcp[0].frac = KC_WHOLE - bestcp[1].frac - bestcp[2].frac;
                    nbestcp = 3;
                    mindist = dist;
                }
        }
    }

    plan_from_cplan(plan, nplan, bestcp, nbestcp, KC_WHOLE);
}

static void set_ordered_dither_plan(uint8_t* plan, int nplan, int nc,
                                    Gif_Color* gfc, kd3_tree* kd3) {
    kcolor want, cur;
    wkcolor err;
    int i, d;

    kc_set8g(&want, gfc->gfc_red, gfc->gfc_green, gfc->gfc_blue);
    if (kd3->transform)
        kd3->transform(&want);

    wkc_clear(&err);
    for (i = 0; i != nplan; ++i) {
        for (d = 0; d != 3; ++d) {
            int v = want.a[d] + err.a[d];
            cur.a[d] = KC_CLAMPV(v);
        }
        plan[i] = kd3_closest_transformed(kd3, &cur, NULL);
        for (d = 0; d != 3; ++d)
            err.a[d] += want.a[d] - kd3->ks[plan[i]].a[d];
    }

    qsort(plan, nplan, 1, ordered_dither_plan_compare);

    if (nc < nplan && plan[0] != plan[nplan-1]) {
        int ncp = 1;
        for (i = 1; i != nplan; ++i)
            ncp += plan[i-1] != plan[i];
        if (ncp > nc)
            limit_ordered_dither_plan(plan, nplan, nc, &want, kd3);
    }

    gfc->haspixel = 1;
}

static void pow2_ordered_dither(Gif_Image* gfi, uint8_t* all_new_data,
                                Gif_Colormap* old_cm, kd3_tree* kd3,
                                uint32_t* histogram, const uint8_t* matrix,
                                uint8_t* plan) {
    int mws, nplans, i, x, y;
    for (mws = 0; (1 << mws) != matrix[0]; ++mws)
        /* nada */;
    for (nplans = 0; (1 << nplans) != matrix[2]; ++nplans)
        /* nada */;

    for (y = 0; y != gfi->height; ++y) {
        uint8_t *data, *new_data, *thisplan;
        data = gfi->img[y];
        new_data = all_new_data + y * (unsigned) gfi->width;

        for (x = 0; x != gfi->width; ++x)
            /* the transparent color never gets adjusted */
            if (data[x] != gfi->transparent) {
                thisplan = &plan[data[x] << nplans];
                if (!old_cm->col[data[x]].haspixel)
                    set_ordered_dither_plan(thisplan, 1 << nplans, matrix[3],
                                            &old_cm->col[data[x]], kd3);
                i = matrix[4 + ((x + gfi->left) & (matrix[0] - 1))
                           + (((y + gfi->top) & (matrix[1] - 1)) << mws)];
                new_data[x] = thisplan[i];
                histogram[new_data[x]]++;
            }
    }
}

static void colormap_image_ordered(Gif_Image* gfi, uint8_t* all_new_data,
                                   Gif_Colormap* old_cm, kd3_tree* kd3,
                                   uint32_t* histogram,
                                   const uint8_t* matrix) {
    int mw = matrix[0], mh = matrix[1], nplan = matrix[2];
    uint8_t* plan = Gif_NewArray(uint8_t, nplan * old_cm->ncol);
    int i, x, y;

    /* Written with reference to Joel Ylilouma's versions. */

    /* Initialize colors */
    for (i = 0; i != old_cm->ncol; ++i)
        old_cm->col[i].haspixel = 0;

    /* Initialize luminances, create luminance sorter */
    ordered_dither_lum = Gif_NewArray(int, kd3->nitems);
    for (i = 0; i != kd3->nitems; ++i)
        ordered_dither_lum[i] = kc_luminance(&kd3->ks[i]);

    /* Do the image! */
    if ((mw & (mw - 1)) == 0 && (mh & (mh - 1)) == 0
        && (nplan & (nplan - 1)) == 0)
        pow2_ordered_dither(gfi, all_new_data, old_cm, kd3, histogram,
                            matrix, plan);
    else
        for (y = 0; y != gfi->height; ++y) {
            uint8_t *data, *new_data, *thisplan;
            data = gfi->img[y];
            new_data = all_new_data + y * (unsigned) gfi->width;

            for (x = 0; x != gfi->width; ++x)
                /* the transparent color never gets adjusted */
                if (data[x] != gfi->transparent) {
                    thisplan = &plan[nplan * data[x]];
                    if (!old_cm->col[data[x]].haspixel)
                        set_ordered_dither_plan(thisplan, nplan, matrix[3],
                                                &old_cm->col[data[x]], kd3);
                    i = matrix[4 + (x + gfi->left) % mw
                               + ((y + gfi->top) % mh) * mw];
                    new_data[x] = thisplan[i];
                    histogram[new_data[x]]++;
                }
        }

    /* delete temporary storage */
    Gif_DeleteArray(ordered_dither_lum);
    Gif_DeleteArray(plan);
}


static void dither(Gif_Image* gfi, uint8_t* new_data, Gif_Colormap* old_cm,
                   kd3_tree* kd3, uint32_t* histogram, Gt_OutputData* od) {
    if (od->dither_type == dither_default
        || od->dither_type == dither_floyd_steinberg)
        colormap_image_floyd_steinberg(gfi, new_data, old_cm, kd3, histogram);
    else if (od->dither_type == dither_ordered
             || od->dither_type == dither_ordered_new)
        colormap_image_ordered(gfi, new_data, old_cm, kd3, histogram,
                               od->dither_data);
    else
        colormap_image_posterize(gfi, new_data, old_cm, kd3, histogram);
}

/* return value 1 means run the dither again */
static int
try_assign_transparency(Gif_Image *gfi, Gif_Colormap *old_cm, uint8_t *new_data,
                        Gif_Colormap *new_cm, int *new_ncol,
                        kd3_tree* kd3, uint32_t *histogram)
{
  uint32_t min_used;
  int i, j;
  int transparent = gfi->transparent;
  int new_transparent = -1;
  Gif_Color transp_value;

  if (transparent < 0)
    return 0;

  if (old_cm)
    transp_value = old_cm->col[transparent];
  else
    GIF_SETCOLOR(&transp_value, 0, 0, 0);

  /* look for an unused pixel in the existing colormap; prefer the same color
     we had */
  for (i = 0; i < *new_ncol; i++)
    if (histogram[i] == 0 && GIF_COLOREQ(&transp_value, &new_cm->col[i])) {
      new_transparent = i;
      goto found;
    }
  for (i = 0; i < *new_ncol; i++)
    if (histogram[i] == 0) {
      new_transparent = i;
      goto found;
    }

  /* try to expand the colormap */
  if (*new_ncol < 256) {
    assert(*new_ncol < new_cm->capacity);
    new_transparent = *new_ncol;
    new_cm->col[new_transparent] = transp_value;
    (*new_ncol)++;
    goto found;
  }

  /* not found: mark the least-frequently-used color as the new transparent
     color and return 1 (meaning 'dither again') */
  assert(*new_ncol == 256);
  min_used = 0xFFFFFFFFU;
  for (i = 0; i < 256; i++)
    if (histogram[i] < min_used) {
      new_transparent = i;
      min_used = histogram[i];
    }
  kd3_disable(kd3, new_transparent); /* mark it as unusable */
  return 1;

 found:
  for (j = 0; j < gfi->height; j++) {
    uint8_t *data = gfi->img[j];
    for (i = 0; i < gfi->width; i++, data++, new_data++)
      if (*data == transparent)
        *new_data = new_transparent;
  }

  gfi->transparent = new_transparent;
  return 0;
}

void
colormap_stream(Gif_Stream* gfs, Gif_Colormap* new_cm, Gt_OutputData* od)
{
  kd3_tree kd3;
  Gif_Color *new_col = new_cm->col;
  int new_ncol = new_cm->ncol, new_gray;
  int imagei, j;
  int compress_new_cm = 1;

  /* make sure colormap has enough space */
  if (new_cm->capacity < 256) {
    Gif_Color *x = Gif_NewArray(Gif_Color, 256);
    memcpy(x, new_col, sizeof(Gif_Color) * new_ncol);
    Gif_DeleteArray(new_col);
    new_cm->col = new_col = x;
    new_cm->capacity = 256;
  }
  assert(new_cm->capacity >= 256);

  /* new_col[j].pixel == number of pixels with color j in the new image. */
  for (j = 0; j < 256; j++)
    new_col[j].pixel = 0;

  /* initialize kd3 tree */
  new_gray = 1;
  for (j = 0; new_gray && j < new_cm->ncol; ++j)
      if (new_col[j].gfc_red != new_col[j].gfc_green
          || new_col[j].gfc_red != new_col[j].gfc_blue)
          new_gray = 0;
  kd3_init_build(&kd3, new_gray ? kc_luminance_transform : NULL, new_cm);

  for (imagei = 0; imagei < gfs->nimages; imagei++) {
    Gif_Image *gfi = gfs->images[imagei];
    Gif_Colormap *gfcm = gfi->local ? gfi->local : gfs->global;
    int only_compressed = (gfi->img == 0);

    if (gfcm) {
      /* If there was an old colormap, change the image data */
      uint8_t *new_data = Gif_NewArray(uint8_t, (unsigned) gfi->width * (unsigned) gfi->height);
      uint32_t histogram[256];
      unmark_colors(new_cm);
      unmark_colors(gfcm);

      if (only_compressed)
          Gif_UncompressImage(gfs, gfi);

      kd3_enable_all(&kd3);
      do {
        for (j = 0; j < 256; j++)
            histogram[j] = 0;
        dither(gfi, new_data, gfcm, &kd3, histogram, od);
      } while (try_assign_transparency(gfi, gfcm, new_data, new_cm, &new_ncol,
                                       &kd3, histogram));

      Gif_ReleaseUncompressedImage(gfi);
      /* version 1.28 bug fix: release any compressed version or it'll cause
         bad images */
      Gif_ReleaseCompressedImage(gfi);
      Gif_SetUncompressedImage(gfi, new_data, Gif_Free, 0);

      /* update count of used colors */
      for (j = 0; j < 256; j++)
        new_col[j].pixel += histogram[j];
      if (gfi->transparent >= 0)
        /* we don't have data on the number of used colors for transparency
           so fudge it. */
        new_col[gfi->transparent].pixel += (unsigned) gfi->width * (unsigned) gfi->height / 8;

    } else {
      /* Can't compress new_cm afterwards if we didn't actively change colors
         over */
      compress_new_cm = 0;
    }

    if (gfi->local) {
      Gif_DeleteColormap(gfi->local);
      gfi->local = 0;
    }

    /* 1.92: recompress *after* deleting the local colormap */
    if (gfcm && only_compressed) {
        Gif_FullCompressImage(gfs, gfi, &gif_write_info);
        Gif_ReleaseUncompressedImage(gfi);
    }
  }

  /* Set new_cm->ncol from new_ncol. We didn't update new_cm->ncol before so
     the closest-color algorithms wouldn't see any new transparent colors.
     That way added transparent colors were only used for transparency. */
  new_cm->ncol = new_ncol;

  /* change the background. I hate the background by now */
  if ((gfs->nimages == 0 || gfs->images[0]->transparent < 0)
      && gfs->global && gfs->background < gfs->global->ncol) {
      Gif_Color *c = &gfs->global->col[gfs->background];
      gfs->background = kd3_closest8g(&kd3, c->gfc_red, c->gfc_green, c->gfc_blue);
      new_col[gfs->background].pixel++;
  } else if (gfs->nimages > 0 && gfs->images[0]->transparent >= 0)
      gfs->background = gfs->images[0]->transparent;
  else
      gfs->background = 0;

  Gif_DeleteColormap(gfs->global);
  kd3_cleanup(&kd3);

  /* We may have used only a subset of the colors in new_cm. We try to store
     only that subset, just as if we'd piped the output of 'gifsicle
     --use-colormap=X' through 'gifsicle' another time. */
  gfs->global = Gif_CopyColormap(new_cm);
  for (j = 0; j < new_cm->ncol; ++j)
      gfs->global->col[j].haspixel = 0;
  if (compress_new_cm) {
    /* only bother to recompress if we'll get anything out of it */
    compress_new_cm = 0;
    for (j = 0; j < new_cm->ncol - 1; j++)
      if (new_col[j].pixel == 0 || new_col[j].pixel < new_col[j+1].pixel) {
        compress_new_cm = 1;
        break;
      }
  }

  if (compress_new_cm) {
    int map[256];

    /* Gif_CopyColormap copies the 'pixel' values as well */
    new_col = gfs->global->col;
    for (j = 0; j < new_cm->ncol; j++)
      new_col[j].haspixel = j;

    /* sort based on popularity */
    qsort(new_col, new_cm->ncol, sizeof(Gif_Color), popularity_sort_compare);

    /* set up the map and reduce the number of colors */
    for (j = 0; j < new_cm->ncol; j++)
      map[ new_col[j].haspixel ] = j;
    for (j = 0; j < new_cm->ncol; j++)
      if (!new_col[j].pixel) {
        gfs->global->ncol = j;
        break;
      }

    /* map the image data, transparencies, and background */
    if (gfs->background < gfs->global->ncol)
        gfs->background = map[gfs->background];
    for (imagei = 0; imagei < gfs->nimages; imagei++) {
      Gif_Image *gfi = gfs->images[imagei];
      int only_compressed = (gfi->img == 0);
      uint32_t size;
      uint8_t *data;
      if (only_compressed)
          Gif_UncompressImage(gfs, gfi);

      data = gfi->image_data;
      for (size = (unsigned) gfi->width * (unsigned) gfi->height; size > 0; size--, data++)
        *data = map[*data];
      if (gfi->transparent >= 0)
        gfi->transparent = map[gfi->transparent];

      if (only_compressed) {
        Gif_FullCompressImage(gfs, gfi, &gif_write_info);
        Gif_ReleaseUncompressedImage(gfi);
      }
    }
  }
}


/* Halftone algorithms */

static const uint8_t dither_matrix_o3x3[4 + 3*3] = {
    3, 3, 9, 9,
    2, 6, 3,
    5, 0, 8,
    1, 7, 4
};

static const uint8_t dither_matrix_o4x4[4 + 4*4] = {
    4, 4, 16, 16,
     0,  8,  3, 10,
    12,  4, 14,  6,
     2, 11,  1,  9,
    15,  7, 13,  5
};

static const uint8_t dither_matrix_o8x8[4 + 8*8] = {
    8, 8, 64, 64,
     0, 48, 12, 60,  3, 51, 15, 63,
    32, 16, 44, 28, 35, 19, 47, 31,
     8, 56,  4, 52, 11, 59,  7, 55,
    40, 24, 36, 20, 43, 27, 39, 23,
     2, 50, 14, 62,  1, 49, 13, 61,
    34, 18, 46, 30, 33, 17, 45, 29,
    10, 58,  6, 54,  9, 57,  5, 53,
    42, 26, 38, 22, 41, 25, 37, 21
};

static const uint8_t dither_matrix_ro64x64[4 + 64*64] = {
    64, 64, 16, 16,
     6, 15,  2, 15,  2, 14,  1, 13,  2, 14,  5, 13,  0, 14,  0,  9,  6, 10,  7, 13,  6, 13,  3, 10,  5, 15,  4, 11,  0, 11,  6, 10,  7, 12,  7, 13,  0,  9,  6, 15,  6, 10,  0, 15,  1, 15,  0,  8,  0, 15,  6, 15,  7, 15,  7,  9,  1, 15,  3,  8,  1,  8,  0, 14,
     9,  3, 10,  5, 10,  6, 10,  5,  9,  6,  9,  2,  9,  4, 13,  4, 13,  3,  8,  3, 10,  1, 13,  6, 11,  1, 12,  3, 14,  5, 15,  3,  8,  3,  8,  3, 12,  4, 11,  3, 13,  3,  8,  4,  9,  6, 12,  4, 11,  6, 11,  3, 10,  0, 12,  1, 11,  7, 12,  4, 12,  4, 11,  5,
     1, 14,  0, 10,  2,  9,  2, 11,  1,  8,  1,  8,  3,  9,  4, 15,  7, 13,  7, 14,  7, 14,  0, 10,  0, 14,  7,  9,  0, 11,  1, 15,  0, 11,  0, 11,  3, 15,  7, 14,  6, 10,  5,  8,  0, 11,  0,  8,  7, 11,  0, 15,  0, 12,  1, 13,  6,  9,  0, 15,  4,  9,  1,  8,
    10,  5, 15,  6, 13,  6, 14,  7, 14,  4, 12,  5, 15,  6, 10,  2, 11,  3, 10,  3, 11,  3, 13,  6, 11,  5, 14,  3, 14,  6,  8,  5, 14,  5, 14,  7, 10,  7, 11,  3, 13,  3, 13,  2, 14,  4, 15,  5, 15,  3,  8,  4, 11,  4,  9,  5, 12,  3,  8,  5, 15,  2, 13,  5,
     3, 10,  2,  9,  5, 15,  4,  9,  0, 11,  7, 11,  0, 14,  5, 11,  5, 14,  7, 15,  6,  9,  0,  9,  0,  8,  4, 14,  6, 12,  0, 11,  4, 15,  5,  8,  6, 10,  6, 11,  6, 10,  3, 12,  5,  9,  6,  9,  5, 12,  6, 12,  7, 14,  0, 14,  2, 15,  5,  8,  2, 10,  3,  9,
    14,  7, 14,  7,  8,  1, 12,  2, 14,  4, 15,  3, 11,  5, 12,  2,  8,  0, 10,  3, 12,  1, 13,  5, 14,  5, 11,  0, 11,  3, 13,  7,  8,  1, 12,  3, 15,  3, 14,  2, 15,  3, 11,  6, 15,  1, 12,  1,  9,  3,  9,  3, 11,  3, 11,  7, 11,  5, 12,  0, 14,  6, 13,  6,
     5, 10,  6, 12,  3, 15,  3,  8,  7, 10,  0, 15,  7, 10,  5,  8,  1, 14,  5, 10,  7, 14,  2, 14,  0, 14,  1,  8,  3,  8,  5, 10,  5, 15,  0,  8,  6, 14,  0,  8,  2, 15,  4, 11,  6, 10,  0,  8,  5,  9,  4, 14,  1, 15,  3, 10,  1, 15,  0, 15,  5,  8,  7, 13,
    15,  3,  8,  0,  9,  6, 12,  6, 15,  0, 11,  4, 14,  3, 15,  3, 10,  5, 12,  3, 10,  3,  9,  6,  9,  5, 13,  5, 15,  6, 14,  0,  9,  0, 12,  4, 11,  3, 15,  4, 11,  6, 15,  2, 15,  3, 14,  4, 13,  2, 11,  1,  8,  5, 12,  7,  9,  4,  8,  5, 12,  2, 11,  0,
     0, 13,  5, 11,  5, 14,  5, 15,  7,  9,  5,  8,  2, 10,  3, 15,  6,  9,  3, 12,  5, 11,  6, 14,  3, 13,  6, 13,  0,  9,  1, 11,  3,  8,  3,  9,  4,  9,  6, 14,  7,  8,  6, 12,  7, 13,  6,  9,  5, 13,  3, 15,  5, 14,  3, 15,  4, 12,  4, 12,  2, 13,  0, 15,
    10,  7, 14,  1,  8,  2, 10,  2, 12,  0, 13,  1, 13,  5, 11,  6, 12,  3,  9,  6, 15,  1,  9,  1, 10,  6, 10,  3, 15,  6, 15,  5, 15,  7, 12,  6, 12,  0,  9,  3, 12,  3, 11,  3,  8,  3, 13,  2,  9,  3,  9,  6, 10,  2, 11,  6,  9,  0,  9,  1,  9,  6, 10,  4,
     3, 10,  4,  9,  4,  9,  6, 13,  3, 15,  0,  8,  4, 10,  0, 14,  5, 15,  7, 15,  2, 12,  7, 10,  5, 15,  7,  8,  0,  9,  0,  8,  6, 14,  4, 15,  2, 15,  1, 15,  0,  9,  5, 14,  0, 12,  7, 10,  6,  8,  6, 11,  0, 13,  7, 14,  1, 13,  2, 10,  5,  9,  0, 14,
    14,  6, 13,  1, 12,  3,  8,  3,  9,  6, 12,  4, 15,  2,  8,  5,  9,  1, 10,  1,  8,  5, 15,  0,  8,  2, 12,  3, 12,  5, 12,  5, 11,  2, 10,  2,  8,  5, 10,  5, 12,  4,  9,  3,  8,  4, 13,  1, 15,  0, 12,  3,  8,  4, 10,  3,  9,  5, 13,  5, 13,  3, 11,  4,
     4, 12,  3, 10,  6,  9,  0, 14,  0,  8,  1,  9,  1,  9,  6, 15,  0, 10,  2,  9,  6, 10,  7, 10,  2, 14,  4,  8,  0, 13,  4,  8,  1, 11,  6, 14,  7, 14,  0,  8,  4, 14,  7, 14,  4,  9,  2, 12,  0, 12,  3,  8,  5,  9,  6, 14,  0,  9,  2, 14,  5, 10,  1, 15,
    10,  2, 15,  6, 14,  3, 11,  6, 15,  4, 15,  4, 13,  5, 11,  3, 12,  4, 14,  5, 12,  0, 13,  3,  8,  6, 15,  2, 10,  6, 13,  2, 12,  6,  8,  3, 10,  3, 14,  5,  8,  0, 10,  3, 13,  2,  9,  6, 10,  6, 15,  4, 13,  2,  8,  3, 14,  6, 11,  7, 13,  0, 10,  5,
     2, 13,  1,  9,  5, 10,  3, 15,  6, 10,  0, 15,  4, 15,  4, 15,  6,  9,  0, 14,  1,  9,  6, 12,  0, 11,  5,  9,  0, 14,  5, 14,  6,  9,  3, 11,  7, 11,  1, 15,  1,  9,  3, 13,  5, 12,  0,  9,  2,  9,  6, 13,  2,  9,  6, 13,  7, 11,  7,  8,  0, 13,  3,  9,
     9,  6, 13,  6, 13,  0,  8,  5, 14,  2, 11,  7, 11,  2, 11,  2, 12,  3, 11,  4, 13,  4, 10,  3, 15,  5, 13,  0, 11,  6, 10,  0, 13,  3, 15,  7, 14,  3,  8,  4, 12,  6,  9,  6, 10,  3, 14,  5, 13,  5, 11,  3, 13,  6,  8,  2, 14,  3, 15,  3, 11,  4, 12,  6,
     4, 10,  7, 11,  1, 10,  2, 14,  7, 14,  0, 14,  4, 15,  0,  8,  0, 15,  5, 15,  7, 11,  7, 14,  6, 12,  0,  8,  1, 15,  1, 14,  2,  8,  0, 11,  4,  8,  5, 11,  1, 15,  0, 12,  7, 14,  0, 11,  4,  8,  2,  8,  0, 14,  2, 14,  4,  9,  4, 10,  7, 15,  5, 14,
    14,  0, 13,  2, 14,  5, 11,  5, 10,  2, 10,  6, 11,  3, 12,  5, 11,  7, 11,  1, 12,  3, 11,  3,  9,  3, 12,  4, 11,  6,  9,  5, 14,  5, 12,  7, 14,  2, 12,  1,  8,  4, 11,  7, 10,  3, 14,  4, 14,  0, 14,  5, 11,  7, 11,  5, 15,  1, 14,  0, 10,  3, 10,  0,
     5, 12,  7, 10,  7, 11,  0, 14,  5, 14,  1, 14,  3,  8,  0, 15,  0,  9,  5,  9,  1,  8,  6, 15,  6, 15,  7, 13,  7,  9,  4, 11,  0, 10,  6, 10,  7, 12,  7, 15,  0,  9,  7, 14,  2, 11,  0, 12,  7, 11,  7, 13,  0,  9,  6, 14,  2, 11,  1, 15,  0,  8,  0, 10,
    10,  3, 15,  3, 15,  3,  8,  4, 11,  3, 11,  6, 15,  6, 11,  5, 12,  5, 13,  0, 15,  4,  8,  1, 11,  3, 10,  3, 15,  3, 14,  1, 14,  6, 13,  3, 11,  3, 11,  3, 13,  6,  8,  3, 12,  5,  8,  4, 13,  3,  8,  2, 13,  4,  8,  3, 14,  7,  8,  5, 14,  4, 15,  5,
     3,  9,  0, 10,  0, 12,  3, 11,  7, 10,  6,  8,  7, 12,  0, 11,  1,  8,  5, 12,  5, 12,  7, 11,  2, 14,  0, 10,  0, 12,  0, 15,  2,  9,  5,  9,  0, 15,  4, 14,  6,  9,  7, 11,  6, 10,  0,  9,  1,  9,  0, 14,  4, 12,  5,  9,  6, 10,  4, 11,  6, 12,  5, 12,
    12,  6, 15,  4,  8,  4, 15,  7, 15,  3, 14,  0,  9,  3, 13,  5, 14,  5, 11,  1,  9,  0, 15,  3,  8,  7, 12,  7,  8,  5,  9,  4, 12,  6, 12,  1, 10,  6,  9,  0, 15,  3, 14,  0, 15,  3, 13,  5, 15,  5,  9,  6, 10,  0, 14,  0, 13,  1, 15,  1,  8,  3,  8,  1,
     2,  8,  7, 15,  0, 11,  1, 12,  1, 12,  0, 15,  0, 11,  2, 13,  1, 10,  5,  8,  7, 13,  6, 10,  0, 11,  4, 12,  7, 10,  1,  9,  1,  9,  0, 12,  2, 11,  7, 15,  1, 11,  0,  9,  2,  8,  4, 12,  2, 12,  6, 14,  4, 14,  3, 13,  3, 15,  1, 14,  4, 15,  6, 12,
    13,  6, 11,  3, 12,  4,  8,  4,  9,  4,  9,  5, 12,  6, 10,  5, 14,  7, 14,  3, 10,  2, 13,  3, 14,  5,  8,  2, 13,  1, 15,  6, 15,  4,  8,  4, 15,  5, 10,  1, 14,  5, 12,  4, 13,  5, 11,  1,  9,  5,  9,  3,  9,  0, 10,  7, 10,  7, 11,  6, 10,  3,  9,  0,
     3, 15,  5,  8,  1,  9,  5,  8,  4, 13,  6,  8,  4, 15,  6, 10,  6, 14,  7, 11,  0,  9,  1, 12,  0,  8,  6, 12,  6, 10,  3, 12,  7, 13,  7, 11,  3,  8,  0, 13,  7, 12,  0, 15,  4, 12,  5,  8,  4, 15,  5, 10,  0, 15,  3, 15,  1,  8,  1, 15,  4, 14,  6, 15,
    10,  6, 14,  0, 13,  4, 13,  1,  9,  1, 13,  3, 11,  2, 12,  1,  8,  3, 12,  3, 14,  6,  8,  4, 15,  4,  8,  3, 15,  1,  9,  6, 11,  2, 14,  3, 13,  7, 10,  4, 10,  3,  8,  7, 11,  2, 12,  1, 11,  1, 14,  3,  9,  6,  9,  7, 13,  5, 11,  7, 10,  2,  9,  3,
     1,  9,  0, 14,  7, 12,  7, 14,  5,  9,  6, 10,  6, 14,  6, 15,  1, 14,  7, 10,  3, 12,  0, 14,  5,  9,  6,  8,  1, 12,  0, 13,  2,  8,  4,  9,  6, 15,  0, 14,  4, 10,  7, 11,  5, 14,  2,  8,  4, 15,  3,  8,  7, 13,  1,  9,  0, 15,  0, 14,  5, 14,  0, 14,
    15,  4, 10,  6, 11,  1, 10,  2, 14,  3, 15,  1, 10,  3, 10,  3,  8,  4, 12,  3, 10,  6,  9,  6, 14,  2, 12,  3, 11,  4,  9,  6, 12,  5, 15,  0, 10,  3,  8,  4, 14,  1, 14,  3, 11,  2, 12,  7, 11,  2, 12,  6, 10,  1, 13,  4, 11,  4, 10,  5, 11,  2, 11,  7,
     0,  8,  1, 11,  4,  8,  4, 10,  6, 11,  3, 10,  6, 10,  6,  8,  1, 11,  1, 15,  7, 15,  6,  9,  1,  9,  7,  8,  0, 14,  5, 12,  3, 13,  4, 12,  0, 15,  5, 12,  1, 12,  7,  9,  6,  8,  3, 15,  0, 13,  6, 15,  7, 15,  7, 13,  2, 15,  1, 14,  2, 15,  0,  8,
    12,  5, 13,  5, 15,  1, 12,  1, 15,  1, 14,  6, 14,  2, 14,  2, 14,  6, 11,  6, 11,  3, 12,  2, 13,  6, 15,  3,  9,  5,  8,  2,  8,  5,  9,  1,  9,  4,  9,  2,  8,  4, 13,  1, 13,  0, 11,  6, 11,  6,  9,  2, 11,  3, 10,  1, 10,  6,  9,  6, 11,  7, 15,  5,
     5, 12,  0,  9,  5,  8,  5,  9,  0, 10,  2, 13,  4,  8,  5, 14,  2,  9,  2, 15,  4,  8,  2,  8,  0, 12,  6, 12,  0, 10,  5, 15,  4,  9,  3, 13,  0,  9,  4, 15,  1, 14,  1,  9,  0,  9,  2, 14,  6, 12,  0, 15,  7,  9,  2,  9,  0, 15,  6, 10,  3, 15,  3, 13,
     9,  1, 15,  5, 12,  3, 14,  0, 15,  7,  8,  5, 14,  1, 11,  1, 13,  6, 10,  7, 13,  0, 14,  5, 10,  7, 11,  3, 15,  6, 11,  1, 12,  2,  8,  4, 13,  5, 10,  2, 10,  6, 13,  5, 13,  4, 10,  5,  9,  3,  8,  4, 12,  1, 12,  5, 11,  6, 14,  3, 11,  7, 11,  7,
     2, 11,  4, 12,  7,  8,  3, 15,  3, 11,  4, 11,  7, 10,  1, 10,  2, 10,  0, 11,  4, 12,  0, 10,  1,  8,  7, 11,  1, 10,  1, 15,  7, 10,  2, 11,  1,  9,  6, 14,  2, 11,  1, 14,  4, 12,  2, 13,  1,  9,  5,  8,  7, 11,  7, 10,  7, 15,  4, 13,  3, 10,  1, 11,
    13,  5,  8,  0, 15,  3, 11,  6, 12,  7, 15,  3, 15,  1, 15,  4, 14,  5, 13,  6,  8,  1, 15,  6, 13,  5, 12,  2, 15,  4,  9,  4, 15,  3, 14,  6, 14,  5,  9,  3, 12,  6, 11,  5,  8,  2,  9,  5, 12,  5, 15,  1, 13,  3, 14,  2,  8,  0,  8,  0, 12,  7, 15,  5,
     0, 14,  1,  8,  2,  8,  2, 15,  2,  9,  6, 12,  0, 12,  7, 12,  7, 13,  0, 15,  6, 14,  5, 11,  1, 12,  5,  8,  5,  8,  1, 14,  0, 10,  4, 12,  6,  9,  6,  8,  0,  9,  0, 15,  4, 15,  5, 13,  1, 15,  5, 12,  7, 10,  6, 14,  4,  9,  6, 15,  0, 15,  7, 13,
    10,  4, 13,  5, 13,  5, 11,  5, 12,  5,  9,  3,  9,  4,  8,  3,  9,  3, 10,  6, 11,  1, 15,  2,  8,  5, 15,  1, 13,  2, 11,  6, 14,  6,  8,  2, 15,  3, 14,  3, 13,  6,  9,  6,  9,  0, 10,  2, 10,  7, 11,  1, 15,  3,  9,  3, 13,  3, 10,  3,  9,  4, 10,  1,
     4, 11,  7, 10,  2, 14,  4, 15,  3, 15,  2, 15,  7, 10,  2, 14,  1,  8,  0, 15,  2,  8,  1, 12,  2, 13,  1,  8,  7, 12,  6, 11,  1, 10,  4, 11,  2, 15,  0, 13,  0, 12,  7, 11,  5, 12,  7,  8,  6, 15,  4, 12,  7, 10,  6, 10,  0, 10,  0, 11,  4, 12,  7, 10,
    13,  2, 14,  2, 10,  7, 11,  1, 11,  7, 11,  7, 12,  3, 11,  7, 13,  4, 11,  6, 12,  5, 11,  6, 10,  7, 13,  4, 11,  3, 15,  3, 14,  4, 15,  3,  8,  5, 10,  7, 10,  6, 15,  3,  9,  3, 15,  1, 11,  3,  9,  0, 15,  1, 15,  3, 15,  4, 13,  5,  8,  3, 15,  3,
     3,  9,  1, 12,  4, 14,  0, 11,  7, 15,  1, 11,  3, 14,  0, 11,  5, 10,  2, 10,  2,  8,  6, 14,  7, 11,  0, 10,  7, 14,  7, 15,  6,  9,  6,  9,  1, 12,  7,  9,  4,  8,  7, 13,  0, 13,  6, 12,  6,  8,  1,  9,  1, 15,  4, 12,  0, 12,  4, 10,  0, 13,  0, 12,
    13,  5, 10,  6,  9,  1, 13,  6, 11,  0, 14,  7, 11,  7, 14,  5, 15,  2, 13,  5, 15,  4,  9,  2, 13,  3, 14,  7, 10,  3, 10,  3, 12,  3, 12,  2,  8,  5, 15,  3, 13,  3, 11,  3, 10,  4,  9,  3, 15,  1, 14,  4, 11,  6, 10,  3,  8,  4, 13,  0, 10,  4,  8,  5,
     5,  8,  3, 13,  4, 14,  1, 13,  7,  9,  2, 14,  4, 14,  4, 12,  7, 10,  6, 10,  5, 12,  2, 11,  6, 12,  7, 13,  4, 11,  2, 11,  3,  8,  5, 15,  2, 10,  1,  9,  2, 12,  0, 12,  0, 10,  1, 12,  0,  9,  7, 10,  0, 15,  4,  9,  0,  8,  1, 14,  4, 14,  4, 13,
    12,  2,  8,  6, 10,  0,  8,  5, 13,  3,  8,  5, 10,  3, 11,  2, 15,  0, 13,  3,  8,  2, 15,  6,  9,  1,  8,  3, 14,  1, 15,  5, 14,  7,  8,  0, 12,  6, 15,  5,  8,  5,  9,  4, 15,  5,  8,  5, 15,  4, 12,  3,  9,  6, 12,  3, 13,  5, 11,  7, 10,  1,  8,  3,
     4, 15,  2, 11,  2, 10,  6, 11,  3, 14,  7, 15,  0, 10,  0, 14,  3, 12,  4,  8,  7,  9,  3, 11,  0, 13,  2, 12,  2, 13,  5,  8,  5,  8,  2, 11,  4, 12,  2, 10,  7,  8,  6, 10,  7, 12,  7, 11,  0, 15,  7,  8,  7, 15,  0,  9,  2, 12,  6, 13,  0,  9,  4,  9,
     8,  3, 12,  5, 15,  5, 13,  1,  8,  7,  8,  3, 12,  5,  9,  5,  9,  6, 14,  1, 12,  3, 15,  7, 10,  7,  8,  5,  8,  5, 15,  0, 13,  1, 14,  7,  9,  1, 14,  7, 13,  2, 12,  3,  8,  1, 13,  3,  8,  4, 12,  0, 11,  3, 12,  5, 11,  5,  9,  3, 12,  6, 14,  2,
     5, 13,  2,  8,  6, 12,  7, 10,  5, 14,  4, 11,  2, 11,  2, 13,  2, 13,  5, 10,  5,  9,  0, 10,  7, 13,  4, 15,  2,  9,  1,  9,  5,  9,  6, 10,  4, 12,  5, 12,  4, 10,  3, 14,  3, 15,  1,  8,  4, 12,  5,  8,  5, 14,  7, 11,  7, 15,  5, 11,  2, 13,  1,  9,
    10,  0, 13,  6, 10,  2, 15,  3,  8,  1, 12,  2, 14,  6,  8,  5, 10,  7, 13,  1, 13,  1, 12,  7,  9,  1,  9,  2, 12,  5, 12,  4, 12,  3, 12,  2,  9,  0,  8,  1, 15,  1, 10,  7, 10,  6, 12,  4, 11,  1, 14,  2, 11,  0, 14,  1,  9,  2, 12,  2,  8,  5, 13,  5,
     6, 10,  7, 11,  0,  9,  3,  9,  5, 11,  0,  8,  0, 13,  2, 13,  4, 13,  5,  9,  4,  8,  2,  8,  0, 10,  0, 14,  6,  9,  6,  9,  4, 14,  7,  9,  0, 11,  7, 10,  7, 11,  7, 14,  4, 14,  5, 15,  4,  8,  4, 11,  5, 14,  0,  8,  1, 14,  7, 14,  2, 11,  7, 13,
    12,  3, 13,  3, 14,  5, 15,  6, 12,  0, 15,  4, 11,  5,  9,  5,  9,  2, 14,  1, 12,  1, 13,  5, 15,  4, 11,  7, 12,  1, 13,  2,  8,  1, 12,  0, 15,  6, 14,  2, 14,  3, 10,  1, 11,  3, 11,  0, 12,  1, 15,  2, 11,  1, 12,  4,  9,  4, 10,  2, 13,  6, 10,  2,
     2,  8,  4,  8,  4,  9,  0,  9,  2, 15,  5,  9,  6, 11,  7, 14,  0,  9,  2, 12,  2,  8,  0, 10,  1,  9,  7,  8,  2,  9,  1, 11,  2, 11,  2,  8,  1,  9,  1, 11,  7, 13,  6, 11,  1, 11,  0,  9,  2, 13,  4, 14,  1, 15,  2,  8,  7, 15,  0, 13,  6,  9,  4, 13,
    13,  5, 13,  1, 14,  0, 12,  6,  9,  6, 12,  0, 13,  0, 11,  3, 13,  6, 11,  7, 12,  5, 14,  6, 14,  5, 15,  3, 13,  6, 14,  7, 15,  6, 15,  5, 13,  4, 14,  5,  8,  2, 14,  3, 14,  6, 14,  5, 10,  6, 10,  2,  9,  5, 15,  5, 11,  1,  8,  5, 13,  3, 11,  2,
     2,  8,  6,  8,  0,  9,  1, 15,  0, 11,  4, 15,  7,  8,  4,  8,  1, 13,  7, 11,  1, 13,  5, 15,  1, 15,  2,  9,  0, 13,  5, 12,  3, 12,  2,  8,  1, 10,  1, 13,  5, 15,  3,  9,  3,  9,  2, 12,  5, 14,  6, 13,  1,  9,  6,  9,  1,  8,  4, 15,  7, 10,  7, 15,
    14,  5, 14,  3, 12,  4, 10,  7, 12,  7,  8,  2, 12,  1, 13,  2, 11,  4, 13,  3,  8,  5,  8,  1, 10,  7, 12,  6,  9,  5,  8,  0,  8,  7, 14,  5, 13,  6,  8,  4,  8,  2, 12,  6, 13,  5,  8,  7,  8,  3, 10,  3, 13,  4, 12,  1, 13,  4, 11,  1, 14,  3,  8,  1,
     2, 13,  6, 10,  0,  9,  1, 15,  0, 12,  4, 11,  2,  9,  0,  9,  0, 13,  1,  8,  0, 11,  5,  9,  6, 13,  2, 15,  2, 12,  7, 12,  6, 15,  2, 14,  1, 13,  1, 14,  4, 11,  2, 14,  1,  9,  0, 15,  1, 12,  5, 14,  6, 13,  2, 15,  3,  9,  1, 15,  7,  8,  1, 14,
    11,  7, 13,  3, 15,  6,  8,  4,  8,  4, 14,  2, 14,  6, 13,  6, 11,  4, 14,  4, 15,  4, 14,  3, 10,  2, 10,  5,  9,  6,  9,  3,  9,  1, 11,  6,  9,  4,  8,  4, 15,  0,  8,  4, 13,  4,  9,  4,  9,  5,  9,  3, 10,  2,  9,  5, 12,  6,  8,  4, 12,  0, 11,  4,
     4, 10,  4, 13,  1, 12,  7,  8,  0, 13,  4, 12,  2, 13,  1, 14,  7, 15,  3,  8,  7, 12,  3, 10,  4,  9,  4, 11,  7,  8,  2, 11,  2, 10,  0, 12,  1, 14,  6, 14,  7, 13,  7, 11,  3, 10,  0, 10,  4,  9,  3, 13,  0, 13,  7,  8,  7,  9,  7, 11,  2,  8,  1, 14,
    15,  0, 11,  1,  9,  6, 15,  3, 10,  7,  9,  1,  9,  7, 10,  7, 10,  1, 12,  5,  8,  1, 15,  7, 13,  3, 12,  2, 13,  2, 14,  7, 14,  5, 10,  4,  8,  4, 11,  1, 10,  3, 14,  0, 14,  6, 14,  5, 13,  2,  8,  7, 10,  7, 13,  2, 14,  2, 13,  2, 14,  5, 11,  6,
     7, 11,  5, 13,  1, 11,  7,  8,  5, 14,  4, 13,  1, 14,  5, 10,  2, 15,  4, 13,  7, 11,  7, 10,  1,  8,  5,  9,  4, 12,  7, 10,  1, 13,  7, 13,  7, 11,  0, 14,  5, 14,  2, 14,  4, 12,  4, 13,  7, 13,  7,  8,  2, 14,  2, 13,  5,  9,  5,  9,  2,  8,  1, 10,
    12,  0,  8,  2, 13,  4, 12,  1, 11,  1, 10,  1, 10,  4, 12,  2, 11,  5,  8,  1, 15,  3, 14,  1, 13,  4, 14,  2,  9,  2, 12,  2,  8,  4,  8,  2, 12,  3, 10,  6, 11,  0, 11,  7, 10,  1,  9,  2, 10,  0, 13,  1, 11,  7, 10,  6, 13,  1, 12,  1, 12,  6, 14,  7,
     2, 14,  4, 15,  7, 14,  5,  8,  4, 13,  2, 10,  4, 13,  0, 13,  1, 11,  1, 12,  7, 12,  6, 13,  4, 13,  4, 13,  2, 12,  2, 13,  1, 13,  7, 10,  2, 11,  2, 10,  7, 15,  7, 11,  5, 13,  2,  8,  2, 14,  4,  9,  2, 14,  0,  8,  1,  8,  7, 10,  5, 15,  6, 14,
    11,  7, 10,  2, 11,  1, 13,  0, 11,  1, 15,  7,  8,  1,  9,  4, 13,  4,  8,  4,  8,  3, 10,  0,  9,  0,  9,  2,  9,  7,  9,  6, 10,  7, 13,  1, 13,  7, 14,  6, 11,  3, 12,  2,  8,  2, 12,  5, 11,  5, 12,  2, 10,  5, 14,  4, 12,  4, 13,  2,  9,  1, 10,  0,
     2, 12,  0, 13,  7, 13,  2, 12,  4,  8,  1, 12,  4, 13,  6, 11,  7, 13,  7, 13,  1,  9,  7, 10,  2, 10,  1,  9,  2, 10,  1, 11,  6,  9,  4, 13,  2, 10,  0, 10,  2, 14,  0, 13,  7, 10,  7, 10,  0, 12,  0,  9,  0, 13,  2, 13,  1,  9,  0, 15,  2, 14,  2, 13,
    11,  7,  8,  4, 10,  2, 10,  7, 12,  1, 11,  7,  8,  2, 13,  1, 10,  0, 10,  2, 13,  6, 14,  0, 14,  7, 13,  6, 14,  5, 13,  4, 15,  1, 10,  2, 13,  7, 13,  7,  9,  4,  9,  6, 13,  3, 13,  3,  8,  4, 13,  4, 10,  6, 10,  5, 12,  4, 10,  7, 11,  6,  9,  6,
     4, 14,  6, 11,  7, 13,  6, 10,  4,  8,  4, 11,  6,  8,  5, 13,  7, 14,  7, 14,  1,  9,  0, 12,  1,  9,  1, 12,  4, 14,  7, 10,  4, 13,  7, 13,  7, 11,  4, 10,  1, 11,  7, 13,  4, 12,  1, 10,  4, 12,  2,  8,  2, 12,  2, 10,  2, 12,  1, 12,  4, 12,  2, 12,
     8,  2, 13,  2,  8,  1, 13,  1, 12,  1, 12,  2, 12,  2, 11,  1,  9,  2, 11,  3, 12,  4,  8,  4, 12,  4, 10,  6, 10,  1, 13,  2, 11,  2, 10,  1, 12,  0, 14,  2, 14,  4,  9,  2,  8,  1, 13,  4,  8,  0, 12,  4, 11,  6, 12,  6, 11,  7, 10,  7, 11,  2, 10,  7
};

/*static const uint8_t dither_matrix_halftone8[4 + 8*8] = {
    8, 8, 64, 3,
    60, 53, 42, 26, 27, 43, 54, 61,
    52, 41, 25, 13, 14, 28, 44, 55,
    40, 24, 12,  5,  6, 15, 29, 45,
    39, 23,  4,  0,  1,  7, 16, 30,
    38, 22, 11,  3,  2,  8, 17, 31,
    51, 37, 21, 10,  9, 18, 32, 46,
    59, 50, 36, 20, 19, 33, 47, 56,
    63, 58, 49, 35, 34, 48, 57, 62
}; */

static const uint8_t dither_matrix_diagonal45_8[4 + 8*8] = {
    8, 8, 64, 2,
    16, 32, 48, 56, 40, 24,  8,  0,
    36, 52, 60, 44, 28, 12,  4, 20,
    49, 57, 41, 25,  9,  1, 17, 33,
    61, 45, 29, 13,  5, 21, 37, 53,
    42, 26, 10,  2, 18, 34, 50, 58,
    30, 14,  6, 22, 38, 54, 62, 46,
    11,  3, 19, 35, 51, 59, 43, 27,
     7, 23, 39, 55, 63, 47, 31, 15
};


typedef struct halftone_pixelinfo {
    int x;
    int y;
    double distance;
    double angle;
} halftone_pixelinfo;

static inline halftone_pixelinfo* halftone_pixel_make(int w, int h) {
    int x, y, k;
    halftone_pixelinfo* hp = Gif_NewArray(halftone_pixelinfo, w * h);
    for (y = k = 0; y != h; ++y)
        for (x = 0; x != w; ++x, ++k) {
            hp[k].x = x;
            hp[k].y = y;
            hp[k].distance = -1;
        }
    return hp;
}

static inline void halftone_pixel_combine(halftone_pixelinfo* hp,
                                          double cx, double cy) {
    double d = (hp->x - cx) * (hp->x - cx) + (hp->y - cy) * (hp->y - cy);
    if (hp->distance < 0 || d < hp->distance) {
        hp->distance = d;
        hp->angle = atan2(hp->y - cy, hp->x - cx);
    }
}

static inline int halftone_pixel_compare(const void* va, const void* vb) {
    const halftone_pixelinfo* a = (const halftone_pixelinfo*) va;
    const halftone_pixelinfo* b = (const halftone_pixelinfo*) vb;
    if (fabs(a->distance - b->distance) > 0.01)
        return a->distance < b->distance ? -1 : 1;
    else
        return a->angle < b->angle ? -1 : 1;
}

static uint8_t* halftone_pixel_matrix(halftone_pixelinfo* hp,
                                      int w, int h, int nc) {
    int i;
    uint8_t* m = Gif_NewArray(uint8_t, 4 + w * h);
    m[0] = w;
    m[1] = h;
    m[3] = nc;
    if (w * h > 255) {
        double s = 255. / (w * h);
        m[2] = 255;
        for (i = 0; i != w * h; ++i)
            m[4 + hp[i].x + hp[i].y*w] = (int) (i * s);
    } else {
        m[2] = w * h;
        for (i = 0; i != w * h; ++i)
            m[4 + hp[i].x + hp[i].y*w] = i;
    }
    Gif_DeleteArray(hp);
    return m;
}

static uint8_t* make_halftone_matrix_square(int w, int h, int nc) {
    halftone_pixelinfo* hp = halftone_pixel_make(w, h);
    int i;
    for (i = 0; i != w * h; ++i)
        halftone_pixel_combine(&hp[i], (w-1)/2.0, (h-1)/2.0);
    qsort(hp, w * h, sizeof(*hp), halftone_pixel_compare);
    return halftone_pixel_matrix(hp, w, h, nc);
}

static uint8_t* make_halftone_matrix_triangular(int w, int h, int nc) {
    halftone_pixelinfo* hp = halftone_pixel_make(w, h);
    int i;
    for (i = 0; i != w * h; ++i) {
        halftone_pixel_combine(&hp[i], (w-1)/2.0, (h-1)/2.0);
        halftone_pixel_combine(&hp[i], -0.5, -0.5);
        halftone_pixel_combine(&hp[i], w-0.5, -0.5);
        halftone_pixel_combine(&hp[i], -0.5, h-0.5);
        halftone_pixel_combine(&hp[i], w-0.5, h-0.5);
    }
    qsort(hp, w * h, sizeof(*hp), halftone_pixel_compare);
    return halftone_pixel_matrix(hp, w, h, nc);
}

int set_dither_type(Gt_OutputData* od, const char* name) {
    int parm[4], nparm = 0;
    const char* comma = strchr(name, ',');
    char buf[256];

    /* separate arguments from dither name */
    if (comma && (size_t) (comma - name) < sizeof(buf)) {
        memcpy(buf, name, comma - name);
        buf[comma - name] = 0;
        name = buf;
    }
    for (nparm = 0;
         comma && *comma && isdigit((unsigned char) comma[1]);
         ++nparm)
        parm[nparm] = strtol(&comma[1], (char**) &comma, 10);

    /* parse dither name */
    if (od->dither_type == dither_ordered_new)
        Gif_DeleteArray(od->dither_data);
    od->dither_type = dither_none;

    if (strcmp(name, "none") == 0 || strcmp(name, "posterize") == 0)
        /* ok */;
    else if (strcmp(name, "default") == 0)
        od->dither_type = dither_default;
    else if (strcmp(name, "floyd-steinberg") == 0
             || strcmp(name, "fs") == 0)
        od->dither_type = dither_floyd_steinberg;
    else if (strcmp(name, "o3") == 0 || strcmp(name, "o3x3") == 0
             || (strcmp(name, "o") == 0 && nparm >= 1 && parm[0] == 3)) {
        od->dither_type = dither_ordered;
        od->dither_data = dither_matrix_o3x3;
    } else if (strcmp(name, "o4") == 0 || strcmp(name, "o4x4") == 0
               || (strcmp(name, "o") == 0 && nparm >= 1 && parm[0] == 4)) {
        od->dither_type = dither_ordered;
        od->dither_data = dither_matrix_o4x4;
    } else if (strcmp(name, "o8") == 0 || strcmp(name, "o8x8") == 0
               || (strcmp(name, "o") == 0 && nparm >= 1 && parm[0] == 8)) {
        od->dither_type = dither_ordered;
        od->dither_data = dither_matrix_o8x8;
    } else if (strcmp(name, "ro64") == 0 || strcmp(name, "ro64x64") == 0
               || strcmp(name, "o") == 0 || strcmp(name, "ordered") == 0) {
        od->dither_type = dither_ordered;
        od->dither_data = dither_matrix_ro64x64;
    } else if (strcmp(name, "diag45") == 0 || strcmp(name, "diagonal") == 0) {
        od->dither_type = dither_ordered;
        od->dither_data = dither_matrix_diagonal45_8;
    } else if (strcmp(name, "halftone") == 0 || strcmp(name, "half") == 0
               || strcmp(name, "trihalftone") == 0
               || strcmp(name, "trihalf") == 0) {
        int size = nparm >= 1 && parm[0] > 0 ? parm[0] : 6;
        int ncol = nparm >= 2 && parm[1] > 1 ? parm[1] : 2;
        od->dither_type = dither_ordered_new;
        od->dither_data = make_halftone_matrix_triangular(size, (int) (size * sqrt(3) + 0.5), ncol);
    } else if (strcmp(name, "sqhalftone") == 0 || strcmp(name, "sqhalf") == 0
               || strcmp(name, "squarehalftone") == 0) {
        int size = nparm >= 1 && parm[0] > 0 ? parm[0] : 6;
        int ncol = nparm >= 2 && parm[1] > 1 ? parm[1] : 2;
        od->dither_type = dither_ordered_new;
        od->dither_data = make_halftone_matrix_square(size, size, ncol);
    } else
        return -1;

    if (od->dither_type == dither_ordered
        && nparm >= 2 && parm[1] > 1 && parm[1] != od->dither_data[3]) {
        int size = od->dither_data[0] * od->dither_data[1];
        uint8_t* dd = Gif_NewArray(uint8_t, 4 + size);
        memcpy(dd, od->dither_data, 4 + size);
        dd[3] = parm[1];
        od->dither_data = dd;
        od->dither_type = dither_ordered_new;
    }
    return 0;
}
