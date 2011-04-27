/* $Header$ */
#include	<stdlib.h>

/* replace undef by define */
#define  ALIGN_EIGHT_BYTES /* Use 8-byte alignment. */

#ifdef  ALIGN_EIGHT_BYTES
#define ALIGN_SIZE 8
#else
#define ALIGN_SIZE sizeof(size_t)
#endif

#define ALIGN(x)	(((x) + (ALIGN_SIZE - 1)) & ~(ALIGN_SIZE - 1))

void *
calloc(size_t nelem, size_t elsize)
{
	register char *p;
	register size_t *q;
	size_t size = ALIGN(nelem * elsize);

	p = malloc(size);
	if (p == NULL) return NULL;
	q = (size_t *) (p + size);
	while ((char *) q > p) *--q = 0;
	return p;
}

