#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#ifndef PATHNAME_SEPARATOR
# define PATHNAME_SEPARATOR '/'
#endif
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>

int is_static = 1;
int is_const = 1;

static void *
fmalloc(int size)
{
  void *p = malloc(size);
  if (!p) {
    fputs("giftoc: Out of memory!\n", stderr);
    exit(1);
  }
  return p;
}

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
  
  printf("\nstatic %sunsigned char %s_data[] = {",
	 (is_const ? "const " : ""), gifrecname);
  size = 0;
  c = getc(f);
  while (c != EOF) {
    if (size % 20 == 0) printf("\n");
    printf("%d,", c);
    size++;
    c = getc(f);
  }
  printf("};\n%s%sGif_Record %s = { %s_data, %lu };\n",
	 (is_static ? "static " : ""),
	 (is_const ? "const " : ""),
	 gifrecname, gifrecname, size);
}

int
main(int argc, char *argv[])
{
  int reckless = 0;
  int make_name = 0;
  const char *directory = "";
  
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
    else if (!strcmp(argv[0], "-nonconst"))
      is_const = 0, argc--, argv++;
    else if (!strcmp(argv[0], "-const"))
      is_const = 1, argc--, argv++;
    else if (!strcmp(argv[0], "-dir") && argc > 1) {
      directory = argv[1], argc -= 2, argv += 2;
      /* make sure directory is slash-terminated */
      if (directory[ strlen(directory) - 1 ] != PATHNAME_SEPARATOR
	  && directory[0]) {
	char *ndirectory = (char *)fmalloc(strlen(directory) + 2);
	sprintf(ndirectory, "%s%c", directory, PATHNAME_SEPARATOR);
	directory = ndirectory;
      }
    } else
      break;
  }
  
  if ((!make_name && argc % 2 != 0)
      || argc < 1
      || (argv[0] && argv[0][0] == '-')) {
    fprintf(stderr, "\
usage: giftoc [OPTIONS] FILE NAME [FILE NAME...]\n\
or:    giftoc -makename [OPTIONS] FILE [FILE...]\n\
       OPTIONS are -reckless, -extern, -nonconst, -dir DIR\n");
    exit(1);
  }
  
  if (!is_static)
    printf("#include \"config.h\"\n#include <lcdfgif/gif.h>\n\n");
  
  for (; argc > 0; argc--, argv++) {
    FILE *f;
    char *rec_name = 0;
    char *file_name = (char *)fmalloc(strlen(argv[0]) + strlen(directory) + 1);
    
    strcpy(file_name, directory);
    strcat(file_name, argv[0]);
    f = fopen(file_name, "rb");
    
    if (!f) {
      fprintf(stderr, "%s: %s\n", file_name, strerror(errno));
      goto done;
    }
    
    if (make_name) {
      char *sin, *sout;
      sin = strrchr(file_name, PATHNAME_SEPARATOR) + 1;
      if (!sin) sin = file_name;
      sout = rec_name = (char *)fmalloc(strlen(sin) + 2);
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
    
   done:
    if (make_name)
      free(rec_name);
    free(file_name);
    if (f)
	fclose(f);
  }
  
  exit(0);

}
