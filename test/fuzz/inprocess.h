/* inprocess.h - Force-included (with -include) into gifsicle sources when
   they are compiled for the in-process fuzz targets. This file is part of
   gifsicle's fuzzing harness.

   gifsicle calls exit() when it runs out of memory, which would end an
   in-process fuzzing run; uses the standard streams, which an in-process
   target must capture; and runs shell commands for --transform-colormap,
   which a fuzzer must never do. Redirect all of these without modifying
   gifsicle's sources. Each target defines the replacements. The system
   headers are included first so that their include guards keep them from
   redefining the streams. */

#ifndef GIFSICLE_FUZZ_INPROCESS_H
#define GIFSICLE_FUZZ_INPROCESS_H
#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef __cplusplus
extern "C" {
#endif

extern FILE* gifsicle_fuzz_stdin;
extern FILE* gifsicle_fuzz_stdout;
extern FILE* gifsicle_fuzz_stderr;
void gifsicle_fuzz_exit(int status)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((noreturn))
#endif
    ;
FILE* gifsicle_fuzz_popen(const char* command, const char* mode);

#undef stdin
#define stdin gifsicle_fuzz_stdin
#undef stdout
#define stdout gifsicle_fuzz_stdout
#undef stderr
#define stderr gifsicle_fuzz_stderr
#define exit gifsicle_fuzz_exit
#define popen gifsicle_fuzz_popen
#define main gifsicle_cli_main      /* gifsicle.c's; the fuzzer has its own */

#ifdef __cplusplus
}
#endif
#endif
