#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>


int is_static = 1;

void
print_reckless(FILE *f, char *gifrecname)
{
  unsigned long size = 0;
  int c;
  int lasthex = 0;
  
  printf("\n%sGifRecord %s = { (unsigned char *)\"",
	 is_static ? "static " : "", gifrecname);
  size = 0;
  c = getc(f);
  while (c != EOF) {
    
    if (size % 60 == 0) printf("\"\n  \"");
    
    switch (c) {
      
     case '\\':
      fputs("\\\\", stdout);
      lasthex = 0;
      break;
      
     case '\"':
      fputs("\\\"", stdout);
      lasthex = 0;
      break;
      
     case '\b':
      fputs("\\b", stdout);
      lasthex = 0;
      break;
      
     case '\r':
      fputs("\\r", stdout);
      lasthex = 0;
      break;
      
     case '\n':
      fputs("\\n", stdout);
      lasthex = 0;
      break;
      
     case '\f':
      fputs("\\f", stdout);
      lasthex = 0;
      break;
      
     case '\t':
      fputs("\\t", stdout);
      lasthex = 0;
      break;
      
     case '0': case '1': case '2': case '3': case '4': case '5': case '6':
     case '7':
      if (lasthex)
	printf("\\%o", c);
      else
	putchar(c);
      break;
      
     default:
      if (isprint(c)) {
	putchar(c);
	lasthex = 0;
      } else {
	printf("\\%o", c);
	lasthex = 1;
      }
      break;
      
    }
    
    size++;
    c = getc(f);
  }
  
  printf("\",\n  %lu\n};\n", size); 
}


void
print_unreckless(FILE *f, char *gifrecname)
{
  unsigned long size = 0;
  int c;
  
  printf("\nstatic unsigned char %s_data[] = {", gifrecname);
  size = 0;
  c = getc(f);
  while (c != EOF) {
    if (size % 20 == 0) printf("\n");
    printf("%d,", c);
    size++;
    c = getc(f);
  }
  printf("};\n%sGif_Record %s = { %s_data, %lu };\n",
	 is_static ? "static " : "", gifrecname, gifrecname, size);
}


int
main(int argc, char **argv)
{
  int reckless = 0;
  int make_name = 0;
  
  argc--, argv++;
  
  while (argv[0] && argv[0][0] == '-') {
    if (!strcmp(argv[0], "-reckless"))
      reckless = 1, argc--, argv++;
    else if (!strcmp(argv[0], "-static"))
      is_static = 1, argc--, argv++;
    else if (!strcmp(argv[0], "-extern"))
      is_static = 0, argc--, argv++;
    else if (!strcmp(argv[0], "-makename"))
      make_name = 1, argc--, argv++;
    else
      break;
  }
  
  if ((!make_name && argc % 2 != 0)
      || argc < 1
      || (argv[0] && argv[0][0] == '-')) {
    fprintf(stderr, "\
usage: giftoc [-reckless] [-extern] giffile gifname [giffile gifname...]\n\
or:    giftoc -makename [-reckless] [-extern] giffile [giffile...]\n");
    exit(1);
  }
  
  if (!is_static)
    printf("#include \"gif.h\"\n\n");
  
  for (; argc > 0; argc--, argv++) {
    char *rec_name;
    FILE *f = fopen(argv[0], "rb");
    
    if (!f) {
      fprintf(stderr, "couldn't open %s for reading\n", argv[0]);
      continue;
    }
    
    if (make_name) {
      char *sin, *sout;
      sin = strrchr(argv[0], '/') + 1;
      if (!sin) sin = argv[0];
      sout = rec_name = (char *)malloc(strlen(sin) + 2);
      if (isdigit(*sin)) *sout++ = 'N';
      for (; *sin; sin++, sout++)
	if (isalnum(*sin))
	  *sout = *sin;
	else
	  *sout = '_';
      *sout = 0;
      
    } else {
      argv++, argc--;
      rec_name = argv[0];
    }
    
    if (reckless) print_reckless(f, rec_name);
    else print_unreckless(f, rec_name);
    
    if (make_name)
      free(rec_name);
    fclose(f);
  }
  
  exit(0);
}
