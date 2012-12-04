/*	$NetBSD: curses.h,v 1.104 2012/04/21 12:27:27 roy Exp $	*/

/*
 * Copyright (c) 1981, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)curses.h	8.5 (Berkeley) 4/29/95
 *
 *	Modified by Ruibiao Qiu <ruibiao@arl.wustl.edu,ruibiao@gmail.com> 2005
 *	to add wide-character support
 *  - Add complex character structure (cchar_t)
 *	- Add definitions of wide-character routines
 *	- Add KEY_CODE_YES
 */

#ifndef _CURSES_H_
#define	_CURSES_H_

#include <sys/types.h>
#include <sys/cdefs.h>
#include <wchar.h>

#include <stdio.h>
#include <stdbool.h>

/*
 * attr_t must be the same size as wchar_t (see <wchar.h>) to avoid padding
 * in __LDATA.
 */
typedef wchar_t	chtype;
typedef wchar_t	attr_t;

#if !defined(HAVE_WCHAR) && !defined(DISABLE_WCHAR)
#define HAVE_WCHAR 1
#endif

#ifdef HAVE_WCHAR
/* 
 * The complex character structure required by the X/Open reference and used
 * in * functions such as in_wchstr(). It includes a string of up to 8 wide
 * characters and its length, an attribute, and a color-pair.
 */
#define CURSES_CCHAR_MAX 8
#define CCHARW_MAX       5
typedef struct {
	attr_t		attributes;		/* character attributes */
	unsigned	elements;		/* number of wide char in
						   vals[] */
	wchar_t		vals[CURSES_CCHAR_MAX]; /* wide chars including
						   non-spacing */
} cchar_t;
#else 
typedef chtype cchar_t;
#endif /* HAVE_WCHAR */

#ifndef TRUE
#define	TRUE	(/*CONSTCOND*/1)
#endif
#ifndef FALSE
#define	FALSE	(/*CONSTCOND*/0)
#endif

#ifndef _CURSES_PRIVATE

#define _puts(s)        tputs(s, 0, __cputchar)
#define _putchar(c)     __cputchar(c)

/* Old-style terminal modes access. */
#define crmode()        cbreak()
#define nocrmode()      nocbreak()
#endif /* _CURSES_PRIVATE */


/* symbols for values returned by getch in keypad mode */
#define    KEY_MIN        0x101    /* minimum extended key value */
#define    KEY_BREAK      0x101    /* break key */
#define    KEY_DOWN       0x102    /* down arrow */
#define    KEY_UP         0x103    /* up arrow */
#define    KEY_LEFT       0x104    /* left arrow */
#define    KEY_RIGHT      0x105    /* right arrow*/
#define    KEY_HOME       0x106    /* home key */
#define    KEY_BACKSPACE  0x107    /* Backspace */

/* First function key (block of 64 follow) */
#define    KEY_F0         0x108
/* Function defining other function key values*/
#define    KEY_F(n)       (KEY_F0+(n))

#define    KEY_DL         0x148    /* Delete Line */
#define    KEY_IL         0x149    /* Insert Line*/
#define    KEY_DC         0x14A    /* Delete Character */
#define    KEY_IC         0x14B    /* Insert Character */
#define    KEY_EIC        0x14C    /* Exit Insert Char mode */
#define    KEY_CLEAR      0x14D    /* Clear screen */
#define    KEY_EOS        0x14E    /* Clear to end of screen */
#define    KEY_EOL        0x14F    /* Clear to end of line */
#define    KEY_SF         0x150    /* Scroll one line forward */
#define    KEY_SR         0x151    /* Scroll one line back */
#define    KEY_NPAGE      0x152    /* Next Page */
#define    KEY_PPAGE      0x153    /* Prev Page */
#define    KEY_STAB       0x154    /* Set Tab */
#define    KEY_CTAB       0x155    /* Clear Tab */
#define    KEY_CATAB      0x156    /* Clear All Tabs */
#define    KEY_ENTER      0x157    /* Enter or Send */
#define    KEY_SRESET     0x158    /* Soft Reset */
#define    KEY_RESET      0x159    /* Hard Reset */
#define    KEY_PRINT      0x15A    /* Print */
#define    KEY_LL         0x15B    /* Home Down */

/*
 * "Keypad" keys arranged like this:
 *
 *  A1   up  A3
 * left  B2 right
 *  C1  down C3
 *
 */
#define    KEY_A1         0x15C    /* Keypad upper left */
#define    KEY_A3         0x15D    /* Keypad upper right */
#define    KEY_B2         0x15E    /* Keypad centre key */
#define    KEY_C1         0x15F    /* Keypad lower left */
#define    KEY_C3         0x160    /* Keypad lower right */

