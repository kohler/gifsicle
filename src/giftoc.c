#include <stdio.h>
#include <ctype.h>
#include <string.h>


void
print_gif(FILE *f, char *gifrecname)
{
  unsigned long size = 0;
  int c;
  
  printf("static unsigned char %s_data[] = {", gifrecname);
  size = 0;
  c = getc(f);
  while (c != EOF) {
    if (size % 20 == 0) printf("\n");
    printf("%d,", c);
    size++;
    c = getc(f);
  }
  printf("};\nstatic Gif_Record %s_rec = { %s_data, %lu };\n\n",
	 gifrecname, gifrecname, size);
}


int
main(int argc, char **argv)
{
  char buf[256];
  
  argc--, argv++;
  
  if (argc < 1) {
    fprintf(stderr, "usage: giftoc giffile...\n");
    exit(1);
  }
  
  for (; argc > 0; argc--, argv++) {
    FILE *f = fopen(argv[0], "rb");
    
    if (f) {
      char buf[1024];
      char *s;
      
      char *in_name = strrchr(argv[0], '/');
      if (in_name) in_name++;
      else in_name = argv[0];
      
      if (strlen(in_name) > 1023) {
	fprintf(stderr, "giftoc: file name too long\n");
	
      } else {
	strcpy(buf, in_name);
	for (s = buf; *s; s++)
	  if (!isalnum(*s))
	    *s = '_';
	
	print_gif(f, buf);
      }
      
      fclose(f);
      
    } else
      fprintf(stderr, "couldn't open %s for reading\n", argv[0]);
  }
  
  exit(0);
}
