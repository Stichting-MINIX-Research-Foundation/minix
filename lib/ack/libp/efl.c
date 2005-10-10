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
#include	<pc_err.h>

extern struct file	*_curfil;
extern			_trp();
extern			_incpt();

int _efl(f) struct file *f; {

	_curfil = f;
	if ((f->flags & 0377) != MAGIC)
		_trp(EBADF);
	if ((f->flags & (WINDOW|WRBIT|EOFBIT)) == 0)
		_incpt(f);
	return((f->flags & EOFBIT) != 0);
}
