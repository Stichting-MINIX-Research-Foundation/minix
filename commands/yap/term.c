/* Copyright (c) 1985 Ceriel J.H. Jacobs */

/*
 * Terminal handling routines, mostly initializing.
 */

# ifndef lint
static char rcsid[] = "$Header$";
# endif

# define _TERM_

#include "in_all.h"
#include "term.h"
#include "machine.h"
#include "output.h"
#include "display.h"
#include "options.h"
#include "getline.h"
#include "keys.h"
#include "main.h"

#ifdef TIOCGWINSZ
static struct winsize w;
#endif

char	*strcpy(),
	*strcat(),
	*tgoto(),
	*tgetstr(),
	*getenv();

static char	tcbuf1[1024];	/* Holds terminal capability strings */
static char *	ptc;		/* Pointer in it */
static char	tcbuf[1024];	/* Another termcap buffer */
short	ospeed;		/* Needed for tputs() */
char	PC;		/* Needed for tputs() */
char *	UP;		/* Needed for tgoto() */
static char	*ll;

struct linelist _X[100];	/* 100 is enough ? */

# if USG_TTY
static struct termio _tty,_svtty;
# elif POSIX_TTY
static struct termios _tty, _svtty;
# else
# ifdef TIOCSPGRP
static int proc_id, saved_pgrpid;
# endif
static struct sgttyb _tty,_svtty;
# ifdef TIOCGETC
static struct tchars _ttyc, _svttyc;
# endif
# ifdef TIOCGLTC
static int line_discipline;
static struct ltchars _lttyc, _svlttyc;
# endif
# endif

static VOID
handle(c) char *c; {	/* if character *c is used, set it to undefined */

	if (isused(*c)) *c = 0377;
}

/*
 * Set terminal in cbreak mode.
 * Also check if tabs need expanding.
 */

VOID
inittty() {
# if USG_TTY
	register struct termio *p = &_tty;

	ioctl(0,TCGETA,(char *) p);
	_svtty = *p;
	if (p->c_oflag & TAB3) {
		/*
		 * We do tab expansion ourselves
		 */
		expandtabs = 1;
	}
	p->c_oflag &= ~(TAB3|OCRNL|ONLRET|ONLCR);
	p->c_oflag |= (/*ONOCR|*/OPOST);	/* ONOCR does not seem to work
						   very well in combination with
						   ~ONLCR
						*/
	p->c_lflag &= ~(ECHO|ECHOE|ECHOK|ECHONL|ICANON);
	if (isused('S'&037) || isused('Q'&037)) p->c_iflag &= ~IXON;
	handle(&(p->c_cc[0]));	/* INTR and QUIT (mnemonics not defined ??) */
	handle(&(p->c_cc[1]));
	erasech = p->c_cc[VERASE];
	killch = p->c_cc[VKILL];
	p->c_cc[VMIN] = 1;	/* Just wait for one character */
	p->c_cc[VTIME] = 0;
	ospeed = p->c_cflag & CBAUD;
	ioctl(0,TCSETAW,(char *) p);
#elif POSIX_TTY
	register struct termios *p = &_tty;

	tcgetattr(0, p);
	_svtty = *p;
#ifdef _MINIX	/* Should be XTABS */
	if (p->c_oflag & XTABS) {
		/*
		 * We do tab expansion ourselves
		 */
		expandtabs = 1;
	}
	p->c_oflag &= (OPOST|XTABS);
#else
	p->c_oflag &= ~OPOST;
#endif
	p->c_lflag &= ~(ECHO|ECHOE|ECHOK|ECHONL|ICANON);
	if (isused('S'&037) || isused('Q'&037)) p->c_iflag &= ~IXON;
	handle(&(p->c_cc[VINTR]));	
	handle(&(p->c_cc[VQUIT]));
	erasech = p->c_cc[VERASE];
	killch = p->c_cc[VKILL];
	p->c_cc[VMIN] = 1;	/* Just wait for one character */
	p->c_cc[VTIME] = 0;
	ospeed = cfgetospeed(p);
	tcsetattr(0, TCSANOW, p);
# else
	register struct sgttyb *p = &_tty;

# ifdef TIOCSPGRP
	/*
	 * If we can, we put yap in another process group, and the terminal
	 * with it. This is done, so that interrupts given by the user
	 * will only affect yap and not it's children (processes writing
	 * on a pipe to yap)
	 */
	if (ioctl(0, TIOCSPGRP, (char *) &proc_id) != -1) {
		setpgrp(0, proc_id);
	}
# endif
	ioctl(0,TIOCGETP,(char *) p);
	_svtty = *p;
	erasech = p->sg_erase;
	killch = p->sg_kill;
	ospeed = p->sg_ospeed;
	if (p->sg_flags & XTABS) {
		/*
		 * We do tab expansion ourselves
		 */
		expandtabs = 1;
	}
	p->sg_flags |= (CBREAK);
	p->sg_flags &= ~(ECHO|XTABS|RAW|LCASE|CRMOD);
#ifdef TIOCSETN
	ioctl(0, TIOCSETN, (char *) p);
#else
	ioctl(0,TIOCSETP,(char *) p);
#endif
/* Bloody Sun ... */
#undef t_startc
#undef t_stopc
#undef t_intrc
#undef t_quitc
#undef t_suspc
#undef t_dsuspc
#undef t_flushc
#undef t_lnextc
# ifdef TIOCGETC
	{   register struct tchars *q = &_ttyc;

		ioctl(0,TIOCGETC,(char *) q);
		_svttyc = *q;
		handle(&(q->t_intrc));
		handle(&(q->t_quitc));
		if (isused(q->t_startc) || isused(q->t_stopc)) {
			q->t_startc = q->t_stopc = 0377;
		}
		ioctl(0,TIOCSETC, (char *) q);
	}
# endif
# ifdef TIOCGLTC
	{   register struct ltchars *q = &_lttyc;

		ioctl(0,TIOCGETD,(char *) &line_discipline);
		if (line_discipline == NTTYDISC) {
			ioctl(0, TIOCGLTC,(char *) q);
			_svlttyc = *q;
			handle(&(q->t_suspc));
			handle(&(q->t_dsuspc));
			q->t_flushc = q->t_lnextc = 0377;
			ioctl(0,TIOCSLTC, (char *) q);
		}
	}
# endif
# endif
}

