#include "gif.h"
extern void fatal_error(char *, ...);

/* be careful about memory allocation */
#undef malloc
#undef realloc
#ifdef __cplusplus
extern "C" {
#endif

void *
fail_die_malloc(int size, const char *file, int line)
{
  void *p = malloc(size);
  if (!p && size)
    fatal_error("out of memory (wanted %d at %s:%d)", size, file, line);
  return p;
}

void *
fail_die_realloc(void *p, int size, const char *file, int line)
{
  if (!p)
    return fail_die_malloc(size, file, line);
  p = realloc(p, size);
  if (!p && size)
    fatal_error("out of memory (wanted %d at %s:%d)", size, file, line);
  return p;
}

#ifdef __cplusplus
}
#endif
