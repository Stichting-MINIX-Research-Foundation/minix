/* sedexec.c -- axecute compiled form of stream editor commands
   Copyright (C) 1995-2003 Eric S. Raymond
   Copyright (C) 2004-2006 Rene Rebe

   The single entry point of this module is the function execute(). It
may take a string argument (the name of a file to be used as text)  or
the argument NULL which tells it to filter standard input. It executes
the compiled commands in cmds[] on each line in turn.
   The function command() does most of the work.  match() and advance()
are used for matching text against precompiled regular expressions and
dosub() does right-hand-side substitution.  Getline() does text input;
readout() and memcmp() are output and string-comparison utilities.  
*/

#include <stdlib.h>	/* exit */
#include <stdio.h>	/* {f}puts, {f}printf, getc/putc, f{re}open, fclose */
#include <ctype.h>	/* for isprint(), isdigit(), toascii() macros */
#include <string.h>	/* for memcmp(3) */
#include "sed.h"	/* command type structures & miscellaneous constants */

/***** shared variables imported from the main ******/

/* main data areas */
extern char	linebuf[];	/* current-line buffer */
extern sedcmd	cmds[];		/* hold compiled commands */
extern long	linenum[];	/* numeric-addresses table */

/* miscellaneous shared variables */
extern int	nflag;		/* -n option flag */
extern int	eargc;		/* scratch copy of argument count */
extern sedcmd	*pending;	/* ptr to command waiting to be executed */

extern int	last_line_used; /* last line address ($) used */

/***** end of imported stuff *****/

#define MAXHOLD		MAXBUF	/* size of the hold space */
#define GENSIZ		MAXBUF	/* maximum genbuf size */

static char LTLMSG[]	= "sed: line too long\n";

static char	*spend;		/* current end-of-line-buffer pointer */
static long	lnum = 0L;	/* current source line number */

/* append buffer maintenance */
static sedcmd	*appends[MAXAPPENDS];	/* array of ptrs to a,i,c commands */
static sedcmd	**aptr = appends;	/* ptr to current append */

/* genbuf and its pointers */
static char	genbuf[GENSIZ];
static char	*loc1;
static char	*loc2;
static char	*locs;

/* command-logic flags */
static int	lastline;		/* do-line flag */
static int	line_with_newline;	/* line had newline */
static int	jump;			/* jump to cmd's link address if set */
static int	delete;			/* delete command flag */
static int	needs_advance;		/* needs inc after substitution */
					/* ugly HACK - neds REWORK */

/* tagged-pattern tracking */
static char	*bracend[MAXTAGS];	/* tagged pattern start pointers */
static char	*brastart[MAXTAGS];	/* tagged pattern end pointers */

/* prototypes */
static char *getline(char *buf, int max);
static char *place(char* asp, char* al1, char* al2);
static int advance(char* lp, char* ep, char** eob);
static int match(char *expbuf, int gf);
static int selected(sedcmd *ipc);
static int substitute(sedcmd *ipc);
static void command(sedcmd *ipc);
static void dosub(char *rhsbuf);
static void dumpto(char *p1, FILE *fp);
static void listto(char *p1, FILE *fp);
static void readout(void);
static void truncated(int h);

/* execute the compiled commands in cmds[] on a file
   file:  name of text source file to be filtered */
