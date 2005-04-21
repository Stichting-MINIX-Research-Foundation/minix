/* curses.h - defines macros and prototypes for curses */

#ifndef _CURSES_H
#define _CURSES_H

#include <termios.h>
#include <stdarg.h>
#include <stdio.h>

typedef int bool;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef ERR
#define ERR (-1)	/* general error flag */
#endif
#ifndef OK
#define OK 0		/* general OK flag */
#endif

/* Macros. */
#define box(win,vc,hc) wbox(win,0,0,0,0,vc,hc)
#define addch(ch) waddch(stdscr,ch)
#define mvaddch(y,x,ch) (wmove(stdscr,y,x)==ERR?ERR:waddch(stdscr,ch))
#define mvwaddch(win,y,x,ch) (wmove(win,y,x)==ERR?ERR:waddch(win,ch))
#define getch() wgetch(stdscr)
#define mvgetch(y,x) (wmove(stdscr,y,x)==ERR?ERR:wgetch(stdscr))
#define mvwgetch(win,y,x) (wmove(win,y,x)==ERR?ERR:wgetch(win))
#define addstr(str) waddstr(stdscr,str)
#define mvaddstr(y,x,str) (wmove(stdscr,y,x)==ERR?ERR:waddstr(stdscr,str))
#define mvwaddstr(win,y,x,str) (wmove(win,y,x)==ERR?ERR:waddstr(win,str))
#define getstr(str) wgetstr(stdscr,str)
#define mvgetstr(y,x,str) (wmove(stdscr,y,x)==ERR?ERR:wgetstr(stdscr,str))
#define mvwgetstr(win,y,x,str) (wmove(win,y,x)==ERR?ERR:wgetstr(win,str))
#define move(y,x) wmove(stdscr,y,x)
#define clear() wclear(stdscr)
#define erase() werase(stdscr)
#define clrtobot() wclrtobot(stdscr)
#define mvclrtobot(y,x) (wmove(stdscr,y,x)==ERR?ERR:wclrtobot(stdscr))
#define mvwclrtobot(win,y,x) (wmove(win,y,x)==ERR?ERR:wclrtobot(win))
#define clrtoeol() wclrtoeol(stdscr)
#define mvclrtoeol(y,x) (wmove(stdscr,y,x)==ERR?ERR:wclrtoeol(stdscr))
#define mvwclrtoeol(win,y,x) (wmove(win,y,x)==ERR?ERR:wclrtoeol(win))
#define insertln() winsertln(stdscr)
#define mvinsertln(y,x) (wmove(stdscr,y,x)==ERR?ERR:winsertln(stdscr))
#define mvwinsertln(win,y,x) (wmove(win,y,x)==ERR?ERR:winsertln(win))
#define deleteln() wdeleteln(stdscr)
#define mvdeleteln(y,x) (wmove(stdscr,y,x)==ERR?ERR:wdeleteln(stdscr))
#define mvwdeleteln(win,y,x) (wmove(win,y,x)==ERR?ERR:wdeleteln(win))
#define refresh() wrefresh(stdscr)
#define inch() winch(stdscr)
#define insch(ch) winsch(stdscr,ch)
#define mvinsch(y,x,ch) (wmove(stdscr,y,x)==ERR?ERR:winsch(stdscr,ch))
#define mvwinsch(win,y,x,ch) (wmove(win,y,x)==ERR?ERR:winsch(win,ch))
#define delch() wdelch(stdscr)
#define mvdelch(y,x) (wmove(stdscr,y,x)==ERR?ERR:wdelch(stdscr))
#define mvwdelch(win,y,x) (wmove(win,y,x)==ERR?ERR:wdelch(win))
#define standout() wstandout(stdscr)
#define wstandout(win) ((win)->_attrs |= A_STANDOUT)
#define standend() wstandend(stdscr)
#define wstandend(win) ((win)->_attrs &= ~A_STANDOUT)
#define attrset(attrs) wattrset(stdscr, attrs)
#define wattrset(win, attrs) ((win)->_attrs = (attrs))
#define attron(attrs) wattron(stdscr, attrs)
#define wattron(win, attrs) ((win)->_attrs |= (attrs))
#define attroff(attrs) wattroff(stdscr,attrs)
#define wattroff(win, attrs) ((win)->_attrs &= ~(attrs))
#define resetty() tcsetattr(1, TCSANOW, &_orig_tty)
#define getyx(win,y,x) (y = (win)->_cury, x = (win)->_curx)

