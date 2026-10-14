/* runner.h - Run gifsicle command lines in-process and check the results.
   This file is part of gifsicle's fuzzing harness. See README.md.

   Both fuzz targets build a gifsicle command line and input files (a
   run_spec), then call fuzz_run(), which runs gifsicle_main() in a private
   scratch directory and checks everything it does. Gifsicle's sources must
   be compiled with inprocess.h, whose hooks this file defines. */

#ifndef GIFSICLE_FUZZ_RUNNER_H
#define GIFSICLE_FUZZ_RUNNER_H
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif

#define RUN_MAX_ARGS    64
#define RUN_MAX_FILES   4

/* A gifsicle command and its input files. Owns its strings and data. */
typedef struct {
    int argc;
    char* argv[RUN_MAX_ARGS + 1];
    int nfiles;
    char* name[RUN_MAX_FILES];
    uint8_t* data[RUN_MAX_FILES];
    size_t len[RUN_MAX_FILES];
} run_spec;

void spec_init(run_spec* spec);         /* sets argv[0] to "gifsicle" */
void spec_arg(run_spec* spec, const char* format, ...);
void spec_file(run_spec* spec, const char* name, const void* data, size_t len);
void spec_free(run_spec* spec);

typedef struct {
    char* name;
    char* data;
    size_t len;
} dir_entry;

/* The results of a run. */
typedef struct {
    int status;                 /* -1 if gifsicle called exit() */
    char* out;                  /* captured stdout */
    size_t out_len;
    char* err;                  /* captured stderr */
    size_t err_len;
    dir_entry* files;           /* the scratch directory afterwards, by name */
    int nfiles;
} snapshot;

/* An additional check for a target's runs. */
typedef void (*fuzz_check)(const run_spec* spec, const snapshot* snap);

/* Call from LLVMFuzzerInitialize. `name` prefixes messages. */
void fuzz_initialize(int* argc, char*** argv, const char* name);

/* Run `spec` and check it; `seed` chooses among optional checks. Calls
   `extra`, if nonnull, on the run's results. */
void fuzz_run(const run_spec* spec, uint64_t seed, fuzz_check extra);

/* Report a failed check and abort. */
void fuzz_failed(const char* format, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((noreturn, format(printf, 1, 2)))
#endif
    ;

/* Return 1 if the GIFs in `data` are small enough to be worth running. */
int fuzz_within_budget(const uint8_t* data, size_t len);
/* Set *w and *h to the screen size gifsicle would compute for the first GIF
   in `data`. Returns 0 if `data` isn't a GIF. */
int fuzz_screen_size(const uint8_t* data, size_t len,
                     unsigned long* w, unsigned long* h);
int fuzz_is_gif(const char* data, size_t len);
/* Return 1 if `e` is an input file that the run left unchanged. */
int fuzz_unchanged_input(const run_spec* spec, const dir_entry* e);
uint64_t fuzz_hash(const uint8_t* data, size_t len);

#ifdef __cplusplus
}
#endif
#endif
