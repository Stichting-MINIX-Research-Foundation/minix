/* Copyright (c) 1985 Ceriel J.H. Jacobs */

# ifndef lint
static char rcsid[] = "$Header$";
# endif

# define _COMMANDS_

# include "in_all.h"
# include "commands.h"
# include "output.h"
# include "process.h"
# include "help.h"
# include "term.h"
# include "prompt.h"
# include "getline.h"
# include "getcomm.h"
# include "pattern.h"
# include "display.h"
# include "options.h"
# include "machine.h"
# include "keys.h"
# include "main.h"
# include "assert.h"
# if USG_OPEN
# include <fcntl.h>
# include <errno.h>
extern int errno;
# endif
# if BSD4_2_OPEN
# include <sys/file.h>
# include <errno.h>
extern int errno;
# endif
# if POSIX_OPEN
# include <sys/types.h>
# include <fcntl.h>
# include <errno.h>
# endif

char	*strcpy(), *strcat();

static long lastcount;		/* Save last count for '.' command */
static int lastcomm;		/* Save last command for '.' command */

/*ARGSUSED*/
STATIC int
do_nocomm(cnt) long cnt; {	/* Do nothing */
}

/*ARGSUSED*/
int
do_chkm(cnt) long cnt; {	/* Change key map */
	register struct keymap *p;

	if (!(p = othermap)) {
		error("No other keymap");
		return;
	}
	othermap = currmap;
	currmap = p;
}

static int searchdir;		/* Direction of last search */

/*
 * Perform searches
 */

STATIC VOID
do_search(str,cnt,dir) char *str; long cnt; int dir; {
	register char *p;
	register long lineno;

	if (str) {
		/*
		 * We have to get a pattern, which we have to prompt for
		 * with the string "str".
		 */
		if ((p = readline(str)) == 0) {
			/*
			 * User cancelled command
			 */
			return;
		}
		if ((p = re_comp(p))) {
			/*
			 * There was an error in the pattern
			 */
			error(p);
			return;
		}
		searchdir = dir;
	}
	if (dir < 0) lineno = scr_info.firstline;
	else lineno = scr_info.lastline;
	for (;;) {
		p = 0;
		if ((lineno += dir) > 0) p = getline(lineno, 0);
		if (interrupt) return;
		if (!p) {	/* End of file reached */
			error("pattern not found");
			return;
		}
		if (re_exec(p) && --cnt <= 0) {
			/*
			 * We found the pattern, and we found it often enough.
			 * Pity that we still don't know where the match is.
			 * We only know the linenumber. So, we just hope the
			 * following will at least bring it on the screen ...
			 */
			(VOID) display(lineno,0,pagesize,0);
			(VOID) scrollb(2,0);
			redraw(0);
			return;
		}
	}
	/* NOTREACHED */
}

STATIC int
do_fsearch(cnt) long cnt; {	/* Forward search */

	do_search("/", cnt, 1);
}

STATIC int
do_bsearch(cnt) long cnt; {	/* Backward search */

	do_search("?", cnt, -1);
}

/*
 * Repeat last search in direction "dir"
 */

STATIC int
n_or_rn_search(cnt,dir) long cnt; int dir; {
	register char *p;

	if (dir == 1) {
		p = "/\r";
	}
	else if (dir == -1) {
		p = "?\r";
	}
	else {
		error("No previous pattern");
		return;
	}
	if (!stupid) clrbline();
	putline(p);
	flush();
	do_search((char *) 0, cnt, dir);
}

STATIC int
do_nsearch(cnt) long cnt; {	/* Repeat search in same direction */
	
	n_or_rn_search(cnt,searchdir);
}

STATIC int
do_rnsearch(cnt) long cnt; {	/* Repeat search in opposite direction */

	n_or_rn_search(cnt, -searchdir);
}

STATIC int shell(esc_ch, cnt) long cnt;
{
	register char *p;
	static char buf[2];

	buf[0] = esc_ch;
	if (p = readline(buf)) {
		shellescape(p, esc_ch);
		if (cnt >= 0 && !hardcopy) {
			p = startcomm;
			startcomm = 0;
			ret_to_continue();
			putline(TI);
			if (!p) {
				/*
				 * Avoid double redraw.
				 * After a "startcomm", a redraw will
				 * take place anyway.
				 */
				redraw(1);
			}
		}
	}
}

STATIC int
do_shell(cnt) long cnt; {	/* Execute a shell escape */
	shell('!', cnt);
}

STATIC int
do_pipe(cnt) long cnt; {	/* Execute a shell escape */
	shell('|', cnt);
}

