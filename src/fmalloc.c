#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdlib.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

static void
fail_die_malloc_die(size_t size, const char *file, int line)
{
  fprintf(stderr, "Out of memory! (wanted %lu at %s:%d)\n",
	  (unsigned long)size, file, line);
  exit(1);
}

void *
fail_die_malloc(size_t size, const char *file, int line)
{
  void *p = malloc(size);
  if (!p && size)
    fail_die_malloc_die(size, file, line);
  return p;
}

void *
fail_die_realloc(void *p, size_t size, const char *file, int line)
{
  if (!p)
    return fail_die_malloc(size, file, line);
  p = realloc(p, size);
  if (!p && size)
    fail_die_malloc_die(size, file, line);
  return p;
}

#ifdef __cplusplus
}
#endif
