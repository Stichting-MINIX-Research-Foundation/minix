#include <curses.h>
#include "curspriv.h"
#include <termcap.h>

extern char *bl, *vb;

/* Flash() flashes the terminal screen. */
void flash()
{
  if (vb)
	tputs(vb, 1, outc);
  else if (bl)
	tputs(bl, 1, outc);
}
