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
#include	<pc_file.h>

extern		_wstrin();
extern char	*_ecvt();

#define	PREC_DIG	80	/* maximum digits produced by _ecvt() */

_wsr(w,r,f) int w; double r; struct file *f; {
	char *p,*b; int s,d,i; char buf[PREC_DIG+7];

	if (w < 0) _trp(EWIDTH);
	p = buf;
	if ((i = w-6) < 2)
		i = 2;
	b = _ecvt(r,i,&d,&s);
	*p++ = s? '-' : ' ';
	if (*b == '0')
		d++;
	*p++ = *b++;
	*p++ = '.';
	while (--i > 0)
		*p++ = *b++;
	*p++ = 'e';
	d--;
	if (d < 0) {
		d = -d;
		*p++ = '-';
	} else
		*p++ = '+';

	if (d >= 1000) {
		*p++ = '*';
		*p++ = '*';
		*p++ = '*';
	}
	else {
		*p++ = '0' + d/100;
		*p++ = '0' + (d/10) % 10;
		*p++ = '0' + d%10;
	}
	_wstrin(w,(int)(p-buf),buf,f);
}

_wrr(r,f) double r; struct file *f; {
	_wsr(13,r,f);
}
