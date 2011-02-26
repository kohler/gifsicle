#ifndef DMALLOC_H
#define DMALLOC_H
#include <stddef.h>

void *debug_malloc_id(size_t, const char *, int);
void *debug_realloc_id(void *, size_t, const char *, int);
void debug_free_id(void *, const char *, int);
void *debug_malloc(size_t);
void *debug_realloc(void *, size_t);
void debug_free(void *);
void dmalloc_info(void *);
void dmalloc_report(void);
void dmalloc_verbose(const char *);
extern size_t dmalloc_live_memory;

#define xmalloc(s)		debug_malloc_id((s),__FILE__,__LINE__)
#define xrealloc(p,s)		debug_realloc_id((p),(s),__FILE__,__LINE__)
#define xfree(p)		debug_free_id((p),__FILE__,__LINE__)
#define debug_malloc(s)		debug_malloc_id((s),__FILE__,__LINE__)
#define debug_realloc(p,s)	debug_realloc_id((p),(s),__FILE__,__LINE__)
#define debug_free(p)		debug_free_id((p),__FILE__,__LINE__)

#endif
