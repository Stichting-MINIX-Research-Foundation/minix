/* refresh.c */

#include <curses.h>
#include "curspriv.h"

/* Wrefresh() updates window win's area of the physical screen.	*/
void wrefresh(win)
WINDOW *win;
{
  if (win == curscr)
	curscr->_clear = TRUE;
  else
	wnoutrefresh(win);
  doupdate();
}

/****************************************************************/
/* Wnoutrefresh() updates the image of the desired screen,	*/
/* Without doing physical update (copies window win's image to	*/
/* The _cursvar.tmpwin window, which is hidden from the user).	*/
/****************************************************************/

void wnoutrefresh(win)
register WINDOW *win;
{
  register int *dst;		/* start destination in temp window */
  register int *end;		/* end destination in temp window */
  register int *src;		/* source in user window */
  register int first;		/* first changed char on line */
  register int last;		/* last changed char on line */
  WINDOW *nscr;
  int begy;			/* window's place on screen */
  int begx;
  int i;
  int j;

  nscr = _cursvar.tmpwin;
  begy = win->_begy;
  begx = win->_begx;

  for (i = 0, j = begy; i <= win->_maxy; i++, j++) {
	if (win->_minchng[i] != _NO_CHANGE) {
		first = win->_minchng[i];
		last = win->_maxchng[i];
		dst = &(nscr->_line[j][begx + first]);
		end = &(nscr->_line[j][begx + last]);
		src = &(win->_line[i][first]);

		while (dst <= end)	/* copy user line to temp window */
			*dst++ = *src++;

		first += begx;	/* nscr's min/max change positions */
		last += begx;

		if ((nscr->_minchng[j] == _NO_CHANGE) || (nscr->_minchng[j] > first))
			nscr->_minchng[j] = first;
		if (last > nscr->_maxchng[j]) nscr->_maxchng[j] = last;

		win->_minchng[i] = _NO_CHANGE;	/* updated now */
	}			/* if */
	win->_maxchng[i] = _NO_CHANGE;	/* updated now */
  }				/* for */

  if (win->_clear) {
	win->_clear = FALSE;
	nscr->_clear = TRUE;
  }				/* if */
  if (!win->_leave) {
	nscr->_cury = win->_cury + begy;
	nscr->_curx = win->_curx + begx;
  }				/* if */
}				/* wnoutrefresh */
