/* $Header$ */
/*
 * (c) copyright 1983 by the Vrije Universiteit, Amsterdam, The Netherlands.
 *
 *          This product is part of the Amsterdam Compiler Kit.
 *
 * Permission to use, sell, duplicate or disclose this software must be
 * obtained in writing. Requests for such permissions may be sent to
 *
 *      Dr. Andrew S. Tanenbaum
 *      Wiskundig Seminarium
 *      Vrije Universiteit
 *      Postbox 7161
 *      1007 MC Amsterdam
 *      The Netherlands
 *
 */

/* Author: J.W. Stevenson */

#include	<pc_err.h>

extern		_trp();

#define	assert(x)	/* nothing */

#ifndef EM_WSIZE
#define EM_WSIZE _EM_WSIZE
#endif

struct descr {
	int	low;
	int	diff;
	int	size;
};

_unp(ad,zd,i,ap,zp,noext) int i; struct descr *ad,*zd; char *ap,*zp; int noext; {

	if (zd->diff > ad->diff ||
			(i -= ad->low) < 0 ||
			(i+zd->diff) > ad->diff)
		_trp(EUNPACK);
	ap += (i * ad->size);
	i = (zd->diff + 1) * zd->size;
	if (zd->size == 1) {
		int *aptmp = (int *) ap;
		assert(ad->size == EM_WSIZE);
		while (--i >= 0)
			if (noext) *aptmp++ = *zp++ & 0377;
			else *aptmp++ = *zp++;
#if EM_WSIZE > 2
	} else if (zd->size == 2) {
		int *aptmp = (int *) ap;
		short *zptmp = (short *) zp;
		assert(ad->size == EM_WSIZE);
		while (--i >= 0)
			if (noext) *aptmp++ = *zptmp++ & 0177777;
			else *aptmp++ = *zptmp++;
#endif
	} else {
		assert(ad->size == zd->size);
		while (--i >= 0)
			*ap++ = *zp++;
	}
}
