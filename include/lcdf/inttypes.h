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
#endif

#ifdef HAVE_FAKE_INT_TYPES
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef signed short int16_t;
typedef unsigned short uint16_t;
typedef signed int int32_t;
typedef unsigned int uint32_t;
#endif

#ifndef HAVE_UINTPTR_T
# if SIZEOF_VOID_P == SIZEOF_UNSIGNED_INT
typedef unsigned int uintptr_t;
# elif SIZEOF_VOID_P == SIZEOF_UNSIGNED_LONG
typedef unsigned long uintptr_t;
# endif
#endif

/* Note: Windows compilers call these types '[un]signed __int8', etc. */
#endif
