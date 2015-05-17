/*
 *	expand.c - macro expansion functions for cawf(1)
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
 * Expand(line) - expand macro or if/ie/el line
 */

void Expand(unsigned char *line) {

	unsigned char buf[2*MAXLINE];	/* line buffer */
	unsigned char cmd[4];		/* nroff command */
	int cmdl;			/* command length */
	int cmdx;			/* cmd index in Macrotab[] */
	int cond = 0;			/* conditional statuses */
	int i, j;			/* temporary indexes */
	int iflen;			/* if statement length */
	int invert;			/* inversion status */
	unsigned char *lp;		/* line pointer */
	int mx = -1;			/* Macrotab[] index */
	int n1, n2;			/* temporary numbers */
	int nargs = 0;			/* number of arguments */
	int nleft = 0;			/* number of macro lines left */
	char op;			/* comparison operator */
	int prevcond;			/* previous condition (for else's) */
	int ptr = -1;			/* Macrotxt[] index */
	int quote;			/* quoted string status */
	unsigned char *s1, *s2;		/* temporary string pointers */


	(void) sprintf((char *)buf, ".^= %d %s", NR, (char *)Inname);
	Pass2(buf);

	for (lp = line; *lp; ) {
		invert = regexec(Pat[1].pat, lp);
		prevcond = cond;
		cond = 0;
		if (regexec(Pat[0].pat, lp) == 0) {
	    /*
	     * Not conditional: - ! "^[.'](i[ef]|el)"
	     */
			cond = 1;
			iflen = 0;
		}

		else if (regexec(Pat[2].pat, lp)) {
	    /*
	     * Argument count comparison: -
	     *		"^[.']i[ef] !?\\n\(\.\$(>|>=|=|<|<=)[0-9] "
	     */
			iflen = strlen(".if \\n(.$=n ") + invert;
			s1 = lp + iflen - 3;
			op = *s1++;
			if (*s1 == '=' && (op == '>' || op == '<')) {
				s1++;
				op = (op == '>') ? 'G' : 'L';
			}
			n1 = (int)(*s1 - '0');
			switch (op) {
				case '=':
					if ((nargs - 1) == n1)
						cond = 1;
					break;
				case '<':
					if ((nargs - 1) < n1)
						cond = 1;
					break;
				case '>':
					if ((nargs - 1) > n1)
						cond = 1;
					break;
				case 'G':	/* >= */
					if ((nargs - 1) >= n1)
						cond = 1;
					break;
				case 'L':	/* <= */
					if ((nargs - 1) <= n1)
						cond = 1;
			}
		}

		else if (regexec(Pat[3].pat, lp)) {
	    /*
	     * Argument string comparison: - "^[.']i[ef] !?'\\\$[0-9]'[^']*' "
	     */
			iflen = strlen(".if '\\$n'") + invert;
			n1 = (int)(*(lp + iflen - 2) - '0');
			if (n1 >= 0 && n1 < nargs)
				s1 = Args[n1];
			else
				s1 = (unsigned char *)"";
			if ((s2 = (unsigned char *)strchr((char *)lp
				  + iflen, '\''))
			!= NULL) {
				n2 = s2 - lp - iflen;
				if (strncmp((char *)s1, (char *)lp + iflen, n2)
				== 0)
					cond = 1;
				iflen += n2 + 2;
			}
		}

		else if (regexec(Pat[4].pat, lp)) {
	    /*
	     * Nroff or troff: - "^[.']i[ef] !?[nt] "
	     */
			iflen = strlen(".if n ") + invert;
			if (*(lp + iflen - 2) == 'n')
				cond = 1;
		}

		else if ((*lp == '.' || *lp == '\'')
		     &&  strncmp((char *)lp+1, "el ", 3) == 0) {
	    /*
	     * Else clause: - "^[.']el "
	     */
			cond = 1 - prevcond;
			iflen = 4;
		}

		else {
	    /*
	     * Unknown conditional:
	     */
			cond = 1;
			iflen = 0;
			(void) sprintf((char *)buf,
				".tm unknown .if/.ie form: %s", (char *)lp);
			lp = buf;
		}
	   /*
	    * Handle conditional.  If case is true, locate predicate.
	    * If predicate is an .i[ef], process it.
	    */
		if (invert)
			cond = 1 - cond;
		if (cond && iflen > 0) {
			lp += iflen;
			if (regexec(Pat[15].pat, lp))
				continue;
		}
	    /*
	     * Do argument substitution, as necessary.
	     */
		if (cond && regexec(Pat[5].pat, lp)) {      /* "\$[0-9]" ??? */
			for (s1 = buf;;) {
				if ((n1 = Pat[5].pat->startp[0] - lp) > 0) {
					(void) strncpy((char *)s1, (char *)lp,
						n1);
					s1 += n1;
				}
				*s1 = '\0';
				lp = Pat[5].pat->endp[0];
				n1 = (int)(*(lp-1) - '0');
				if (n1 >= 0 && n1 < nargs) {
					(void) strcpy((char *)s1,
						(char *)Args[n1]);
					s1 += strlen((char *)Args[n1]);
				}
				if (*lp == '\0')
					break;
				if (regexec(Pat[5].pat, lp) == 0) {
					(void) strcpy((char *)s1, (char *)lp);
					break;
				}
			}
			lp = buf;
		}
	    /*
	     * Check for nroff command.
	     */
		if (cond) {
			cmdl = 0;
			if (cond && (*lp == '.' || *lp == '\'')) {
				if ((*cmd = *(lp+1)) != '\0') {
					cmdl++;
					if ((*(cmd+1) = *(lp+2)) == ' ')
						*(cmd+1) = '\0';
					else
						cmdl++;
				}
			}
			cmd[cmdl] = '\0';
		}
		if (cond == 0)
			i = i;		/* do nothing if condition is false */
		else if (cmdl == 0 || ((cmdx = Findmacro(cmd, 0)) < 0))
			Pass2(lp);
		else if (Sp >= MAXSP) {
			(void) sprintf((char *)buf, " macro nesting > %d",
				MAXSP);
			Error(WARN, LINE, (char *)buf, NULL);
		} else {
	    /*
	     * Stack macros.
	     */
		  /*
		   * Push stack.
		   */
			Sp++;
			Nleftstack[Sp] = nleft;
			Ptrstack[Sp] = ptr;
			Mxstack[Sp] = mx;
			Condstack[Sp] = cond;
			for (i = 10*Sp, j = 0; j < 10; i++, j++) {
				Argstack[i] = Args[j];
				Args[j] = NULL;
			}
		   /*
		    * Start new stack entry.
		    */
			mx = cmdx;
			ptr = Macrotab[mx].bx;
			cond = 0;
			nleft = Macrotab[mx].ct;
			Args[0] = Newstr(cmd);
		   /*
		    * Parse arguments.
		    */
			for (s1 = lp + cmdl + 1, nargs = 1; nargs < 10;) {
				while (*s1 && (*s1 == ' ' || *s1 == '\t'))
					s1++;
				if (*s1 == '\0')
					break;
				if (*s1 == '"') {
					s1++;
					quote = 1;
				} else
					quote = 0;
				for (s2 = buf;;) {
				    if (!quote && (*s1 == ' ' || *s1 == '\t')) {
					*s2 = '\0';
					break;
				    }
				    if ((*s2 = *s1) == '\0')
					break;
				    s1++;
				    if (quote && *s2 == '"') {
					*s2 = '\0';
					break;
				    }
				    s2++;
			    	}
				if (buf[0])
					Args[nargs++] = Newstr(buf);
			}
			for (i = nargs; i < 10; i++) {
				Args[i] = NULL;
			}
		}
	    /*
	     * Unstack completed macros.
	     */
		while (nleft <= 0 && Sp >= 0) {
			nleft = Nleftstack[Sp];
			mx = Mxstack[Sp];
			ptr = Ptrstack[Sp];
			cond = Condstack[Sp];
			for (i = 10*Sp, j = 0, nargs = -1; j < 10; i++, j++) {
				Free(&Args[j]);
				if ((Args[j] = Argstack[i]) != NULL)
					nargs = j;
			}
			Sp--;
			nargs++;
		}
	    /*
	     * Get next line.
	     */
		if (nleft > 0) {
			lp = Macrotxt[ptr++];
			nleft--;
		} else
			lp = (unsigned char *)"";
	}
	(void) sprintf((char *)buf, ".^# %d %s", NR, (char *)Inname);
	Pass2(buf);
}
