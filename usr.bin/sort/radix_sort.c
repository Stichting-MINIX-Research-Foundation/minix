/*	$NetBSD: radix_sort.c,v 1.4 2009/09/19 16:18:00 dsl Exp $	*/

/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Peter McIlroy and by Dan Bernstein at New York University, 
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)radixsort.c	8.2 (Berkeley) 4/28/95";
#else
__RCSID("$NetBSD: radix_sort.c,v 1.4 2009/09/19 16:18:00 dsl Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

/*
 * 'stable' radix sort initially from libc/stdlib/radixsort.c
 */

#include <sys/types.h>

#include <assert.h>
#include <errno.h>
#include <util.h>
#include "sort.h"

typedef struct {
	RECHEADER **sa;		/* Base of saved area */
	int sn;			/* Number of entries */
	int si;			/* index into data for compare */
} stack;

static void simplesort(RECHEADER **, int, int);

#define	THRESHOLD	20		/* Divert to simplesort(). */

#define empty(s)	(s >= sp)
#define pop(a, n, i)	a = (--sp)->sa, n = sp->sn, i = sp->si
#define push(a, n, i)	sp->sa = a, sp->sn = n, (sp++)->si = i
#define swap(a, b, t)	t = a, a = b, b = t

void
radix_sort(RECHEADER **a, RECHEADER **ta, int n)
{
	u_int count[256], nc, bmin;
	u_int c;
	RECHEADER **ak, **tai, **lim;
	RECHEADER *hdr;
	int stack_size = 512;
	stack *s, *sp, *sp0, *sp1, temp;
	RECHEADER **top[256];
	u_int *cp, bigc;
	int data_index = 0;

	if (n < THRESHOLD && !DEBUG('r')) {
		simplesort(a, n, 0);
		return;
	}

	s = emalloc(stack_size * sizeof *s);
	memset(&count, 0, sizeof count);
	/* Technically 'top' doesn't need zeroing */
	memset(&top, 0, sizeof top);

	sp = s;
	push(a, n, data_index);
	while (!empty(s)) {
		pop(a, n, data_index);
		if (n < THRESHOLD && !DEBUG('r')) {
			simplesort(a, n, data_index);
			continue;
		}

		/* Count number of times each 'next byte' occurs */
		nc = 0;
		bmin = 255;
		lim = a + n;
		for (ak = a, tai = ta; ak < lim; ak++) {
			hdr = *ak;
			if (data_index >= hdr->keylen) {
				/* Short key, copy to start of output */
				if (UNIQUE && a != sp->sa)
					/* Stop duplicate being written out */
					hdr->keylen = -1;
				*a++ = hdr;
				n--;
				continue;
			}
			/* Save in temp buffer for distribute */
			*tai++ = hdr;
			c = hdr->data[data_index];
			if (++count[c] == 1) {
				if (c < bmin)
					bmin = c;
				nc++;
			}
		}
		/*
		 * We need save the bounds for each 'next byte' that
		 * occurs more so we can sort each block.
		 */
		if (sp + nc > s + stack_size) {
			stack_size *= 2;
			sp1 = erealloc(s, stack_size * sizeof *s);
			sp = sp1 + (sp - s);
			s = sp1;
		}

		/* Minor optimisation to do the largest set last */
		sp0 = sp1 = sp;
		bigc = 2;
		/* Convert 'counts' positions, saving bounds for later sorts */
		ak = a;
		for (cp = count + bmin; nc > 0; cp++) {
			while (*cp == 0)
				cp++;
			if ((c = *cp) > 1) {
				if (c > bigc) {
					bigc = c;
					sp1 = sp;
				}
				push(ak, c, data_index+1);
			}
			ak += c;
			top[cp-count] = ak;
			*cp = 0;			/* Reset count[]. */
			nc--;
		}
		swap(*sp0, *sp1, temp);

		for (ak = ta+n; --ak >= ta;)		/* Deal to piles. */
			*--top[(*ak)->data[data_index]] = *ak;
	}

	free(s);
}

/* insertion sort, short records are sorted before long ones */
static void
simplesort(RECHEADER **a, int n, int data_index)
{
	RECHEADER **ak, **ai;
	RECHEADER *akh;
	RECHEADER **lim = a + n;
	const u_char *s, *t;
	int s_len, t_len;
	int i;
	int r;

	if (n <= 1)
		return;

	for (ak = a+1; ak < lim; ak++) {
		akh = *ak;
		s = akh->data;
		s_len = akh->keylen;
		for (ai = ak; ;) {
			ai--;
			t_len = (*ai)->keylen;
			if (t_len != -1) {
				t = (*ai)->data;
				for (i = data_index; ; i++) {
					if (i >= s_len || i >= t_len) {
						r = s_len - t_len;
						break;
					}
					r =  s[i]  - t[i];
					if (r != 0)
						break;
				}
				if (r >= 0) {
					if (r == 0 && UNIQUE) {
						/* Put record below existing */
						ai[1] = ai[0];
						/* Mark as duplicate - ignore */
						akh->keylen = -1;
					} else {
						ai++;
					}
					break;
				}
			}
			ai[1] = ai[0];
			if (ai == a)
				break;
		}
		ai[0] = akh;
	}
}