#define    KEY_BTAB       0x161    /* Back Tab */
#define    KEY_BEG        0x162    /* Begin key */
#define    KEY_CANCEL     0x163    /* Cancel key */
#define    KEY_CLOSE      0x164    /* Close Key */
#define    KEY_COMMAND    0x165    /* Command Key */
#define    KEY_COPY       0x166    /* Copy key */
#define    KEY_CREATE     0x167    /* Create key */
#define    KEY_END        0x168    /* End key */
#define    KEY_EXIT       0x169    /* Exit key */
#define    KEY_FIND       0x16A    /* Find key */
#define    KEY_HELP       0x16B    /* Help key */
#define    KEY_MARK       0x16C    /* Mark key */
#define    KEY_MESSAGE    0x16D    /* Message key */
#define    KEY_MOVE       0x16E    /* Move key */
#define    KEY_NEXT       0x16F    /* Next Object key */
#define    KEY_OPEN       0x170    /* Open key */
#define    KEY_OPTIONS    0x171    /* Options key */
#define    KEY_PREVIOUS   0x172    /* Previous Object key */
#define    KEY_REDO       0x173    /* Redo key */
#define    KEY_REFERENCE  0x174    /* Ref Key */
#define    KEY_REFRESH    0x175    /* Refresh key */
#define    KEY_REPLACE    0x176    /* Replace key */
#define    KEY_RESTART    0x177    /* Restart key */
#define    KEY_RESUME     0x178    /* Resume key */
#define    KEY_SAVE       0x179    /* Save key */
#define    KEY_SBEG       0x17A    /* Shift begin key */
#define    KEY_SCANCEL    0x17B    /* Shift Cancel key */
#define    KEY_SCOMMAND   0x17C    /* Shift Command key */
#define    KEY_SCOPY      0x17D    /* Shift Copy key */
#define    KEY_SCREATE    0x17E    /* Shift Create key */
#define    KEY_SDC        0x17F    /* Shift Delete Character */
#define    KEY_SDL        0x180    /* Shift Delete Line */
#define    KEY_SELECT     0x181    /* Select key */
#define    KEY_SEND       0x182    /* Send key */
#define    KEY_SEOL       0x183    /* Shift Clear Line key */
#define    KEY_SEXIT      0x184    /* Shift Exit key */
#define    KEY_SFIND      0x185    /* Shift Find key */
#define    KEY_SHELP      0x186    /* Shift Help key */
#define    KEY_SHOME      0x187    /* Shift Home key */
#define    KEY_SIC        0x188    /* Shift Input key */
#define    KEY_SLEFT      0x189    /* Shift Left Arrow key */
#define    KEY_SMESSAGE   0x18A    /* Shift Message key */
#define    KEY_SMOVE      0x18B    /* Shift Move key */
#define    KEY_SNEXT      0x18C    /* Shift Next key */
#define    KEY_SOPTIONS   0x18D    /* Shift Options key */
#define    KEY_SPREVIOUS  0x18E    /* Shift Previous key */
#define    KEY_SPRINT     0x18F    /* Shift Print key */
#define    KEY_SREDO      0x190    /* Shift Redo key */
#define    KEY_SREPLACE   0x191    /* Shift Replace key */
#define    KEY_SRIGHT     0x192    /* Shift Right Arrow key */
#define    KEY_SRSUME     0x193    /* Shift Resume key */
#define    KEY_SSAVE      0x194    /* Shift Save key */
#define    KEY_SSUSPEND   0x195    /* Shift Suspend key */
#define    KEY_SUNDO      0x196    /* Shift Undo key */
#define    KEY_SUSPEND    0x197    /* Suspend key */
#define    KEY_UNDO       0x198    /* Undo key */
#define    KEY_MOUSE      0x199    /* Mouse event has occurred */
#define    KEY_RESIZE     0x200    /* Resize event has occurred */
#define    KEY_MAX        0x240    /* maximum extended key value */
#define    KEY_CODE_YES   0x241    /* A function key pressed */

#include <unctrl.h>

/*
 * A window an array of __LINE structures pointed to by the 'lines' pointer.
 * A line is an array of __LDATA structures pointed to by the 'line' pointer.
 */

/*
 * Definitions for characters and attributes in __LDATA
 */
#define __CHARTEXT	0x000000ff	/* bits for 8-bit characters */
#define __NORMAL	0x00000000	/* Added characters are normal. */
#define __STANDOUT	0x00000100	/* Added characters are standout. */
#define __UNDERSCORE	0x00000200	/* Added characters are underscored. */
#define __REVERSE	0x00000400	/* Added characters are reverse
					   video. */
#define __BLINK		0x00000800	/* Added characters are blinking. */
#define __DIM		0x00001000	/* Added characters are dim. */
#define __BOLD		0x00002000	/* Added characters are bold. */
#define __BLANK		0x00004000	/* Added characters are blanked. */
#define __PROTECT	0x00008000	/* Added characters are protected. */
#define __ALTCHARSET	0x00010000	/* Added characters are ACS */
#define __COLOR		0x03fe0000	/* Color bits */
#define __ATTRIBUTES	0x03ffff00	/* All 8-bit attribute bits */
#ifdef HAVE_WCHAR
#define __ACS_IS_WACS	0x04000000 /* internal: use wacs table for ACS char */
#endif

typedef struct __ldata __LDATA;
typedef struct __line  __LINE;
typedef struct __window  WINDOW;
typedef struct __screen SCREEN;

/*
 * Attribute definitions
 */
#define	A_NORMAL	__NORMAL
#define	A_STANDOUT	__STANDOUT
#define	A_UNDERLINE	__UNDERSCORE
#define	A_REVERSE	__REVERSE
#define	A_BLINK		__BLINK
#define	A_DIM		__DIM
#define	A_BOLD		__BOLD
#define	A_BLANK		__BLANK
#define	A_INVIS		__BLANK
#define	A_PROTECT	__PROTECT
#define	A_ALTCHARSET	__ALTCHARSET
#define	A_ATTRIBUTES	__ATTRIBUTES
#define	A_CHARTEXT	__CHARTEXT
#define	A_COLOR		__COLOR

