# Fuzzing gifsicle

This directory holds a fuzzing harness for gifsicle. It looks for memory
errors, undefined behavior, leaks, and wrong output, using two
coverage-guided, in-process [libFuzzer] targets:

* **`fuzz-gifsicle-main`** runs free-form gifsicle command lines
  (`fuzz_main.c`). Each input is a command line plus up to three input
  files. This covers everything gifsicle does, from option parsing, frame
  selection, and frame editing through batch and explode modes and output.
* **`fuzz-gifsicle`** runs `gifsicle OPTIONS in.gif -o out.gif`, where a
  16-byte configuration decodes to the options (`fuzz_gifsicle.c`). Every
  input is a meaningful command, so it reaches deep combinations of
  processing options quickly.

Both pass their commands to `gifsicle_main()`, in-process, and check the
results the same way (`runner.c`). Both are built with AddressSanitizer and
UndefinedBehaviorSanitizer.

[libFuzzer]: https://llvm.org/docs/LibFuzzer.html

## Building

Configure gifsicle first (`autoreconf -i && ./configure` at the top level),
then:

    test/fuzz/build.sh

This builds into `test/fuzz/build/`:

| Binary | What it is |
|---|---|
| `gifsicle` | Sanitized gifsicle, for reproducing failures |
| `fuzz-gifsicle-main-run`, `fuzz-gifsicle-run` | The in-process targets with a standalone driver; works with any clang |
| `fuzz-gifsicle-main`, `fuzz-gifsicle` | The in-process targets with libFuzzer, if a clang with libFuzzer is found |

Apple's clang lacks libFuzzer and LeakSanitizer; `brew install llvm`
provides a clang with both, which `build.sh` uses automatically (or set
`CC` and `FUZZ_CC`). Set `FUZZ_BUILD=DIR` to
build elsewhere, for instance to keep several versions around, and
`EXTRA_CFLAGS` to add flags.

Then make seed corpora:

    test/fuzz/make_corpus.py --main
    test/fuzz/make_corpus.py

The first writes `build/corpus-main/` for `fuzz-gifsicle-main`: the
gifsicle commands from `test/*.testie` with their input files, and 200
command lines with random options. It also writes
`build/main.dict`, a libFuzzer dictionary of gifsicle's options. The second
writes `build/corpus/` for `fuzz-gifsicle`, pairing the GIFs embedded in
`test/*.testie` and 200 generated GIFs (`gifgen.py`) with random option
configurations. The generated GIFs include 100 aimed at specific edge
cases, such as transparent indexes past the end of the colormap, duplicate
colors, and named frames. Pass more GIF files or directories as arguments
to add seeds; `-n` changes the number of generated GIFs and `--seed` the
random seed.

## Running

**Mind memory.** gifsicle can legitimately allocate a lot of memory for
large GIFs, and parallel fuzzers multiply that. The commands below cap each
process; keep the number of parallel jobs low on a laptop.

    export ASAN_OPTIONS=hard_rss_limit_mb=1024:quarantine_size_mb=64:detect_leaks=1
    export LSAN_OPTIONS=suppressions=$PWD/test/fuzz/lsan.supp
    export UBSAN_OPTIONS=print_stacktrace=1

gifsicle should never leak memory, so leak detection stays on. Use
`detect_leaks=0` if the binaries were built with Apple's clang, which
doesn't support it. `lsan.supp` suppresses leaks in libFuzzer itself. A
smaller quarantine keeps ASan's memory use well under the RSS limit.

Command lines can legitimately ask for enormous images (`--resize
60000x60000`). libFuzzer's `-malloc_limit_mb`, below, reports such
allocations as out-of-memory, which `-ignore_ooms` skips; so don't also set
ASan's `max_allocation_size_mb`, which would report them as crashes. With
the standalone driver, which has no such limit, add
`max_allocation_size_mb=512` to `ASAN_OPTIONS`.

Command-line fuzzing with libFuzzer, collecting failures and continuing
after each one:

    cd test/fuzz
    mkdir -p findings build/corpus-main-new
    build/fuzz-gifsicle-main -fork=2 -ignore_crashes=1 -ignore_ooms=1 -ignore_timeouts=1 \
        -dict=build/main.dict -rss_limit_mb=1024 -malloc_limit_mb=512 \
        -max_len=65536 -timeout=30 -max_total_time=3600 -artifact_prefix=findings/ \
        build/corpus-main-new build/corpus-main

libFuzzer adds interesting new inputs to the first corpus directory. The
structured target runs the same way:

    build/fuzz-gifsicle -fork=2 -ignore_crashes=1 -ignore_ooms=1 -ignore_timeouts=1 \
        -rss_limit_mb=1024 -malloc_limit_mb=512 -max_len=65536 -timeout=30 \
        -max_total_time=3600 -artifact_prefix=findings/ build/corpus

