/* kcolor.h - Color-oriented function declarations for gifsicle.
   Copyright (C) 2013-2017 Eddie Kohler, ekohler@gmail.com
   This file is part of gifsicle.

   Gifsicle is free software. It is distributed under the GNU Public License,
   version 2; you can copy, distribute, or alter it at will, as long
   as this notice is kept intact and this source code is made available. There
   is no warranty, express or implied. */

#ifndef GIFSICLE_KCOLOR_H
#define GIFSICLE_KCOLOR_H
#include <lcdfgif/gif.h>
#include <assert.h>

/* kcolor: a 3D vector, each component has 15 bits of precision */
/* 15 bits means KC_MAX * KC_MAX always fits within a signed 32-bit
   integer, and a 3-D squared distance always fits within an unsigned 32-bit
   integer. */
#define KC_MAX     0x7FFF
#define KC_WHOLE   0x8000
#define KC_HALF    0x4000
#define KC_QUARTER 0x2000
#define KC_BITS  15
typedef struct kcolor {
    int16_t a[3];
} kcolor;

#undef min
#undef max
#define min(a, b)       ((a) < (b) ? (a) : (b))
#define max(a, b)       ((a) > (b) ? (a) : (b))

#define KC_CLAMPV(v) (max(0, min((v), KC_MAX)))

typedef union kacolor {
    kcolor k;
    int16_t a[4];
#if HAVE_INT64_T
    int64_t q; /* to get better alignment */
#endif
} kacolor;


/* gamma_tables[0]: array of 256 gamma-conversion values
   gamma_tables[1]: array of 256 reverse gamma-conversion values */
extern uint16_t* gamma_tables[2];


/* set `*kc` to the gamma transformation of `a0/a1/a2` [RGB] */
static inline void kc_set8g(kcolor* kc, int a0, int a1, int a2) {
    kc->a[0] = gamma_tables[0][a0];
    kc->a[1] = gamma_tables[0][a1];
    kc->a[2] = gamma_tables[0][a2];
}

/* return the gamma transformation of `a0/a1/a2` [RGB] */
static inline kcolor kc_make8g(int a0, int a1, int a2) {
    kcolor kc;
    kc_set8g(&kc, a0, a1, a2);
    return kc;
}

/* return the gamma transformation of `*gfc` */
static inline kcolor kc_makegfcg(const Gif_Color* gfc) {
    return kc_make8g(gfc->gfc_red, gfc->gfc_green, gfc->gfc_blue);
}

/* return the uncorrected representation of `a0/a1/a2` [RGB] */
static inline kcolor kc_make8ng(int a0, int a1, int a2) {
    kcolor kc;
    kc.a[0] = (a0 << 7) + (a0 >> 1);
    kc.a[1] = (a1 << 7) + (a1 >> 1);
    kc.a[2] = (a2 << 7) + (a2 >> 1);
    return kc;
}

/* return the kcolor representation of `*gfc` (no gamma transformation) */
static inline kcolor kc_makegfcng(const Gif_Color* gfc) {
    return kc_make8ng(gfc->gfc_red, gfc->gfc_green, gfc->gfc_blue);
}

/* return transparency */
static inline kacolor kac_transparent() {
    kacolor x;
    x.a[0] = x.a[1] = x.a[2] = x.a[3] = 0;
    return x;
}

/* return a hex color string definition for `x` */
const char* kc_debug_str(kcolor x);

/* set `*x` to the reverse gamma transformation of `*x` */
void kc_revgamma_transform(kcolor* x);

/* return the reverse gramma transformation of `*x` as a Gif_Color */
static inline Gif_Color kc_togfcg(const kcolor* x) {
    kcolor xx = *x;
    Gif_Color gfc;
    kc_revgamma_transform(&xx);
    gfc.gfc_red = (uint8_t) (xx.a[0] >> 7);
    gfc.gfc_green = (uint8_t) (xx.a[1] >> 7);
    gfc.gfc_blue = (uint8_t) (xx.a[2] >> 7);
    gfc.haspixel = 0;
    return gfc;
}


/* return the squared Euclidean distance between `*x` and `*y` */
static inline uint32_t kc_distance(const kcolor* x, const kcolor* y) {
    /* It’s OK to use unsigned multiplication for this: the low 32 bits
    are the same either way. Unsigned avoids undefined behavior. */
    uint32_t d0 = x->a[0] - y->a[0];
    uint32_t d1 = x->a[1] - y->a[1];
    uint32_t d2 = x->a[2] - y->a[2];
    return d0 * d0 + d1 * d1 + d2 * d2;
}

