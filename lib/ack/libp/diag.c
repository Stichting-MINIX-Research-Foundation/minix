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

/* procedure diag(var f:text); */

diag(f) struct file *f; {

	f->ptr = f->bufadr;
	f->flags = WRBIT|EOFBIT|ELNBIT|TXTBIT|MAGIC;
	f->fname = "DIAG";
	f->ufd = 2;
	f->size = 1;
	f->count = 1;
	f->buflen = 1;
}
