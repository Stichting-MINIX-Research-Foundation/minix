/*
 *	proto.h - function prototype definitions for cawf(1)
 */

/*
 *	Copyright (c) 1991 Purdue University Research Foundation,
 *	West Lafayette, Indiana 47907.  All rights reserved.
 *
 *	Written by Victor A. Abell <abe@mace.cc.purdue.edu>,  Purdue
 *	University Computing Center.  Not derived from licensed software;
 *	derived from awf(1) by Henry Spencer of the University of Toronto.
 *
 *	Permission is granted to anyone to use this software for any
 *	purpose on any computer system, and to alter it and redistribute
 *	it freely, subject to the following restrictions:
 *
 *	1. The author is not responsible for any consequences of use of
 *	   this software, even if they arise from flaws in it.
 *
 *	2. The origin of this software must not be misrepresented, either
 *	   by explicit claim or by omission.  Credits must appear in the
 *	   documentation.
 *
 *	3. Altered versions must be plainly marked as such, and must not
 *	   be misrepresented as being the original software.  Credits must
 *	   appear in the documentation.
 *
 *	4. This notice may not be removed or altered.
 */

#include "ansi.h"

#ifdef	UNIX
# ifdef	USG
#include <string.h>
# else	/* not USG */
#include <strings.h>
# endif	/* USG */
#else	/* not UNIX */
#include <string.h>
#endif	/* UNIX */

/*
 * The following conditional rat's nest intends to:
 *
 * for MS-DOS	#include <stdlib.h> and <malloc.h>.  <stdlib.h> exists in
 *		MS-DOS so the STDLIB symbol should be defined and UNIX
 *		shouldn't be.
 *
 * for Unix	#include <stdlib.h> if the STDLIB symbol is defined.  If
 *		STDLIB isn't defined, define a prototype for getenv().
 *		If the UNIX symbol is defined (and it should be) and if
 *		the MALLOCH symbol is defined, #include <malloc.h>; else
 *		define a prototype for malloc() if UNIX is defined and
 *		MALLOCH isn't.  (The Unix <stdlib.h> usually defines the
 *		malloc() prototype.)
 *
 * for ???	Define a prototype for getenv() and #include <malloc.h>
 *		if neither STDLIB nor UNIX are defined.  (What system is
 *		this?)
 */

#ifdef	STDLIB
#include <stdlib.h>
# ifndef	UNIX
#include <malloc.h>
# endif		/* UNIX */
#else	/* not STDLIB */
_PROTOTYPE(char *getenv,(char *name));
# ifdef	UNIX
#  ifdef	MALLOCH
#include <malloc.h>
#  else		/* not MALLOCH */
_PROTOTYPE(char *malloc,(unsigned size));
#  endif	/* MALLOCH */
# else	/* not UNIX */
#include <malloc.h>
# endif	/* UNIX */
#endif	/* STDLIB */

_PROTOTYPE(unsigned char *Asmcode,(unsigned char **s, unsigned char *c));
_PROTOTYPE(int Asmname,(unsigned char *s, unsigned char *c));
_PROTOTYPE(void Charput,(int c));
_PROTOTYPE(int Delmacro,(int mx));
_PROTOTYPE(int Defdev,());
_PROTOTYPE(void Delstr,(int sx));
_PROTOTYPE(void Error,(int t, int l, char *s1, char *s2));
_PROTOTYPE(void Error3,(int len, char *word, char *sarg, int narg, char *msg));
_PROTOTYPE(void Expand,(unsigned char *line));
_PROTOTYPE(void Delnum,(int nx));
_PROTOTYPE(unsigned char *Field,(int n, unsigned char *p, int c));
_PROTOTYPE(void Endword,());
_PROTOTYPE(int Findchar,(unsigned char *nm, int l, unsigned char *s, int e));
_PROTOTYPE(int Findhy,(unsigned char *s, int l, int e));
_PROTOTYPE(int Findmacro,(unsigned char *p, int e));
_PROTOTYPE(int Findnum,(unsigned char *n, int v, int e));
_PROTOTYPE(int Findparms,(unsigned char *n));
_PROTOTYPE(int Findscale,(int n, double v, int e));
_PROTOTYPE(unsigned char *Findstr,(unsigned char *nm, unsigned char *s, int e));
_PROTOTYPE(int getopt,(int argc, char **argv, char *opts));
_PROTOTYPE(int LenprtHF,(unsigned char *s, int p, int t));
_PROTOTYPE(int main,(int argc, char *argv[]));
_PROTOTYPE(void Macro,(unsigned char *inp));
_PROTOTYPE(void Nreq,(unsigned char *line, int brk));
_PROTOTYPE(void Free,(unsigned char **p));
_PROTOTYPE(unsigned char *Newstr,(unsigned char *s));
_PROTOTYPE(void Pass2,(unsigned char *line));
_PROTOTYPE(void Pass3,(int len, unsigned char *word, unsigned char *sarg, int narg));
_PROTOTYPE(void regerror,(char *s));
_PROTOTYPE(unsigned char *reg,(int paren, int *flagp));
_PROTOTYPE(unsigned char *regatom,(int *flagp));
_PROTOTYPE(unsigned char *regbranch,(int *flagp));
_PROTOTYPE(regexp *regcomp,(char *exp));
_PROTOTYPE(void regdump,(regexp *r));
_PROTOTYPE(int regexec,(regexp *prog, unsigned char *string));
_PROTOTYPE(void Stringput,(unsigned char *s));
_PROTOTYPE(int Str2word,(unsigned char *s, int len));
