#ifndef MM_H
#define MM_H
#ifdef __cplusplus
extern "C" {
#endif

/* mm.h - Simple version of the MM interface.
   Copyright (C) 1997 Eddie Kohler, eddietwo@lcs.mit.edu
   This file is part of MM, a memory management package.

   MM is free software; you can copy, distribute, or alter it at will, as long
   as this notice is kept intact and this source code is made available.
   Hypo(pa)thetical commerical developers are asked to write the author a note,
   which might make his day. There is no warranty, express or implied. */


#include <stddef.h>
/* Simple memory management routines. */

#define Mm_FINE			__FILE__, __LINE__
#define Mm_New(t)		((t *)Mm_Malloc(sizeof(t), Mm_FINE))
#define Mm_NewArray(t, n)	((t *)Mm_Malloc(sizeof(t) * (n), Mm_FINE))
#define Mm_ReArray(v, t, n) 	(v = (t*)Mm_Realloc(v, sizeof(t)*(n), Mm_FINE))
#define Mm_Delete(v)		Mm_Free(v, Mm_FINE)
#define Mm_DeleteArray(v)	Mm_Free(v, Mm_FINE)

#define MM_TRY			do {
#define MM_FAIL			if (0)
#define MM_ENDTRY		} while (0)
#define MM_RETURN		return


void	Mm_SetFullHandler(void (*f)(size_t, const char *, int));
void	Mm_SetErrorHandler(void (*f)(void *, const char *, int));

void *	Mm_Malloc(size_t, const char *, int);
void *	Mm_Realloc(void *, size_t, const char *, int);
void	Mm_Free(void *, const char *, int);

#ifdef __cplusplus
}
#endif
#endif
