#include <curses.h>
#include "curspriv.h"

/****************************************************************/
/* Newline() does line advance and returns the new cursor line.	*/
/* If error, return -1.						*/
/****************************************************************/

_PROTOTYPE( static short newline, (WINDOW *win, int lin));

static short newline(win, lin)
WINDOW *win;
int lin;
{
  if (++lin > win->_regbottom) {
	lin--;
	if (win->_scroll)
		scroll(win);
	else
		return(-1);
  }				/* if */
  return(lin);
}				/* newline */

/****************************************************************/
/* Waddch() inserts character 'c' at the current cursor posi-	*/
/* Tion in window 'win', and takes any actions as dictated by	*/
/* The character.						*/
/****************************************************************/

int waddch(win, c)
WINDOW *win;
int c;
{
  int x = win->_curx;
  int y = win->_cury;
  int newx;
  int ch = c;
  int ts = win->_tabsize;

  ch &= (A_ALTCHARSET | 0xff);
  if (y > win->_maxy || x > win->_maxx || y < 0 || x < 0) return(ERR);
  switch (ch) {
      case '\t':
	for (newx = ((x / ts) + 1) * ts; x < newx; x++) {
		if (waddch(win, ' ') == ERR) return(ERR);
		if (win->_curx == 0)	/* if tab to next line */
			return(OK);	/* exit the loop */
	}
	return(OK);

      case '\n':
	if (NONL) x = 0;
	if ((y = newline(win, y)) < 0) return (ERR);
	break;

      case '\r':	x = 0;	break;

      case '\b':
	if (--x < 0)		/* no back over left margin */
		x = 0;
	break;

      case 0x7f:
	{
		if (waddch(win, '^') == ERR) return(ERR);
		return(waddch(win, '?'));
	}

      default:
	if (ch < ' ') {		/* handle control chars */
		if (waddch(win, '^') == ERR) return(ERR);
		return(waddch(win, c + '@'));
	}
	ch |= (win->_attrs & ATR_MSK);
	if (win->_line[y][x] != ch) {	/* only if data change */
		if (win->_minchng[y] == _NO_CHANGE)
			win->_minchng[y] = win->_maxchng[y] = x;
		else if (x < win->_minchng[y])
			win->_minchng[y] = x;
		else if (x > win->_maxchng[y])
			win->_maxchng[y] = x;
	}			/* if */
	win->_line[y][x++] = ch;
	if (x > win->_maxx) {	/* wrap around test */
		x = 0;
		if ((y = newline(win, y)) < 0) return(ERR);
	}
	break;

  }				/* switch */
  win->_curx = x;
  win->_cury = y;
  return(OK);
}
