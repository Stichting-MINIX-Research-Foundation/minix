/* Copyright (c) 1985 Ceriel J.H. Jacobs */

/* $Header$ */

/* All terminal and terminal dependent stuff */

# ifndef _TERM_
# define PUBLIC extern
# else
# define PUBLIC
# endif

# if USG_TTY
# include <termio.h>
# elif POSIX_TTY
# include <termios.h>
# else
# include <sgtty.h>
# endif

#include <sys/types.h>
#include <signal.h>
#include <sys/ioctl.h>

/* Terminal setting */

PUBLIC int expandtabs;		/* Tabs need expanding? */
PUBLIC int stupid;		/* Stupid terminal */
PUBLIC int hardcopy;		/* Hardcopy terminal */

/* termcap stuff */
PUBLIC
char	*CE,			/* clear to end of line */
	*CL,			/* clear screen */
	*SO,			/* stand out */
	*SE,			/* stand end */
	*US,			/* underline start */
	*UE,			/* underline end */
	*UC,			/* underline character */
	*MD,			/* bold start */
	*ME,			/* attributes (like bold) off */
	*TI,			/* initialize for CM */
	*TE,			/* End of CM */
	*CM,			/* Cursor addressing */
	*TA,			/* Tab */
	*SR,			/* Scroll reverse */
	*AL;			/* insert line */
PUBLIC
int	LINES,			/* # of lines on screen */
	COLS,			/* # of colums */
	AM,			/* Automatic margins */
	XN,			/* newline ignored after wrap */
	DB;			/* terminal retains lines below */
PUBLIC
char	HO[20],			/* Sequence to get to home position */
	BO[20];			/* sequence to get to lower left hand corner */
PUBLIC
int	erasech,		/* users erase character */
	killch;			/* users kill character */
PUBLIC struct state *sppat;	/* Special patterns to be recognized */
PUBLIC char
	*BC;			/* Back space */

#define backspace()	putline(BC)
#define clrscreen()	tputs(CL,LINES,fputch)
#define clrtoeol()	tputs(CE,1,fputch)
#define scrollreverse()	tputs(SR,LINES,fputch)
#ifdef VT100_PATCH
#define insert_line(l)	ins_line(l)
#define standout()	tputs(SO,1,fputch)
#define standend()	tputs(SE,1,fputch)
#define underline()	tputs(US,1,fputch)
#define end_underline() tputs(UE,1,fputch)
#define bold()		tputs(MD,1,fputch)
#define end_bold()	tputs(ME,1,fputch)
#define underchar()	tputs(UC,1,fputch)
# else
#define insert_line()	tputs(AL,LINES,fputch)
#define standout()	putline(SO)
#define standend()	putline(SE)
#define underline()	putline(US)
#define end_underline() putline(UE)
#define bold()		putline(MD)
#define end_bold()	putline(ME)
#define underchar()	putline(UC)
# endif
#define givetab()	tputs(TA,1,fputch)

VOID	inittty();
/*
 * void inittty()
 *
 * Initialises the terminal (sets it in cbreak mode, etc)
 */

VOID	resettty();
/*
 * void resettty()
 *
 * resets the terminal to the mode in which it was before yap was invoked
 */

VOID	ini_terminal();
/*
 * void ini_terminal()
 *
 * Handles the termcap entry for your terminal. In some cases, the terminal
 * will be considered stupid.
 */

VOID	mgoto();
/*
 * void mgoto(n)
 * int n;		Line to go to
 *
 * Put the cursor at the start of the n'th screen line.
 * This can be done in several ways (of course).
 */

VOID	clrbline();
/*
 * void clrbline()
 *
 * clears the bottom line, by either clearing it to end of line,
 * or pushing it of the screen by inserting a line before it.
 */

VOID	home();
VOID	bottom();
/*
 * Obvious
 */

#ifdef WINDOW
int	window();
#endif
# undef PUBLIC
