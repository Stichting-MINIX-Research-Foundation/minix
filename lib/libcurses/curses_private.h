/*	$NetBSD: curses_private.h,v 1.50 2014/02/20 09:42:42 blymn Exp $	*/

/*-
 * Copyright (c) 1998-2000 Brett Lymn
 *                         (blymn@baea.com.au, brett_lymn@yahoo.com.au)
 * All rights reserved.
 *
 * This code has been donated to The NetBSD Foundation by the Author.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 */

/* Modified by Ruibiao Qiu <ruibiao@arl.wustl.edu,ruibiao@gmail.com>
 * to add support for wide characters
 * Changes:
 * - Add a compiler variable HAVE_WCHAR for wide character only code
 * - Add a pointer to liked list of non-spacing characters in __ldata
 *   and the macro to access the width field in the attribute field
 * - Add a circular input character buffer in __screen to handle
 *   wide-character input (used in get_wch())
 */

#include <term.h>
#include <termios.h>

/* Private structure definitions for curses. */

/* Termcap capabilities. */
#ifdef HAVE_WCHAR
/*
 * Add a list of non-spacing characters to each spacing
 * character in a singly linked list
 */
typedef struct nschar_t {
	wchar_t			ch;		/* Non-spacing character */
	struct nschar_t	*next;	/* Next non-spacing character */
} nschar_t;
#endif /* HAVE_WCHAR */

/*
 * A window is an array of __LINE structures pointed to by the 'lines' pointer.
 * A line is an array of __LDATA structures pointed to by the 'line' pointer.
 *
 * IMPORTANT: the __LDATA structure must NOT induce any padding, so if new
 * fields are added -- padding fields with *constant values* should ensure
 * that the compiler will not generate any padding when storing an array of
 *  __LDATA structures.  This is to enable consistent use of memcmp, and memcpy
 * for comparing and copying arrays.
 */

struct __ldata {
	wchar_t	ch;			/* Character */
	attr_t	attr;			/* Attributes */
#ifdef HAVE_WCHAR
	nschar_t	*nsp;	/* Foreground non-spacing character pointer */
#endif /* HAVE_WCHAR */
};

#ifdef HAVE_WCHAR
/* macros to extract the width of a wide character */
#define __WCWIDTH 0xfc000000
#define WCW_SHIFT 26
#define WCOL(wc) ((((unsigned) (wc).attr) >> WCW_SHIFT ) > MB_LEN_MAX ? ((int)(((unsigned) (wc).attr ) >> WCW_SHIFT )) - 64 : ((int)(((unsigned) (wc).attr ) >> WCW_SHIFT)))
#define SET_WCOL(c, w) do { 						\
	((c).attr) = ((((c).attr) & WA_ATTRIBUTES ) | ((w) << WCW_SHIFT )); \
} while(/*CONSTCOND*/0)
#define BGWCOL(wc) ((((wc).battr) >> WCW_SHIFT ) > MB_LEN_MAX ? (((wc).battr ) >> WCW_SHIFT ) - 64 : (((wc).battr ) >> WCW_SHIFT ))
#define SET_BGWCOL(c, w) do { 						\
	((c).battr) = ((((c).battr) & WA_ATTRIBUTES ) | ((w) << WCW_SHIFT )); \
} while(/*CONSTCOND*/0)
#endif /* HAVE_WCHAR */

#define __LDATASIZE	(sizeof(__LDATA))

struct __line {
#ifdef DEBUG
#define SENTINEL_VALUE 0xaac0ffee

	unsigned int sentinel;          /* try to catch line overflows */
#endif
#define	__ISDIRTY	0x01		/* Line is dirty. */
#define __ISPASTEOL	0x02		/* Cursor is past end of line */
#define __ISFORCED	0x04		/* Force update, no optimisation */
	unsigned int flags;
	unsigned int hash;		/* Hash value for the line. */
	int *firstchp, *lastchp;	/* First and last chngd columns ptrs */
	int firstch, lastch;		/* First and last changed columns. */
	__LDATA *line;			/* Pointer to the line text. */
};

