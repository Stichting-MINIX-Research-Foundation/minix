#include <curses.h>
#include "curspriv.h"

/* Wdelch() deletes the character at the window cursor, and the
   characters to the right of it are shifted left, inserting a
   space at the last position of the line.
*/

int wdelch(win)
WINDOW *win;
{
  int *temp1;
  int *temp2;
  int *end;
  int y = win->_cury;
  int x = win->_curx;
  int maxx = win->_maxx;

  end = &win->_line[y][maxx];
  temp1 = &win->_line[y][x];
  temp2 = temp1 + 1;
  while (temp1 < end) *temp1++ = *temp2++;
  *temp1 = ' ' | (win->_attrs & ATR_MSK);
  win->_maxchng[y] = maxx;
  if (win->_minchng[y] == _NO_CHANGE || win->_minchng[y] > x)
	win->_minchng[y] = x;
  return(OK);
}