#ifdef HAVE_WCHAR
#define WA_ATTRIBUTES	0x03ffffff	/* Wide character attributes mask */
#define WA_STANDOUT	__STANDOUT	/* Best highlighting mode */
#define WA_UNDERLINE	__UNDERSCORE	/* Underlining */
#define WA_REVERSE	__REVERSE	/* Reverse video */
#define WA_BLINK	__BLINK		/* Blinking */
#define WA_DIM		__DIM		/* Half bright */
#define WA_BOLD		__BOLD		/* Extra bright or bold */
#define WA_INVIS	__BLANK		/* Invisible */
#define WA_PROTECT	__PROTECT	/* Protected */
#define WA_ALTCHARSET	__ALTCHARSET	/* Alternate character set */
#define WA_LOW		0x00000002	/* Low highlight */
#define WA_TOP		0x00000004	/* Top highlight */
#define WA_HORIZONTAL	0x00000008	/* Horizontal highlight */
#define WA_VERTICAL	0x00000010	/* Vertical highlight */
#define WA_LEFT		0x00000020	/* Left highlight */
#define WA_RIGHT	0x00000040	/* Right highlight */
#endif /* HAVE_WCHAR */

/*
 * Alternate character set definitions
 */

#define	NUM_ACS	128

extern chtype _acs_char[NUM_ACS];
#ifdef __cplusplus
#define __UC_CAST(a)	static_cast<unsigned char>(a)
#else
#define __UC_CAST(a)	(unsigned char)(a)
#endif

/* Standard definitions */
#define	ACS_RARROW	_acs_char[__UC_CAST('+')]
#define	ACS_LARROW	_acs_char[__UC_CAST(',')]
#define	ACS_UARROW	_acs_char[__UC_CAST('-')]
#define	ACS_DARROW	_acs_char[__UC_CAST('.')]
#define	ACS_BLOCK	_acs_char[__UC_CAST('0')]
#define	ACS_DIAMOND	_acs_char[__UC_CAST('`')]
#define	ACS_CKBOARD	_acs_char[__UC_CAST('a')]
#define	ACS_DEGREE	_acs_char[__UC_CAST('f')]
#define	ACS_PLMINUS	_acs_char[__UC_CAST('g')]
#define	ACS_BOARD	_acs_char[__UC_CAST('h')]
#define	ACS_LANTERN	_acs_char[__UC_CAST('i')]
#define	ACS_LRCORNER	_acs_char[__UC_CAST('j')]
#define	ACS_URCORNER	_acs_char[__UC_CAST('k')]
#define	ACS_ULCORNER	_acs_char[__UC_CAST('l')]
#define	ACS_LLCORNER	_acs_char[__UC_CAST('m')]
#define	ACS_PLUS	_acs_char[__UC_CAST('n')]
#define	ACS_HLINE	_acs_char[__UC_CAST('q')]
#define	ACS_S1		_acs_char[__UC_CAST('o')]
#define	ACS_S9		_acs_char[__UC_CAST('s')]
#define	ACS_LTEE	_acs_char[__UC_CAST('t')]
#define	ACS_RTEE	_acs_char[__UC_CAST('u')]
#define	ACS_BTEE	_acs_char[__UC_CAST('v')]
#define	ACS_TTEE	_acs_char[__UC_CAST('w')]
#define	ACS_VLINE	_acs_char[__UC_CAST('x')]
#define	ACS_BULLET	_acs_char[__UC_CAST('~')]

/* Extensions */
#define	ACS_S3		_acs_char[__UC_CAST('p')]
#define	ACS_S7		_acs_char[__UC_CAST('r')]
#define	ACS_LEQUAL	_acs_char[__UC_CAST('y')]
#define	ACS_GEQUAL	_acs_char[__UC_CAST('z')]
#define	ACS_PI		_acs_char[__UC_CAST('{')]
#define	ACS_NEQUAL	_acs_char[__UC_CAST('|')]
#define	ACS_STERLING	_acs_char[__UC_CAST('}')]

#ifdef HAVE_WCHAR
extern cchar_t _wacs_char[NUM_ACS];

#define	WACS_RARROW     (&_wacs_char[(unsigned char)'+'])
#define	WACS_LARROW     (&_wacs_char[(unsigned char)','])
#define	WACS_UARROW     (&_wacs_char[(unsigned char)'-'])
#define	WACS_DARROW     (&_wacs_char[(unsigned char)'.'])
#define	WACS_BLOCK      (&_wacs_char[(unsigned char)'0'])
#define	WACS_DIAMOND    (&_wacs_char[(unsigned char)'`'])
#define	WACS_CKBOARD    (&_wacs_char[(unsigned char)'a'])
#define	WACS_DEGREE     (&_wacs_char[(unsigned char)'f'])
#define	WACS_PLMINUS    (&_wacs_char[(unsigned char)'g'])
#define	WACS_BOARD      (&_wacs_char[(unsigned char)'h'])
#define	WACS_LANTERN    (&_wacs_char[(unsigned char)'i'])
#define	WACS_LRCORNER   (&_wacs_char[(unsigned char)'j'])
#define	WACS_URCORNER   (&_wacs_char[(unsigned char)'k'])
#define	WACS_ULCORNER   (&_wacs_char[(unsigned char)'l'])
#define	WACS_LLCORNER   (&_wacs_char[(unsigned char)'m'])
#define	WACS_PLUS       (&_wacs_char[(unsigned char)'n'])
#define	WACS_HLINE      (&_wacs_char[(unsigned char)'q'])
#define	WACS_S1         (&_wacs_char[(unsigned char)'o'])
#define	WACS_S9         (&_wacs_char[(unsigned char)'s'])
#define	WACS_LTEE       (&_wacs_char[(unsigned char)'t'])
#define	WACS_RTEE       (&_wacs_char[(unsigned char)'u'])
#define	WACS_BTEE       (&_wacs_char[(unsigned char)'v'])
#define	WACS_TTEE       (&_wacs_char[(unsigned char)'w'])
#define	WACS_VLINE      (&_wacs_char[(unsigned char)'x'])
#define	WACS_BULLET     (&_wacs_char[(unsigned char)'~'])
#define	WACS_S3		(&_wacs_char[(unsigned char)'p'])
#define	WACS_S7		(&_wacs_char[(unsigned char)'r'])
#define	WACS_LEQUAL	(&_wacs_char[(unsigned char)'y'])
#define	WACS_GEQUAL	(&_wacs_char[(unsigned char)'z'])
#define	WACS_PI		(&_wacs_char[(unsigned char)'{'])
#define	WACS_NEQUAL	(&_wacs_char[(unsigned char)'|'])
#define	WACS_STERLING	(&_wacs_char[(unsigned char)'}'])
#endif /* HAVE_WCHAR */

