/* libppm.h - internal header file for libppm portable pixmap library
*/

#ifndef _LIBPPM_H_
#define _LIBPPM_H_

/* Here are some routines internal to the ppm library. */

void ppm_readppminitrest ARGS(( FILE* file, int* colsP, int* rowsP, pixval* maxvalP ));

#endif /*_LIBPPM_H_*/