/* Video attribute definitions. */
#define	A_BLINK        0x0100
#define	A_BLANK        0
#define	A_BOLD         0x0200
#define	A_DIM          0
#define	A_PROTECT      0
#define	A_REVERSE      0x0400
#define	A_STANDOUT     0x0800
#define	A_UNDERLINE    0x1000
#define	A_ALTCHARSET   0x2000

/* Type declarations. */
typedef struct {
  int	   _cury;			/* current pseudo-cursor */
  int	   _curx;
  int      _maxy;			/* max coordinates */
  int      _maxx;
  int      _begy;			/* origin on screen */
  int      _begx;
  int	   _flags;			/* window properties */
  int	   _attrs;			/* attributes of written characters */
  int      _tabsize;			/* tab character size */
  bool	   _clear;			/* causes clear at next refresh */
  bool	   _leave;			/* leaves cursor as it happens */
  bool	   _scroll;			/* allows window scrolling */
  bool	   _nodelay;			/* input character wait flag */
  bool	   _keypad;			/* flags keypad key mode active */
  int    **_line;			/* pointer to line pointer array */
  int	  *_minchng;			/* First changed character in line */
  int	  *_maxchng;			/* Last changed character in line */
  int	   _regtop;			/* Top/bottom of scrolling region */
  int	   _regbottom;
} WINDOW;

/* External variables */
extern int LINES;			/* terminal height */
extern int COLS;			/* terminal width */
extern bool NONL;			/* \n causes CR too ? */
extern WINDOW *curscr;			/* the current screen image */
extern WINDOW *stdscr;			/* the default screen window */
extern struct termios _orig_tty, _tty;

extern unsigned int ACS_ULCORNER;	/* terminal dependent block grafic */
extern unsigned int ACS_LLCORNER;	/* charcters.  Forget IBM, we are */
extern unsigned int ACS_URCORNER;	/* independent of their charset. :-) */
extern unsigned int ACS_LRCORNER;
extern unsigned int ACS_RTEE;
extern unsigned int ACS_LTEE;
extern unsigned int ACS_BTEE;
extern unsigned int ACS_TTEE;
extern unsigned int ACS_HLINE;
extern unsigned int ACS_VLINE;
extern unsigned int ACS_PLUS;
extern unsigned int ACS_S1;
extern unsigned int ACS_S9;
extern unsigned int ACS_DIAMOND;
extern unsigned int ACS_CKBOARD;
extern unsigned int ACS_DEGREE;
extern unsigned int ACS_PLMINUS;
extern unsigned int ACS_BULLET;
extern unsigned int ACS_LARROW;
extern unsigned int ACS_RARROW;
extern unsigned int ACS_DARROW;
extern unsigned int ACS_UARROW;
extern unsigned int ACS_BOARD;
extern unsigned int ACS_LANTERN;
extern unsigned int ACS_BLOCK;

_PROTOTYPE( char *unctrl, (int _c) );
_PROTOTYPE( int baudrate, (void));
_PROTOTYPE( void beep, (void));
_PROTOTYPE( void cbreak, (void));
_PROTOTYPE( void clearok, (WINDOW *_win, bool _flag) );
_PROTOTYPE( void clrscr, (void));
_PROTOTYPE( void curs_set, (int _visibility) );
_PROTOTYPE( void delwin, (WINDOW *_win) );
_PROTOTYPE( void doupdate, (void));
_PROTOTYPE( void echo, (void));
_PROTOTYPE( int endwin, (void));
_PROTOTYPE( int erasechar, (void));
_PROTOTYPE( void fatal, (char *_s) );
_PROTOTYPE( int fixterm, (void));
_PROTOTYPE( void flash, (void));
_PROTOTYPE( void gettmode, (void));
_PROTOTYPE( void idlok, (WINDOW *_win, bool _flag) );
_PROTOTYPE( WINDOW *initscr, (void));
_PROTOTYPE( void keypad, (WINDOW *_win, bool _flag) );
_PROTOTYPE( int killchar, (void));
_PROTOTYPE( void leaveok, (WINDOW *_win, bool _flag) );
_PROTOTYPE( char *longname, (void));
_PROTOTYPE( void meta, (WINDOW *_win, bool _flag) );
_PROTOTYPE( int mvcur, (int _oldy, int _oldx, int _newy, int _newx) );
_PROTOTYPE( int mvinch, (int _y, int _x) );
_PROTOTYPE( int mvprintw, (int _y, int _x, const char *_fmt, ...) );
_PROTOTYPE( int mvscanw, (int _y, int _x, const char *_fmt, ...) );
_PROTOTYPE( int mvwin, (WINDOW *_win, int _begy, int _begx) );
_PROTOTYPE( int mvwinch, (WINDOW *_win, int _y, int _x) );
_PROTOTYPE( int mvwprintw, (WINDOW *_win, int _y, int _x, const char *_fmt,
									...) );