/* System V compatibility */
#define	ACS_SBBS	ACS_LRCORNER
#define	ACS_BBSS	ACS_URCORNER
#define	ACS_BSSB	ACS_ULCORNER
#define	ACS_SSBB	ACS_LLCORNER
#define	ACS_SSSS	ACS_PLUS
#define	ACS_BSBS	ACS_HLINE
#define	ACS_SSSB	ACS_LTEE
#define	ACS_SBSS	ACS_RTEE
#define	ACS_SSBS	ACS_BTEE
#define	ACS_BSSS	ACS_TTEE
#define	ACS_SBSB	ACS_VLINE
#define	_acs_map	_acs_char

/*
 * Color definitions (ANSI color numbers)
 */

#define	COLOR_BLACK	0x00
#define	COLOR_RED	0x01
#define	COLOR_GREEN	0x02
#define	COLOR_YELLOW	0x03
#define	COLOR_BLUE	0x04
#define	COLOR_MAGENTA	0x05
#define	COLOR_CYAN	0x06
#define	COLOR_WHITE	0x07

#ifdef __cplusplus
#define __UINT32_CAST(a)	static_cast<u_int32_t>(a)
#else
#define __UINT32_CAST(a)	(u_int32_t)(a)
#endif
#define	COLOR_PAIR(n)	(((__UINT32_CAST(n)) << 17) & A_COLOR)
#define	PAIR_NUMBER(n)	(((__UINT32_CAST(n)) & A_COLOR) >> 17)

/* Curses external declarations. */
extern WINDOW	*curscr;		/* Current screen. */
extern WINDOW	*stdscr;		/* Standard screen. */

extern int	__tcaction;		/* If terminal hardware set. */

extern int	 COLS;			/* Columns on the screen. */
extern int	 LINES;			/* Lines on the screen. */
extern int	 COLORS;		/* Max colors on the screen. */
extern int	 COLOR_PAIRS;		/* Max color pairs on the screen. */

extern int	 ESCDELAY;		/* Delay between keys in esc seq's. */

#ifndef OK
#define	ERR	(-1)			/* Error return. */
#define	OK	(0)			/* Success return. */
#endif

/*
 * The following have, traditionally, been macros but X/Open say they
 * need to be functions.  Keep the old macros for debugging.
 */
#ifdef _CURSES_USE_MACROS
/* Standard screen pseudo functions. */
#define	addbytes(s, n)			__waddbytes(stdscr, s, n, 0)
#define	addch(ch)			waddch(stdscr, ch)
#define	addchnstr(s)			waddchnstr(stdscr, s, n)
#define	addchstr(s)			waddchnstr(stdscr, s, -1)
#define	addnstr(s, n)			waddnstr(stdscr, s, n)
#define	addstr(s)			waddnstr(stdscr, s, -1)
#define attr_get(a, p, o)		wattr_get(stdscr, a, p, o)
#define attr_off(a, o)			wattr_off(stdscr, a, o)
#define attr_on(a, o)			wattr_on(stdscr, a, o)
#define attr_set(a, p, o)		wattr_set(stdscr, a, p, o)
#define	attroff(attr)			wattroff(stdscr, attr)
#define	attron(attr)			wattron(stdscr, attr)
#define	attrset(attr)			wattrset(stdscr, attr)
#define bkgd(ch)			wbkgd(stdscr, ch)
#define bkgdset(ch)			wbkgdset(stdscr, ch)
#define	border(l, r, t, b, tl, tr, bl, br) \
	wborder(stdscr, l, r, t, b, tl, tr, bl, br)
#define	clear()				wclear(stdscr)
#define	clrtobot()			wclrtobot(stdscr)
#define	clrtoeol()			wclrtoeol(stdscr)
#define color_set(c, o)			wcolor_set(stdscr, c, o)
#define	delch()				wdelch(stdscr)
#define	deleteln()			wdeleteln(stdscr)
#define	echochar(c)			wechochar(stdscr, c)
#define	erase()				werase(stdscr)
#define	getch()				wgetch(stdscr)
#define	getnstr(s, n)			wgetnstr(stdscr, s, n)
#define	getstr(s)			wgetstr(stdscr, s)
#define	inch()				winch(stdscr)
#define	inchnstr(c)			winchnstr(stdscr, c)
#define	inchstr(c)			winchstr(stdscr, c)
#define	innstr(s, n)			winnstr(stdscr, s, n)
#define	insch(ch)			winsch(stdscr, ch)
#define	insdelln(n)			winsdelln(stdscr, n)
#define	insertln()			winsertln(stdscr)
#define	instr(s)			winstr(stdscr, s)
#define	move(y, x)			wmove(stdscr, y, x)
#define	refresh()			wrefresh(stdscr)
#define	scrl(n)				wscrl(stdscr, n)
#define	setscrreg(t, b)			wsetscrreg(stdscr, t, b)
#define	standend()			wstandend(stdscr)
#define	standout()			wstandout(stdscr)
#define	timeout(delay)			wtimeout(stdscr, delay)
#define	underscore()			wunderscore(stdscr)
#define	underend()			wunderend(stdscr)
#define	waddbytes(w, s, n)		__waddbytes(w, s, n, 0)
#define	waddstr(w, s)			waddnstr(w, s, -1)

