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
char *getenv(char *name);
# ifdef	UNIX
#  ifdef	MALLOCH
#include <malloc.h>
#  else		/* not MALLOCH */
char *malloc(unsigned size);
#  endif	/* MALLOCH */
# else	/* not UNIX */
#include <malloc.h>
# endif	/* UNIX */
#endif	/* STDLIB */

unsigned char *Asmcode(unsigned char **s, unsigned char *c);
int Asmname(unsigned char *s, unsigned char *c);
void Charput(int c);
int Delmacro(int mx);
int Defdev();
void Delstr(int sx);
void Error(int t, int l, char *s1, char *s2);
void Error3(int len, char *word, char *sarg, int narg, char *msg);
void Expand(unsigned char *line);
void Delnum(int nx);
unsigned char *Field(int n, unsigned char *p, int c);
void Endword();
int Findchar(unsigned char *nm, int l, unsigned char *s, int e);
int Findhy(unsigned char *s, int l, int e);
int Findmacro(unsigned char *p, int e);
int Findnum(unsigned char *n, int v, int e);
int Findparms(unsigned char *n);
int Findscale(int n, double v, int e);
unsigned char *Findstr(unsigned char *nm, unsigned char *s, int e);
int LenprtHF(unsigned char *s, int p, int t);
int main(int argc, char *argv[]);
void Macro(unsigned char *inp);
void Nreq(unsigned char *line, int brk);
void Free(unsigned char **p);
unsigned char *Newstr(unsigned char *s);
void Pass2(unsigned char *line);
void Pass3(int len, unsigned char *word, unsigned char *sarg, int narg);
void regerror(char *s);
unsigned char *reg(int paren, int *flagp);
unsigned char *regatom(int *flagp);
unsigned char *regbranch(int *flagp);
regexp *regcomp(char *exp);
void regdump(regexp *r);
int regexec(regexp *prog, unsigned char *string);
void Stringput(unsigned char *s);
int Str2word(unsigned char *s, int len);
