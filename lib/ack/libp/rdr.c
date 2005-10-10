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

#include	<pc_file.h>

#define	BIG	1e17

extern		_rf();
extern		_incpt();
extern		_skipsp();
extern int	_getsig();
extern int	_getint();
extern int	_fstdig();
extern int	_nxtdig();

static double		r;
static int		pow10;

static dig(ch) int ch; {

	if (r>BIG)
		pow10++;
	else
		r = r*10.0 + ch;
}

double _rdr(f) struct file *f; {
	int i; double e; int is_signed,ch;

	r = 0;
	pow10 = 0;
	_rf(f);
	_skipsp(f);
	is_signed = _getsig(f);
	ch = _fstdig(f);
	do
		dig(ch);
	while ((ch = _nxtdig(f)) >= 0);
	if (*f->ptr == '.') {
		_incpt(f);
		ch = _fstdig(f);
		do {
			dig(ch);
			pow10--;
		} while ((ch = _nxtdig(f)) >= 0);
	}
	if ((*f->ptr == 'e') || (*f->ptr == 'E')) {
		_incpt(f);
		pow10 += _getint(f);
	}
	if ((i = pow10) < 0)
		i = -i;
	e = 1.0;
	while (--i >= 0)
		e *= 10.0;
	if (pow10<0)
		r /= e;
	else
		r *= e;
	return(is_signed? -r : r);
}