/* Standard screen plus movement pseudo functions. */
#define	mvaddbytes(y, x, s, n)		mvwaddbytes(stdscr, y, x, s, n)
#define	mvaddch(y, x, ch)		mvwaddch(stdscr, y, x, ch)
#define	mvaddchnstr(y, x, s, n)		mvwaddchnstr(stdscr, y, x, s, n)
#define	mvaddchstr(y, x, s)		mvwaddchstr(stdscr, y, x, s)
#define	mvaddnstr(y, x, s, n)		mvwaddnstr(stdscr, y, x, s, n)
#define	mvaddstr(y, x, s)		mvwaddstr(stdscr, y, x, s)
#define	mvdelch(y, x)			mvwdelch(stdscr, y, x)
#define	mvgetch(y, x)			mvwgetch(stdscr, y, x)
#define	mvgetnstr(y, x, s)		mvwgetnstr(stdscr, y, x, s, n)
#define	mvgetstr(y, x, s)		mvwgetstr(stdscr, y, x, s)
#define	mvinch(y, x)			mvwinch(stdscr, y, x)
#define	mvinchnstr(y, x, c, n)		mvwinchnstr(stdscr, y, x, c, n)
#define	mvinchstr(y, x, c)		mvwinchstr(stdscr, y, x, c)
#define	mvinnstr(y, x, s, n)		mvwinnstr(stdscr, y, x, s, n)
#define	mvinsch(y, x, c)		mvwinsch(stdscr, y, x, c)
#define	mvinstr(y, x, s)		mvwinstr(stdscr, y, x, s)
#define	mvwaddbytes(w, y, x, s, n) \
	(wmove(w, y, x) == ERR ? ERR : __waddbytes(w, s, n, 0))
#define	mvwaddch(w, y, x, ch) \
	(wmove(w, y, x) == ERR ? ERR : waddch(w, ch))
#define	mvwaddchnstr(w, y, x, s, n) \
	(wmove(w, y, x) == ERR ? ERR : waddchnstr(w, s, n))
#define	mvwaddchstr(w, y, x, s) \
	(wmove(w, y, x) == ERR ? ERR : waddchnstr(w, s, -1))
#define	mvwaddnstr(w, y, x, s, n) \
	(wmove(w, y, x) == ERR ? ERR : waddnstr(w, s, n))
#define	mvwaddstr(w, y, x, s) \
	(wmove(w, y, x) == ERR ? ERR : waddnstr(w, s, -1))
#define	mvwdelch(w, y, x) \
	(wmove(w, y, x) == ERR ? ERR : wdelch(w))
#define	mvwgetch(w, y, x) \
	(wmove(w, y, x) == ERR ? ERR : wgetch(w))
#define	mvwgetnstr(w, y, x, s, n) \
	(wmove(w, y, x) == ERR ? ERR : wgetnstr(w, s, n))
#define	mvwgetstr(w, y, x, s) \
	(wmove(w, y, x) == ERR ? ERR : wgetstr(w, s))
#define	mvwinch(w, y, x) \
	(wmove(w, y, x) == ERR ? ERR : winch(w))
#define	mvwinchnstr(w, y, x, c, n) \
	(wmove(w, y, x) == ERR ? ERR : winchnstr(w, c, n))
#define	mvwinchstr(w, y, x, s) \
	(wmove(w, y, x) == ERR ? ERR : winchstr(w, c))
#define	mvwinnstr(w, y, x, s, n) \
	(wmove(w, y, x) == ERR ? ERR : winnstr(w, s, n))
#define	mvwinsch(w, y, x, c) \
	(wmove(w, y, x) == ERR ? ERR : winsch(w, c))
#define	mvwinstr(w, y, x, s) \
	(wmove(w, y, x) == ERR ? ERR : winstr(w, s))

/* Miscellaneous. */
#define	noqiflush()		intrflush(stdscr, FALSE)
#define	qiflush()		intrflush(stdscr, TRUE)

#else
/* Use functions not macros... */
__BEGIN_DECLS
int	 addbytes(const char *, int);
int	 addch(chtype);
int	 addchnstr(const chtype *, int);
int	 addchstr(const chtype *);
int	 addnstr(const char *, int);
int	 addstr(const char *);
int	 attr_get(attr_t *, short *, void *);
int	 attr_off(attr_t, void *);
int	 attr_on(attr_t, void *);
int	 attr_set(attr_t, short, void *);
int	 attroff(int);
int	 attron(int);
int	 attrset(int);
int	 bkgd(chtype);
void	 bkgdset(chtype);
int	 border(chtype, chtype, chtype, chtype,
	   chtype, chtype, chtype, chtype);
int	 clear(void);
int	 clrtobot(void);
int	 clrtoeol(void);
int	 color_set(short, void *);
int	 delch(void);
int	 deleteln(void);
int	 echochar(const chtype);
int	 erase(void);
int	 getch(void);
int	 getnstr(char *, int);
int	 getstr(char *);
chtype	 inch(void);
int	 inchnstr(chtype *, int);
int	 inchstr(chtype *);
int	 innstr(char *, int);
int	 insch(chtype);
int	 insdelln(int);
int	 insertln(void);
int	 instr(char *);
int	 move(int, int);
int	 refresh(void);
int	 scrl(int);
int	 setscrreg(int, int);
int	 standend(void);
int	 standout(void);
void	 timeout(int);
int	 underscore(void);
int	 underend(void);
int	 waddbytes(WINDOW *, const char *, int);
int	 waddstr(WINDOW *, const char *);

