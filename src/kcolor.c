/* kcolor.c - Kcolor subsystem for gifsicle.
   Copyright (C) 1997-2025 Eddie Kohler, ekohler@gmail.com
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

static const float linear_srgb_table_256[256] = {
    0.00000, 0.04984009, 0.08494473, 0.11070206, 0.13180381, 0.1500052, 0.1661857, 0.18085852,
    0.19435316, 0.20689574, 0.21864912, 0.22973509, 0.2402475, 0.25026038, 0.25983337, 0.26901522,
    0.27784654, 0.28636143, 0.29458886, 0.3025538, 0.31027776, 0.31777957, 0.32507575, 0.33218095,
    0.33910814, 0.34586892, 0.35247374, 0.35893196, 0.3652521, 0.3714419, 0.37750843, 0.38345808,
    0.38929683, 0.39503005, 0.40066284, 0.40619975, 0.41164514, 0.417003, 0.42227703, 0.42747074,
    0.4325873, 0.4376298, 0.44260103, 0.4475037, 0.45234028, 0.45711315, 0.46182457, 0.4664766,
    0.47107124, 0.4756104, 0.4800958, 0.4845292, 0.48891217, 0.49324623, 0.49753287, 0.5017734,
    0.5059693, 0.5101216, 0.5142317, 0.5183006, 0.5223295, 0.5263194, 0.53027135, 0.53418624,
    0.53806514, 0.54190874, 0.5457181, 0.54949385, 0.5532369, 0.556948, 0.5606278, 0.5642771,
    0.56789654, 0.5714868, 0.57504845, 0.5785821, 0.5820884, 0.58556795, 0.58902115, 0.59244865,
    0.59585094, 0.5992285, 0.60258186, 0.60591143, 0.60921764, 0.612501, 0.61576194, 0.6190008,
    0.622218, 0.62541395, 0.62858903, 0.6317436, 0.63487804, 0.6379926, 0.6410878, 0.6441637,
    0.64722085, 0.6502595, 0.6532799, 0.65628237, 0.65926725, 0.6622347, 0.6651851, 0.66811866,
    0.67103565, 0.6739363, 0.67682093, 0.6796897, 0.6825429, 0.6853807, 0.6882034, 0.69101113,
    0.69380414, 0.6965826, 0.69934684, 0.70209694, 0.7048331, 0.7075556, 0.7102645, 0.71296,
    0.7156424, 0.7183118, 0.7209683, 0.7236121, 0.7262435, 0.7288625, 0.73146933, 0.73406404,
    0.73664695, 0.73921806, 0.7417776, 0.74432564, 0.7468624, 0.749388, 0.75190246, 0.7544061,
    0.7568989, 0.759381, 0.76185256, 0.7643137, 0.7667645, 0.7692052, 0.7716358, 0.7740564,
    0.77646714, 0.77886814, 0.78125954, 0.78364134, 0.7860138, 0.7883768, 0.79073066, 0.7930754,
    0.795411, 0.7977377, 0.80005556, 0.8023647, 0.8046651, 0.80695695, 0.8092403, 0.8115152,
    0.8137818, 0.81604016, 0.8182903, 0.8205324, 0.8227665, 0.8249926, 0.8272109, 0.8294214,
    0.8316242, 0.8338194, 0.836007, 0.8381871, 0.84035975, 0.84252506, 0.8446831, 0.84683394,
    0.84897757, 0.85111415, 0.8532437, 0.85536623, 0.8574819, 0.8595907, 0.8616927, 0.86378807,
    0.8658767, 0.8679587, 0.87003416, 0.87210315, 0.87416565, 0.8762218, 0.8782716, 0.8803151,
    0.8823524, 0.8843835, 0.8864085, 0.8884274, 0.8904402, 0.8924471, 0.89444804, 0.8964431,
    0.8984324, 0.9004158, 0.90239346, 0.9043654, 0.9063318, 0.9082925, 0.91024756, 0.9121972,
    0.9141413, 0.91608, 0.9180133, 0.9199412, 0.92186373, 0.92378104, 0.9256931, 0.92759997,
    0.92950165, 0.9313982, 0.93328965, 0.9351761, 0.9370575, 0.9389339, 0.9408054, 0.9426719,
    0.9445336, 0.94639045, 0.9482424, 0.9500897, 0.9519322, 0.95377004, 0.9556032, 0.9574316,
    0.9592555, 0.9610748, 0.96288955, 0.9646998, 0.9665055, 0.9683068, 0.9701037, 0.9718961,
    0.9736842, 0.9754679, 0.97724736, 0.9790225, 0.9807934, 0.9825601, 0.98432255, 0.9860808,
    0.987835, 0.989585, 0.9913309, 0.99307275, 0.9948106, 0.99654436, 0.99827415, 1.00000
};

const uint16_t* gamma_tables[2] = {
    srgb_gamma_table_256,
    srgb_revgamma_table_256
};

#if ENABLE_THREADS
pthread_mutex_t kd3_sort_lock;
#endif


const char* kc_debug_str(kcolor x) {
    static int whichbuf = 0;
    static char buf[8][64];
    whichbuf = (whichbuf + 1) % 8;
    if (x.a[0] >= 0 && x.a[1] >= 0 && x.a[2] >= 0) {
        x = kc_revgamma_transform(x);
        snprintf(buf[whichbuf], sizeof(buf[whichbuf]), "#%02X%02X%02X",
                 x.a[0] >> 7, x.a[1] >> 7, x.a[2] >> 7);
    } else
        snprintf(buf[whichbuf], sizeof(buf[whichbuf]), "<%d,%d,%d>",
                 x.a[0], x.a[1], x.a[2]);
    return buf[whichbuf];
}

void kc_set_gamma(int type, double gamma) {
    static int cur_type = KC_GAMMA_SRGB;
    static double cur_gamma = 2.2;
    int i, j;
    uint16_t* g[2];
#if !HAVE_POW
    if (type == KC_GAMMA_NUMERIC && gamma != 1.0) {
        type = KC_GAMMA_SRGB;
    }
#endif
    if (type == cur_type && (type != KC_GAMMA_NUMERIC || gamma == cur_gamma)) {
        return;
    }
    if (type != KC_GAMMA_NUMERIC) {
        if (gamma_tables[0] != srgb_gamma_table_256) {
            Gif_DeleteArray(gamma_tables[0]);
        }
        gamma_tables[0] = srgb_gamma_table_256;
        gamma_tables[1] = srgb_revgamma_table_256;
    } else {
        if (gamma_tables[0] == srgb_gamma_table_256) {
            gamma_tables[0] = Gif_NewArray(uint16_t, 512);
            gamma_tables[1] = gamma_tables[0] + 256;
        }
        g[0] = (uint16_t*) gamma_tables[0];
        g[1] = (uint16_t*) gamma_tables[1];
        for (j = 0; j != 256; ++j) {
#if HAVE_POW
            g[0][j] = (int) (pow(j / 255.0, gamma) * 32767 + 0.5);
            g[1][j] = (int) (pow(j / 256.0, 1/gamma) * 32767 + 0.5);
#else
            g[0][j] = (int) (j / 255.0 * 32767 + 0.5);
            g[1][j] = (int) (j / 256.0 * 32767 + 0.5);
#endif
            /* The ++gamma_tables[][] ensures that round-trip gamma correction
               always preserve the input colors. Without it, one might have,
               for example, input values 0, 1, and 2 all mapping to
               gamma-corrected value 0. Then a round-trip through gamma
               correction loses information.
            */
            for (i = j ? 0 : 2; i != 2; ++i) {
                while (g[i][j] <= g[i][j-1] && g[i][j] < 32767)
                    ++g[i][j];
            }
        }
    }
    cur_type = type;
    cur_gamma = gamma;
}