/*ARGSUSED*/
STATIC int
do_writefile(cnt) long cnt; {	/* Write input to a file */
	register char *p;
	int fd;

	if ((p = readline("Filename: ")) == 0 || !*p) {
		/*
		 * No file name given
		 */
		return;
	}
# if USG_OPEN || BSD4_2_OPEN || POSIX_OPEN
	if ((fd = open(p,O_CREAT|O_EXCL|O_WRONLY,0644)) < 0) {
		if (errno == EEXIST) {
			error("File exists");
			return;
		}
		error("Could not open file");
		return;
	}
# else
	if (!access(p,0)) {
		error("File exists");
		return;
	}
	if ((fd = creat(p,0644)) < 0) {
		error("Could not open file");
		return;
	}
# endif
	wrt_fd(fd);
	(VOID) close(fd);
}

VOID
wrt_fd(fd)
{
	register long l = 1;
	register char *p = getline(l,0), *pbuf;
	char buf[1024];

	while (p) {
		pbuf = buf;
		while (p && pbuf < &buf[1024]) {
			if (!*p) {
				*pbuf++ = '\n';
				p = getline(++l,0);
			}
			else *pbuf++ = *p++ & 0177;
		}
		if (write(fd, buf, pbuf - buf) < 0) {
			error("Write failed");
			break;
		}
	}
}

STATIC int
do_absolute(cnt) long cnt; {	/* Go to linenumber "cnt" */

	if (!getline(cnt,0)) {	/* Not there or interrupt */
		if (!interrupt) {
			/*
			 * User did'nt give an interrupt, so the line number
			 * was too high. Go to the last line.
			 */
			do_lline(cnt);
		}
		return;
	}
	(VOID) display(cnt,0,pagesize,1);
}

/*ARGSUSED*/
STATIC int
do_visit(cnt) long cnt; {	/* Visit a file */
	register char *p;
	static char fn[128];	/* Keep file name */

	if ((p = readline("Filename: ")) == 0) {
		return;
	}
	if (*p) {
		(VOID) strcpy(fn,p);
		visitfile(fn);
	}
	else {
		/*
		 * User typed a return. Visit the current file
		 */
		if (!(p = filenames[filecount])) {
			error("No current file");
			return;
		}
		visitfile(p);
	}
	(VOID) display(1L, 0, pagesize, 1);
}

/*ARGSUSED*/
STATIC int
do_error(cnt) long cnt; {	/* Called when user types wrong key sequence */

	error(currmap->k_help);
}

/*
 * Interface routine for displaying previous screen,
 * depending on cflag.
 */

STATIC int
prev_screen(sz,really) int sz, really; {
	register int retval;

	retval = scrollb(sz - 1, really && cflag);
	if (really && !cflag) {
		/*
		 * The previous call did not display anything, but at least we
		 * know where to start
		 */
		return display(scr_info.firstline, scr_info.nf, sz, 1);
	}
	return retval;
}

/*
 * Interface routine for displaying the next screen,
 * dependent on cflag.
 */

STATIC int
next_screen(sz,really) int sz, really; {

	register int t;
	register struct scr_info *p = &scr_info;

	if (cflag) {
		return scrollf(sz-1,really);
	}
	t = p->tail->cnt - 1;
	if (p->lastline == p->firstline) {
		t += p->nf;
	}
	return display(p->lastline, t, sz, really);
}

/*ARGSUSED*/
STATIC int
do_redraw(cnt) long cnt; {

	redraw(1);
}

STATIC int
page_size(cnt) unsigned cnt; {

	if (cnt) {
		if (cnt > maxpagesize) return maxpagesize;
		if (cnt < MINPAGESIZE) return MINPAGESIZE;
		return (int) cnt;
	}
	return pagesize;
}

STATIC int
do_forward(cnt) long cnt; {	/* Display next page */
	register int i;

	i = page_size((unsigned) cnt);
	if (status & EOFILE) {
		/*
		 * May seem strange, but actually a visit to the next file
		 * has already been done here
		 */
		(VOID) display(1L,0,i,1);
		return;
	}
	(VOID) next_screen(i,1);
}

STATIC int
do_backward(cnt) long cnt; {
	register int i, temp;

	i = page_size((unsigned) cnt);
	if (!(status & START)) {
		(VOID) prev_screen(i,1);
		return;
	}
	if (stdf < 0) {
		(VOID) display(1L,0,i,1);
		return;
	}
	/*
	 * The next part is a bit clumsy.
	 * We want to display the last page of the previous file (for which
	 * a visit has already been done), but the pagesize may temporarily
	 * be different because the command had a count
	 */
	temp = pagesize;
	pagesize = i;
	do_lline(cnt);
	pagesize = temp;
}

/*ARGSUSED*/
STATIC int
do_firstline(cnt) long cnt; {	/* Go to start of input */

	do_absolute(1L);
}

