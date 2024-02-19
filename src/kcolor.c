/* kcolor.c - Kcolor subsystem for gifsicle.
   Copyright (C) 1997-2024 Eddie Kohler, ekohler@gmail.com
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
    static char buf[4][64];
    whichbuf = (whichbuf + 1) % 4;
    if (x.a[0] >= 0 && x.a[1] >= 0 && x.a[2] >= 0) {
        kc_revgamma_transform(&x);
        snprintf(buf[whichbuf], sizeof(buf[whichbuf]), "#%02X%02X%02X",
                 x.a[0] >> 7, x.a[1] >> 7, x.a[2] >> 7);
    } else
        snprintf(buf[whichbuf], sizeof(buf[whichbuf]), "<%d,%d,%d>",
                 x.a[0], x.a[1], x.a[2]);
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

static int kd3_item_compare_0(const void* a, const void* b) {
    const int* aa = (const int*) a, *bb = (const int*) b;
    return kd3_sorter->ks[*aa].a[0] - kd3_sorter->ks[*bb].a[0];
}

static int kd3_item_compare_1(const void* a, const void* b) {
    const int* aa = (const int*) a, *bb = (const int*) b;
    return kd3_sorter->ks[*aa].a[1] - kd3_sorter->ks[*bb].a[1];
}

static int kd3_item_compare_2(const void* a, const void* b) {
    const int* aa = (const int*) a, *bb = (const int*) b;
    return kd3_sorter->ks[*aa].a[2] - kd3_sorter->ks[*bb].a[2];
}

static int (*kd3_item_compares[])(const void*, const void*) = {
    &kd3_item_compare_0, &kd3_item_compare_1, &kd3_item_compare_2
};

static int kd3_item_compare_all(const void* a, const void* b) {
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

    qsort(perm, nperm, sizeof(int), kd3_item_compares[aindex]);

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
            snprintf(x[2*i], sizeof(x[2*i]), "*");
        else
            snprintf(x[2*i], sizeof(x[2*i]), "%d", a[i]);
        if (b[i] == INT_MAX)
            snprintf(x[2*i+1], sizeof(x[2*i+1]), "*");
        else
            snprintf(x[2*i+1], sizeof(x[2*i+1]), "%d", b[i]);
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
    qsort(perm, kd3->nitems, sizeof(int), kd3_item_compare_all);
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