struct __window {		/* Window structure. */
	struct __window	*nextp, *orig;	/* Subwindows list and parent. */
	int begy, begx;			/* Window home. */
	int cury, curx;			/* Current x, y coordinates. */
	int maxy, maxx;			/* Maximum values for curx, cury. */
	int reqy, reqx;			/* Size requested when created */
	int ch_off;			/* x offset for firstch/lastch. */
	__LINE **alines;		/* Array of pointers to the lines */
	__LINE  *lspace;		/* line space (for cleanup) */
	__LDATA *wspace;		/* window space (for cleanup) */

#define	__ENDLINE	0x00000001	/* End of screen. */
#define	__FLUSH		0x00000002	/* Fflush(stdout) after refresh. */
#define	__FULLWIN	0x00000004	/* Window is a screen. */
#define	__IDLINE	0x00000008	/* Insert/delete sequences. */
#define	__SCROLLWIN	0x00000010	/* Last char will scroll window. */
#define	__SCROLLOK	0x00000020	/* Scrolling ok. */
#define	__CLEAROK	0x00000040	/* Clear on next refresh. */
#define	__LEAVEOK	0x00000100	/* If cursor left */
#define	__KEYPAD	0x00010000	/* If interpreting keypad codes */
#define	__NOTIMEOUT	0x00020000	/* Wait indefinitely for func keys */
#define __IDCHAR	0x00040000	/* insert/delete char sequences */
#define __ISPAD		0x00080000	/* "window" is a pad */
#define __ISDERWIN	0x00100000	/* "window" is derived from parent */
	unsigned int flags;
	int	delay;			/* delay for getch() */
	attr_t	wattr;			/* Character attributes */
	wchar_t	bch;			/* Background character */
	attr_t	battr;			/* Background attributes */
	int	scr_t, scr_b;		/* Scrolling region top, bottom */
	SCREEN	*screen;		/* Screen for this window */
	int	pbegy, pbegx,
		sbegy, sbegx,
		smaxy, smaxx;		/* Saved prefresh() values */
	int	dery, derx;		/* derived window coordinates
					   - top left corner of source 
					   relative to parent win */
#ifdef HAVE_WCHAR
	nschar_t *bnsp;			/* Background non-spacing char list */
#endif /* HAVE_WCHAR */
};

/* Set of attributes unset by 'me' - 'mb', 'md', 'mh', 'mk', 'mp' and 'mr'. */
#ifndef HAVE_WCHAR
#define	__TERMATTR \
	(__REVERSE | __BLINK | __DIM | __BOLD | __BLANK | __PROTECT)
#else
#define	__TERMATTR \
	(__REVERSE | __BLINK | __DIM | __BOLD | __BLANK | __PROTECT \
	| WA_TOP | WA_LOW | WA_LEFT | WA_RIGHT | WA_HORIZONTAL | WA_VERTICAL)
#endif /* HAVE_WCHAR */

struct __winlist {
	struct __window		*winp;	/* The window. */
	struct __winlist	*nextp;	/* Next window. */
};

struct __color {
	short	num;
	short	red;
	short	green;
	short	blue;
	int	flags;
};

/* List of colour pairs */
struct __pair {
	short	fore;
	short	back;
	int	flags;
};

/* Maximum colours */
#define	MAX_COLORS	64
/* Maximum colour pairs - determined by number of colour bits in attr_t */
#define	MAX_PAIRS	PAIR_NUMBER(__COLOR)

typedef struct keymap keymap_t;

/* this is the encapsulation of the terminal definition, one for
 * each terminal that curses talks to.
 */
struct __screen {
	FILE    *infd, *outfd;  /* input and output file descriptors */
	WINDOW	*curscr;	/* Current screen. */
	WINDOW	*stdscr;	/* Standard screen. */
	WINDOW	*__virtscr;	/* Virtual screen (for doupdate()). */
	int      curwin;        /* current window for refresh */
	int      lx, ly;        /* loop parameters for refresh */
	int	 COLS;		/* Columns on the screen. */
	int	 LINES;		/* Lines on the screen. */
	int	 TABSIZE;	/* Size of a tab. */
	int	 COLORS;	/* Maximum colors on the screen */
	int	 COLOR_PAIRS;	/* Maximum color pairs on the screen */
	int	 My_term;	/* Use Def_term regardless. */
	char	 GT;		/* Gtty indicates tabs. */
	char	 NONL;		/* Term can't hack LF doing a CR. */
	char	 UPPERCASE;	/* Terminal is uppercase only. */

	chtype acs_char[NUM_ACS];
#ifdef HAVE_WCHAR
	cchar_t wacs_char[ NUM_ACS ];
#endif /* HAVE_WCHAR */
	struct __color colours[MAX_COLORS];
	struct __pair  colour_pairs[MAX_PAIRS];
	attr_t	nca;

/* Style of colour manipulation */
#define COLOR_NONE	0
#define COLOR_ANSI	1	/* ANSI/DEC-style colour manipulation */
#define COLOR_HP	2	/* HP-style colour manipulation */
#define COLOR_TEK	3	/* Tektronix-style colour manipulation */
#define COLOR_OTHER	4	/* None of the others but can set fore/back */
	int color_type;

	attr_t mask_op;
	attr_t mask_me;
	attr_t mask_ue;
	attr_t mask_se;
	TERMINAL *term;
	int old_mode; /* old cursor visibility state for terminal */
	keymap_t *base_keymap;
	int echoit;
	int pfast;
	int rawmode;
	int nl;
	int noqch;
	int clearok;
	int useraw;
	struct __winlist *winlistp;
	struct   termios cbreakt, rawt, *curt, save_termios;
	struct termios orig_termios, baset, savedtty;
	int ovmin;
	int ovtime;
	char *stdbuf;
	unsigned int len;
	int meta_state;
	char padchar;
	int endwin;
	int notty;
	int half_delay;
	int resized;
	wchar_t *unget_list;
	int unget_len, unget_pos;
#ifdef HAVE_WCHAR
#define MB_LEN_MAX 8
#define MAX_CBUF_SIZE MB_LEN_MAX
	int		cbuf_head;		/* header to cbuf */
	int		cbuf_tail;		/* tail to cbuf */
	int		cbuf_cur;		/* the current char in cbuf */
	mbstate_t	sp;			/* wide char processing state */
	char		cbuf[ MAX_CBUF_SIZE ];	/* input character buffer */
#endif /* HAVE_WCHAR */
};


