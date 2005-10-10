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

extern		_rf();
extern		_skipsp();
extern int	_getsig();
extern int	_fstdig();
extern int	_nxtdig();

long _rdl(f) struct file *f; {
	int is_signed,ch; long l;

	_rf(f);
	_skipsp(f);
	is_signed = _getsig(f);
	ch = _fstdig(f);
	l = 0;
	do
		l = l*10 - ch;
	while ((ch = _nxtdig(f)) >= 0);
	return(is_signed ? l : -l);
}