void execute(char* file)
{
	register sedcmd		*ipc;		/* ptr to current command */
	char			*execp;		/* ptr to source */

	if (file != NULL)	/* filter text from a named file */ 
		if (freopen(file, "r", stdin) == NULL)
			fprintf(stderr, "sed: can't open %s\n", file);

	if (pending)		/* there's a command waiting */
	{
		ipc = pending;		/* it will be first executed */
		pending = FALSE;	/*   turn off the waiting flag */
		goto doit;		/*   go to execute it immediately */
	}

	/* here's the main command-execution loop */
	for(;;)
	{
		/* get next line to filter */
		if ((execp = getline(linebuf, MAXBUF+1)) == BAD)
			return;
		spend = execp;

		/* loop through compiled commands, executing them */
		for(ipc = cmds; ipc->command; )
		{
			/* address command to select? - If not address
			   but allbut then invert, that is skip, the commmand */
			if (ipc->addr1 || ipc->flags.allbut) {
				if (!ipc->addr1 || !selected(ipc)) {
					ipc++;	/* not selected, next cmd */
					continue;
				}
			}
	doit:
			command(ipc);	/* execute the command pointed at */

			if (delete)	/* if delete flag is set */
				break;	/* don't exec rest of compiled cmds */

			if (jump)	/* if jump set, follow cmd's link */
			{
				jump = FALSE;
				if ((ipc = ipc->u.link) == 0)
				{
					ipc = cmds;
					break;
				}
			}
			else		/* normal goto next command */
				ipc++;
		}
		/* we've now done all modification commands on the line */

		/* here's where the transformed line is output */
		if (!nflag && !delete)
		{
			fwrite(linebuf, spend - linebuf, 1, stdout);
			if (line_with_newline)
				putc('\n', stdout);
		}

		/* if we've been set up for append, emit the text from it */
		if (aptr > appends)
			readout();

		delete = FALSE;	/* clear delete flag; about to get next cmd */
	}
}

/* is current command selected */
static int selected(sedcmd *ipc)
{
	register char	*p1 = ipc->addr1;	/* point p1 at first address */
	register char	*p2 = ipc->addr2;	/*   and p2 at second */
	unsigned char	c;
	int selected = FALSE;

	if (ipc->flags.inrange)
	{
		selected = TRUE;
		if (*p2 == CEND)
			;
		else if (*p2 == CLNUM)
		{
			c = p2[1];
			if (lnum >= linenum[c])
				ipc->flags.inrange = FALSE;
		}
		else if (match(p2, 0))
			ipc->flags.inrange = FALSE;
	}
	else if (*p1 == CEND)
	{
		if (lastline)
			selected = TRUE;
	}
	else if (*p1 == CLNUM)
	{
		c = p1[1];
		if (lnum == linenum[c]) {
			selected = TRUE;
			if (p2)
				ipc->flags.inrange = TRUE;
		}
	}
	else if (match(p1, 0))
	{
		selected = TRUE;
		if (p2)
			ipc->flags.inrange = TRUE;
	}
	return ipc->flags.allbut ? !selected : selected;
}

/* match RE at expbuf against linebuf; if gf set, copy linebuf from genbuf */
static int match(char *expbuf, int gf)	/* uses genbuf */
{
	char *p1, *p2, c;

	if (gf)
	{
		if (*expbuf)
			return(FALSE);
		p1 = linebuf; p2 = genbuf;
		while ((*p1++ = *p2++));
		if (needs_advance) {
			loc2++;
		}
		locs = p1 = loc2;
	}
	else
	{
		p1 = linebuf + needs_advance;
		locs = FALSE;
	}
	needs_advance = 0;

	p2 = expbuf;
	if (*p2++)
	{
		loc1 = p1;
		if(*p2 == CCHR && p2[1] != *p1)	/* 1st char is wrong */
			return(FALSE);		/*   so fail */
		return(advance(p1, p2, NULL));	/* else try to match rest */
	}

	/* quick check for 1st character if it's literal */
	if (*p2 == CCHR)
	{
		c = p2[1];		/* pull out character to search for */
		do {
			if (*p1 != c)
				continue;	/* scan the source string */
			if (advance(p1, p2,NULL)) /* found it, match the rest */
				return(loc1 = p1, 1);
		} while
			(*p1++);
		return(FALSE);		/* didn't find that first char */
	}

	/* else try for unanchored match of the pattern */
	do {
		if (advance(p1, p2, NULL))
			return(loc1 = p1, 1);
	} while
		(*p1++);

	/* if got here, didn't match either way */
	return(FALSE);
}

/* attempt to advance match pointer by one pattern element
   lp:	source (linebuf) ptr
   ep:	regular expression element ptr */
