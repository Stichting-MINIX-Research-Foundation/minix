/* Copyright (c) 1985 Ceriel J.H. Jacobs */

# ifndef lint
static char rcsid[] = "$Header$";
# endif

# define _DISPLAY_

# include "in_all.h"
# include "display.h"
# include "assert.h"
# include "machine.h"
# include "term.h"
# include "output.h"
# include "options.h"
# include "process.h"
# include "getline.h"
# include "main.h"

STATIC char * do_line();

/*
 * Fill n lines of the screen, each with "str".
 */

STATIC VOID
fillscr(n,str) char *str; int n; {

	while (n-- > 0) {
		putline(str);
	}
}

/*
 * Skip "n" screenlines of line "p", and return what's left of it.
 */

STATIC char *
skiplines(p,n) char *p; int n; {

	while (n-- > 0) {
		p = do_line(p,0);
		scr_info.currentpos--;
	}
	return p;
}

/*
 * Redraw screen.
 * "n" = 1 if it is a real redraw, 0 if one page must be displayed.
 * It is also called when yap receives a stop signal.
 */

VOID
redraw(n) int n; {
	register struct scr_info *p = &scr_info;
	register int i;

	i = pagesize;
	if (n && p->currentpos) {
		i = p->currentpos;
	}
	(VOID) display(p->firstline,p->nf,i,1);
}

/*
 * Compute return value for the routines "display" and "scrollf".
 * This return value indicates wether we are at the end of file
 * or at the start, or both.
 * "s" contains that part of the last line that was not displayed.
 */

STATIC int
compretval(s) char *s; {
	register int i;
	register struct scr_info *p = &scr_info;

	i = 0;
	if (!s || (!*s && !getline(p->lastline+1, 1))) {
		i = EOFILE;
	}
	if (p->firstline == 1 && !p->nf) {
		i |= START;
	}
	status = i;
	return i;
}

/*
 * Display nlines, starting at line n, not displaying the first
 * nd screenlines of n.
 * If reallydispl = 0, the actual displaying is not performed,
 * only the computing associated with it is done.
 */

int
display(n,nd,nlines,reallydispl)
  long n; int nd; register int nlines; int reallydispl; {

	register struct scr_info *s = &scr_info;
	register char *p;	/* pointer to line to be displayed */

	if (startcomm)	{	/* No displaying on a command from the
				 * yap command line. In this case, displaying
				 * will be done after executing the command,
				 * by a redraw.
				 */
		reallydispl = 0;
	}
	if (!n) {
		n = 1L;
		nd = 0;
	}
	if (reallydispl) {	/* move cursor to starting point */
		if (stupid) {
			putline(currentfile);
			putline(", line ");
			prnum(n);
			nlines--;
		}
		if (cflag) {
			putline("\r\n");
		}
		else {
			home();
			clrscreen();
		}
	}
	/*
	 * Now, do computations and display
	 */
	s->currentpos = 0;
	s->nf = nd;
	s->head = s->tail;
	s->tail->cnt = 0;
	s->tail->line = n;
	p = skiplines(getline(n,1),nd);
	while (nlines && p) {
		/*
		 * While there is room,
		 * and there is something left to display ...
		 */
		(s->tail->cnt)++;
		nlines--;
		if (*(p = do_line(p,reallydispl)) == '\0') {
			/*
			 * File-line finished, get next one ...
			 */
			p = getline(++n,1);
			if (nlines && p) {
				s->tail = s->tail->next;
				s->tail->cnt = 0;
				s->tail->line = n;
			}
		}
	}
	if (!stupid) {
		s->currentpos += nlines;
		if (reallydispl) {
			fillscr(nlines, "~\r\n");
			fillscr(maxpagesize - s->currentpos, "\r\n");
		}
	}
	return compretval(p);
}

/*
 * Scroll forwards n lines.
 */

