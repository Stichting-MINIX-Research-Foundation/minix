/* Copyright (c) 1985 Ceriel J.H. Jacobs */

# ifndef lint
static char rcsid[] = "$Header$";
# endif

# define _PROMPT_

# include "in_all.h"
# include "prompt.h"
# include "term.h"
# include "output.h"
# include "options.h"
# include "display.h"
# include "process.h"
# include "getline.h"
# include "main.h"
# include "getcomm.h"
# include "keys.h"
# include "assert.h"
# include "commands.h"

#define basename(x) x

#ifndef basename
STATIC char *	basename();
#endif

static char *errorgiven;	/* Set to error message, if there is one */

char *
copy(p, ep, s)
	register char *p, *s;
	char *ep;
{
	while (p < ep && *s) {
		*p++ = *s++;
	}
	return p;
}

/*
 * display the prompt and refresh the screen.
 */

VOID
give_prompt() {

	register char **name;
	register struct scr_info *p = &scr_info;
	char buf[256];
	register char *pb = buf;

	if (startcomm) return;
	flush();
	if (window()) {
		redraw(0);
		flush();
	}
	if (!stupid) {
		/*
		 * fancy prompt
		 */
		clrbline();
		standout();
		pb = copy(pb, &buf[255], basename(currentfile));
		if (stdf >= 0) {
			pb = copy(pb, &buf[255], ", ");
			pb = copy(pb, &buf[255], getnum(p->firstline));
			pb = copy(pb, &buf[255], "-");
			pb = copy(pb, &buf[255], getnum(p->lastline));
		}
	}
	else {
		*pb++ = '\007';	/* Stupid terminal, stupid prompt */
	}
	if (errorgiven) {
		/*
		 * display error message
		 */
		pb = copy(pb, &buf[255], " ");
		pb = copy(pb, &buf[255], errorgiven);
		if (stupid) {
			pb = copy(pb, &buf[255], "\r\n");
		}
		errorgiven = 0;
	}
	else if (!stupid && (status || maxpos)) {
		pb = copy(pb, &buf[255], " (");
		name = &filenames[filecount];
		if (status) {
			/*
			 * indicate top and/or bottom
			 */
			if (status & START) {
				if (!*(name - 1)) {
					pb = copy(pb, &buf[255], "Top");
				}
				else {
					pb = copy(pb, &buf[255], "Previous: ");
					pb = copy(pb, &buf[255], basename(*(name - 1)));
				}
				if (status & EOFILE) {
					pb = copy(pb, &buf[255], ", ");
				}
			}
			if (status & EOFILE) {
				if (!*(name+1)) {
					pb = copy(pb, &buf[255], "Bottom");
				}
				else {
					pb = copy(pb, &buf[255], "Next: ");
					pb = copy(pb, &buf[255], basename(*(name + 1)));
				}
			}
		}
		else {	/* display percentage */
			pb = copy(pb, &buf[255], getnum((100 * getpos(p->lastline))/maxpos));
			pb = copy(pb, &buf[255], "%");
		}
		pb = copy(pb, &buf[255], ")");
	}
	*pb = '\0';
	if (!stupid) {
		buf[COLS-1] = 0;
		putline(buf);
		standend();
	}
	else	putline(buf);
}

/*
 * Remember error message
 */

VOID
error(str) char *str; {

	errorgiven = str;
}

#ifndef basename
STATIC char *
basename(fn) char *fn; {	/* Return name without path */

	register char *s;

	s = fn;
	while (*s++) ;		/* Search end of name */
	for (;;) {
		if (*--s == '/') {
			/*
			 * Backwards to first '/'
			 */
			if (*(s+1)) {
				/*
				 * There is a name after the '/'
				 */
				return s + 1;
			}
			*s = 0; /* No name after the '/' */
		}
		if (s == fn) return s;
	}
	/* NOTREACHED */
}
#endif

VOID
ret_to_continue() {		/* Obvious */
	int c;
	static char buf[2];

	for (;;) {
		clrbline();
		standout();
		if (errorgiven) {
			putline(errorgiven);
			putline(" ");
			errorgiven = 0;
		}
		putline("[Type anything to continue]");
		standend();
		if (is_escape(c = getch())) {
			buf[0] = c;
			(VOID) match(buf, &c, currmap->k_mach);
			assert(c > 0);
			do_comm(c, -1L);
		}
		else	break;
	}
	clrbline();
}
