#!/bin/bash -eu
# oss-fuzz.sh - Build gifsicle's fuzz targets for OSS-Fuzz.
# This file is part of gifsicle's fuzzing harness. See README.md.
#
# OSS-Fuzz's projects/gifsicle/build.sh runs this script in its build
# container. It uses OSS-Fuzz's compilers and flags ($CC, $CFLAGS, $CXX,
# $CXXFLAGS, $LIB_FUZZING_ENGINE) and writes the targets, their seed
# corpora, and a dictionary to $OUT. To try it outside OSS-Fuzz, set those
# variables, for instance:
#
#   CC=clang CXX=clang++ CFLAGS="-g -O1 -fsanitize=address,fuzzer-no-link" \
#   CXXFLAGS="$CFLAGS" LIB_FUZZING_ENGINE=-fsanitize=fuzzer \
#   OUT=/tmp/out WORK=/tmp/work test/fuzz/oss-fuzz.sh

FUZZ=$(cd "$(dirname "$0")" && pwd)
TOP=$(cd "$FUZZ/../.." && pwd)
SRC_DIR=$TOP/src
WORK=${WORK:-$(mktemp -d)}
mkdir -p "$OUT" "$WORK"
cd "$TOP"

# Only config.h is needed, so configure without OSS-Fuzz's sanitizer flags.
if [ ! -f config.h ]; then
    autoreconf -i
    env CFLAGS= CXXFLAGS= ./configure --disable-gifview --disable-gifdiff
fi

CPPFLAGS="-DHAVE_CONFIG_H -I$TOP -I$TOP/include -I$SRC_DIR"
INPROC_SRCS="clp fmalloc giffunc gifread gifunopt gifwrite kcolor merge optimize quantize support xform"
OBJ=$WORK/gifsicle-fuzz-obj
mkdir -p "$OBJ"

# gifsicle's sources, adapted for in-process fuzzing by inprocess.h
for s in $INPROC_SRCS; do
    $CC $CFLAGS $CPPFLAGS -include "$FUZZ/inprocess.h" -c "$SRC_DIR/$s.c" -o "$OBJ/$s.o"
done
$CC $CFLAGS $CPPFLAGS -include "$FUZZ/inprocess.h" -DGIFSICLE_FUZZ_HOOK_CALLS \
    -c "$SRC_DIR/gifsicle.c" -o "$OBJ/gifsicle.o"

# the harness
for s in runner budget gifcheck fuzz_main fuzz_gifsicle; do
    $CC $CFLAGS $CPPFLAGS -c "$FUZZ/$s.c" -o "$OBJ/$s.o"
done

COMMON_OBJS=""
for s in $INPROC_SRCS gifsicle runner budget gifcheck; do
    COMMON_OBJS="$COMMON_OBJS $OBJ/$s.o"
done
$CXX $CXXFLAGS -o "$OUT/fuzz-gifsicle-main" $COMMON_OBJS "$OBJ/fuzz_main.o" \
    $LIB_FUZZING_ENGINE -lm -lpthread
$CXX $CXXFLAGS -o "$OUT/fuzz-gifsicle" $COMMON_OBJS "$OBJ/fuzz_gifsicle.o" \
    $LIB_FUZZING_ENGINE -lm -lpthread

# Seed corpora and the command-line dictionary (see make_corpus.py)
rm -rf "$WORK/corpus-main" "$WORK/corpus"
python3 "$FUZZ/make_corpus.py" --main -o "$WORK/corpus-main" --dict "$OUT/fuzz-gifsicle-main.dict"
python3 "$FUZZ/make_corpus.py" -o "$WORK/corpus"
rm -f "$OUT/fuzz-gifsicle-main_seed_corpus.zip" "$OUT/fuzz-gifsicle_seed_corpus.zip"
(cd "$WORK/corpus-main" && zip -q -r "$OUT/fuzz-gifsicle-main_seed_corpus.zip" .)
(cd "$WORK/corpus" && zip -q -r "$OUT/fuzz-gifsicle_seed_corpus.zip" .)