int
scrollf(n,reallydispl) int n; int reallydispl; {

	register struct scr_info *s = &scr_info;
	register char *p;
	register long ll;
	register int i;

	/*
	 * First, find out how many screenlines of the last line were already
	 * on the screen, and possibly above it.
	 */

	if (n <= 0 || (status & EOFILE)) return status;
	if (startcomm) reallydispl = 0;
	/*
	 * Find out where to begin displaying
	 */
	i = s->tail->cnt;
	if ((ll = s->lastline) == s->firstline) i += s->nf;
	p = skiplines(getline(ll, 1), i);
	/*
	 * Now, place the cursor at the first free line
	 */
	if (reallydispl && !stupid) {
		clrbline();
		mgoto(s->currentpos);
	}
	/*
	 * Now display lines, keeping track of which lines are on the screen.
	 */
	while (n-- > 0) {	/* There are still rows to be displayed */
		if (!*p) {	/* End of line, get next one */
			if (!(p = getline(++ll, 1))) {
				/*
				 * No lines left. At end of file
				 */
				break;
			}
			s->tail = s->tail->next;
			s->tail->cnt = 0;
			s->tail->line = ll;
		}
		if (s->currentpos >= maxpagesize) {
			/*
			 * No room, delete first screen-line
			 */
			s->currentpos--;
			s->nf++;
			if (--(s->head->cnt) == 0) {
				/*
				 * The first file-line on the screen is wiped
				 * out completely; update administration
				 * accordingly.
				 */
				s->nf = 0;
				s->head = s->head->next;
				assert(s->head->cnt > 0);
			}
		}
		s->tail->cnt++;
		p = do_line(p, reallydispl);
	}
	return compretval(p);
}

/*
 * Scroll back n lines
 */

int
scrollb(n, reallydispl) int n, reallydispl; {

	register struct scr_info *s = &scr_info;
	register char *p;	/* Holds string to be displayed */
	register int i;
	register int count;
	register long ln;	/* a line number */
	register int nodispl;
	int cannotscroll;	/* stupid or no insert-line */

	/*
	 * First, find out where to start
	 */
	if ((count = n) <= 0 || (status & START)) return status;
	if (startcomm) reallydispl = 0;
	cannotscroll = stupid || (!*AL && !*SR);
	ln = s->firstline;
	nodispl = s->nf;
	while (count) { /* While scrolling back ... */
		if (i = nodispl) {
			/*
			 * There were screen-lines of s->firstline that were not
			 * displayed.
			 * We can use them now, but only "count" of them.
			 */
			if (i > count) i = count;
			s->currentpos += i;
			nodispl -= i;
			count -= i;
		}
		else {	/* Get previous line */
			if (ln == 1) break; /* isn't there ... */
			p = getline(--ln, 1);
			/*
			 * Make it the first line of the screen and compute
			 * how many screenlines it takes. These lines are not
			 * displayed, but nodispl is set to this count, so
			 * that it will be nonzero next time around
			 */
			nodispl = 0;
			do {	/* Find out how many screenlines */
				nodispl++;
				p = skiplines(p, 1);
			} while (*p);
		}
	}
	n -= count;
	if ((i = s->currentpos) > maxpagesize) i = maxpagesize;
	if (reallydispl && hardcopy) i = n;
	/*
	 * Now that we know where to start, we can use "display" to do the
	 * rest of the computing for us, and maybe even the displaying ...
	 */
	i = display(ln,
		    nodispl,
		    i,
		    reallydispl && cannotscroll);
	if (cannotscroll || !reallydispl) {
		/*
		 * Yes, "display" did the displaying, or we did'nt have to
		 * display at all.
		 * I like it, but the user obviously does not.
		 * Let him buy another (smarter) terminal ...
		 */
		return i;
	}
	/*
	 * Now, all we have to do is the displaying. And we are dealing with
	 * a smart terminal (it can insert lines or scroll back).
	 */
	home();
	/*
	 * Insert lines all at once
	 */
	for (i = n; i; i--) {
		if (DB && *CE) {
			/*
			 * Grumble..., terminal retains lines below, so we have
			 * to clear the lines that we push off the screen
			 */
			clrbline();
			home();
		}
		if (*SR) {
			scrollreverse();
		}
		else {
# ifdef VT100_PATCH
			insert_line(0);
# else
			insert_line();
# endif
		}
	}
	p = skiplines(getline(ln = s->firstline, 1), s->nf);
	for (i = 0; i < n; i++) {
		p = do_line(p,1);
		s->currentpos--;
		if (!*p) {
			p = getline(++ln, 1);
		}
	}
	return count;
}

/*
 * Process a line.
 * If reallydispl > 0 then display it.
 */

