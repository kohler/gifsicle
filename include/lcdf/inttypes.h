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

#endif
