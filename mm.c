/* mm.c - Simple version of the MM implementation.
   Copyright (C) 1997 Eddie Kohler, eddietwo@lcs.mit.edu
   This file is part of MM, a memory management package.

   MM is free software; you can copy, distribute, or alter it at will, as long
   as this notice is kept intact and this source code is made available.
   Hypo(pa)thetical commerical developers are asked to write the author a note,
   which might make his day. There is no warranty, express or implied. */

#include "mm.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#ifdef __cplusplus
extern "C" {
#endif


static void
default_full_handler(size_t s, const char *file, int line)
{
  fprintf(stderr, "Out of memory allocating %ld at %s, line %d!\n",
	  (long)s, file, line);
  abort();
}


static void
default_error_handler(void *p, const char *file, int line)
{
  fprintf(stderr, "Memory error about %p at %s, line %d!\n", p, file, line);
  abort();
}


static void (*full_handler)(size_t, const char *, int)
     = default_full_handler;
static void (*error_handler)(void *, const char *, int)
     = default_error_handler;


void
Mm_SetFullHandler(void (*f)(size_t, const char *, int))
{
  full_handler = f ? f : default_full_handler;
}


void
Mm_SetErrorHandler(void (*f)(void *, const char *, int))
{
  error_handler = f ? f : default_error_handler;
}


void *
Mm_Malloc(size_t s, const char *file, int line)
{
  void *p = malloc(s);
  if (!p && s != 0) {
    (*full_handler)(s, file, line);
    assert(!"The full handler can't return");
  }
  return p;
}


void *
Mm_Realloc(void *p, size_t s, const char *file, int line)
{
  if (!p)
    p = malloc(s);
  else
    p = realloc(p, s);
  if (!p && s != 0) {
    (*full_handler)(s, file, line);
    assert(!"The full handler can't return");
  }
  return p;
}


void
Mm_Free(void *p, const char *file, int line)
{
  if (p) free(p);
}


#ifdef __cplusplus
}
#endif
