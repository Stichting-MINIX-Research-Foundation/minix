/*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/

/*
  Module:	default modula-2 trap handler
  Author:	Ceriel J.H. Jacobs
  Version:	$Header$
*/
#include <em_abs.h>
#include <m2_traps.h>
#include <signal.h>

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

	{ M2_TOOLARGE,	"stack size of process too large"},
	{ M2_TOOMANY,	"too many nested traps + handlers"},
	{ M2_NORESULT,	"no RETURN from function procedure"},
	{ M2_UOVFL,	"cardinal overflow"},
	{ M2_FORCH,	"(warning) FOR-loop control variable was changed in the body"},
	{ M2_UUVFL,	"cardinal underflow"},
	{ M2_INTERNAL,	"internal error; ask an expert for help"},
	{ M2_UNIXSIG,	"got a unix signal"},
	{ -1,		0}
};

catch(trapno)
	int trapno;
{
	register struct errm *ep = &errors[0];
	char *errmessage;
	char buf[20];
	register char *p, *s;

	while (ep->errno != trapno && ep->errmes != 0) ep++;
	if (p = ep->errmes) {
		while (*p) p++;
		_Traps__Message(ep->errmes, 0, (int) (p - ep->errmes), 1);
	}
	else {
		int i = trapno;
		static char q[] = "error number xxxxxxxxxxxxx";

		p = &q[13];
		s = buf;
		if (i < 0) {
			i = -i;
			*p++ = '-';
		}
		do
			*s++ = i % 10 + '0';
		while (i /= 10);
		while (s > buf) *p++ = *--s;
		*p = 0;
		_Traps__Message(q, 0, (int) (p - q), 1);
	}
#if !defined(__em24) && !defined(__em44) && !defined(__em22)
	if (trapno == M2_UNIXSIG) {
		extern int __signo;
		signal(__signo, SIG_DFL);
		_cleanup();
		kill(getpid(), __signo);
		_exit(trapno);
	}
#endif
	if (trapno != M2_FORCH) {
		_cleanup();
		_exit(trapno);
	}
	SIG(catch);
}
