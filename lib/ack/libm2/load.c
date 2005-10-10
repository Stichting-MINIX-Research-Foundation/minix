/*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/

/*
  Module:	get value on stack, byte by byte
  Author:	Ceriel J.H. Jacobs
  Version:	$Header$
*/

#include <m2_traps.h>

#ifndef EM_WSIZE
#define EM_WSIZE _EM_WSIZE
#define EM_PSIZE _EM_PSIZE
#endif

#if EM_WSIZE==EM_PSIZE
typedef unsigned pcnt;
#else
typedef long pcnt;
#endif

load(siz, addr, p)
	register char *addr;
	register pcnt siz;
{
	/*	Make sure, that a value with a size that could have been
		handled by the LOI instruction ends up at the same place,
		where it would, were the LOI instruction used.
	*/
	register char *q = (char *) &p;
	char t[4];

	if (siz < EM_WSIZE && EM_WSIZE % siz == 0) {
		/* as long as EM_WSIZE <= 4 ... */
		if (siz != 2) TRP(M2_INTERNAL);	/* internal error */
		q = &t[0];
	}
	while (siz--) *q++ = *addr++;
	if (q - t == 2) {
		*((unsigned *)(&p)) = *((unsigned short *) (&t[0]));
	}
}