/* Standard screen plus movement functions. */
int	 mvaddbytes(int, int, const char *, int);
int	 mvaddch(int, int, chtype);
int	 mvaddchnstr(int, int, const chtype *, int);
int	 mvaddchstr(int, int, const chtype *);
int	 mvaddnstr(int, int, const char *, int);
int	 mvaddstr(int, int, const char *);
int	 mvdelch(int, int);
int	 mvgetch(int, int);
int	 mvgetnstr(int, int, char *, int);
int	 mvgetstr(int, int, char *);
chtype	 mvinch(int, int);
int	 mvinchnstr(int, int, chtype *, int);
int	 mvinchstr(int, int, chtype *);
int	 mvinnstr(int, int, char *, int);
int	 mvinsch(int, int, chtype);
int	 mvinstr(int, int, char *);

int	 mvwaddbytes(WINDOW *, int, int, const char *, int);
int	 mvwaddch(WINDOW *, int, int, chtype);
int	 mvwaddchnstr(WINDOW *, int, int, const chtype *, int);
int	 mvwaddchstr(WINDOW *, int, int, const chtype *);
int	 mvwaddnstr(WINDOW *, int, int, const char *, int);
int	 mvwaddstr(WINDOW *, int, int, const char *);
int	 mvwdelch(WINDOW *, int, int);
int	 mvwgetch(WINDOW *, int, int);
int	 mvwgetnstr(WINDOW *, int, int, char *, int);
int	 mvwgetstr(WINDOW *, int, int, char *);
chtype	 mvwinch(WINDOW *, int, int);
int	 mvwinsch(WINDOW *, int, int, chtype);
__END_DECLS
#endif /* _CURSES_USE_MACROS */

#define	getyx(w, y, x)		(y) = getcury(w), (x) = getcurx(w)
#define	getbegyx(w, y, x)	(y) = getbegy(w), (x) = getbegx(w)
#define	getmaxyx(w, y, x)	(y) = getmaxy(w), (x) = getmaxx(w)
#define	getparyx(w, y, x)	(y) = getpary(w), (x) = getparx(w)

/* Public function prototypes. */
__BEGIN_DECLS
int	 assume_default_colors(short, short);
int	 baudrate(void);
int	 beep(void);
int	 box(WINDOW *, chtype, chtype);
bool	 can_change_color(void);
int	 cbreak(void);
int	 clearok(WINDOW *, bool);
int	 color_content(short, short *, short *, short *);
int	 copywin(const WINDOW *, WINDOW *, int, int, int, int, int, int, int);
int	 curs_set(int);
int	 def_prog_mode(void);
int	 def_shell_mode(void);
int      define_key(char *, int);
int	 delay_output(int);
void     delscreen(SCREEN *);
int	 delwin(WINDOW *);
WINDOW	*derwin(WINDOW *, int, int, int, int);
WINDOW	*dupwin(WINDOW *);
int	 doupdate(void);
int	 echo(void);
int	 endwin(void);
char     erasechar(void);
int	 flash(void);
int	 flushinp(void);
int	 flushok(WINDOW *, bool);
char	*fullname(const char *, char *);
chtype	 getattrs(WINDOW *);
chtype	 getbkgd(WINDOW *);
int	 getcury(WINDOW *);
int	 getcurx(WINDOW *);
int	 getbegy(WINDOW *);
int	 getbegx(WINDOW *);
int	 getmaxy(WINDOW *);
int	 getmaxx(WINDOW *);
int	 getpary(WINDOW *);
int	 getparx(WINDOW *);
int	 gettmode(void);
WINDOW	*getwin(FILE *);
int	 halfdelay(int);
bool	 has_colors(void);
bool	 has_ic(void);
bool	 has_il(void);
int	 hline(chtype, int);
int	 idcok(WINDOW *, bool);
int	 idlok(WINDOW *, bool);
int	 init_color(short, short, short, short);
int	 init_pair(short, short, short);
WINDOW	*initscr(void);
int	 intrflush(WINDOW *, bool);
bool	 isendwin(void);
bool	 is_linetouched(WINDOW *, int);
bool	 is_wintouched(WINDOW *);
int      keyok(int, bool);
int	 keypad(WINDOW *, bool);
char	*keyname(int);
char     killchar(void);
int	 leaveok(WINDOW *, bool);
int	 meta(WINDOW *, bool);
int	 mvcur(int, int, int, int);
int      mvderwin(WINDOW *, int, int);
int	 mvhline(int, int, chtype, int);
int	 mvprintw(int, int, const char *, ...) __printflike(3, 4);
int	 mvscanw(int, int, const char *, ...) __scanflike(3, 4);
int	 mvvline(int, int, chtype, int);
int	 mvwhline(WINDOW *, int, int, chtype, int);
int	 mvwvline(WINDOW *, int, int, chtype, int);
int	 mvwin(WINDOW *, int, int);
int	 mvwinchnstr(WINDOW *, int, int, chtype *, int);
int	 mvwinchstr(WINDOW *, int, int, chtype *);
int	 mvwinnstr(WINDOW *, int, int, char *, int);
int	 mvwinstr(WINDOW *, int, int, char *);
int	 mvwprintw(WINDOW *, int, int, const char *, ...) __printflike(4, 5);
int	 mvwscanw(WINDOW *, int, int, const char *, ...) __scanflike(4, 5);
int	 napms(int);
WINDOW	*newpad(int, int);
SCREEN  *newterm(char *, FILE *, FILE *);
WINDOW	*newwin(int, int, int, int);
int	 nl(void);
attr_t	 no_color_attributes(void);
int	 nocbreak(void);
int	 nodelay(WINDOW *, bool);
int	 noecho(void);
int	 nonl(void);
void	 noqiflush(void);
int	 noraw(void);
int	 notimeout(WINDOW *, bool);
int	 overlay(const WINDOW *, WINDOW *);
int	 overwrite(const WINDOW *, WINDOW *);
int	 pair_content(short, short *, short *);
int	 pechochar(WINDOW *, const chtype);
int	 pnoutrefresh(WINDOW *, int, int, int, int, int, int);
int	 prefresh(WINDOW *, int, int, int, int, int, int);
int	 printw(const char *, ...) __printflike(1, 2);
int	 putwin(WINDOW *, FILE *);
void	 qiflush(void);
int	 raw(void);
int	 redrawwin(WINDOW *);
int	 reset_prog_mode(void);
int	 reset_shell_mode(void);
int	 resetty(void);
int      resizeterm(int, int);
int	 savetty(void);
int	 scanw(const char *, ...) __scanflike(1, 2);
int	 scroll(WINDOW *);
int	 scrollok(WINDOW *, bool);
int	 setterm(char *);
SCREEN  *set_term(SCREEN *);
int	 start_color(void);
WINDOW	*subpad(WINDOW *, int, int, int, int);
WINDOW	*subwin(WINDOW *, int, int, int, int);
chtype	 termattrs(void);
attr_t	 term_attrs(void);
int	 touchline(WINDOW *, int, int);
int	 touchoverlap(WINDOW *, WINDOW *);
int	 touchwin(WINDOW *);
int	 ungetch(int);
int	 untouchwin(WINDOW *);
int	 use_default_colors(void);
int	 vline(chtype, int);
int	 vw_printw(WINDOW *, const char *, __va_list) __printflike(2, 0);
int	 vw_scanw(WINDOW *, const char *, __va_list) __scanflike(2, 0);
int	 vwprintw(WINDOW *, const char *, __va_list) __printflike(2, 0);
int	 vwscanw(WINDOW *, const char *, __va_list) __scanflike(2, 0);
int	 waddch(WINDOW *, chtype);
int	 waddchnstr(WINDOW *, const chtype *, int);
int	 waddchstr(WINDOW *, const chtype *);
int	 waddnstr(WINDOW *, const char *, int);
int	 wattr_get(WINDOW *, attr_t *, short *, void *);
int	 wattr_off(WINDOW *, attr_t, void *);
int	 wattr_on(WINDOW *, attr_t, void *);
int	 wattr_set(WINDOW *, attr_t, short, void *);
int	 wattroff(WINDOW *, int);
int	 wattron(WINDOW *, int);
int	 wattrset(WINDOW *, int);
int	 wbkgd(WINDOW *, chtype);
void	 wbkgdset(WINDOW *, chtype);
int	 wborder(WINDOW *, chtype, chtype, chtype, chtype, chtype, chtype,
		chtype, chtype);