/*
 * Reset the terminal to its original state
 */

VOID
resettty() {

# if USG_TTY
	ioctl(0,TCSETAW,(char *) &_svtty);
# elif POSIX_TTY
	tcsetattr(0, TCSANOW, &_svtty);
# else
# ifdef TIOCSPGRP
	ioctl(0, TIOCSPGRP, (char *) &saved_pgrpid);
	setpgrp(0, saved_pgrpid);
# endif
	ioctl(0,TIOCSETP,(char *) &_svtty);
# ifdef TIOCGETC
	ioctl(0,TIOCSETC, (char *) &_svttyc);
# endif
# ifdef TIOCGLTC
	if (line_discipline == NTTYDISC) ioctl(0,TIOCSLTC, (char *) &_svlttyc);
# endif
# endif
	putline(TE);
	flush();
}

/*
 * Get terminal capability "cap".
 * If not present, return an empty string.
 */

STATIC char *
getcap(cap) char *cap; {
	register char *s;

	s = tgetstr(cap, &ptc);
	if (!s) return "";
	return s;
}

/*
 * Initialize some terminal-dependent stuff.
 */

VOID
ini_terminal() {

	register char * s;
	register struct linelist *lp, *lp1;
	register i;
	register UG, SG;
	char tempbuf[20];
	char *mb, *mh, *mr;	/* attributes */

	initkeys();
#if !_MINIX
# ifdef TIOCSPGRP
	proc_id = getpid();
	ioctl(0,TIOCGPGRP, (char *) &saved_pgrpid);
# endif
#endif
	inittty();
	stupid = 1;
	ptc = tcbuf1;
	BC = "\b";
	TA = "\t";
	if (!(s = getenv("TERM"))) s = "dumb";
	if (tgetent(tcbuf, s) <= 0) {
		panic("No termcap entry");
	}
	stupid = 0;
	hardcopy = tgetflag("hc");	/* Hard copy terminal?*/
	PC = *(getcap("pc"));
	if (*(s = getcap("bc"))) {
		/*
		 * Backspace if not ^H
		 */
		BC = s;
	}
	UP = getcap("up");		/* move up a line */
	CE = getcap("ce");		/* clear to end of line */
	CL = getcap("cl");		/* clear screen */
	if (!*CL) cflag = 1;
	TI = getcap("ti");		/* Initialization for CM */
	TE = getcap("te");		/* end for CM */
	CM = getcap("cm");		/* cursor addressing */
	SR = getcap("sr");		/* scroll reverse */
	AL = getcap("al");		/* Insert line */
	SO = getcap("so");		/* standout */
	SE = getcap("se");		/* standend */
	SG = tgetnum("sg");		/* blanks left by SO, SE */
	if (SG < 0) SG = 0;
	US = getcap("us");		/* underline */
	UE = getcap("ue");		/* end underline */
	UG = tgetnum("ug");		/* blanks left by US, UE */
	if (UG < 0) UG = 0;
	UC = getcap("uc");		/* underline a character */
	mb = getcap("mb");		/* blinking attribute */
	MD = getcap("md");		/* bold attribute */
	ME = getcap("me");		/* turn off attributes */
	mh = getcap("mh");		/* half bright attribute */
	mr = getcap("mr");		/* reversed video attribute */
	if (!nflag) {
		/*
		 * Recognize special strings
		 */
		(VOID) addstring(SO,SG,&sppat);
		(VOID) addstring(SE,SG,&sppat);
		(VOID) addstring(US,UG,&sppat);
		(VOID) addstring(UE,UG,&sppat);
		(VOID) addstring(mb,0,&sppat);
		(VOID) addstring(MD,0,&sppat);
		(VOID) addstring(ME,0,&sppat);
		(VOID) addstring(mh,0,&sppat);
		(VOID) addstring(mr,0,&sppat);
		if (*UC) {
			(VOID) strcpy(tempbuf,BC);
			(VOID) strcat(tempbuf,UC);
			(VOID) addstring(tempbuf,0,&sppat);
		}
	}
	if (UG > 0 || uflag) {
		US = "";
		UE = "";
	}
	if (*US || uflag) UC = "";
	COLS = tgetnum("co");		/* columns on page */
	i = tgetnum("li");		/* Lines on page */
	AM = tgetflag("am");		/* terminal wraps automatically? */
	XN = tgetflag("xn");		/* and then ignores next newline? */
	DB = tgetflag("db");		/* terminal retains lines below */
	if (!*(s = getcap("ho")) && *CM) {
		s = tgoto(CM,0,0);	/* Another way of getting home */
	}
	if ((!*CE && !*AL) || !*s || hardcopy) {
		cflag = stupid = 1;
	}
	(VOID) strcpy(HO,s);
	if (*(s = getcap("ta"))) {
		/*
		 * Tab (other than ^I or padding)
		 */
		TA = s;
	}
       if (!*(ll = getcap("ll")) && *CM && i > 0) {
		/*
		 * Lower left hand corner
		 */
               (VOID) strcpy(BO, tgoto(CM,0,i-1));
	}
       else    (VOID) strcpy(BO, ll);
	if (COLS <= 0 || COLS > 256) {
		if ((unsigned) COLS >= 65409) {
			/* SUN bug */
			COLS &= 0xffff;
			COLS -= (65409 - 128);
		}
		if (COLS <= 0 || COLS > 256) COLS = 80;
	}
	if (i <= 0) {
		i = 24;
		cflag = stupid = 1;
	}
	LINES = i;
	maxpagesize = i - 1;
	scrollsize = maxpagesize / 2;
	if (scrollsize <= 0) scrollsize = 1;
	if (!pagesize || pagesize >= i) {
		pagesize = maxpagesize;
	}

	/*
	 * The next part does not really belong here, but there it is ...
	 * Initialize a circular list for the screenlines.
	 */

       scr_info.tail = lp = _X;
       lp1 = lp + (100 - 1);
	for (; lp <= lp1; lp++) {
		/*
		 * Circular doubly linked list
		 */
		lp->next = lp + 1;
		lp->prev = lp - 1;
	}
       lp1->next = scr_info.tail;
	lp1->next->prev = lp1;
	if (stupid) {
		(VOID) strcpy(BO,"\r\n");
	}
	putline(TI);
	window();
}

