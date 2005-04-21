#include <curses.h>
#include "curspriv.h"

/****************************************************************/
/* Wbox(win,ymin,xmin,ymax,xmax,v,h) draws a box in window	*/
/* 'win', enclosing the area xmin-xmax and ymin-xmax. If	*/
/* Xmax and/or ymax is 0, the window max value is used. 'v' and	*/
/* 'h' are the vertical and horizontal characters to use. If	*/
/* 'v' and 'h' are 0, wbox will use the alternate character set */
/* In a pretty way.						*/
/****************************************************************/

int wbox(win, ymin, xmin, ymax, xmax, v, h)
WINDOW *win;
int ymin, xmin, ymax, xmax;
unsigned int v;
unsigned int h;
{
  unsigned int vc, hc, ulc, urc, llc, lrc;	/* corner chars */
  int i;

  if (ymax == 0) ymax = win->_maxy;
  if (xmax == 0) xmax = win->_maxx;

  if (ymin >= win->_maxy || ymax > win->_maxy ||
      xmin >= win->_maxx || xmax > win->_maxx ||
      ymin >= ymax || xmin >= xmax)
	return(ERR);

  vc = v;
  hc = h;
  ulc = urc = llc = lrc = vc;	/* default same as vertical */

  if (v == 0 && h == 0) {
	ulc = ACS_ULCORNER;
	urc = ACS_URCORNER;
	llc = ACS_LLCORNER;
	lrc = ACS_LRCORNER;
	hc = ACS_HLINE;
	vc = ACS_VLINE;
  }
  for (i = xmin + 1; i <= xmax - 1; i++) {
	win->_line[ymin][i] = hc | win->_attrs;
	win->_line[ymax][i] = hc | win->_attrs;
  }
  for (i = ymin + 1; i <= ymax - 1; i++) {
	win->_line[i][xmin] = vc | win->_attrs;
	win->_line[i][xmax] = vc | win->_attrs;
  }
  win->_line[ymin][xmin] = ulc | win->_attrs;
  win->_line[ymin][xmax] = urc | win->_attrs;
  win->_line[ymax][xmin] = llc | win->_attrs;
  win->_line[ymax][xmax] = lrc | win->_attrs;

  for (i = ymin; i <= ymax; i++) {
	if (win->_minchng[i] == _NO_CHANGE) {
		win->_minchng[i] = xmin;
		win->_maxchng[i] = xmax;
	} else {
		win->_minchng[i] = min(win->_minchng[i], xmin);
		win->_maxchng[i] = max(win->_maxchng[i], xmax);
	}
  }
  return(OK);
}
