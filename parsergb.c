#include "parsergb.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

typedef struct colorstruct {
  char *name;
  unsigned char r, g, b;
  struct colorstruct *next;
} color;

static color *h[256];


char *
LookupNameOfColor(int r, int g, int b, int tole)
{
  static char buf[20];
  int dist = 10000, dwist;
  color *t = h[(r & 0xE) | ((g & 0xE) >> 3) | ((b & 0xC) >> 6)], *too = NULL;
  int tol = tole == -2 ? 1 : tole;
  while (t) {
    if (abs(t->r - r) <= tol && abs(t->g - g) <= tol && abs(t->b - b) <= tol) {
      dwist = abs(t->r - r) + abs(t->g - g) + abs(t->b - b);
      if (dwist < dist) {
	dist = dwist;
	too = t;
      }
    }
    t = t->next;
  }
  if (!too && tole != -2) {
    sprintf(buf, "#%02X%02X%02X", r, g, b);
    return buf;
  } else if (!too) return NULL;
  else return too->name;
}


char
LookupColorName(char *n, int *r, int *g, int *b)
{
  char *s = strchr(n, '\n');
  if (s) *s = 0;
  *r = *g = *b = 0;
  if (*n == '#') {
    int i = (strlen(n) - 1) / 3, R, G, B;
    char thing[12];
    sprintf(thing, "%%%dx%%%dx%%%dx", i, i, i);
    sscanf(n+1, thing, &R, &G, &B);
    *r = R;
    *g = G;
    *b = B;
    return 1;
  } else {
    int i = 256;
    color **t = h, *q;
    while (i--)
      if ((q = *t++))
	while (q) {
	  if (!strcmp(n, q->name)) {
	    *r = q->r;
	    *g = q->g;
	    *b = q->b;
	    return 1;
	  }
	  q = q->next;
	}
  }
  return 0;
}

void
Adjoin(char *name, int r, int g, int b)
{
  color *q = (color *)malloc(sizeof(color));
  int val = (r & 0xE) | ((g & 0xE) >> 3) | ((b & 0xC) >> 6);
  q->next = h[val];
  h[val] = q;
  q->r = r;
  q->g = g;
  q->b = b;
  q->name = (char *)malloc(strlen(name)+1);
  strcpy(q->name, name);
}

int
ParseColorFile(char *fname)
{
  int i, r, g, b;
  FILE *f;
  char buf[BUFSIZ], eeny, *x;
  for (i = 0; i < 256; i++) h[i] = NULL;
  f = fopen(fname ? fname : "/usr/lib/X11/rgb.txt", "r");
  if (!f) return 0;
  while (!feof(f)) {
    fscanf(f, "%d %d %d ", &r, &g, &b);
    while (isspace(eeny = fgetc(f)));
    ungetc(eeny, f);
    fgets(buf, BUFSIZ, f);
    if ((x = strchr(buf, '\n'))) *x = 0;
    Adjoin(buf, r, g, b);
  }
  Adjoin("None", 0, 0, 0);
  return 1;
}
