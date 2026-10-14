/* standalone.c - Run gifsicle's fuzz target without libFuzzer.
   This file is part of gifsicle's fuzzing harness. See README.md.

   Usage:
     fuzz-gifsicle-run FILE-OR-DIR...
       Run each input once. Use this to reproduce crashes and to check that a
       corpus still runs cleanly.
     fuzz-gifsicle-run -runs=N [-seed=S] [-max_len=L] [-timeout=T]
                       [-artifact_prefix=DIR/] CORPUS-FILE-OR-DIR...
       Run N randomly mutated corpus inputs. This is a simple fallback for
       compilers without libFuzzer; it is not coverage-guided. An input that
       crashes, fails a check, or times out is saved under the artifact
       prefix. Flags mimic libFuzzer's. */

#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#if defined(__has_include)
# if __has_include(<sanitizer/common_interface_defs.h>)
#  include <sanitizer/common_interface_defs.h>
#  define HAVE_SANITIZER_INTERFACE 1
# endif
#endif

#ifdef __cplusplus
extern "C" {
#endif
int LLVMFuzzerInitialize(int* argc, char*** argv);
int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);
#ifdef __cplusplus
}
#endif

typedef struct {
    uint8_t* data;
    size_t len;
    char* name;
} blob;

static blob* corpus;
static size_t ncorpus, corpuscap;

/* The input being run, saved if the run dies. */
static const uint8_t* cur_data;
static size_t cur_len;
static const char* artifact_prefix = "./";


/* Save the current input as ARTIFACT_PREFIX + KIND + "-" + hash. Called from
   signal handlers, so uses only async-signal-safe functions. */
static void save_current(const char* kind) {
    char path[4096];
    size_t n = 0, i;
    uint64_t h = 14695981039346656037ULL;
    int fd;
    if (!cur_data)
        return;
    for (i = 0; i < cur_len; ++i)
        h = (h ^ cur_data[i]) * 1099511628211ULL;
    for (i = 0; artifact_prefix[i] && n < sizeof(path) - 64; ++i)
        path[n++] = artifact_prefix[i];
    for (i = 0; kind[i]; ++i)
        path[n++] = kind[i];
    path[n++] = '-';
    for (i = 0; i < 16; ++i)
        path[n++] = "0123456789abcdef"[(h >> (60 - 4 * i)) & 15];
    path[n] = 0;
    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        ssize_t w = write(fd, cur_data, cur_len);
        (void) w;
        close(fd);
        w = write(2, "fuzz-gifsicle-run: saved input to ", 34);
        w = write(2, path, n);
        w = write(2, "\n", 1);
    }
}

static void on_sanitizer_death(void) {
    save_current("crash");
    cur_data = NULL;
}

static void on_signal(int sig) {
    save_current(sig == SIGALRM ? "timeout" : "crash");
    cur_data = NULL;
    signal(sig, SIG_DFL);
    raise(sig);
}


static void add_blob(const char* name) {
    FILE* f = fopen(name, "rb");
    blob b;
    size_t cap = 4096;
    if (!f) {
        perror(name);
        exit(1);
    }
    b.data = (uint8_t*) malloc(cap);
    b.len = 0;
    for (;;) {
        size_t r = fread(b.data + b.len, 1, cap - b.len, f);
        b.len += r;
        if (b.len < cap)
            break;
        cap *= 2;
        b.data = (uint8_t*) realloc(b.data, cap);
    }
    fclose(f);
    b.name = strdup(name);
    if (ncorpus == corpuscap) {
        corpuscap = corpuscap ? 2 * corpuscap : 64;
        corpus = (blob*) realloc(corpus, sizeof(blob) * corpuscap);
    }
    corpus[ncorpus++] = b;
}

static void add_path(const char* path) {
    struct stat st;
    DIR* d;
    struct dirent* de;
    if (stat(path, &st) != 0) {
        perror(path);
        exit(1);
    }
    if (!S_ISDIR(st.st_mode)) {
        add_blob(path);
        return;
    }
    if (!(d = opendir(path))) {
        perror(path);
        exit(1);
    }
    while ((de = readdir(d))) {
        char sub[4096];
        struct stat sst;
        if (de->d_name[0] == '.')
            continue;
        snprintf(sub, sizeof(sub), "%s/%s", path, de->d_name);
        if (stat(sub, &sst) == 0 && S_ISREG(sst.st_mode))
            add_blob(sub);
    }
    closedir(d);
}


/* Mutation (xorshift64* random numbers) */
static uint64_t rng_state = 88172645463325252ULL;

static uint64_t rnd(void) {
    rng_state ^= rng_state >> 12;
    rng_state ^= rng_state << 25;
    rng_state ^= rng_state >> 27;
    return rng_state * 2685821657736338717ULL;
}

static size_t rndn(size_t n) {
    return n ? (size_t) (rnd() % n) : 0;
}