kcolor kc_revgamma_transform(kcolor x) {
    int d;
    for (d = 0; d != 3; ++d) {
        int c = gamma_tables[1][x.a[d] >> 7];
        while (c < 0x7F80 && x.a[d] >= gamma_tables[0][(c + 0x80) >> 7])
            c += 0x80;
        x.a[d] = c;
    }
    return x;
}

#if 0
static void kc_test_gamma() {
    int x, y, z;
    for (x = 0; x != 256; ++x)
        for (y = 0; y != 256; ++y)
            for (z = 0; z != 256; ++z) {
                kcolor k = kc_make8g(x, y, z);
                kc_revgamma_transform(&k);
                if ((k.a[0] >> 7) != x || (k.a[1] >> 7) != y
                    || (k.a[2] >> 7) != z) {
                    kcolor kg = kc_make8g(x, y, z);
                    fprintf(stderr, "#%02X%02X%02X ->g #%04X%04X%04X ->revg #%02X%02X%02X!\n",
                            x, y, z, kg.a[0], kg.a[1], kg.a[2],
                            k.a[0] >> 7, k.a[1] >> 7, k.a[2] >> 7);
                    assert(0);
                }
            }
}
#endif

kcolor kc_oklab_transform(int a0, int a1, int a2) {
#if HAVE_CBRTF
    float cr = linear_srgb_table_256[a0];
    float cg = linear_srgb_table_256[a1];
    float cb = linear_srgb_table_256[a2];

    float l = 0.4122214708f * cr + 0.5363325363f * cg + 0.0514459929f * cb;
    float m = 0.2119034982f * cr + 0.6806995451f * cg + 0.1073969566f * cb;
    float s = 0.0883024619f * cr + 0.2817188376f * cg + 0.6299787005f * cb;

    float l_ = cbrtf(l);
    float m_ = cbrtf(m);
    float s_ = cbrtf(s);

    float okl = 0.2104542553f*l_ + 0.7936177850f*m_ - 0.0040720468f*s_;
    float oka = 1.9779984951f*l_ - 2.4285922050f*m_ + 0.4505937099f*s_;
    float okb = 0.0259040371f*l_ + 0.7827717662f*m_ - 0.8086757660f*s_;

    // On sRGB colors, `okl` ranges from 0 to 1,
    // `oka` ranges from -0.23388740 to +0.27621666,
    // `okb` ranges from -0.31152815 to +0.19856972.

    kcolor kc;
    kc.a[0] = okl * 32767;
    kc.a[1] = (oka + 0.5) * 32767;
    kc.a[2] = (okb + 0.5) * 32767;
    return kc;
#else
    /* fall back to sRGB gamma */
    return kc_make8g(a0, a1, a2);
#endif
}


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

