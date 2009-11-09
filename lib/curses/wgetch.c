#include <curses.h>
#include <stdio.h>
#include "curspriv.h"

int wgetch(win)
WINDOW *win;
{
  bool weset = FALSE;
  char inp;

  if (!win->_scroll && (win->_flags & _FULLWIN)
      && win->_curx == win->_maxx - 1 && win->_cury == win->_maxy - 1)
	return ERR;
  if (_cursvar.echoit && !_cursvar.rawmode) {
	cbreak();
	weset++;
  }
  inp = getchar();
  if (_cursvar.echoit) {
	mvwaddch(curscr, win->_cury + win->_begy,
		 win->_curx + win->_begx, inp);
	waddch(win, inp);
  }
  if (weset) nocbreak();
  return inp;
}
