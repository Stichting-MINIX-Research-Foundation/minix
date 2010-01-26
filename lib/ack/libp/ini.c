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

#include        <pc_file.h>

extern          (*_sig())();
extern          _catch();
#ifndef CPM
extern int      _isatty();
#endif

struct file     **_extfl;
int		_extflc;	/* number of external files */
char            *_m_lb;         /* LB of _m_a_i_n */
struct file     *_curfil;       /* points to file struct in case of errors */
int             _pargc;
char            **_pargv;
char            ***_penviron;
int		_fp_hook = 1;	/* This is for Minix, but does not harm others */

_ini(args,c,p,mainlb) char *args,*mainlb; int c; struct file **p; {
	struct file *f;
	char buf[128];

	_pargc= *(int *)args; args += sizeof (int);
	_pargv= *(char ***)args;
	_sig(_catch);
	_extfl = p;
	_extflc = c;
	if( !c ) return;
	_m_lb = mainlb;
	if ( (f = _extfl[0]) != (struct file *) 0) {
		f->ptr = f->bufadr;
		f->flags = MAGIC|TXTBIT;
		f->fname = "INPUT";
		f->ufd = 0;
		f->size = 1;
		f->count = 0;
		f->buflen = PC_BUFLEN;
	}
	if ( (f = _extfl[1]) != (struct file *) 0) {
		f->ptr = f->bufadr;
		f->flags = MAGIC|TXTBIT|WRBIT|EOFBIT|ELNBIT;
		f->fname = "OUTPUT";
		f->ufd = 1;
		f->size = 1;
#ifdef CPM
		f->count = 1;
#else
		f->count = (_isatty(1) ? 1 : PC_BUFLEN);
#endif
		f->buflen = f->count;
	}
}