/* return the luminance value for `*x`; result is between 0 and KC_MAX */
static inline int kc_luminance(const kcolor* x) {
    return (55 * x->a[0] + 183 * x->a[1] + 19 * x->a[2]) >> 8;
}

/* set `*x` to the grayscale version of `*x`, transformed by luminance */
static inline void kc_luminance_transform(kcolor* x) {
    /* For grayscale colormaps, use distance in luminance space instead of
       distance in RGB space. The weights for the R,G,B components in
       luminance space are 0.2126,0.7152,0.0722. (That's ITU primaries, which
       are compatible with sRGB; NTSC recommended our previous values,
       0.299,0.587,0.114.) Using the proportional factors 55,183,19 we get a
       scaled gray value between 0 and 255 * 257; dividing by 256 gives us
       what we want. Thanks to Christian Kumpf, <kumpf@igd.fhg.de>, for
       providing a patch.*/
    x->a[0] = x->a[1] = x->a[2] = kc_luminance(x);
}


/* wkcolor: like kcolor, but components are 32 bits instead of 16 */

typedef struct wkcolor {
    int32_t a[3];
} wkcolor;

static inline void wkc_clear(wkcolor* x) {
    x->a[0] = x->a[1] = x->a[2] = 0;
}


/* kd3_tree: kd-tree for 3 dimensions, indexing kcolors */

typedef struct kd3_tree kd3_tree;
typedef struct kd3_treepos kd3_treepos;

struct kd3_tree {
    kd3_treepos* tree;
    int ntree;
    int disabled;
    kcolor* ks;
    int nitems;
    int items_cap;
    int maxdepth;
    void (*transform)(kcolor*);
    unsigned* xradius;
};

/* initialize `kd3` with the given color `transform` (may be NULL) */
void kd3_init(kd3_tree* kd3, void (*transform)(kcolor*));

/* free `kd3` */
void kd3_cleanup(kd3_tree* kd3);

/* add the transformed color `k` to `*kd3` (do not apply `kd3->transform`). */
void kd3_add_transformed(kd3_tree* kd3, const kcolor* k);

/* given 8-bit color `a0/a1/a2` (RGB), gamma-transform it, transform it
   by `kd3->transform` if necessary, and add it to `*kd3` */
void kd3_add8g(kd3_tree* kd3, int a0, int a1, int a2);

/* set `kd3->xradius`. given color `i`, `kd3->xradius[i]` is the square of the
   color's uniquely owned neighborhood.
   If `kc_distance(&kd3->ks[i], &k) < kd3->xradius[i]`, then
   `kd3_closest_transformed(kd3, &k) == i`. */
void kd3_build_xradius(kd3_tree* kd3);

/* build the actual kd-tree for `kd3`. must be called before kd3_closest. */
void kd3_build(kd3_tree* kd3);

/* kd3_init + kd3_add8g for all colors in `gfcm` + kd3_build */
void kd3_init_build(kd3_tree* kd3, void (*transform)(kcolor*),
                    const Gif_Colormap* gfcm);

/* return the index of the color in `*kd3` closest to `k`.
   if `dist!=NULL`, store the distance from `k` to that index in `*dist`. */
int kd3_closest_transformed(kd3_tree* kd3, const kcolor* k,
                            unsigned* dist);

/* given 8-bit color `a0/a1/a2` (RGB), gamma-transform it, transform it by
   `kd3->transform` if necessary, and return the index of the color in
   `*kd3` closest to it. */
int kd3_closest8g(kd3_tree* kd3, int a0, int a1, int a2);

/* disable color index `i` in `*kd3`: it will never be returned by
   `kd3_closest*` */
static inline void kd3_disable(kd3_tree* kd3, int i) {
    assert((unsigned) i < (unsigned) kd3->nitems);
    assert(kd3->disabled < 0 || kd3->disabled == i);
    kd3->disabled = i;
}

/* enable all color indexes in `*kd3` */
static inline void kd3_enable_all(kd3_tree* kd3) {
    kd3->disabled = -1;
}


typedef uint32_t kchist_count_t;
typedef struct kchistitem {
    kacolor ka;
    kchist_count_t count;
} kchistitem;