STATIC int
do_lline(cnt) long cnt; {	/* Go to end of input */
	register int i = 0;
	register int j = pagesize - 1;

	if ((cnt = to_lastline()) < 0) {
		/*
		 * Interrupted by the user
		 */
		return;
	}
	/*
	 * Display the page such that only the last line of the page is
	 * a "~", independant of the pagesize
	 */
	while (!(display(cnt,i,j,0) & EOFILE)) {
		/*
		 * The last line could of course be very long ...
		 */
		i+= j;
	}
	(VOID) scrollb(j - scr_info.tail->cnt, 0);
	redraw(0);
}

STATIC int
do_lf(cnt) long cnt; {		/* Display next line, or go to line */

	if (cnt) {		/* Go to line */
		do_absolute(cnt);
		return;
	}
	(VOID) scrollf(1,1);
}

STATIC int
do_upline(cnt) long cnt; {	/* Display previous line, or go to line */

	if (cnt) {		/* Go to line */
		do_absolute(cnt);
		return;
	}
	(VOID) scrollb(1,1);
}

STATIC int
do_skiplines(cnt) long cnt; {	/* Skip lines forwards */

	/* Should be interruptable ... */
	(VOID) scrollf((int) (cnt + maxpagesize - 1), 0);
	redraw(0);
}

STATIC int
do_bskiplines(cnt) long cnt; {	/* Skip lines backwards */

	/* Should be interruptable ... */
	(VOID) scrollb((int) (cnt + pagesize - 1), 0);
	redraw(0);
}

STATIC int
do_fscreens(cnt) long cnt; {	/* Skip screens forwards */

	do {
		if ((next_screen(pagesize,0) & EOFILE) || interrupt) break;
	} while (--cnt >= 0);
	redraw(0);
}

STATIC int
do_bscreens(cnt) long cnt; {	/* Skip screens backwards */

	do {
		if ((prev_screen(pagesize,0) & START) || interrupt) break;
	} while (--cnt >= 0);
	redraw(0);
}

STATIC int
scro_size(cnt) unsigned cnt; {

	if (cnt >= maxpagesize) return maxpagesize;
	if (cnt) return (int) cnt;
	return scrollsize;
}

STATIC int
do_f_scroll(cnt) long cnt; {	/* Scroll forwards */

	(VOID) scrollf(scro_size((unsigned) cnt),1);
}

STATIC int
do_b_scroll(cnt) long cnt; {	/* Scroll backwards */

	(VOID) scrollb(scro_size((unsigned) cnt),1);
}

STATIC int
do_previousfile(cnt) long cnt; {/* Visit previous file */

	if (nextfile(- (int) cnt)) {
		error("No (Nth) previous file");
		return;
	}
	redraw(0);
}

STATIC int
do_nextfile(cnt) long cnt; {	/* Visit next file */

	if (nextfile((int) cnt)) {
		error("No (Nth) next file");
		return;
	}
	redraw(0);
}

STATIC int do_lcomm();

/*
 * The next array is initialized, sorted on the first element of the structs,
 * so that we can perform binary search
 */
