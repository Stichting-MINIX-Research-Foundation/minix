#include <curses.h>
#include "curspriv.h"
#include <termcap.h>

static WINDOW *twin;		/* used by many routines */

/****************************************************************/
/* Gotoxy() moves the physical cursor to the desired address on */
/* The screen. We don't optimize here - on a PC, it takes more  */
/* Time to optimize than to do things directly.                 */
/****************************************************************/

_PROTOTYPE(static void gotoxy, (int row, int col ));
_PROTOTYPE(static void newattr, (int ch ));
_PROTOTYPE(static void Putchar, (int ch ));
_PROTOTYPE(static void clrupdate, (WINDOW *scr ));
_PROTOTYPE(static void transformline, (int lineno ));

static void gotoxy(row, col)
int row, col;
{
  poscur(row, col);
  _cursvar.cursrow = row;
  _cursvar.curscol = col;
}

/* Update attributes */
static void newattr(ch)
int ch;
{
  extern char *me, *as, *ae, *mb, *md, *mr, *so, *us;
  static int lastattr = 0;

  if (lastattr != (ch &= ATR_MSK)) {
	lastattr = ch;

	tputs(me, 1, outc);
	if (ae) tputs(ae, 1, outc);

	if (ch & A_ALTCHARSET)
		if (as) tputs(as, 1, outc);
	if (ch & A_BLINK) tputs(mb, 1, outc);
	if (ch & A_BOLD) tputs(md, 1, outc);
	if (ch & A_REVERSE) tputs(mr, 1, outc);
	if (ch & A_STANDOUT) tputs(so, 1, outc);
	if (ch & A_UNDERLINE) tputs(us, 1, outc);
  }
}

/* Putchar() writes a character, with attributes, to the physical
   screen, but avoids writing to the lower right screen position.
   Should it care about am?
*/

/* Output char with attribute */
static void Putchar(ch)
int ch;
{
  if ((_cursvar.cursrow < LINES) || (_cursvar.curscol < COLS)) {
	newattr(ch);
	putchar(ch);
  }
}

/****************************************************************/
/* Clrupdate(scr) updates the screen by clearing it and then    */
/* Redraw it in it's entirety.					*/
/****************************************************************/

static void clrupdate(scr)
WINDOW *scr;
{
  register int *src;
  register int *dst;
  register int i;
  register int j;
  WINDOW *w;

  w = curscr;

  if (scr != w) {		/* copy scr to curscr */
	for (i = 0; i < LINES; i++) {
		src = scr->_line[i];
		dst = w->_line[i];
		for (j = 0; j < COLS; j++) *dst++ = *src++;
	}			/* for */
  }				/* if */
  newattr(scr->_attrs);
  clrscr();
  scr->_clear = FALSE;
  for (i = 0; i < LINES; i++) {	/* update physical screen */
	src = w->_line[i];
	j = 0;
	while (j < COLS) {
		if (*src != (' ' | ATR_NRM)) {
			gotoxy(i, j);
			while (j < COLS && (*src != (' ' | ATR_NRM))) {
				Putchar(*src++);
				j++;
			}
		} else {
			src++;
			j++;
		}
	}			/* for */
  }				/* for */
  fflush(stdout);
}				/* clrupdate */

/****************************************************************/
/* Transformline() updates the given physical line to look      */
/* Like the corresponding line in _cursvar.tmpwin.		*/
/****************************************************************/

static void transformline(lineno)
register int lineno;
{
  register int *dstp;
  register int *srcp;
  register int dstc;
  register int srcc;
  int x;
  int endx;

  x = twin->_minchng[lineno];
  endx = twin->_maxchng[lineno];
  dstp = curscr->_line[lineno] + x;
  srcp = twin->_line[lineno] + x;

  while (x <= endx) {
	if ((*dstp != *srcp) || (dstc != srcc)) {
		gotoxy(lineno, x);
		while (x <= endx && ((*dstp != *srcp) || (dstc != srcc))) {
			Putchar(*srcp);
			*dstp++ = *srcp++;
			x++;
		}
	} else {
		*dstp++ = *srcp++;
		x++;
	}
  }				/* for */
  twin->_minchng[lineno] = _NO_CHANGE;
  twin->_maxchng[lineno] = _NO_CHANGE;
}				/* transformline */

/****************************************************************/
/* Doupdate() updates the physical screen to look like _curs-   */
/* Var.tmpwin if curscr is not 'Clear-marked'. Otherwise it     */
/* Updates the screen to look like curscr.                      */
/****************************************************************/

void doupdate()
{
  int i;

  twin = _cursvar.tmpwin;
  if (curscr->_clear)
	clrupdate(curscr);
  else {
	if (twin->_clear)
		clrupdate(twin);
	else {
		for (i = 0; i < LINES; i++)
			if (twin->_minchng[i] != _NO_CHANGE)
				transformline(i);
	}
  }
  curscr->_curx = twin->_curx;
  curscr->_cury = twin->_cury;
  gotoxy(curscr->_cury, curscr->_curx);
  fflush(stdout);
}				/* doupdate */
