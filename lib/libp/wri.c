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

#include	<pc_err.h>
#include	<pc_file.h>

extern		_wstrin();

#ifndef EM_WSIZE
#ifdef _EM_WSIZE
#define EM_WSIZE _EM_WSIZE
#endif
#endif

#if EM_WSIZE==4
#define SZ 11
#define MININT -2147483648
#define STRMININT "-2147483648"
#endif
#if EM_WSIZE==2
#define SZ 6
#define MININT -32768
#define STRMININT "-32768"
#endif
#if EM_WSIZE==1
#define SZ 4
#define MININT -128
#define STRMININT "-128"
#endif

#ifndef STRMININT
Something wrong here!
#endif

_wsi(w,i,f) int w,i; struct file *f; {
	char *p; int j; char buf[SZ];

	if (w < 0) _trp(EWIDTH);
	p = &buf[SZ];
	if ((j=i) < 0) {
		if (i == MININT) {
			_wstrin(w,SZ,STRMININT,f);
			return;
		}
		j = -j;
	}
	do
		*--p = '0' + j%10;
	while (j /= 10);
	if (i<0)
		*--p = '-';
	_wstrin(w,(int)(&buf[SZ]-p),p,f);
}

_wri(i,f) int i; struct file *f; {
	_wsi(SZ,i,f);
}
