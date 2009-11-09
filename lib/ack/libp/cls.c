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
extern			_flush();
extern			_outcpt();
extern int		_close();

_xcls(f) struct file *f; {

	if ((f->flags & WRBIT) == 0)
		return;
	if ((f->flags & (TXTBIT|ELNBIT)) == TXTBIT) {
#ifdef CPM
		*f->ptr = '\r';
		_outcpt(f);
#endif
		*f->ptr = '\n';
		_outcpt(f);
	}
	_flush(f);
}

_cls(f) struct file *f; {
#ifdef MAYBE
	char *p;
#endif

	_curfil = f;
	if ((f->flags&0377) != MAGIC)
		return;
#ifdef MAYBE
	p = f->bufadr;
	if (f->ptr < p)
		return;
	if (f->buflen <= 0)
		return;
	p += f->buflen;
	if (f->ptr >= p)
		return;
#endif
	_xcls(f);
	if (_close(f->ufd) != 0)
		_trp(ECLOSE);
	f->flags = 0;
}
