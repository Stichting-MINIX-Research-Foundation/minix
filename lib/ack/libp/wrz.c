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

extern		_wss();
extern		_wrs();

_wsz(w,s,f) int w; char *s; struct file *f; {
	char *p;

	if (w < 0) _trp(EWIDTH);
	for (p=s; *p; p++);
	_wss(w,(int)(p-s),s,f);
}

_wrz(s,f) char *s; struct file *f; {
	char *p;

	for (p=s; *p; p++);
	_wrs((int)(p-s),s,f);
}