struct commands commands[] = {
{"",	    0,	    do_error,	    ""},
{"",	    0,	    do_nocomm,	    ""},
{"bf",	    STICKY|NEEDS_COUNT,
		    do_previousfile,"Visit previous file"},
{"bl",	    NEEDS_SCREEN|STICKY,
		    do_upline,	    "Scroll one line up, or go to line"},
{"bot",	    STICKY,
		    do_lline,	    "Go to last line of the input"},
{"bp",	    BACK|NEEDS_SCREEN|TOPREVFILE|STICKY,
		    do_backward,    "display previous page"},
{"bps",	    SCREENSIZE_ADAPT|BACK|NEEDS_SCREEN|TOPREVFILE|STICKY,
		    do_backward,    "Display previous page, set pagesize"},
{"bs",	    BACK|NEEDS_SCREEN|STICKY,
		    do_b_scroll,    "Scroll backwards"},
{"bse",	    0,	    do_bsearch,	    "Search backwards for pattern"},
{"bsl",	    BACK|NEEDS_SCREEN|STICKY|NEEDS_COUNT,
		    do_bskiplines,  "Skip lines backwards"},
{"bsp",	    BACK|NEEDS_SCREEN|STICKY|NEEDS_COUNT,
		    do_bscreens,    "Skip screens backwards"},
{"bss",	    SCROLLSIZE_ADAPT|BACK|NEEDS_SCREEN|STICKY,
		    do_b_scroll,    "Scroll backwards, set scrollsize"},
{"chm",	    0,	    do_chkm,	    "Switch to other keymap"},
{"exg",	    STICKY, exgmark,	    "Exchange current page with mark"},
{"ff",	    STICKY|NEEDS_COUNT,
		    do_nextfile,    "Visit next file"},
{"fl",	    NEEDS_SCREEN|STICKY,
		    do_lf,	    "Scroll one line down, or go to line"},
{"fp",	    TONEXTFILE|AHEAD|STICKY,
		    do_forward,	    "Display next page"},
{"fps",	    SCREENSIZE_ADAPT|TONEXTFILE|AHEAD|STICKY,
		    do_forward,	    "Display next page, set pagesize"},
{"fs",	    AHEAD|NEEDS_SCREEN|STICKY,
		    do_f_scroll,    "Scroll forwards"},
{"fse",	    0,	    do_fsearch,	    "Search forwards for pattern"},
{"fsl",	    AHEAD|NEEDS_SCREEN|STICKY|NEEDS_COUNT,
		    do_skiplines,   "Skip lines forwards"},
{"fsp",	    AHEAD|NEEDS_SCREEN|STICKY|NEEDS_COUNT,
		    do_fscreens,    "Skip screens forwards"},
{"fss",	    SCROLLSIZE_ADAPT|AHEAD|NEEDS_SCREEN|STICKY,
		    do_f_scroll,    "Scroll forwards, set scrollsize"},
{"hlp",	    0,	    do_help,	    "Give description of all commands"},
{"mar",	    0,	    setmark,	    "Set a mark on the current page"},
{"nse",	    STICKY, do_nsearch,	    "Repeat the last search"},
{"nsr",	    STICKY, do_rnsearch, "Repeat last search in other direction"},
{"pip",     ESC,    do_pipe,	    "pipe input into shell command"},
{"qui",	    0,	    quit,	    "Exit from yap"},
{"red",	    0,	    do_redraw,	    "Redraw screen"},
{"rep",	    0,	    do_lcomm,	    "Repeat last command"},
{"shl",	    ESC,    do_shell,	    "Execute a shell escape"},
{"tom",	    0,	    tomark,	    "Go to mark"},
{"top",	    STICKY, do_firstline,   "Go to the first line of the input"},
{"vis",	    0,	    do_visit,	    "Visit a file"},
{"wrf",	    0,	    do_writefile,   "Write input to a file"},
};

/*
 * Lookup string "s" in the commands array, and return index.
 * return 0 if not found.
 */

int
lookup(s) char *s; {
	register struct commands *l, *u, *m;

	l = &commands[2];
	u = &commands[sizeof(commands) / sizeof(*u) - 1];
	do {
		/*
		 * Perform binary search
		 */
		m = l + (u - l) / 2;
		if (strcmp(s, m->c_cmd) > 0) l = m + 1;
		else u = m;
	} while (l < u);
	if (!strcmp(s, u->c_cmd)) return u - commands;
	return 0;
}

/*ARGSUSED*/
STATIC int
do_lcomm(cnt) long cnt; {	/* Repeat last command */

	if (!lastcomm) {
		error("No previous command");
		return;
	}
	do_comm(lastcomm, lastcount);
}

/*
 * Execute a command, with optional count "count".
 */

VOID
do_comm(comm, count) register int comm; register long count; {

	register struct commands *pcomm;
	register int temp;
	register int flags;

	pcomm = &commands[comm];
	flags = pcomm->c_flags;

	/*
	 * Check the command.
	 * If the last line of the file is displayed and the command goes
	 * forwards and does'nt have the ability to go to the next file, it
	 * is an error.
	 * If the first line of the file is displayed and the command goes
	 * backwards and does'nt have the ability to go to the previous file,
	 * it is an error.
	 * Also check wether we need the next or previous file. If so, get it.
	 */
	if ((status & EOFILE) && (flags & AHEAD)) {
		if (qflag || !(flags & TONEXTFILE)) return;
		if (nextfile(1)) quit();
	}
	if ((status & START) && (flags & BACK)) {
		if (qflag || !(flags & TOPREVFILE)) return;
		if (nextfile(-1)) quit();
	}
	/*
	 * Does the command stick around for LASTCOMM?
	 */
	if (flags & STICKY) {
		lastcomm = comm;
		lastcount = count;
	}
	if (!count) {
		if (flags & NEEDS_COUNT) count = 1;
	}
	else {
		/*
		 * Does the command adapt the screensize?
		 */
		if (flags & SCREENSIZE_ADAPT) {
			temp = maxpagesize;
			if ((unsigned) count < temp) {
				temp = count;
			}
			if (temp < MINPAGESIZE) {
				temp = MINPAGESIZE;
			}
			count = 0;
			pagesize = temp;
		}
		/*
		 * Does the command adapt the scrollsize?
		 */
		if (flags & SCROLLSIZE_ADAPT) {
			temp = maxpagesize - 1;
			if ((unsigned) count < temp) {
				temp = (int) count;
			}
			scrollsize = temp;
			count = 0;
		}
	}
	/*
	 * Now execute the command.
	 */
	(*(pcomm->c_func))(count);
}
