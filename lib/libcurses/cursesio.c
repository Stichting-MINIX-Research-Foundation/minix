#include <stdlib.h>
#include <termcap.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <curses.h>
#include "curspriv.h"

struct termios _orig_tty, _tty;
cursv _cursvar;

WINDOW *stdscr, *curscr;
int LINES, COLS;
bool NONL;

char termcap[1024];		/* termcap buffer */
char tc[200];			/* area to hold string capabilities */
char *ttytype;			/* terminal type from env */
static char *arp;		/* pointer for use in tgetstr */
char *cp;			/* character pointer */

char *cl;			/* clear screen capability */
char *cm;			/* cursor motion capability */
char *so;			/* start standout capability */
char *se;			/* end standout capability */
char *mr;			/* start of reverse */
char *me;			/* revert to normal */
char *mb;			/* start of blink */
char *md;			/* start of bold */
char *us;			/* start of underscore */
char *ue;			/* end of underscore */
char *vi;			/* cursor invisible */
char *ve;			/* cursor normal */
char *vs;			/* cursor good visible */
char *as;			/* alternative charset start */
char *ae;			/* alternative charset end */
char *bl;			/* ring the bell */
char *vb;			/* visual bell */

/* fatal - report error and die. Never returns */
void fatal(s)
char *s;
{
  (void) fprintf(stderr, "curses: %s\n", s);
  exit(1);
}

/* Outc - call putchar, necessary because putchar is a macro. */
void outc(c)
int c;
{
  putchar(c);
}

/* Move cursor to r,c */
void poscur(r, c)
int r, c;
{
  tputs(tgoto(cm, c, r), 1, outc);
}

/* Clear the screen */
void clrscr()
{
  tputs(cl, 1, outc);
}

/* This are terminal independent characters which can be used in curses */

unsigned int ACS_ULCORNER;
unsigned int ACS_LLCORNER;
unsigned int ACS_URCORNER;
unsigned int ACS_LRCORNER;
unsigned int ACS_RTEE;
unsigned int ACS_LTEE;
unsigned int ACS_BTEE;
unsigned int ACS_TTEE;
unsigned int ACS_HLINE;
unsigned int ACS_VLINE;
unsigned int ACS_PLUS;
unsigned int ACS_S1;
unsigned int ACS_S9;
unsigned int ACS_DIAMOND;
unsigned int ACS_CKBOARD;
unsigned int ACS_DEGREE;
unsigned int ACS_PLMINUS;
unsigned int ACS_BULLET;
unsigned int ACS_LARROW;
unsigned int ACS_RARROW;
unsigned int ACS_DARROW;
unsigned int ACS_UARROW;
unsigned int ACS_BOARD;
unsigned int ACS_LANTERN;
unsigned int ACS_BLOCK;

/* These defines describe the full set of grafic block characters which
 * can be defined via termcap.
 */

#define RIGHTARROW  0
#define LEFTARROW   1
#define DOWNARROW   2
#define UPARROW     3
#define FULLSQUARE  4
#define GREYSQUARE  5
#define EMPTYSQUARE 6
#define LATERN      7
#define DIAMOND     8
#define DEGREE      9
#define PLUSMINUS  10
#define DOWNRIGHT  11
#define UPRIGHT    12
#define UPLEFT     13
#define DOWNLEFT   14
#define CROSS      15
#define UPLINE     16
#define UPMIDLINE  17
#define MIDLINE    18
#define DOMIDLINE  19
#define DOWNLINE   20
#define TEELEFT    21
#define TEERIGHT   22
#define TEEHEAD    23
#define TEENORMAL  24
#define VERTLINE   25
#define PARAGRAPH  26

unsigned int _cursgraftable[27] =
{
 '>', '<', 'v', '^', '#', ':', ' ', '#', '+', '\'', '#', '+', '+',
 '+', '+', '+', '-', ' ', '-', ' ', '_', '+', '+', '+', '+', '|'
};
char _cursident[28] = "+,.-0ahI`fgjklmnopqrstuvwx~";