static int advance(char* lp, char* ep, char** eob)
{
	char	*curlp;		/* save ptr for closures */ 
	char	c;		/* scratch character holder */
	char	*bbeg;
	int	ct;
	signed int	bcount = -1;

	for (;;)
		switch (*ep++)
		{
		case CCHR:		/* literal character */
			if (*ep++ == *lp++)	/* if chars are equal */
				continue;	/* matched */
			return(FALSE);		/* else return false */

		case CDOT:		/* anything but newline */
			if (*lp++)		/* first NUL is at EOL */
				continue;	/* keep going if didn't find */
			return(FALSE);		/* else return false */

		case CNL:		/* start-of-line */
		case CDOL:		/* end-of-line */
			if (*lp == 0)		/* found that first NUL? */
				continue;	/* yes, keep going */
			return(FALSE);		/* else return false */

		case CEOF:		/* end-of-address mark */
			loc2 = lp;		/* set second loc */
			return(TRUE);		/* return true */

		case CCL:		/* a closure */
			c = *lp++ & 0177;
			if (ep[c>>3] & bits(c & 07))	/* is char in set? */
			{
				ep += 16;	/* then skip rest of bitmask */
				continue;	/*   and keep going */
			}
			return(FALSE);		/* else return false */

		case CBRA:		/* start of tagged pattern */
			brastart[(unsigned char)*ep++] = lp;	/* mark it */
			continue;		/* and go */

		case CKET:		/* end of tagged pattern */
			bcount = *ep;
			if (eob) {
				*eob = lp;
				return (TRUE);
			}
			else
				bracend[(unsigned char)*ep++] = lp;    /* mark it */
			continue;		/* and go */

		case CBACK:		/* match back reference */
			bbeg = brastart[(unsigned char)*ep];
			ct = bracend[(unsigned char)*ep++] - bbeg;

			if (memcmp(bbeg, lp, ct) == 0)
			{
				lp += ct;
				continue;
			}
			return(FALSE);

		case CBRA|STAR:		/* \(...\)* */
		{
			char *lastlp;
			curlp = lp;

			if (*ep > bcount)
				brastart[(unsigned char)*ep] = bracend[(unsigned char)*ep] = lp;

			while (advance(lastlp=lp, ep+1, &lp)) {
				if (*ep > bcount && lp != lastlp) {
					bracend[(unsigned char)*ep] = lp;    /* mark it */
					brastart[(unsigned char)*ep] = lastlp;
				}
				if (lp == lastlp) break;
			}
			ep++;

			/* FIXME: scan for the brace end */
			while (*ep != CKET)
				ep++;
			ep+=2;

			needs_advance = 1;
			if (lp == curlp) /* 0 matches */
				continue;
			lp++; 
			goto star;
		}
		case CBACK|STAR:	/* \n* */
			bbeg = brastart[(unsigned char)*ep];
			ct = bracend[(unsigned char)*ep++] - bbeg;
			curlp = lp;
			while(memcmp(bbeg, lp, ct) == 0)
				lp += ct;

			while(lp >= curlp)
			{
				if (advance(lp, ep, eob))
					return(TRUE);
				lp -= ct;
			}
			return(FALSE);

		case CDOT|STAR:		/* match .* */
			curlp = lp;		/* save closure start loc */
			while (*lp++);		/* match anything */ 
			goto star;		/* now look for followers */

		case CCHR|STAR:		/* match <literal char>* */
			curlp = lp;		/* save closure start loc */
			while (*lp++ == *ep);	/* match many of that char */
			ep++;			/* to start of next element */
			goto star;		/* match it and followers */

		case CCL|STAR:		/* match [...]* */
			curlp = lp;		/* save closure start loc */
			do {
				c = *lp++ & 0x7F;	/* match any in set */
			} while
				(ep[c>>3] & bits(c & 07));
			ep += 16;		/* skip past the set */
			goto star;		/* match followers */

		star:		/* the recursion part of a * or + match */
			needs_advance = 1;
			if (--lp == curlp) {	/* 0 matches */
				continue;
			}
#if 0
			if (*ep == CCHR)
			{
				c = ep[1];
				do {
					if (*lp != c)
						continue;
					if (advance(lp, ep, eob))
						return(TRUE);
				} while
				(lp-- > curlp);
				return(FALSE);
			}

			if (*ep == CBACK)
			{
				c = *(brastart[ep[1]]);
				do {
					if (*lp != c)
						continue;
					if (advance(lp, ep, eob))
						return(TRUE);
				} while
					(lp-- > curlp);
				return(FALSE);
			}
#endif
			/* match followers, try shorter match, if needed */
			do {
				if (lp == locs)
					break;
				if (advance(lp, ep, eob))
					return(TRUE);
			} while
				(lp-- > curlp);
			return(FALSE);

		default:
			fprintf(stderr, "sed: internal RE error, %o\n", *--ep);
			exit (2);
		}
}

