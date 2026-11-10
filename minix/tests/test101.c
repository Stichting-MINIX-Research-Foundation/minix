/*
 * test101: calloc().  Memory must read as zero also after the heap was
 * dirtied and the break went down and up again (VM keeps the pages when the
 * break is lowered), and a large calloc() must not touch all of its pages:
 * software like Mesa's softpipe callocs far more than it uses.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int max_error = 5;
#include "common.h"

static int
all_zero(const unsigned char *p, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++)
		if (p[i] != 0)
			return 0;
	return 1;
}

static void
test_dirty(void)
{
	static const size_t sizes[] = { 1, 100, 2048, 4096, 5000, 65536,
		1 << 20, 3 << 20 };
	unsigned char *p, *q;
	unsigned int i, round;

	subtest = 1;

	for (round = 0; round < 3; round++) {
		for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
			/* Dirty memory, give it back, calloc it again. */
			if ((p = malloc(sizes[i])) == NULL) { e(1); return; }
			memset(p, 0xa5, sizes[i]);
			free(p);
			if ((q = calloc(1, sizes[i])) == NULL) { e(2); return; }
			if (!all_zero(q, sizes[i])) e(3);
			memset(q, 0x5a, sizes[i]);
			free(q);
		}
	}

	/* Grow the heap, free everything (the break may go down), dirty and
	 * calloc again across the old top. */
	{
		void *v[16];

		for (i = 0; i < 16; i++) {
			if ((v[i] = malloc(1 << 20)) == NULL) { e(4); return; }
			memset(v[i], 0xc3, 1 << 20);
		}
		for (i = 0; i < 16; i++)
			free(v[i]);
		if ((q = calloc(8, 1 << 20)) == NULL) { e(5); return; }
		if (!all_zero(q, 8 << 20)) e(6);
		free(q);
		if ((q = calloc(24, 1 << 20)) == NULL) { e(7); return; }
		if (!all_zero(q, 24 << 20)) e(8);
		free(q);
	}

	/* Overflowing size. */
	if (calloc((size_t)-1 / 2, 4) != NULL) e(9);
}

static void
test_lazy(void)
{
	unsigned char *p;
	size_t n = 160 << 20, i;

	subtest = 2;

	/* More than half of the test machine's memory: works only if the
	 * pages are not all touched.  Touch a few. */
	if ((p = calloc(1, n)) == NULL) { e(1); return; }
	for (i = 0; i < n; i += 16 << 20)
		if (p[i] != 0) e(2);
	p[n - 1] = 1;
	free(p);
}

int
main(int argc, char **argv)
{

	start(101);

	test_dirty();
	test_lazy();

	quit();
	return 0;
}
