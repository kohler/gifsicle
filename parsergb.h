#ifndef BYTE
#define BYTE 1
typedef unsigned char byte;
#endif
char *LookupNameOfColor(int r, int g, int b, int tol);
char LookupColorName(char *n, int *r, int *g, int *b);
int ParseColorFile(char *fname);
