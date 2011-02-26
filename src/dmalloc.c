#include <stdio.h>
#include <stdlib.h>

typedef struct bucket {
  size_t size;
  const char *file;
  int line;
  struct bucket *next;
} bucket;

#define NBUCK 5779
static bucket *buckets[NBUCK];
static int event_number;
static FILE *verbose_out = 0;

size_t dmalloc_live_memory;


void *
debug_malloc_id(size_t k, const char *file, int line)
{
  void *p = malloc(k + sizeof(bucket));
  bucket *b = (bucket *)p;
  int bnum = (((unsigned long)p) >> 4) % NBUCK;

  if (p == 0) {
    fprintf(stderr, "dmalloc:%s:%d: virtual memory exhausted (wanted %ld)\n",
	    file, line, (long) k);
    abort();
  }

  b->size = k;
  b->next = buckets[bnum];
  b->file = file;
  b->line = line;
  buckets[bnum] = b;
  dmalloc_live_memory += k;
  p = (void *)(((char *)p) + sizeof(bucket));
  /* memset(p, 99, b->size); */
  if (verbose_out)
    fprintf(verbose_out, "%5d: %p +%-7d (%s:%d) ++  %ld\n", event_number,
	    p, b->size, file, line, (long) dmalloc_live_memory);
  event_number++;
  return p;
}

void *
debug_realloc_id(void *p, size_t k, const char *file, int line)
{
  bucket *b_in = (bucket *)(((char *)p) - sizeof(bucket));
  bucket *b;
  bucket *prev;
  bucket *new_b;
  int bnum = (((unsigned long)b_in) >> 4) % NBUCK;
  if (p == 0) return debug_malloc_id(k, file, line);

  for (b = buckets[bnum], prev = 0; b && b != b_in; prev = b, b = b->next)
    ;
  if (b == 0) {
    fprintf(stderr, "debug_realloc given bad pointer %p\n", p);
    abort();
  }

  dmalloc_live_memory += k - b->size;
  if (verbose_out)
    fprintf(verbose_out, "%5d: %p +%-7ld (%s:%d) >> ", event_number,
	    p, (long) b->size, b->file, b->line);

  new_b = (bucket *)realloc(b, k + sizeof(bucket));
  if (new_b == 0) {
    fprintf(stderr, "dmalloc:%s:%d: virtual memory exhausted (wanted %ld)\n",
	    file, line, (long) k);
    abort();
  }

  new_b->size = k;
  if (new_b != b) {
    if (prev) prev->next = new_b->next;
    else buckets[bnum] = new_b->next;

    bnum = (((unsigned long)new_b) >> 4) % NBUCK;
    new_b->next = buckets[bnum];
    buckets[bnum] = new_b;

    p = (void *)(((char *)new_b) + sizeof(bucket));
  }

  if (verbose_out)
    fprintf(verbose_out, "%p +%-7ld (%s:%d)\n", p, (long) k, file, line);
  event_number++;
  return p;
}


void
debug_free_id(void *p, const char *file, int line)
{
  bucket *b_in = (bucket *)(((char *)p) - sizeof(bucket));
  bucket *b;
  bucket *prev;
  int chain_length = 0;
  int bnum = (((unsigned long)b_in) >> 4) % NBUCK;
  if (p == 0) return;

  for (b = buckets[bnum], prev = 0; b && b != b_in; prev = b, b = b->next)
    chain_length++;
  if (b == 0) {
    fprintf(stderr, "my_free given bad pointer %p\n", p);
    abort();
  }

  dmalloc_live_memory -= b->size;
  if (prev) prev->next = b->next;
  else buckets[bnum] = b->next;
  /* memset(p, 97, b->size); */
  if (verbose_out)
    fprintf(verbose_out, "%5d: %p +%-7ld (%s:%d) -- %s:%d  %ld\n", event_number,
	    p, (long) b->size, b->file, b->line, file, line,
	    (long) dmalloc_live_memory);
  event_number++;
  free(b_in);
}


#undef debug_malloc
#undef debug_realloc
#undef debug_free

void *
debug_malloc(size_t k)
{
  return debug_malloc_id(k, "<UNKNOWN>", 0);
}

void *
debug_realloc(void *p, size_t k)
{
  return debug_realloc_id(p, k, "<UNKNOWN>", 0);
}

void
debug_free(void *p)
{
  debug_free_id(p, "<UNKNOWN>", 0);
}


void
dmalloc_info(void *p)
{
  bucket *b_in = (bucket *)(((char *)p) - sizeof(bucket));
  bucket *b;
  int bnum = (((unsigned long)b_in) >> 4) % NBUCK;
  if (p == 0)
    fprintf(stderr, "dmalloc: 0x0\n");
  else {
    for (b = buckets[bnum]; b && b != b_in; b = b->next)
      ;
    if (b == 0) {
      fprintf(stderr, "dmalloc: %p: not my pointer\n", p);
    } else {
      fprintf(stderr, "dmalloc: %p +%-7ld (%s:%d)\n",
	      p, (long) b->size, b->file, b->line);
    }
  }
}


void
dmalloc_report(void)
{
  int i;
  bucket *b;
  fprintf(stderr, "dmalloc: %d bytes allocated\n", dmalloc_live_memory);
  for (i = 0; i < NBUCK; i++)
    for (b = buckets[i]; b; b = b->next)
      fprintf(stderr, "dmalloc: %p +%-7ld (%s:%d)\n",
	      (void *)(((char *)b) + sizeof(bucket)), (long) b->size,
	      b->file, b->line);
}


void
dmalloc_verbose(const char *out_name)
{
  if (out_name)
    verbose_out = fopen(out_name, "w");
  else
    verbose_out = stdout;
}
