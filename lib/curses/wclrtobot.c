#include <curses.h>
#include "curspriv.h"

/****************************************************************/
/* Wclrtobot() fills the right half of the cursor line of	*/
/* Window 'win', and all lines below it with blanks.		*/
/****************************************************************/

int wclrtobot(win)
WINDOW *win;
{
  int y, minx, startx, *ptr, *end, *maxx, blank;

  blank = ' ' | (win->_attrs & ATR_MSK);
  startx = win->_curx;
  for (y = win->_cury; y <= win->_regbottom; y++) {
	minx = _NO_CHANGE;
	end = &win->_line[y][win->_maxx];
	for (ptr = &win->_line[y][startx]; ptr <= end; ptr++) {
		if (*ptr != blank) {
			maxx = ptr;
			if (minx == _NO_CHANGE) minx = ptr - win->_line[y];
			*ptr = blank;
		}		/* if */
	}			/* for */
	if (minx != _NO_CHANGE) {
		if ((win->_minchng[y] > minx) || (win->_minchng[y] == _NO_CHANGE))
			win->_minchng[y] = minx;
		if (win->_maxchng[y] < maxx - win->_line[y])
			win->_maxchng[y] = maxx - win->_line[y];
	}			/* if */
	startx = 0;
  }
  return(OK);
}