/*
 * Place cursor at start of line n.
 */

VOID
mgoto(n) register n; {

	if (n == 0) home();
	else if (n == maxpagesize && *BO) bottom();
	else if (*CM) {
		/*
		 * Cursor addressing
		 */
		tputs(tgoto(CM,0,n),1,fputch);
	}
	else if (*BO && *UP && n >= (maxpagesize >> 1)) {
		/*
		 * Bottom and then up
		 */
		bottom();
		while (n++ < maxpagesize) putline(UP);
	}
	else {	/* Home, and then down */
		home();
		while (n--) putline("\r\n");
	}
}

/*
 * Clear bottom line
 */

VOID
clrbline() {

	if (stupid) {
		putline("\r\n");
		return;
	}
	bottom();
	if (*CE) {
		/*
		 * We can clear to end of line
		 */
		clrtoeol();
		return;
	}
# ifdef VT100_PATCH
	insert_line(maxpagesize);
# else
	insert_line();
# endif
}

# ifdef VT100_PATCH
ins_line(l) {
	tputs(tgoto(AL, l, 0), maxpagesize - l, fputch);
}
# endif

VOID
home() {

	tputs(HO,1,fputch);
}

VOID
bottom() {

	tputs(BO,1,fputch);
	if (!*BO) mgoto(maxpagesize);
}

int
window()
{
#ifdef TIOCGWINSZ
        if (ioctl(1, TIOCGWINSZ, &w) < 0) return 0;

        if (w.ws_col == 0) w.ws_col = COLS;
        if (w.ws_row == 0) w.ws_row = LINES;
        if (w.ws_col != COLS || w.ws_row != LINES) {
		COLS = w.ws_col;
		LINES = w.ws_row;
		maxpagesize = LINES - 1;
		pagesize = maxpagesize;
		if (! *ll) (VOID) strcpy(BO, tgoto(CM,0,maxpagesize));
		scr_info.currentpos = 0;
		scrollsize = maxpagesize / 2; 
		if (scrollsize <= 0) scrollsize = 1;
		return 1;
        }
#endif
        return 0;
}
