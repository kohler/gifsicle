#!/bin/sh
# build.sh - Build gifsicle's fuzzing harness into test/fuzz/build.
# This file is part of gifsicle's fuzzing harness. See README.md.
#
# Builds:
#   build/gifsicle
#       gifsicle with AddressSanitizer and UndefinedBehaviorSanitizer, for
#       reproducing failures.
#   build/fuzz-gifsicle-main-run, build/fuzz-gifsicle-run
#       The in-process fuzz targets (fuzz_main.c, fuzz_gifsicle.c) with a
#       standalone driver (any clang).
#   build/fuzz-gifsicle-main, build/fuzz-gifsicle
#       The same targets with libFuzzer, if a clang with libFuzzer is
#       available. Set FUZZ_CC to choose one; otherwise Homebrew's LLVM and
#       `clang` are tried.
#
# Environment: CC (default Homebrew's LLVM clang if installed, else clang),
# FUZZ_CC, EXTRA_CFLAGS, and FUZZ_BUILD (the build directory, default
# test/fuzz/build). For example, to keep fuzzing
# past a known float-to-int conversion bug:
#   FUZZ_BUILD=/tmp/fb EXTRA_CFLAGS=-fno-sanitize=float-cast-overflow ./build.sh
# Requires a configured source tree (run `./configure` at the top first).

set -e
FUZZ=$(cd "$(dirname "$0")" && pwd)
TOP=$(cd "$FUZZ/../.." && pwd)
SRC=$TOP/src
BUILD=${FUZZ_BUILD:-$FUZZ/build}

if [ ! -f "$TOP/config.h" ]; then
    echo "build.sh: $TOP/config.h is missing." 1>&2
    echo "  Configure gifsicle first: (cd $TOP && autoreconf -i && ./configure)" 1>&2
    exit 1
fi

# Prefer LLVM's clang: Apple's clang lacks LeakSanitizer (and libFuzzer).
if [ -z "${CC:-}" ]; then
    CC=clang
    for c in /opt/homebrew/opt/llvm/bin/clang /usr/local/opt/llvm/bin/clang; do
        if [ -x "$c" ]; then
            CC=$c
            break
        fi
    done
fi
SANFLAGS="-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer"
CFLAGS="-g -O1 $SANFLAGS ${EXTRA_CFLAGS:-}"
CPPFLAGS="-DHAVE_CONFIG_H -I$TOP -I$TOP/include -I$SRC"
LIBS="-lm -lpthread"

# gifsicle sources linked into the in-process targets (with gifsicle.c,
# whose main() inprocess.h renames), and the harness sources.
INPROC_SRCS="clp fmalloc giffunc gifread gifunopt gifwrite kcolor merge optimize quantize support xform"
HARNESS_SRCS="$FUZZ/runner.c $FUZZ/gifcheck.c $FUZZ/fuzz_gifsicle.c $FUZZ/fuzz_main.c"

mkdir -p "$BUILD"

# compile OBJDIR COMPILER EXTRA-FLAGS SOURCE... : compile into OBJDIR
compile () {
    objdir=$1 compiler=$2 extra=$3
    shift 3
    mkdir -p "$objdir"
    for s in "$@"; do
        case $s in
        */*) src=$s; base=$(basename "$s" .c) ;;
        *) src=$SRC/$s.c; base=$s ;;
        esac
        $compiler $CFLAGS $CPPFLAGS $extra -c "$src" -o "$objdir/$base.o"
    done
}

objs () {
    objdir=$1
    shift
    for s in "$@"; do
        printf '%s/%s.o ' "$objdir" "$(basename "$s" .c)"
    done
}

echo "== sanitized gifsicle ($CC)"
GIFSICLE_SRCS="clp fmalloc giffunc gifread gifunopt kcolor merge optimize quantize support xform gifsicle gifwrite"
compile "$BUILD/obj-cli" "$CC" "" $GIFSICLE_SRCS
$CC $CFLAGS -o "$BUILD/gifsicle" $(objs "$BUILD/obj-cli" $GIFSICLE_SRCS) $LIBS

echo "== in-process targets with standalone driver ($CC)"
compile "$BUILD/obj-run" "$CC" "-include $FUZZ/inprocess.h" $INPROC_SRCS gifsicle
compile "$BUILD/obj-run" "$CC" "" $HARNESS_SRCS "$FUZZ/standalone.c"
for t in fuzz_gifsicle:fuzz-gifsicle-run fuzz_main:fuzz-gifsicle-main-run; do
    $CC $CFLAGS -o "$BUILD/${t#*:}" \
        $(objs "$BUILD/obj-run" $INPROC_SRCS gifsicle runner gifcheck ${t%%:*} standalone) $LIBS
done

# Find a compiler with libFuzzer.
probe=$BUILD/probe.c
echo 'int LLVMFuzzerTestOneInput(const unsigned char* d, unsigned long n) { (void) d; (void) n; return 0; }' > "$probe"
FUZZ_CC_FOUND=
for c in ${FUZZ_CC:-} /opt/homebrew/opt/llvm/bin/clang /usr/local/opt/llvm/bin/clang "$CC" clang; do
    if command -v "$c" >/dev/null 2>&1 \
       && "$c" -fsanitize=fuzzer,address "$probe" -o "$BUILD/probe" >/dev/null 2>&1; then
        FUZZ_CC_FOUND=$c
        break
    fi
done
rm -f "$probe" "$BUILD/probe"

if [ -n "$FUZZ_CC_FOUND" ]; then
    echo "== in-process targets with libFuzzer ($FUZZ_CC_FOUND)"
    compile "$BUILD/obj-libfuzzer" "$FUZZ_CC_FOUND" "-fsanitize=fuzzer-no-link -include $FUZZ/inprocess.h" $INPROC_SRCS gifsicle
    compile "$BUILD/obj-libfuzzer" "$FUZZ_CC_FOUND" "-fsanitize=fuzzer-no-link" $HARNESS_SRCS
    for t in fuzz_gifsicle:fuzz-gifsicle fuzz_main:fuzz-gifsicle-main; do
        $FUZZ_CC_FOUND $CFLAGS -fsanitize=fuzzer -o "$BUILD/${t#*:}" \
            $(objs "$BUILD/obj-libfuzzer" $INPROC_SRCS gifsicle runner gifcheck ${t%%:*}) $LIBS
    done
else
    echo "== no compiler with libFuzzer found; skipping the libFuzzer targets"
    echo "   (install LLVM, e.g. \`brew install llvm\`, or set FUZZ_CC)"
fi

echo "== done; see $FUZZ/README.md"