STATIC char *
do_line(str, reallydispl) register char *str; int reallydispl; {

	char buf[1024];
	register char *p = buf;
	register int pos = COLS;
	register int c;
	register int c1;
	register int do_ul = 0, do_hl = 0;
	int lastmode = 0, lasthlmode = 0;
	int c2;

	while (*str && pos > 0) {
		if (*str < ' ' && (c1 = match(str,&c2,sppat)) > 0) {
			/*
			 * We found a string that matches, and thus must be
			 * echoed literally
			 */
			if ((pos - c2) <= 0) {
				/*
				 * It did not fit
				 */
				break;
			}
			pos -= c2;
			str += c1;
			if (reallydispl) {
				c = *str;
				*p = *str = 0;
				cputline(p = buf);
				putline(str - c1);
				*str = c;
			}
			continue;
		}
		c = *str++;
		do_hl = 0;
		if (*str == '\b' && *(str+1) != 0
					&& (c != '_' || *(str+2) == '\b')) {
			while (*str == '\b' && *(str+1) != 0) {
				str++;
				c = *str++;
				do_hl = 1;
			}
		}
		do_ul = 1;
		/*
		 * Find underline sequences ...
		 */
		if (c == '_' && *str == '\b') {
			str++;
			c = *str++;
		}
		else {
			if (*str == '\b' && *(str+1) == '_') {
				str += 2;
			}
			else	do_ul = 0;
		}
		if (reallydispl && do_hl != lasthlmode) {
			*p = 0;
			cputline(p = buf);
			if (do_hl) bold();
			else end_bold();
		}
		lasthlmode = do_hl;
		if (reallydispl && do_ul != lastmode) {
			*p = 0;
			cputline(p = buf);
			if (do_ul) underline();
			else end_underline();
		}
		lastmode = do_ul;
		*p++ = c;
		if (c >= ' ' && c < 0177) {
			pos--;
			if (reallydispl && do_ul && *UC && pos > 0) {
				/*
				 * Underlining apparently is done one
				 * character at a time.
				 */
				*p = 0;
				cputline(p = buf);
				backspace();
				underchar();
			}
			continue;
		}
		if (c == '\t') {
			p--;
			c1 = 8 - ((COLS - pos) & 07);
			/*
			 * Actually, if COLS is a multiple of 8, this can be
			 * simplified to
			 *     c1 = pos & 07;
			 * But of course, we don't know that for sure.
			 */
			if (pos - c1 < 0) break;
			pos -= c1;
			if (reallydispl) {
				if (expandtabs) {
					/*
					 * Expand tabs. We cannot let the
					 * kernel take care of this
					 * for two reasons:
					 * 1. There can be tabs in cursor
					 *    addressing strings,
					 * 2. We probably do it better.
					 */
					while (c1-- > 0) {
						*p++ = ' ';
					}
				}
				else {
					*p = 0;
					cputline(p = buf);
					givetab();
				}
			}
			continue;
		}
		/*
		 * Now we have a control character, which takes two positions
		 */
		if (pos <= 1) {
			p--;
			break;
		}
		pos -= 2;
	}
	if (reallydispl) {
		*p = 0;
		cputline(buf);
		if (pos > 0 || (pos <= 0 && (!AM || XN))) {
			putline("\r\n");
		}
		/*
		 * The next should be here! I.e. it may not be before printing
		 * the newline. This has to do with XN. We don't know exactly
		 * WHEN the terminal will stop ignoring the newline.
		 * I have for example a terminal (Ampex a230) that will
		 * continue to ignore the newline after a clear to end of line
		 * sequence, but not after an end_underline sequence.
		 */
		if (do_ul) {
			end_underline();
		}
		if (do_hl) {
			standend();
		}
	}
	scr_info.currentpos++;
	return str;
}

/* ARGSUSED */
int
setmark(cnt) long cnt; {	/* Set a mark on the current page */
	register struct scr_info *p = &scr_info;

	p->savfirst = p->firstline;
	p->savnf = p->nf;
}

/* ARGSUSED */
int
tomark(cnt) long cnt; {		/* Go to the mark */
	register struct scr_info *p = &scr_info;

	(VOID) display(p->savfirst,p->savnf,pagesize,1);
}

/* ARGSUSED */
int
exgmark(cnt) long cnt; {	/* Exchange mark and current page */
	register struct scr_info *p = &scr_info;
	register long svfirst;
	register int svnf;

	svfirst = p->firstline;
	svnf = p->nf;
	tomark(0L);
	p->savfirst = svfirst;
	p->savnf = svnf;
}

VOID
d_clean() {			/* Clean up */
	register struct scr_info *p = &scr_info;

	p->savnf = 0;
	p->savfirst = 0;
	p->head = p->tail;
	p->head->line = 0;
	p->currentpos = 0;
}