extern char	 __GT;			/* Gtty indicates tabs. */
extern char	 __NONL;		/* Term can't hack LF doing a CR. */
extern char	 __UPPERCASE;		/* Terminal is uppercase only. */
extern int	 My_term;		/* Use Def_term regardless. */
extern const char	*Def_term;	/* Default terminal type. */
extern SCREEN   *_cursesi_screen;       /* The current screen in use */

/* Debugging options/functions. */
#ifdef DEBUG
#define __CTRACE_TSTAMP		0x00000001
#define __CTRACE_MISC		0x00000002
#define __CTRACE_INIT		0x00000004
#define __CTRACE_SCREEN		0x00000008
#define __CTRACE_WINDOW		0x00000010
#define __CTRACE_REFRESH	0x00000020
#define __CTRACE_COLOR		0x00000040
#define __CTRACE_INPUT		0x00000080
#define __CTRACE_OUTPUT		0x00000100
#define __CTRACE_LINE		0x00000200
#define __CTRACE_ATTR		0x00000400
#define __CTRACE_ERASE		0x00000800
#define __CTRACE_FILEIO		0x00001000
#define __CTRACE_ALL		0x7fffffff
void	 __CTRACE_init(void);
void	 __CTRACE(int, const char *, ...) __attribute__((__format__(__printf__, 2, 3)));
#endif

/* Private functions. */
int     __cputchar_args(int, void *);
void     _cursesi_free_keymap(keymap_t *);
int      _cursesi_gettmode(SCREEN *);
void     _cursesi_reset_acs(SCREEN *);
int	_cursesi_addbyte(WINDOW *, __LINE **, int *, int *, int , attr_t, int);
int	_cursesi_addwchar(WINDOW *, __LINE **, int *, int *, const cchar_t *,
			  int);
int	_cursesi_waddbytes(WINDOW *, const char *, int, attr_t, int);
#ifdef HAVE_WCHAR
void     _cursesi_reset_wacs(SCREEN *);
#endif /* HAVE_WCHAR */
void     _cursesi_resetterm(SCREEN *);
int      _cursesi_setterm(char *, SCREEN *);
int	 __delay(void);
u_int	 __hash_more(const void *, size_t, u_int);
#define	__hash(s, len)	__hash_more((s), (len), 0u)
void	 __id_subwins(WINDOW *);
void	 __init_getch(SCREEN *);
void	 __init_acs(SCREEN *);
#ifdef HAVE_WCHAR
void	 __init_get_wch(SCREEN *);
void	 __init_wacs(SCREEN *);
int	__cputwchar_args( wchar_t, void * );
int     _cursesi_copy_nsp(nschar_t *, struct __ldata *);
void	__cursesi_free_nsp(nschar_t *);
void	__cursesi_win_free_nsp(WINDOW *);
void	__cursesi_putnsp(nschar_t *, const int, const int);
void	__cursesi_chtype_to_cchar(chtype, cchar_t *);
#endif /* HAVE_WCHAR */
int	 __unget(wint_t);
int	 __mvcur(int, int, int, int, int);
WINDOW  *__newwin(SCREEN *, int, int, int, int, int);
int	 __nodelay(void);
int	 __notimeout(void);
void	 __restartwin(void);
void	 __restore_colors(void);
void     __restore_cursor_vis(void);
void     __restore_meta_state(void);
void	 __restore_termios(void);
void	 __restore_stophandler(void);
void	 __restore_winchhandler(void);
void	 __save_termios(void);
void	 __set_color(WINDOW *win, attr_t attr);
void	 __set_stophandler(void);
void	 __set_winchhandler(void);
void	 __set_subwin(WINDOW *, WINDOW *);
void	 __startwin(SCREEN *);
void	 __stop_signal_handler(int);
int	 __stopwin(void);
void	 __swflags(WINDOW *);
int	 __timeout(int);
int	 __touchline(WINDOW *, int, int, int);
int	 __touchwin(WINDOW *);
void	 __unsetattr(int);
void	 __unset_color(WINDOW *win);
int	 __waddch(WINDOW *, __LDATA *);
int	 __wgetnstr(WINDOW *, char *, int);
void	 __winch_signal_handler(int);

/* Private #defines. */
#define	min(a,b)	((a) < (b) ? (a) : (b))
#define	max(a,b)	((a) > (b) ? (a ): (b))

/* Private externs. */
extern int		 __echoit;
extern int		 __endwin;
extern int		 __pfast;
extern int		 __rawmode;
extern int		 __noqch;
extern attr_t		 __mask_op, __mask_me, __mask_ue, __mask_se;
extern WINDOW		*__virtscr;
extern int		 __using_color;
extern attr_t		 __default_color;