Drop `-fork` and the `-ignore` flags to stop at the first failure. Without
libFuzzer, `build/fuzz-gifsicle-main-run -runs=N -artifact_prefix=findings/
build/corpus-main` (or `fuzz-gifsicle-run` with `build/corpus`) runs a
simple (not coverage-guided) mutation fuzzer.

## Reproducing a failure

Run a saved input through the matching standalone driver. Setting
`GIFSICLE_FUZZ_REPRO=DIR` writes the input files and a `cmd.sh` that runs
the same command with the `gifsicle` binary; `GIFSICLE_FUZZ_VERBOSE=1`
prints each command and gifsicle's messages:

    GIFSICLE_FUZZ_REPRO=/tmp/repro build/fuzz-gifsicle-main-run findings/crash-...
    cd /tmp/repro && PATH=$OLDPWD/build:$PATH ./cmd.sh

If the failure report shows a "checking command", the failure is in a
second run on the first command's output: copy that GIF output to
`check.gif`, then run the checking command.

Both drivers write artifacts to the current directory by default, so pass
`-artifact_prefix=` to put them elsewhere. A directory argument replays
every input in it: `build/fuzz-gifsicle-run build/corpus` should run
cleanly.

## What the targets check

Each input to `fuzz-gifsicle-main` is a command line, then up to three
files separated by `\n#FILE#\n`, named `in0.gif`, `in1.gif`, and
`palette.gif`. The target skips inputs whose arguments contain `/`, so
gifsicle touches only its scratch directory. It also skips inputs whose
arguments contain `NETSCAPE2.0` or `ANIMEXTS1.0`: gifsicle writes any
`--app-extension` as given, but reads extensions with those names as loop
counts, so such commands can legitimately change the output's loop count.

Each input to `fuzz-gifsicle` is a 16-byte configuration followed by a
GIF. `decode_options` in `fuzz_gifsicle.c` maps the configuration to
command-line options: crop, flip, rotate, position, interlace, disposal,
delay, `-U`, `-O`, `--lossy`, `--careful`, `--conserve-memory`, resizing,
colormaps and dithering, gamma, logical screen, and frame selection.

Both targets skip inputs whose GIFs exceed a resource budget (64 frames,
2^20 pixels per frame or screen, 2^22 pixels in total), since huge valid
GIFs aren't bugs. `runner.c` writes the input files into a private scratch
directory and calls `gifsicle_main()` there. `inprocess.h` gives gifsicle
an empty stdin, captures its stdout and stderr, and stops
`--transform-colormap` from running commands. Besides sanitizer errors, a
run fails if:

* `gifsicle_main` returns a status other than 0 or 1;
* gifsicle writes a GIF (to a file or stdout) that `gifcheck.c`, an
  independent strict decoder, rejects; that gifsicle's own reader decodes
  differently; or that gifsicle can't reread without errors or warnings;
* adding `--careful` or `--conserve-memory` to the command (unless it uses
  `--lossy`) changes its status or how its output GIFs look;
* the command resizes, and running it again with `-j2` (threads) changes
  its status or outputs;
* gifsicle fails on one of its own output GIFs, given 1 to 3
  appearance-preserving options chosen from the input (`-O1`–`-O3`, `-U`,
  `--careful`, `--conserve-memory`, interlacing, `--no-comments`,
  `--no-names`), or produces a GIF that looks different. The target
  composites both GIFs itself, as browsers do (the background is
  transparent), and compares the displayed frames, their delays, the
  screen size, and the loop count, merging identical consecutive frames
  as `gifdiff -w` does.

`fuzz-gifsicle` also checks that if every pixel of its input is
transparent or refers to a color in its colormap, every pixel of the
output is too.

With `GIFSICLE_FUZZ_TWICE=1`, both targets also run each command twice and
fail if the runs differ in any way, which catches state that
`gifsicle_reset()` missed. A failure report shows the command, any checking
command, and gifsicle's messages.

## Files

| File | Purpose |
|---|---|
| `build.sh` | Builds everything into `build/` |
| `lsan.supp` | LeakSanitizer suppressions (for libFuzzer's own leaks) |
| `fuzz_main.c` | The free-form target, `fuzz-gifsicle-main` |
| `fuzz_gifsicle.c` | The structured target, `fuzz-gifsicle` |
| `runner.c`, `runner.h` | Runs commands in-process for both targets, and checks them |
| `inprocess.h` | Included into gifsicle's sources for the in-process targets: redirects `exit`, `popen`, and the standard streams |
| `standalone.c` | Driver for the targets without libFuzzer |
| `gifcheck.c`, `gifcheck.h` | Independent strict GIF decoder |
| `gifgen.py` | Random GIF generator, mutator, and strict checker (Python) |
| `make_corpus.py` | Builds the seed corpora and the dictionary |
