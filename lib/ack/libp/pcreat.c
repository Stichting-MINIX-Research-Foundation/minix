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

extern		_cls();
extern		_trp();
extern int	_creat();

/* procedure pcreat(var f:text; s:string); */

pcreat(f,s) struct file *f; char *s; {

	_cls(f);	/* initializes _curfil */
	f->ptr = f->bufadr;
	f->flags = WRBIT|EOFBIT|ELNBIT|TXTBIT|MAGIC;
	f->fname = s;
	f->size = 1;
	f->count = PC_BUFLEN;
	f->buflen = PC_BUFLEN;
	if ((f->ufd = _creat(s,0644)) < 0)
		_trp(EREWR);
}
