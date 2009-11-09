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

extern		_wf();
extern		_outcpt();

_wstrin(width,len,buf,f) int width,len; char *buf; struct file *f; {

	_wf(f);
	for (width -= len; width>0; width--) {
		*f->ptr = ' ';
		_outcpt(f);
	}
	while (--len >= 0) {
		*f->ptr = *buf++;
		_outcpt(f);
	}
}

_wsc(w,c,f) int w; char c; struct file *f; {

	if (w < 0) _trp(EWIDTH);
	_wss(w,1,&c,f);
}

_wss(w,len,s,f) int w,len; char *s; struct file *f; {

	if (w < 0 || len < 0) _trp(EWIDTH);
	if (w < len)
		len = w;
	_wstrin(w,len,s,f);
}

_wrs(len,s,f) int len; char *s; struct file *f; {
	if (len < 0) _trp(EWIDTH);
	_wss(len,len,s,f);
}

_wsb(w,b,f) int w,b; struct file *f; {
	if (b)
		_wss(w,4,"true",f);
	else
		_wss(w,5,"false",f);
}

_wrb(b,f) int b; struct file *f; {
	_wsb(5,b,f);
}