int setterm(type)
char *type;
{
  unsigned char *ac;
  int i;
#ifdef TIOCGWINSZ
  struct winsize wsize;
#endif

  if (tgetent(termcap, type) != 1) return ERR;

#ifdef TIOCGWINSZ
  if (ioctl(0, TIOCGWINSZ, &wsize) == 0) {
	LINES = wsize.ws_row != 0 ? wsize.ws_row : tgetnum("li");
	COLS = wsize.ws_col != 0 ? wsize.ws_col : tgetnum("co");
  } else {
#endif
	LINES = tgetnum("li");
	COLS = tgetnum("co");
#ifdef TIOCGWINSZ
  }
#endif
  arp = tc;
  cl = tgetstr("cl", &arp);
  so = tgetstr("so", &arp);
  se = tgetstr("se", &arp);
  cm = tgetstr("cm", &arp);
  mr = tgetstr("mr", &arp);
  me = tgetstr("me", &arp);
  mb = tgetstr("mb", &arp);
  md = tgetstr("md", &arp);
  us = tgetstr("us", &arp);
  ue = tgetstr("ue", &arp);
  vi = tgetstr("vi", &arp);
  ve = tgetstr("ve", &arp);
  vs = tgetstr("vs", &arp);
  as = tgetstr("as", &arp);
  ae = tgetstr("ae", &arp);
  ac = (unsigned char *) tgetstr("ac", &arp);
  bl = tgetstr("bl", &arp);
  vb = tgetstr("vb", &arp);

  if (ac) {
	while (*ac) {
		i = 0;
		while (*ac != _cursident[i]) i++;
		_cursgraftable[i] = *++ac | A_ALTCHARSET;
		ac++;
	}
  }

  ACS_ULCORNER = _cursgraftable[UPLEFT];
  ACS_LLCORNER = _cursgraftable[DOWNLEFT];
  ACS_URCORNER = _cursgraftable[UPRIGHT];
  ACS_LRCORNER = _cursgraftable[DOWNRIGHT];
  ACS_RTEE = _cursgraftable[TEERIGHT];
  ACS_LTEE = _cursgraftable[TEELEFT];
  ACS_BTEE = _cursgraftable[TEEHEAD];
  ACS_TTEE = _cursgraftable[TEENORMAL];
  ACS_HLINE = _cursgraftable[MIDLINE];
  ACS_VLINE = _cursgraftable[VERTLINE];
  ACS_PLUS = _cursgraftable[CROSS];
  ACS_S1 = _cursgraftable[UPLINE];
  ACS_S9 = _cursgraftable[DOWNLINE];
  ACS_DIAMOND = _cursgraftable[DIAMOND];
  ACS_CKBOARD = _cursgraftable[GREYSQUARE];
  ACS_DEGREE = _cursgraftable[DEGREE];
  ACS_PLMINUS = _cursgraftable[PLUSMINUS];
  ACS_BULLET = 'o';		/* where the hell is a bullet defined in
			 * termcap ??? */
  ACS_LARROW = _cursgraftable[LEFTARROW];
  ACS_RARROW = _cursgraftable[RIGHTARROW];
  ACS_DARROW = _cursgraftable[DOWNARROW];
  ACS_UARROW = _cursgraftable[UPARROW];
  ACS_BOARD = _cursgraftable[EMPTYSQUARE];
  ACS_LANTERN = _cursgraftable[LATERN];
  ACS_BLOCK = _cursgraftable[FULLSQUARE];
  /* Wow, I got it! */
  return OK;
}

void gettmode()
{
  tcgetattr(0, &_orig_tty);
  tcgetattr(0, &_tty);
  _cursvar.echoit = (_tty.c_lflag & ECHO) != 0;
  _cursvar.rawmode = (_tty.c_lflag & (ICANON|ISIG)) == 0;
  _cursvar.cbrkmode = (_tty.c_lflag & (ICANON|ISIG)) == ISIG;
  NONL = (_tty.c_iflag & ICRNL) != 0;
}
