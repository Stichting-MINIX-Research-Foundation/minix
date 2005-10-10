/*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/

/*
  Module:	runtime support for conformant arrays
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
typedef unsigned long pcnt;
#endif

struct descr {
	char *addr;
	int low;
	unsigned int highminlow;
	unsigned int size;
};

static struct descr *descrs[10];
static struct descr **ppdescr = descrs;

pcnt
new_stackptr(pdscr, a)
	struct descr *pdscr;
{
	register struct descr *pdescr = pdscr;
	pcnt size = (((pdescr->highminlow + 1) * pdescr->size +
				(EM_WSIZE - 1)) & ~(EM_WSIZE - 1));

	if (ppdescr >= &descrs[10]) {
		/* to many nested traps + handlers ! */
		TRP(M2_TOOMANY);
	}
	*ppdescr++ = pdescr;
	if ((char *) &a - (char *) &pdscr > 0) {
		/* stack grows downwards */
		return - size;
	}
	return size;
}

copy_array(pp, a)
	char *pp;
{
	register char *p = pp;
	register char *q;
	register pcnt sz;
	char dummy;

	ppdescr--;
	sz = ((*ppdescr)->highminlow + 1) * (*ppdescr)->size;
	
	if ((char *) &a - (char *) &pp > 0) {
		(*ppdescr)->addr = q = (char *) &a;
	}
	else	(*ppdescr)->addr = q = (char *) &a - 
			((sz + (EM_WSIZE - 1)) & ~ (EM_WSIZE - 1));

	while (sz--) *q++ = *p++;
}
