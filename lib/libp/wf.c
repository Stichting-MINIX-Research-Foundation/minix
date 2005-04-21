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

#include	<pc_file.h>
#include	<pc_err.h>

extern struct file	*_curfil;
extern			_trp();

_wf(f) struct file *f; {

	_curfil = f;
	if ((f->flags&0377) != MAGIC)
		_trp(EBADF);
	if ((f->flags & WRBIT) == 0)
		_trp(EWRITEF);
}
