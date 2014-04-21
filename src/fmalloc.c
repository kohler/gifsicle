#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <lcdfgif/gif.h>

#ifdef __cplusplus
extern "C" {
#endif
extern const char* program_name;

void* Gif_Realloc(void* p, size_t s, size_t n, const char* file, int line) {
    (void) file, (void) line;
    if (s == 0 || n == 0) {
        Gif_Free(p);
        return (void*) 0;
    } else if (s == 1 || n == 1 || s <= ((size_t) -1) / n) {
        p = realloc(p, s * n);
        if (!p) {
            fprintf(stderr, "%s: Out of memory, giving up\n", program_name);
            exit(1);
        }
        return p;
    } else {
        fprintf(stderr, "%s: Out of memory, giving up (huge allocation)\n", program_name);
        exit(1);
        return (void*) 0;
    }
}

#undef Gif_Free
void Gif_Free(void* p) {
    free(p);
}

#ifdef __cplusplus
}
#endif
