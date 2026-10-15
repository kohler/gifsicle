#!/usr/bin/env python3
"""make_corpus.py - Build seed corpora for the in-process fuzz targets.

This file is part of gifsicle's fuzzing harness. See README.md.

For fuzz-gifsicle (the default), each corpus file is a 16-byte
configuration (see fuzz_gifsicle.c) followed by a GIF. Seeds come from the
GIFs embedded in test/*.testie, from gifgen.random_gif(), and from any
extra GIF files or directories given on the command line. Each GIF is
paired with an all-zero configuration (read and write only) and several
random ones.

With --main, build a corpus for fuzz-gifsicle-main instead (see
fuzz_main.c): command lines with input files, taken from the gifsicle
commands in test/*.testie and from random_options() below. This
also writes a libFuzzer dictionary of gifsicle's options to
build/main.dict.

usage: make_corpus.py [--main] [-o OUTDIR] [-n NGENERATED] [-k NCONFIGS]
                      [GIF-OR-DIR...]
"""

import argparse
import base64
import hashlib
import os
import random
import re
import shlex
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import gifgen  # noqa: E402

CONFIG_SIZE = 16


def testie_gifs(testdir):
    """Yield (name, bytes) for each GIF embedded in a testie file."""
    for fn in sorted(os.listdir(testdir)):
        if fn.endswith(".testie"):
            text = open(os.path.join(testdir, fn), encoding="utf-8", errors="replace").read()
            yield from testie_gifs_in(fn, text)


def testie_commands(testdir):
    """Yield (testie, argv, files) for each gifsicle command in a testie
    script, where files maps each embedded GIF's name to its contents."""
    for fn in sorted(os.listdir(testdir)):
        if not fn.endswith(".testie"):
            continue
        text = open(os.path.join(testdir, fn), encoding="utf-8", errors="replace").read()
        files = {name.split(":", 1)[1]: data for name, data in testie_gifs_in(fn, text)}
        script = re.search(r"^%script\n(.*?)(?=^%)", text, re.M | re.S)
        for line in (script.group(1) if script else "").split("\n"):
            # first command of a pipeline or list; `$i` loops use -O2
            line = re.split(r"\||&&|;", line.replace("$i", "2"))[0].strip()
            if not line.startswith("gifsicle"):
                continue
            try:
                words = shlex.split(line)[1:]
            except ValueError:
                continue
            argv, skip = [], False
            for w in words:
                if skip:
                    skip = False
                elif w in (">", "<", "2>"):
                    skip = True     # drop redirections and their files
                elif re.match(r"^\d?[<>]", w):
                    pass
                else:
                    argv.append(w)
            yield fn, argv, files


def testie_gifs_in(fn, text):
    """Yield (name, bytes) for each GIF embedded in testie `fn`'s `text`."""
    lines = text.split("\n")
    i = 0
    while i < len(lines):
        parts = lines[i].split()
        if len(parts) >= 3 and parts[0] == "%file" and "-e" in parts[1:-1]:
            i += 1
            b64 = []
            while i < len(lines) and lines[i].strip() and not lines[i].startswith("%"):
                b64.append(lines[i].strip())
                i += 1
            try:
                data = base64.b64decode("".join(b64))
            except ValueError:
                continue
            if data[:3] == b"GIF":
                yield "%s:%s" % (fn, parts[-1]), data
        else:
            i += 1


MAIN_SEP = b"\n#FILE#\n"
MAIN_FILES = ["in0.gif", "in1.gif", "palette.gif"]


def main_input(argv, files):
    """Encode a fuzz-gifsicle-main input; files maps MAIN_FILES names to
    contents."""
    data = " ".join(argv).encode()
    present = [f for f in MAIN_FILES if f in files]
    for name in MAIN_FILES[:MAIN_FILES.index(present[-1]) + 1] if present else []:
        data += MAIN_SEP + files.get(name, b"")
    return data


# Random gifsicle options, for --main seeds

def color(rng):
    return rng.choice(["#%06x" % rng.randrange(1 << 24),
                       "%d,%d,%d" % (rng.randrange(256), rng.randrange(256), rng.randrange(256)),
                       str(rng.randrange(258)), "black", "white", "red"])


def dims(rng, lo=0, hi=200):
    def d():
        return "_" if rng.random() < .15 else str(rng.randint(lo, hi))
    return "%sx%s" % (d(), d())


def frame_sel(rng):
    n = rng.randint(-3, 6)
    return rng.choice(["#%d" % n, "#%d-" % n, "#%d-%d" % (n, n + rng.randint(-1, 3)),
                       "#0", "#-1", "#-2-", "#a", "#b", "#frame1", "#1-#a"])


DITHERS = ["floyd-steinberg", "fs", "atkinson", "ro64", "o3", "o4", "o8", "ordered",
           "halftone", "squarehalftone", "diagonal", "none", "posterize", "default"]