int	 wclear(WINDOW *);
int	 wclrtobot(WINDOW *);
int	 wclrtoeol(WINDOW *);
int	 wcolor_set(WINDOW *, short, void *);
void	 wcursyncup(WINDOW *);
int	 wdelch(WINDOW *);
int	 wdeleteln(WINDOW *);
int	 wechochar(WINDOW *, const chtype);
int	 werase(WINDOW *);
int	 wgetch(WINDOW *);
int	 wgetnstr(WINDOW *, char *, int);
int	 wgetstr(WINDOW *, char *);
int	 whline(WINDOW *, chtype, int);
chtype	 winch(WINDOW *);
int	 winchnstr(WINDOW *, chtype *, int);
int	 winchstr(WINDOW *, chtype *);
int	 winnstr(WINDOW *, char *, int);
int	 winsch(WINDOW *, chtype);
int	 winsdelln(WINDOW *, int);
int	 winsertln(WINDOW *);
int	 winstr(WINDOW *, char *);
int	 wmove(WINDOW *, int, int);
int	 wnoutrefresh(WINDOW *);
int	 wprintw(WINDOW *, const char *, ...)  __printflike(2, 3);
int	 wredrawln(WINDOW *, int, int);
int	 wrefresh(WINDOW *);
int      wresize(WINDOW *, int, int);
int	 wscanw(WINDOW *, const char *, ...) __scanflike(2, 3);
int	 wscrl(WINDOW *, int);
int	 wsetscrreg(WINDOW *, int, int);
int	 wstandend(WINDOW *);
int	 wstandout(WINDOW *);
void	 wsyncdown(WINDOW *);
void	 wsyncup(WINDOW *);
void	 wtimeout(WINDOW *, int);
int	 wtouchln(WINDOW *, int, int, int);
int	 wunderend(WINDOW *);
int	 wunderscore(WINDOW *);
int	 wvline(WINDOW *, chtype, int);

int insnstr(const char *, int);
int insstr(const char *);
int mvinsnstr(int, int, const char *, int);
int mvinsstr(int, int, const char *);
int mvwinsnstr(WINDOW *, int, int, const char *, int);
int mvwinsstr(WINDOW *, int, int, const char *);
int winsnstr(WINDOW *, const char *, int);
int winsstr(WINDOW *, const char *);

int chgat(int, attr_t, short, const void *);
int wchgat(WINDOW *, int, attr_t, short, const void *);
int mvchgat(int, int, int, attr_t, short, const void *);
int mvwchgat(WINDOW *, int, int, int, attr_t, short, const void *);

/* wide-character support routines */
/* return ERR when HAVE_WCHAR is not defined */
/* add */
int add_wch(const cchar_t *);
int wadd_wch(WINDOW *, const cchar_t *);
int mvadd_wch(int, int, const cchar_t *);
int mvwadd_wch(WINDOW *, int, int, const cchar_t *);