_PROTOTYPE( int mvwscanw, (WINDOW *_win, int _y, int _x, const char *_fmt,
									...) );
_PROTOTYPE( WINDOW *newwin, (int _num_lines, int _num_cols, int _y, int _x));
_PROTOTYPE( void nl, (void));
_PROTOTYPE( void nocbreak, (void));
_PROTOTYPE( void nodelay, (WINDOW *_win, bool _flag) );
_PROTOTYPE( void noecho, (void));
_PROTOTYPE( void nonl, (void));
_PROTOTYPE( void noraw, (void));
_PROTOTYPE( void outc, (int _c) );
_PROTOTYPE( void  overlay, (WINDOW *_win1, WINDOW *_win2) );
_PROTOTYPE( void  overwrite, (WINDOW *_win1, WINDOW *_win2) );
_PROTOTYPE( void poscur, (int _r, int _c) );
_PROTOTYPE( int printw, (const char *_fmt, ...) );
_PROTOTYPE( void raw, (void));
_PROTOTYPE( int resetterm, (void));
_PROTOTYPE( int saveoldterm, (void));
_PROTOTYPE( int saveterm, (void));
_PROTOTYPE( int savetty, (void));
_PROTOTYPE( int scanw, (const char *_fmt, ...) );
_PROTOTYPE( void scroll, (WINDOW *_win) );
_PROTOTYPE( void scrollok, (WINDOW *_win, bool _flag) );
_PROTOTYPE( int setscrreg, (int _top, int _bottom) );
_PROTOTYPE( int setterm, (char *_type) );
_PROTOTYPE( int setupterm, (void));
_PROTOTYPE( WINDOW *subwin, (WINDOW *_orig, int _nlines, int _ncols, int _y,
					int _x));
_PROTOTYPE( int tabsize, (int _ts) );
_PROTOTYPE( void touchwin, (WINDOW *_win) );
_PROTOTYPE( int waddch, (WINDOW *_win, int _c) );
_PROTOTYPE( int waddstr, (WINDOW *_win, char *_str) );
_PROTOTYPE( int wbox, (WINDOW *_win, int _ymin, int _xmin, int _ymax,
				int _xmax, unsigned int _v, unsigned int _h) );
_PROTOTYPE( void wclear, (WINDOW *_win) );
_PROTOTYPE( int wclrtobot, (WINDOW *_win) );
_PROTOTYPE( int wclrtoeol, (WINDOW *_win) );
_PROTOTYPE( int wdelch, (WINDOW *_win) );
_PROTOTYPE( int wdeleteln, (WINDOW *_win) );
_PROTOTYPE( void werase, (WINDOW *_win) );
_PROTOTYPE( int wgetch, (WINDOW *_win) );
_PROTOTYPE( int wgetstr, (WINDOW *_win, char *_str) );
_PROTOTYPE( int winch, (WINDOW *_win) );
_PROTOTYPE( int winsch, (WINDOW *_win, int _c) );
_PROTOTYPE( int winsertln, (WINDOW *_win) );
_PROTOTYPE( int wmove, (WINDOW *_win, int _y, int _x) );
_PROTOTYPE( void wnoutrefresh, (WINDOW *_win) );
_PROTOTYPE( int wprintw, (WINDOW *_win, const char *_fmt, ...));
_PROTOTYPE( void wrefresh, (WINDOW *_win) );
_PROTOTYPE( int wscanw, (WINDOW *_win, const char *_fmt, ...));
_PROTOTYPE( int wsetscrreg, (WINDOW *_win, int _top, int _bottom) );
_PROTOTYPE( int wtabsize, (WINDOW *_win, int _ts) );

#endif /* _CURSES_H */