def dither(rng):
    """A dither name, sometimes with parameters."""
    name = rng.choice(DITHERS)
    if rng.random() < .3:
        name = rng.choice(["halftone", "sqhalftone", "trihalftone", "o", "ro64", "o8", "fs"])
        name += "," + ",".join(str(rng.choice([0, 1, 2, 3, 4, 8, 16, 64, 147, 148, 255, 256]))
                               for _ in range(rng.randint(1, 3)))
    return name


# Application extension names. Gifsicle reads NETSCAPE2.0 and ANIMEXTS1.0
# extensions as loop counts, so fuzz_main.c skips commands that name them.
APP_NAMES = ["FUZZAPP0", "FUZZAPP01.0", "MGKIPTC0000", "MGK8BIM0000", "ICCRGBG1012",
             "NETSCAPE", "NETSCAPE2.1", "X", "12345678901", "123456789012"]


def app_extension(rng):
    """An --app-extension option with a name and body."""
    body = rng.choice(["data", "x", str(rng.randrange(10)),
                       "A" * rng.choice([254, 255, 256])])
    return [rng.choice(["--app-extension", "-x", "--app-ext"]), rng.choice(APP_NAMES), body]


SAME_OPTIONS = ["--same-app-extensions", "--same-background", "--same-clip",
                "--same-comments", "--same-crop", "--same-extensions", "--same-interlace",
                "--same-logical-screen", "--same-loopcount", "--same-disposal",
                "--same-delay", "--same-names", "--same-position", "--same-transparent"]


def random_options(rng, ninputs):
    """Return (pre, per_input, post) option lists."""
    pre, post = [], []

    def maybe(p, *opts):
        if rng.random() < p:
            pre.extend(opts)

    maybe(.03, rng.choice(["-I", "--cinfo", "--xinfo", "--sinfo"]))
    maybe(.15, "-O%d" % rng.randint(1, 3))
    maybe(.10, "--lossy=%d" % rng.choice([1, 20, 80, 200, 400]))
    maybe(.10, "-U")
    maybe(.08, "--careful")
    maybe(.08, "--conserve-memory")
    maybe(.08, "-j%d" % rng.randint(1, 2))
    maybe(.08, "--no-ignore-errors")
    maybe(.06, "--loopcount=%s" % rng.choice(["0", "1", "forever", "65535"]))
    maybe(.03, "--no-loopcount")
    maybe(.05, "--change-color", color(rng), color(rng))
    maybe(.05, "--background", color(rng))
    maybe(.12, "--colors=%d" % rng.choice([2, 3, 16, 64, 200, 256]))
    maybe(.08, "--color-method=%s" % rng.choice(["diversity", "blend-diversity", "median-cut"]))
    maybe(.10, "--dither=%s" % dither(rng))
    maybe(.02, "--dither")
    maybe(.04, "--gray")
    maybe(.04, rng.choice(["--optimize=keep-empty", "-O2", "--optimize=no-keep-empty",
                           "--optimize=drop-empty", "-O3"]))
    maybe(.03, "--lossy")
    maybe(.04, "--max-screen=%s" % dims(rng, 0, 80))
    maybe(.03, "--no-max-screen")
    maybe(.02, "-V")
    maybe(.03, "--ignore-errors")
    maybe(.03, "--threads=%d" % rng.randint(0, 4))
    maybe(.05, "--gamma=%s" % rng.choice(["1", "2.2", "srgb", "oklab", "0.5"]))
    if rng.random() < .10:
        pre.append(rng.choice(["--use-colormap", "--use-exact-colormap"]))
        pre.append(rng.choice(["web", "gray", "bw", "palette.gif"]))
    if rng.random() < .15:
        kind = rng.randrange(7)
        if kind == 0:
            pre.append("--resize=" + dims(rng, 0, 120))
        elif kind == 1:
            pre.append("--scale=%s" % rng.choice(["0.1", "0.5", "1", "1.5", "2", "0.5x2", "3x0.25"]))
        elif kind == 2:
            pre.append("--resize-%s=%d" % (rng.choice(["width", "height"]), rng.randint(0, 120)))
        elif kind == 3:
            pre.append("--resize-%s=%s" % (rng.choice(["fit", "touch"]), dims(rng, 0, 120)))
        elif kind == 4:
            pre.append("--resize-%s-%s=%d" % (rng.choice(["fit", "touch"]),
                                              rng.choice(["width", "height"]),
                                              rng.randint(0, 120)))
        else:
            pre.append("--resize-geometry=%s%s" % (dims(rng, 1, 120),
                                                   rng.choice(["", ">", "<", "^", "!"])))
        maybe(.6, "--resize-method=%s" % rng.choice(
            ["sample", "mix", "box", "catrom", "mitchell", "lanczos2", "lanczos3"]))
        maybe(.3, "--resize-colors=%d" % rng.choice([0, 2, 64, 256]))

    per_input = []
    for _ in range(ninputs):
        opts = []

        def imaybe(p, *o):
            if rng.random() < p:
                opts.extend(o)

        imaybe(.12, "--crop", "%d,%d%s" % (
            rng.randint(0, 40), rng.randint(0, 40),
            rng.choice(["+" + dims(rng, 0, 50),
                        "-%d,%d" % (rng.randint(0, 60), rng.randint(0, 60))])))
        imaybe(.08, "--crop-transparency")
        imaybe(.08, "--flip-horizontal")
        imaybe(.08, "--flip-vertical")
        imaybe(.10, rng.choice(["--rotate-90", "--rotate-180", "--rotate-270"]))
        imaybe(.06, rng.choice(["--interlace", "--no-interlace"]))
        imaybe(.06, "--transparent", color(rng))
        imaybe(.03, "--no-transparent")
        imaybe(.06, "--logical-screen", dims(rng, 0, 150))
        imaybe(.06, "--position", "%d,%d" % (rng.randint(0, 100), rng.randint(0, 100)))
        imaybe(.06, "--delay", str(rng.choice([0, 1, 10, 65535])))
        imaybe(.06, "--disposal", rng.choice(["none", "asis", "background", "previous", "7"]))
        imaybe(.04, "--comment", "fuzz")
        imaybe(.04, "--name", "fuzzname")
        imaybe(.03, "--extension", "254", "data")
        imaybe(.04, *app_extension(rng))
        imaybe(.04, rng.choice(["--no-comments", "--no-names", "--no-extensions",
                                "--no-app-extensions"]))
        imaybe(.03, rng.choice(["--no-flip", "--no-rotate", "--no-crop", "--no-position",
                                "--no-logical-screen", "--no-delay", "--no-disposal"]))
        imaybe(.03, "--clip", "%d,%d-%d,%d" % (rng.randint(0, 20), rng.randint(0, 20),
                                               rng.randint(0, 60), rng.randint(0, 60)))
        imaybe(.04, "--unoptimize")
        imaybe(.03, "--no-loopcount")
        imaybe(.04, rng.choice(SAME_OPTIONS))
        per_input.append(opts)

    if ninputs == 1 and rng.random() < .15:
        # frame changes apply to the most recent input
        change = rng.randrange(5)
        if change == 0:
            post += ["--delete", frame_sel(rng)]
        elif change == 1:
            post += ["--append", "palette.gif"]
        elif change == 2:
            post += ["--insert-before", frame_sel(rng), "palette.gif"]
        elif change == 3:
            post += ["--replace", frame_sel(rng), "palette.gif"]
        else:
            post += ["--delete", frame_sel(rng), frame_sel(rng), "--done",
                     "--append", "palette.gif"]
    return pre, per_input, post


