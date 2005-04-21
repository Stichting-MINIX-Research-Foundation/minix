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

extern		_flush();

/* procedure buff(var f:file of ?); */

buff(f) struct file *f; {
	int sz;

	if ((f->flags & (0377|WRBIT)) != (MAGIC|WRBIT))
		return;
	_flush(f);
	sz = f->size;
	f->count = f->buflen = (sz>PC_BUFLEN ? sz : PC_BUFLEN-PC_BUFLEN%sz);
}
