#ifndef LCDF_INTTYPES_H
#define LCDF_INTTYPES_H
/* Define known-width integer types. */

#ifdef HAVE_INTTYPES_H
# include <inttypes.h>
#elif defined(HAVE_SYS_TYPES_H)
# include <sys/types.h>
# ifdef HAVE_U_INT_TYPES
typedef u_int8_t uint8_t;
typedef u_int16_t uint16_t;
typedef u_int32_t uint32_t;
# endif
#elif defined(_WIN32)
typedef __int8 int8_t;
typedef unsigned __int8 uint8_t;
typedef __int16 int16_t;
typedef unsigned __int16 uint16_t;
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
#endif

#ifndef HAVE_UINTPTR_T
# if SIZEOF_VOID_P == SIZEOF_UNSIGNED_LONG
typedef unsigned long uintptr_t;
# elif SIZEOF_VOID_P == SIZEOF_UNSIGNED_INT
typedef unsigned int uintptr_t;
# endif
#endif

#endif