def main_corpus(args, rng, gifs):
    """Return a list of fuzz-gifsicle-main inputs."""
    out = []
    # testie commands, with their GIFs renamed to MAIN_FILES
    for _, argv, files in testie_commands(os.path.join(HERE, "..")):
        rename, mfiles = {}, {}
        for w in argv:
            if w in files and w not in rename and len(rename) < len(MAIN_FILES):
                rename[w] = MAIN_FILES[len(rename)]
                mfiles[rename[w]] = files[w]
        argv = ["out.gif" if w == "/dev/null" else rename.get(w, w) for w in argv]
        if not any("/" in w for w in argv):
            out.append(main_input(argv, mfiles))
    # random options
    for _ in range(args.generated):
        ninputs = 1 if rng.random() < .8 else 2
        pre, per_input, post = random_options(rng, ninputs)
        argv = ["--no-warnings"] if rng.random() < .5 else []
        argv += pre
        files = {}
        multifile = rng.random() < .06
        if multifile:
            argv.append(rng.choice(["--multifile", "--nextfile"]))
        elif ninputs > 1 and rng.random() < .3:
            argv.append("--merge")
        for i, opts in enumerate(per_input):
            argv += opts + ["in%d.gif" % i]
            if rng.random() < .15:
                argv += [frame_sel(rng) for _ in range(rng.randint(1, 3))]
            if multifile and i == 0:
                files["in0.gif"] = gifgen.concatenated_gif(rng)
                if "--nextfile" in argv:
                    argv += ["in0.gif"] * rng.randint(0, 2)
            else:
                files["in%d.gif" % i] = rng.choice(gifs)[1]
        argv += post
        if any("palette.gif" in w for w in argv):
            files["palette.gif"] = rng.choice(gifs)[1]
        mode = rng.random()
        if mode < .08:
            argv = ["--explode"] + argv
        elif mode < .1:
            argv = ["--explode-by-name"] + argv
        elif mode < .2:
            argv = ["--batch"] + argv
        else:
            argv += ["-o", "out.gif"]
        out.append(main_input(argv, files))
    return out


