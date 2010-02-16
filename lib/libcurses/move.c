#include <curses.h>
#include "curspriv.h"

/****************************************************************/
/* Wmove() moves the cursor in window 'win' to position (x,y).	*/
/****************************************************************/

int wmove(win, y, x)
WINDOW *win;
int y;
int x;
{
  if ((x<0) || (x>win->_maxx) || (y<win->_regtop) || (y>win->_regbottom)) 
	return(ERR);
  win->_curx = x;
  win->_cury = y;
  return(OK);
}
