#include <curses.h>
#include "curspriv.h"
#include <termcap.h>

int endwin()
{
  extern char *me;

  curs_set(1);
  poscur(LINES - 1, 0);
  refresh();
  tputs(me, 1, outc);
  delwin(stdscr);
  delwin(curscr);
  delwin(_cursvar.tmpwin);
  resetty();
  return(OK);
}