def write_main_dict(path):
    """Write a libFuzzer dictionary of gifsicle's options and values."""
    src = open(os.path.join(HERE, "..", "..", "src", "gifsicle.c"), encoding="utf-8").read()
    words = set()
    for m in re.finditer(r'^\s*\{ *"([a-z0-9-]+)", *(?:\'(.)\'|0)', src, re.M):
        words.add("--" + m.group(1))
        if m.group(2):
            words.add("-" + m.group(2))
    words.update(MAIN_FILES + ["out.gif", "-o", "-", "#0", "#1", "#-1", "#0-", "#0-2",
                               "#name", "web", "gray", "bw", "none", "forever",
                               "background", "previous", "asis", "#FF0000", "255,0,0",
                               "transparent", "0", "1", "2", "255", "256", "65535",
                               "10x10", "_x10", "0,0+5x5", "1.5", "srgb", "oklab",
                               "floyd-steinberg", "ro64", "o8", "halftone", "box",
                               "mix", "catrom", "lanczos3", "mitchell", "sample",
                               "median-cut", "blend-diversity", "keep-empty",
                               # large and overflowing numbers
                               "-1", "65536", "2147483647", "2147483648", "-2147483648",
                               "4294967295", "4294967296", "99999999999999999999",
                               "1e9", "1e308", "0x7fffffff", "65535x65535", "65536x1",
                               "100000x100000", "0.0001", "inf", "nan",
                               "^", "<", ">", "!", "%",
                               "drop-empty", "no-keep-empty", "atkinson", "posterize",
                               "sqhalftone", "halftone,10,3", "o,8,4", "#a", "#-2-",
                               "1,1-5,5", "0x0"])
    with open(path, "w") as f:
        f.write("# gifsicle options and values, for fuzz-gifsicle-main\n")
        f.write('sep="\\x0A#FILE#\\x0A"\n')
        for w in sorted(words):
            f.write('"%s"\n' % w.replace("\\", "\\\\").replace('"', '\\"'))
    return len(words) + 1


def extra_gifs(paths):
    for p in paths:
        names = [os.path.join(p, f) for f in sorted(os.listdir(p))] if os.path.isdir(p) else [p]
        for n in names:
            if os.path.isfile(n):
                data = open(n, "rb").read()
                if data[:3] == b"GIF":
                    yield n, data


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--main", action="store_true",
                    help="build the corpus for fuzz-gifsicle-main")
    ap.add_argument("-o", "--outdir")
    ap.add_argument("-n", "--generated", type=int, default=200,
                    help="number of generated GIFs (default 200)")
    ap.add_argument("-k", "--configs", type=int, default=4,
                    help="random configurations per GIF (default 4)")
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--max-size", type=int, default=65536,
                    help="skip GIFs larger than this (default 65536)")
    ap.add_argument("paths", nargs="*", help="extra GIF files or directories")
    args = ap.parse_args()

    rng = random.Random(args.seed)
    if not args.outdir:
        args.outdir = os.path.join(HERE, "build", "corpus-main" if args.main else "corpus")
    os.makedirs(args.outdir, exist_ok=True)
    gifs = list(testie_gifs(os.path.join(HERE, "..")))
    gifs += list(extra_gifs(args.paths))
    gifs += [("generated", gifgen.random_gif(random.Random(args.seed * 100003 + i)))
             for i in range(args.generated)]
    gifs += [("special", gifgen.special_gif(random.Random(args.seed * 100019 + i)))
             for i in range(args.generated // 2)]
    gifs = [g for g in gifs if len(g[1]) <= args.max_size]

    if args.main:
        inputs = main_corpus(args, rng, gifs)
        for blob in inputs:
            with open(os.path.join(args.outdir, hashlib.sha1(blob).hexdigest()), "wb") as f:
                f.write(blob)
        dict_path = os.path.join(HERE, "build", "main.dict")
        nwords = write_main_dict(dict_path)
        print("make_corpus.py: wrote %d inputs to %s, %d words to %s"
              % (len(inputs), args.outdir, nwords, dict_path))
        return

    nwritten = 0
    for name, data in gifs:
        if len(data) > args.max_size:
            continue
        configs = [bytes(CONFIG_SIZE)]
        configs += [bytes(rng.randrange(256) for _ in range(CONFIG_SIZE))
                    for _ in range(args.configs)]
        for cfg in configs:
            blob = cfg + data
            h = hashlib.sha1(blob).hexdigest()
            with open(os.path.join(args.outdir, h), "wb") as f:
                f.write(blob)
            nwritten += 1
    print("make_corpus.py: wrote %d inputs to %s" % (nwritten, args.outdir))


if __name__ == "__main__":
    main()