static size_t mutate(uint8_t* buf, size_t len, size_t max_len) {
    static const uint16_t interesting16[] = {
        0, 1, 2, 3, 4, 7, 8, 15, 16, 255, 256, 4095, 4096, 32767, 32768,
        65534, 65535
    };
    static const uint8_t interesting8[] = {0, 1, 2, 7, 8, 11, 12, 0x7f, 0x80, 0xff};
    int nmut = 1 + (int) rndn(6), m;
    for (m = 0; m < nmut; ++m) {
        size_t pos = rndn(len), n;
        switch (rndn(9)) {
        case 0:                 /* flip a bit */
            if (len)
                buf[pos] ^= (uint8_t) (1 << rndn(8));
            break;
        case 1:                 /* interesting byte */
            if (len)
                buf[pos] = interesting8[rndn(sizeof(interesting8))];
            break;
        case 2:                 /* interesting 16-bit little-endian field */
            if (len >= 2) {
                uint16_t v = interesting16[rndn(sizeof(interesting16) / 2)];
                pos = rndn(len - 1);
                buf[pos] = (uint8_t) v;
                buf[pos + 1] = (uint8_t) (v >> 8);
            }
            break;
        case 3:                 /* random byte */
            if (len)
                buf[pos] = (uint8_t) rnd();
            break;
        case 4:                 /* configuration byte */
            if (len >= 16)
                buf[rndn(16)] = (uint8_t) rnd();
            break;
        case 5:                 /* delete a range */
            n = 1 + rndn(len - pos < 64 ? len - pos : 64);
            if (pos + n <= len) {
                memmove(buf + pos, buf + pos + n, len - pos - n);
                len -= n;
            }
            break;
        case 6:                 /* duplicate a range */
            n = 1 + rndn(64);
            if (pos + n <= len && len + n <= max_len) {
                memmove(buf + pos + n, buf + pos, len - pos);
                len += n;
            }
            break;
        case 7:                 /* splice from another corpus input */
            if (ncorpus) {
                const blob* o = &corpus[rndn(ncorpus)];
                size_t opos = rndn(o->len);
                n = rndn(o->len - opos + 1);
                if (pos + n > max_len)
                    n = max_len - pos;
                memcpy(buf + pos, o->data + opos, n);
                if (pos + n > len)
                    len = pos + n;
            }
            break;
        case 8:                 /* truncate */
            if (len > 16)
                len = 16 + rndn(len - 16);
            break;
        }
    }
    return len;
}


int main(int argc, char** argv) {
    long runs = -1, i;
    size_t max_len = 65536;
    unsigned timeout = 10;
    int a;
    time_t start;

    LLVMFuzzerInitialize(&argc, &argv);
    for (a = 1; a < argc; ++a) {
        if (strncmp(argv[a], "-runs=", 6) == 0)
            runs = strtol(argv[a] + 6, 0, 10);
        else if (strncmp(argv[a], "-seed=", 6) == 0)
            rng_state = strtoull(argv[a] + 6, 0, 10) * 2 + 1;
        else if (strncmp(argv[a], "-max_len=", 9) == 0)
            max_len = strtoul(argv[a] + 9, 0, 10);
        else if (strncmp(argv[a], "-timeout=", 9) == 0)
            timeout = (unsigned) strtoul(argv[a] + 9, 0, 10);
        else if (strncmp(argv[a], "-artifact_prefix=", 17) == 0)
            artifact_prefix = argv[a] + 17;
        else if (argv[a][0] == '-') {
            fprintf(stderr, "fuzz-gifsicle-run: unknown option %s\n", argv[a]);
            return 1;
        } else
            add_path(argv[a]);
    }
    if (ncorpus == 0) {
        fprintf(stderr, "usage: %s [-runs=N] FILE-OR-DIR...\n", argv[0]);
        return 1;
    }

#ifdef HAVE_SANITIZER_INTERFACE
    __sanitizer_set_death_callback(on_sanitizer_death);
#endif
    signal(SIGABRT, on_signal);
    signal(SIGSEGV, on_signal);
    signal(SIGBUS, on_signal);
    signal(SIGALRM, on_signal);

    if (runs < 0) {
        /* replay */
        for (i = 0; i < (long) ncorpus; ++i) {
            fprintf(stderr, "running %s\n", corpus[i].name);
            cur_data = corpus[i].data;
            cur_len = corpus[i].len;
            alarm(timeout);
            LLVMFuzzerTestOneInput(cur_data, cur_len);
            alarm(0);
        }
        cur_data = NULL;
        fprintf(stderr, "fuzz-gifsicle-run: %lu inputs ran cleanly\n",
                (unsigned long) ncorpus);
        return 0;
    }

    /* mutation */
    {
        uint8_t* buf = (uint8_t*) malloc(max_len);
        start = time(0);
        for (i = 0; i < runs; ++i) {
            const blob* b = &corpus[rndn(ncorpus)];
            size_t len = b->len < max_len ? b->len : max_len;
            memcpy(buf, b->data, len);
            len = mutate(buf, len, max_len);
            cur_data = buf;
            cur_len = len;
            alarm(timeout);
            LLVMFuzzerTestOneInput(buf, len);
            alarm(0);
            if ((i + 1) % 10000 == 0)
                fprintf(stderr, "fuzz-gifsicle-run: %ld runs, %ld s\n",
                        i + 1, (long) (time(0) - start));
        }
        cur_data = NULL;
        free(buf);
        fprintf(stderr, "fuzz-gifsicle-run: %ld mutated runs ran cleanly\n", runs);
    }
    return 0;
}
