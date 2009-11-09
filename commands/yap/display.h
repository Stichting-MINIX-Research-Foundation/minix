/* Copyright (c) 1985 Ceriel J.H. Jacobs */

/* $Header$ */

# ifndef _DISPLAY_
# define PUBLIC extern
# else
# define PUBLIC
# endif

# define MINPAGESIZE 5

PUBLIC int	pagesize;	/* How many lines on a page */
PUBLIC int	maxpagesize;	/* Maximum # of lines on a page */
PUBLIC int	scrollsize;	/* number of lines in a scroll */
struct scr_info {
	struct linelist {
	    int cnt;		/* # of screenlines for this line */
	    long line;		/* lineno of this line */
# define firstline head->line
# define lastline tail->line
	    struct linelist *next;
	    struct linelist *prev;
	} *tail, *head;		/* Of all lines of the input file that are
				 * on the screen, remember how many
				 * screenlines they occupy. Keep this
				 * info in a doubly linked list.
				 */
	int	nf;		/* How many screenlines of the first line
				 * on the screen are not on the screen?
				 */
	int	currentpos;	/* Y coordinate of first free line */
	struct linelist ssaavv; /* Mark */
# define savfirst ssaavv.line
# define savnf ssaavv.cnt
};

PUBLIC struct scr_info scr_info;
PUBLIC int	status;		/* EOFILE on end of file
				 * START on start of file
				 * logical "or" if both
				 */
/* Flags for status field */

# define EOFILE 01
# define START 02

VOID	redraw();
/*
 * void redraw(flag)
 * int flag;			Either 0 or 1
 *
 * Redraws the screen. If flag = 1, the screen is redrawn as precisely
 * as possible, otherwise one page is displayed (which possibly does not
 * take a whole screen.
 */

int	display();
/*
 * int display(firstline, nodispl, nlines, really)
 * long firstline;		Line with which to start
 * int nodispl;			Do not display nodispl lines of it
 * int nlines;			Number of screen lines that must be displayed
 * int really;			Either 0 or 1
 *
 * Displays nlines as a page. if "really" = 0, the actual displaying is not
 * performed. Only the computing associated with it is done.
 */

int	scrollf();
/*
 * int scrollf(nlines,really)
 * int nlines;			Number of lines to scroll
 * int really;			Either 0 or 1, see explanation above
 *
 * Scroll forwards "nlines" (screen)lines.
 */

int	scrollb();
/*
 * int scrollb(nlines,really)
 * int nlines;			Number of lines to scroll
 * int really;			Either 0 or 1, see explanation above
 *
 * Scroll backwards "nlines" (screen)lines.
 */

int	tomark();
/*
 * int tomark(cnt)
 * long cnt;			(Argument ignored)
 *
 * Display a page starting at the mark. If there was no
 * mark, display the first page of the file.
 * (There is always assumed to be a mark, the initial one is on the first page
 * of the file).
 */

int	setmark();
/*
 * int setmark(cnt)
 * long cnt;			(Argument ignored)
 *
 * Sets a mark on the current page.
 * It returns nothing (but the address is taken ...)
 */

int	exgmark();
/*
 * int exgmark(cnt)
 * long cnt;			(Argumewnt ignored)
 *
 * Sets the mark on the current page and displays the
 * previously marked page.
 */

VOID	d_clean();
/*
 * void d_clean()
 *
 * Clean up and initialize. To be called before displaying a new file
 */

# undef PUBLIC
