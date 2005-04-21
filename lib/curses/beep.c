#include <curses.h>
#include "curspriv.h"
#include <termcap.h>

extern char *bl, *vb;

/* Beep() sounds the terminal bell. */
void beep()
{
  if (bl)
	tputs(bl, 1, outc);
  else if (vb)
	tputs(vb, 1, outc);
}