/* perform s command
   ipc:	ptr to s command struct */
static int substitute(sedcmd *ipc)
{
	unsigned int n = 1;
	/* find a match */
	/* the needs_advance code got a bit tricky - might needs a clean
	   refactoring */
	while (match(ipc->u.lhs, 0)) {
		/* nth 0 is implied 1 */
		if (!ipc->nth || n == ipc->nth) {
			dosub(ipc->rhs);		/* perform it once */
			n++;				/* mark for return */
			break;
		}
		needs_advance = n++;
	}
	if (n == 1)
		return(FALSE);			/* command fails */

	if (ipc->flags.global)			/* if global flag enabled */
		do {				/* cycle through possibles */
			if (match(ipc->u.lhs, 1)) {	/* found another */
				dosub(ipc->rhs);	/* so substitute */
			}
			else				/* otherwise, */
				break;			/* we're done */
		} while (*loc2);
	return(TRUE);				/* we succeeded */
}

/* generate substituted right-hand side (of s command)
   rhsbuf:	where to put the result */
static void dosub(char *rhsbuf)		/* uses linebuf, genbuf, spend */
{
	char	*lp, *sp, *rp;
	int	c;

	/* copy linebuf to genbuf up to location 1 */
	lp = linebuf; sp = genbuf;
	while (lp < loc1) *sp++ = *lp++;

	for (rp = rhsbuf; (c = *rp++); )
	{
		if (c & 0200 && (c & 0177) == '0')
		{
			sp = place(sp, loc1, loc2);
			continue;
		}
		else if (c & 0200 && (c &= 0177) >= '1' && c < MAXTAGS+'1')
		{
			sp = place(sp, brastart[c-'1'], bracend[c-'1']);
			continue;
		}
		*sp++ = c & 0177;
		if (sp >= genbuf + MAXBUF)
			fprintf(stderr, LTLMSG);

	}
	lp = loc2;
	loc2 = sp - genbuf + linebuf;
	while ((*sp++ = *lp++))
		if (sp >= genbuf + MAXBUF)
			fprintf(stderr, LTLMSG);
	lp = linebuf; sp = genbuf;
	while ((*lp++ = *sp++));
	spend = lp-1;
}

/* place chars at *al1...*(al1 - 1) at asp... in genbuf[] */
static char *place(char* asp, char* al1, char* al2)		/* uses genbuf */
{
	while (al1 < al2)
	{
		*asp++ = *al1++;
		if (asp >= genbuf + MAXBUF)
			fprintf(stderr, LTLMSG);
	}
	return(asp);
}

/* list the pattern space in  visually unambiguous form *p1... to fp
   p1: the source
   fp: output stream to write to */
static void listto(char *p1, FILE *fp)
{
	for (; p1<spend; p1++)
		if (isprint(*p1))
			putc(*p1, fp);		/* pass it through */
		else
		{
			putc('\\', fp);		/* emit a backslash */
			switch(*p1)
			{
			case '\b':	putc('b', fp); break;	/* BS */
			case '\t':	putc('t', fp); break;	/* TAB */
			case '\n':	putc('n', fp); break;	/* NL */
			case '\r':	putc('r', fp); break;	/* CR */
			case '\033':	putc('e', fp); break;	/* ESC */
			default:	fprintf(fp, "%02x", *p1);
			}
		}
	putc('\n', fp);
}

