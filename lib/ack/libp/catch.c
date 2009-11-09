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

#include	<em_abs.h>
#include	<pc_err.h>
#include	<pc_file.h>

/* to make it easier to patch ... */
extern struct file	*_curfil;

static struct errm {
	int errno;
	char *errmes;
} errors[] = {
	{ EARRAY,	"array bound error"},
	{ ERANGE,	"range bound error"},
	{ ESET,		"set bound error"},
	{ EIOVFL,	"integer overflow"},
	{ EFOVFL,	"real overflow"},
	{ EFUNFL,	"real underflow"},
	{ EIDIVZ,	"divide by 0"},
	{ EFDIVZ,	"divide by 0.0"},
	{ EIUND,	"undefined integer"},
	{ EFUND,	"undefined real"},
	{ ECONV,	"conversion error"},

	{ ESTACK,	"stack overflow"},
	{ EHEAP,	"heap overflow"},
	{ EILLINS,	"illegal instruction"},
	{ EODDZ,	"illegal size argument"},
	{ ECASE,	"case error"},
	{ EMEMFLT,	"addressing non existent memory"},
	{ EBADPTR,	"bad pointer used"},
	{ EBADPC,	"program counter out of range"},
	{ EBADLAE,	"bad argument of lae"},
	{ EBADMON,	"bad monitor call"},
	{ EBADLIN,	"argument if LIN too high"},
	{ EBADGTO,	"GTO descriptor error"},

	{ EARGC,	"more args expected" },
	{ EEXP,		"error in exp" },
	{ ELOG,		"error in ln" },
	{ ESQT,		"error in sqrt" },
	{ EASS,		"assertion failed" },
	{ EPACK,	"array bound error in pack" },
	{ EUNPACK,	"array bound error in unpack" },
	{ EMOD,		"only positive j in 'i mod j'" },
	{ EBADF,	"file not yet open" },
	{ EFREE,	"dispose error" },
	{ EFUNASS,	"function not assigned" },
	{ EWIDTH,	"illegal field width" },

	{ EWRITEF,	"not writable" },
	{ EREADF,	"not readable" },
	{ EEOF,		"end of file" },
	{ EFTRUNC,	"truncated" },
	{ ERESET,	"reset error" },
	{ EREWR,	"rewrite error" },
	{ ECLOSE,	"close error" },
	{ EREAD,	"read error" },
	{ EWRITE,	"write error" },
	{ EDIGIT,	"digit expected" },
	{ EASCII,	"non-ASCII char read" },
	{ -1,		0}
};

extern int		_pargc;
extern char		**_pargv;
extern char		***_penviron;

extern char		*_hol0();
extern			_trp();
extern			_exit();
extern int		_write();

_catch(erno) unsigned erno; {
	register struct errm *ep = &errors[0];
	char *p,*q,*s,**qq;
	char buf[20];
	unsigned i;
	int j = erno;
	char *pp[11];
	char xbuf[100];

	qq = pp;
	if (p = FILN)
		*qq++ = p;
	else
		*qq++ = _pargv[0];

	while (ep->errno != erno && ep->errmes != 0) ep++;
	p = buf;
	s = xbuf;
	if (i = LINO) {
		*qq++ = ", ";
		do
			*p++ = i % 10 + '0';
		while (i /= 10);
		while (p > buf) *s++ = *--p;
	}
	*s++ = ':';
	*s++ = ' ';
	*s++ = '\0';
	*qq++ = xbuf;
	if ((erno & ~037) == 0140 && (_curfil->flags&0377)==MAGIC) { 
		/* file error */
		*qq++ = "file ";
		*qq++ = _curfil->fname;
		*qq++ = ": ";
	}
	if (ep->errmes) *qq++ = ep->errmes;
	else {
		*qq++ = "error number ";
		*qq++ = s;
		p = buf;
		if (j < 0) {
			j = -j;
			*s++ = '-';
		}
		do
			*p++ = j % 10 + '0';
		while (j /= 10);
		while (p > buf) *s++ = *--p;
		*s = 0;
	}
	*qq++ = "\n";
	*qq = 0;
	qq = pp;
	while (q = *qq++) {
		p = q;
		while (*p)
			p++;
		if (_write(2,q,(int)(p-q)) < 0)
			;
	}
	_exit(erno);
error:
	_trp(erno);
}
