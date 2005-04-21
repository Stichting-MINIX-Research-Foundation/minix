/*
 *	error.c - error handling functions for cawf(1)
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

#include "cawf.h"


/*
 * Error(t, l, s1, s2) - issue error message
 */

void
Error(t, l, s1, s2)
	int t;				/* type: WARN or FATAL */
	int l;				/* LINE: display Line[] */
	char *s1, *s2;			/* optional text */
{
	char msg[MAXLINE];		/* message */

	if (t == WARN && !Dowarn) return;

	if (l == LINE)
		(void) fprintf(Efs, "%s: (%s, %d):%s%s - %s\n",
			Pname,
			Inname,
			NR,
			(s1 == NULL) ? "" : s1,
			(s2 == NULL) ? "" : s2,
			Line);
	else
		(void) fprintf(Efs, "%s:%s%s\n",
			Pname,
			(s1 == NULL) ? "" : s1,
			(s2 == NULL) ? "" : s2);
	if (t == FATAL)
		exit(1);
	Err = 1;
	return;
}


/*
 * Error3(len, word, sarg, narg) - process error in pass3
 */

void
Error3(len, word, sarg, narg, msg)
	int len;			/* length (negative is special */
        char *word;			/* word */
        char *sarg;			/* string argument */
        int narg;                       /* numeric argument */
	char *msg;			/* message */
{
	if (len == MESSAGE) {
		(void) fprintf(Efs, "%s: (%s, %d) %s\n",
			Pname,
			(word == NULL) ? "<none>" : word,
			narg,
			(sarg == NULL) ? "<none>" : sarg);
		return;
	}
	(void) fprintf(Efs,
		"%s: pass3, len=%d, word=\"%s\", sarg=\"%s\", narg=%d%s%s\n",
		Pname, len,
		(word == NULL) ? "" : word,
		(sarg == NULL) ? "" : sarg,
		narg,
		(msg == NULL) ? "" : " - ",
		(msg == NULL) ? "" : msg);
	Err = 1;
}