/* write a hex dump expansion of *p1... to fp
   p1: source
   fp: output */
static void dumpto(char *p1, FILE *fp)
{
	for (; p1<spend; p1++)
		fprintf(fp, "%02x", *p1);
	fprintf(fp, "%02x", '\n');
	putc('\n', fp);
}

static void truncated(int h)
{
	static long last = 0L;

	if (lnum == last) return;
	last = lnum;

	fprintf(stderr, "sed: ");
	fprintf(stderr, h ? "hold space" : "line %ld", lnum);
	fprintf(stderr, " truncated to %d characters\n", MAXBUF);
}

/* execute compiled command pointed at by ipc */
static void command(sedcmd *ipc)
{
	static int	didsub;			/* true if last s succeeded */
	static char	holdsp[MAXHOLD];	/* the hold space */
	static char	*hspend = holdsp;	/* hold space end pointer */
	register char	*p1, *p2;
	char		*execp;

	needs_advance = 0;
	switch(ipc->command)
	{
	case ACMD:		/* append */
		*aptr++ = ipc;
		if (aptr >= appends + MAXAPPENDS)
			fprintf(stderr,
				"sed: too many appends after line %ld\n",
				lnum);
		*aptr = 0;
		break;

	case CCMD:		/* change pattern space */
		delete = TRUE;
		if (!ipc->flags.inrange || lastline)
			printf("%s\n", ipc->u.lhs);		
		break;

	case DCMD:		/* delete pattern space */
		delete++;
		break;

	case CDCMD:		/* delete a line in hold space */
		p1 = p2 = linebuf;
		while(*p1 != '\n')
			if ((delete = (*p1++ == 0)))
				return;
		p1++;
		while((*p2++ = *p1++)) continue;
		spend = p2-1;
		jump++;
		break;

	case EQCMD:		/* show current line number */
		fprintf(stdout, "%ld\n", lnum);
		break;

	case GCMD:		/* copy hold space to pattern space */
		p1 = linebuf;	p2 = holdsp;	while((*p1++ = *p2++));
		spend = p1-1;
		break;

	case CGCMD:		/* append hold space to pattern space */
		*spend++ = '\n';
		p1 = spend;	p2 = holdsp;
		do {
			if (p1 > linebuf + MAXBUF) {
				truncated(FALSE);
				p1[-1] = 0;
  				break;
			}
		} while((*p1++ = *p2++));

		spend = p1-1;
		break;

	case HCMD:		/* copy pattern space to hold space */
		p1 = holdsp;	p2 = linebuf;	while((*p1++ = *p2++));
		hspend = p1-1;
		break;

	case CHCMD:		/* append pattern space to hold space */
		*hspend++ = '\n';
		p1 = hspend;	p2 = linebuf;
		do {
			if (p1 > holdsp + MAXBUF) {
				truncated(TRUE);
				p1[-1] = 0;
  				break;
			}
		} while((*p1++ = *p2++));

		hspend = p1-1;
		break;

	case ICMD:		/* insert text */
		printf("%s\n", ipc->u.lhs);
		break;

	case BCMD:		/* branch to label */
		jump = TRUE;
		break;

	case LCMD:		/* list text */
		listto(linebuf, (ipc->fout != NULL)?ipc->fout:stdout); break;

	case CLCMD:		/* dump text */
		dumpto(linebuf, (ipc->fout != NULL)?ipc->fout:stdout); break;

	case NCMD:		/* read next line into pattern space */
		if (!nflag)
			puts(linebuf);	/* flush out the current line */
		if (aptr > appends)
			readout();	/* do pending a, r commands */
		if ((execp = getline(linebuf, MAXBUF+1)) == BAD)
		{
			pending = ipc;
			delete = TRUE;
			break;
		}
		spend = execp;
		break;

	case CNCMD:		/* append next line to pattern space */
		if (aptr > appends)
			readout();
		*spend++ = '\n';
		if ((execp = getline(spend,
		                     linebuf + MAXBUF+1 - spend)) == BAD)
		{
			pending = ipc;
			delete = TRUE;
			break;
		}
		spend = execp;
		break;

	case PCMD:		/* print pattern space */
		puts(linebuf);
		break;

	case CPCMD:		/* print one line from pattern space */
		cpcom:		/* so s command can jump here */
		for(p1 = linebuf; *p1 != '\n' && *p1 != '\0'; )
			putc(*p1++, stdout);
		putc('\n', stdout);
		break;

	case QCMD:		/* quit the stream editor */
		if (!nflag)
			puts(linebuf);	/* flush out the current line */
		if (aptr > appends)
			readout();	/* do any pending a and r commands */
		exit(0);

	case RCMD:		/* read a file into the stream */
		*aptr++ = ipc;
		if (aptr >= appends + MAXAPPENDS)
			fprintf(stderr,
				"sed: too many reads after line %ld\n",
				lnum);
		*aptr = 0;
		break;

	case SCMD:		/* substitute RE */
		didsub = substitute(ipc);
		if (ipc->flags.print && didsub)
		{
			if (ipc->flags.print == TRUE)
				puts(linebuf);
			else
				goto cpcom;
		}
		if (didsub && ipc->fout)
			fprintf(ipc->fout, "%s\n", linebuf);
		break;

	case TCMD:		/* branch on last s successful */
	case CTCMD:		/* branch on last s failed */
		if (didsub == (ipc->command == CTCMD))
			break;		/* no branch if last s failed, else */
		didsub = FALSE;
		jump = TRUE;		/*  set up to jump to assoc'd label */
		break;

	case CWCMD:		/* write one line from pattern space */
		for(p1 = linebuf; *p1 != '\n' && *p1 != '\0'; )
			putc(*p1++, ipc->fout);
		putc('\n', ipc->fout);
		break;

	case WCMD:		/* write pattern space to file */
		fprintf(ipc->fout, "%s\n", linebuf);
		break;

	case XCMD:		/* exchange pattern and hold spaces */
		p1 = linebuf;	p2 = genbuf;	while((*p2++ = *p1++)) continue;
		p1 = holdsp;	p2 = linebuf;	while((*p2++ = *p1++)) continue;
		spend = p2 - 1;
		p1 = genbuf;	p2 = holdsp;	while((*p2++ = *p1++)) continue;
		hspend = p2 - 1;
		break;

	case YCMD:
		p1 = linebuf;	p2 = ipc->u.lhs;
		while((*p1 = p2[(unsigned char)*p1]))
			p1++;
		break;
	}
}