void kd3_init(kd3_tree* kd3, kcolor (*transform)(int, int, int)) {
    kd3->tree = NULL;
    kd3->ks = Gif_NewArray(kcolor, 256);
    kd3->nitems = 0;
    kd3->items_cap = 256;
    kd3->transform = transform ? transform : kc_make8g;
    kd3->xradius = NULL;
    kd3->disabled = -1;
}

void kd3_cleanup(kd3_tree* kd3) {
    Gif_DeleteArray(kd3->tree);
    Gif_DeleteArray(kd3->ks);
    Gif_DeleteArray(kd3->xradius);
}

void kd3_add_transformed(kd3_tree* kd3, kcolor k) {
    if (kd3->nitems == kd3->items_cap) {
        kd3->items_cap *= 2;
        Gif_ReArray(kd3->ks, kcolor, kd3->items_cap);
    }
    kd3->ks[kd3->nitems] = k;
    ++kd3->nitems;
    if (kd3->tree) {
        Gif_DeleteArray(kd3->tree);
        Gif_DeleteArray(kd3->xradius);
        kd3->tree = NULL;
        kd3->xradius = NULL;
    }
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
            unsigned dist = kc_distance(kd3->ks[i], kd3->ks[j]);
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

void kd3_init_build(kd3_tree* kd3, kcolor (*transform)(int, int, int),
                    const Gif_Colormap* gfcm) {
    int i;
    kd3_init(kd3, transform);
    for (i = 0; i < gfcm->ncol; ++i)
        kd3_add8g(kd3, gfcm->col[i].gfc_red, gfcm->col[i].gfc_green,
                  gfcm->col[i].gfc_blue);
    kd3_build(kd3);
}

int kd3_closest_transformed(kd3_tree* kd3, kcolor k, unsigned* dist_store) {
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
                unsigned dist = kc_distance(kd3->ks[p->pivot], k);
                if (dist < mindist) {
                    mindist = dist;
                    result = p->pivot;
                }
            }
            if (--stackpos >= 0)
                ++state[stackpos];
        } else if (state[stackpos] == 0) {
            if (k.a[stackpos % 3] < p->pivot)
                stack[stackpos + 1] = p + 1;
            else
                stack[stackpos + 1] = p + p->offset;
            ++stackpos;
            state[stackpos] = 0;
        } else {
            int delta = k.a[stackpos % 3] - p->pivot;
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