int add_wchnstr(const cchar_t *, int);
int add_wchstr(const cchar_t *);
int wadd_wchnstr(WINDOW *, const cchar_t *, int);
int wadd_wchstr(WINDOW *, const cchar_t *);
int mvadd_wchnstr(int, int, const cchar_t *, int);
int mvadd_wchstr(int, int, const cchar_t *);
int mvwadd_wchnstr(WINDOW *, int, int, const cchar_t *, int);
int mvwadd_wchstr(WINDOW *, int, int, const cchar_t *);

int addnwstr(const wchar_t *, int);
int addwstr(const wchar_t *);
int mvaddnwstr(int, int x, const wchar_t *, int);
int mvaddwstr(int, int x, const wchar_t *);
int mvwaddnwstr(WINDOW *, int, int, const wchar_t *, int);
int mvwaddwstr(WINDOW *, int, int, const wchar_t *);
int waddnwstr(WINDOW *, const wchar_t *, int);
int waddwstr(WINDOW *, const wchar_t *);

int echo_wchar(const cchar_t *);
int wecho_wchar(WINDOW *, const cchar_t *);
int pecho_wchar(WINDOW *, const cchar_t *);

/* insert */
int ins_wch(const cchar_t *);
int wins_wch(WINDOW *, const cchar_t *);
int mvins_wch(int, int, const cchar_t *);
int mvwins_wch(WINDOW *, int, int, const cchar_t *);

int ins_nwstr(const wchar_t *, int);
int ins_wstr(const wchar_t *);
int mvins_nwstr(int, int, const wchar_t *, int);
int mvins_wstr(int, int, const wchar_t *);
int mvwins_nwstr(WINDOW *, int, int, const wchar_t *, int);
int mvwins_wstr(WINDOW *, int, int, const wchar_t *);
int wins_nwstr(WINDOW *, const wchar_t *, int);
int wins_wstr(WINDOW *, const wchar_t *);

/* input */
int get_wch(wint_t *);
int unget_wch(const wchar_t);
int mvget_wch(int, int, wint_t *);
int mvwget_wch(WINDOW *, int, int, wint_t *);
int wget_wch(WINDOW *, wint_t *);

int getn_wstr(wchar_t *, int);
int get_wstr(wchar_t *);
int mvgetn_wstr(int, int, wchar_t *, int);
int mvget_wstr(int, int, wchar_t *);
int mvwgetn_wstr(WINDOW *, int, int, wchar_t *, int);
int mvwget_wstr(WINDOW *, int, int, wchar_t *);
int wgetn_wstr(WINDOW *, wchar_t *, int);
int wget_wstr(WINDOW *, wchar_t *);

int in_wch(cchar_t *);
int mvin_wch(int, int, cchar_t *);
int mvwin_wch(WINDOW *, int, int, cchar_t *);
int win_wch(WINDOW *, cchar_t *);

int in_wchnstr(cchar_t *, int);
int in_wchstr(cchar_t *);
int mvin_wchnstr(int, int, cchar_t *, int);
int mvin_wchstr(int, int, cchar_t *);
int mvwin_wchnstr(WINDOW *, int, int, cchar_t *, int);
int mvwin_wchstr(WINDOW *, int, int, cchar_t *);
int win_wchnstr(WINDOW *, cchar_t *, int);
int win_wchstr(WINDOW *, cchar_t *);

int innwstr(wchar_t *, int);
int inwstr(wchar_t *);
int mvinnwstr(int, int, wchar_t *, int);
int mvinwstr(int, int, wchar_t *);
int mvwinnwstr(WINDOW *, int, int, wchar_t *, int);
int mvwinwstr(WINDOW *, int, int, wchar_t *);
int winnwstr(WINDOW *, wchar_t *, int);
int winwstr(WINDOW *, wchar_t *);

/* cchar handlgin */
int setcchar(cchar_t *, const wchar_t *, const attr_t, short, const void *);
int getcchar(const cchar_t *, wchar_t *, attr_t *, short *, void *);

/* misc */
char *key_name( wchar_t );
int border_set(const cchar_t *, const cchar_t *, const cchar_t *,
               const cchar_t *, const cchar_t *, const cchar_t *,
               const cchar_t *, const cchar_t *);
int wborder_set(WINDOW *, const cchar_t *, const cchar_t *,
                const cchar_t *, const cchar_t *, const cchar_t *, 
                const cchar_t *, const cchar_t *, const cchar_t *);
int box_set(WINDOW *, const cchar_t *, const cchar_t *);
int erasewchar(wchar_t *);
int killwchar(wchar_t *);
int hline_set(const cchar_t *, int);
int mvhline_set(int, int, const cchar_t *, int);
int mvvline_set(int, int, const cchar_t *, int);
int mvwhline_set(WINDOW *, int, int, const cchar_t *, int);
int mvwvline_set(WINDOW *, int, int, const cchar_t *, int);
int vline_set(const cchar_t *, int);
int whline_set(WINDOW *, const cchar_t *, int);
int wvline_set(WINDOW *, const cchar_t *, int);
int bkgrnd(const cchar_t *);
void bkgrndset(const cchar_t *);
int getbkgrnd(cchar_t *);
int wbkgrnd(WINDOW *, const cchar_t *);
void wbkgrndset(WINDOW *, const cchar_t *);
int wgetbkgrnd(WINDOW *, cchar_t *);

/* Private functions that are needed for user programs prototypes. */
int	 __cputchar(int);
int	 __waddbytes(WINDOW *, const char *, int, attr_t);
#ifdef HAVE_WCHAR
int __cputwchar( wchar_t );
#endif /* HAVE_WCHAR */
__END_DECLS

#endif /* !_CURSES_H_ */