/* get next line of text to be filtered
   buf: where to send the input
   max: max chars to read */
static char *getline(char *buf, int max)
{
	if (fgets(buf, max, stdin) != NULL)
	{
		int c;

		lnum++;			/* note that we got another line */
		/* find the end of the input and overwrite a possible '\n' */
		while (*buf != '\n' && *buf != 0)
		    buf++;
		line_with_newline = *buf == '\n';
		*buf=0;

		/* detect last line - but only if the address was used in a command */
		if  (last_line_used) {
		  if ((c = fgetc(stdin)) != EOF)
			ungetc (c, stdin);
		  else {
			if (eargc == 0)		/* if no more args */
				lastline = TRUE;	/* set a flag */
		  }
		}

		return(buf);		/* return ptr to terminating null */ 
	}
	else
	{
		return(BAD);
	}
}

/* write file indicated by r command to output */
static void readout(void)
{
	register int	t;	/* hold input char or EOF */
	FILE		*fi;	/* ptr to file to be read */

	aptr = appends - 1;	/* arrange for pre-increment to work right */
	while(*++aptr)
		if ((*aptr)->command == ACMD)		/* process "a" cmd */
			printf("%s\n", (*aptr)->u.lhs);
		else					/* process "r" cmd */
		{
			if ((fi = fopen((*aptr)->u.lhs, "r")) == NULL)
				continue;
			while((t = getc(fi)) != EOF)
				putc((char) t, stdout);
			fclose(fi);
		}
	aptr = appends;		/* reset the append ptr */
	*aptr = 0;
}

/* sedexec.c ends here */