typedef struct kchist {
    kchistitem* h;
    int n;
    int capacity;
} kchist;

void kchist_init(kchist* kch);
void kchist_cleanup(kchist* kch);
void kchist_make(kchist* kch, Gif_Stream* gfs, uint32_t* ntransp);
kchistitem* kchist_add(kchist* kch, kcolor color, kchist_count_t count);
void kchist_compress(kchist* kch);


typedef struct {
    kchist* kch;
    int* closest;
    uint32_t* min_dist;
    uint32_t* min_dither_dist;
    int* chosen;
    int nchosen;
} kcdiversity;

void kcdiversity_init(kcdiversity* div, kchist* kch, int dodither);
void kcdiversity_cleanup(kcdiversity* div);
int kcdiversity_find_popular(kcdiversity* div);
int kcdiversity_find_diverse(kcdiversity* div, double ditherweight);
int kcdiversity_choose(kcdiversity* div, int chosen, int dodither);


Gif_Colormap* colormap_blend_diversity(kchist* kch, Gt_OutputData* od);
Gif_Colormap* colormap_flat_diversity(kchist* kch, Gt_OutputData* od);
Gif_Colormap* colormap_median_cut(kchist* kch, Gt_OutputData* od);


#if HAVE_SIMD && HAVE_VECTOR_SIZE_VECTOR_TYPES
typedef float float4 __attribute__((vector_size (sizeof(float) * 4)));
typedef int int4 __attribute__((vector_size (sizeof(int) * 4)));
#elif HAVE_SIMD && HAVE_EXT_VECTOR_TYPE_VECTOR_TYPES
typedef float float4 __attribute__((ext_vector_type (4)));
#else
typedef float float4[4];
#endif

typedef union scale_color {
    float4 a;
} scale_color;

static inline void sc_clear(scale_color* x) {
    x->a[0] = x->a[1] = x->a[2] = x->a[3] = 0;
}

static inline scale_color sc_makekc(const kcolor* k) {
    scale_color sc;
    sc.a[0] = k->a[0];
    sc.a[1] = k->a[1];
    sc.a[2] = k->a[2];
    sc.a[3] = KC_MAX;
    return sc;
}

static inline scale_color sc_make(float a0, float a1, float a2, float a3) {
    scale_color sc;
    sc.a[0] = a0;
    sc.a[1] = a1;
    sc.a[2] = a2;
    sc.a[3] = a3;
    return sc;
}

#if HAVE_SIMD
# define SCVEC_ADDV(sc, sc2) (sc).a += (sc2).a
# define SCVEC_MULV(sc, sc2) (sc).a *= (sc2).a
# define SCVEC_MULF(sc, f) (sc).a *= (f)
# define SCVEC_DIVF(sc, f) (sc).a /= (f)
# define SCVEC_ADDVxF(sc, sc2, f) (sc).a += (sc2).a * (f)
# if HAVE___BUILTIN_SHUFFLEVECTOR
#  define SCVEC_ROT3(out, sc) do { (out).a = __builtin_shufflevector((sc).a, (sc).a, 1, 2, 0, 3); } while (0)
# else
#  define SCVEC_ROT3(out, sc) do { int4 shufmask__ = {1, 2, 0, 3}; (out).a = __builtin_shuffle((sc).a, shufmask__); } while (0)
# endif
#else
# define SCVEC_FOREACH(t) do { int k__; for (k__ = 0; k__ != 4; ++k__) { t; } } while (0)
# define SCVEC_ADDV(sc, sc2) SCVEC_FOREACH((sc).a[k__] += (sc2).a[k__])
# define SCVEC_MULV(sc, sc2) SCVEC_FOREACH((sc).a[k__] *= (sc2).a[k__])
# define SCVEC_MULF(sc, f) SCVEC_FOREACH((sc).a[k__] *= (f))
# define SCVEC_DIVF(sc, f) SCVEC_FOREACH((sc).a[k__] /= (f))
# define SCVEC_ADDVxF(sc, sc2, f) SCVEC_FOREACH((sc).a[k__] += (sc2).a[k__] * (f))
# define SCVEC_ROT3(out, sc) do { float __a0 = (sc).a[0]; (out).a[0] = (sc).a[1]; (out).a[1] = (sc).a[2]; (out).a[2] = __a0; (out).a[3] = (sc).a[3]; } while (0)
#endif

#endif
