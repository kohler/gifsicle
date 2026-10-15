/* fuzz_main.c - Fuzz target that runs gifsicle command lines.
   This file is part of gifsicle's fuzzing harness. See README.md.

   Each input is a command line followed by up to three input files:

     ARGS [SEP IN0 [SEP IN1 [SEP PALETTE]]]      SEP = "\n#FILE#\n"

   ARGS is split into arguments at whitespace and NUL bytes. The files are
   named in0.gif, in1.gif, and palette.gif, and the target runs `gifsicle
   ARGS` with them, checking the results (see runner.c). Inputs with
   arguments that contain `/` are skipped, so gifsicle can only touch files
   in its scratch directory. So are inputs with arguments that name a loop
   extension (gifsicle writes any `--app-extension` as given, but reads
   NETSCAPE2.0 and ANIMEXTS1.0 extensions as loop counts), and inputs whose
   files are huge GIFs, since gifsicle's time on those isn't interesting. */

#include "runner.h"
#include <string.h>

#define SEP             "\n#FILE#\n"
#define SEPLEN          (sizeof(SEP) - 1)
#define NFILES          3
#define MAX_ARGS        48      /* leaves room for the runner's options */
#define MAX_ARG_LEN     256

static const char* const input_names[NFILES] = {
    "in0.gif", "in1.gif", "palette.gif"
};

static const uint8_t* find_sep(const uint8_t* p, const uint8_t* end) {
    while ((size_t) (end - p) >= SEPLEN) {
        const uint8_t* q = (const uint8_t*) memchr(p, SEP[0], end - p - SEPLEN + 1);
        if (!q)
            return NULL;
        if (memcmp(q, SEP, SEPLEN) == 0)
            return q;
        p = q + 1;
    }
    return NULL;
}

/* Returns 0 if `data` encodes a command line we are willing to run. */
static int decode(const uint8_t* data, size_t size, run_spec* spec) {
    const uint8_t* end = data + size;
    const uint8_t* args_end = find_sep(data, end);
    const uint8_t* p = data;
    int i;
    spec_init(spec);

    /* arguments */
    if (!args_end)
        args_end = end;
    while (p < args_end) {
        const uint8_t* start;
        while (p < args_end && *p <= ' ')
            ++p;
        if (p == args_end)
            break;
        start = p;
        while (p < args_end && *p > ' ')
            ++p;
        if (spec->argc > MAX_ARGS || p - start > MAX_ARG_LEN
            || memchr(start, '/', p - start))
            return -1;          /* also covers absolute and parent paths */
        spec_arg(spec, "%.*s", (int) (p - start), (const char*) start);
        if (strstr(spec->argv[spec->argc - 1], "NETSCAPE2.0")
            || strstr(spec->argv[spec->argc - 1], "ANIMEXTS1.0"))
            return -1;
    }

    /* files */
    p = args_end;
    for (i = 0; i < NFILES && p < end; ++i) {
        const uint8_t* next;
        p += SEPLEN;
        next = i < NFILES - 1 ? find_sep(p, end) : NULL;
        if (!next)
            next = end;
        if (!fuzz_within_budget(p, next - p))
            return -1;
        spec_file(spec, input_names[i], p, next - p);
        p = next;
    }
    return 0;
}

int LLVMFuzzerInitialize(int* argc, char*** argv) {
    fuzz_initialize(argc, argv, "gifsicle-fuzz-main");
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    run_spec spec;
    int ok = decode(data, size, &spec) == 0;
    if (ok)
        fuzz_run(&spec, fuzz_hash(data, size), 0);
    spec_free(&spec);
    return ok ? 0 : -1;         /* -1: not worth adding to the corpus */
}
