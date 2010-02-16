#include <curses.h>
#include "curspriv.h"

/****************************************************************/
/* Waddstr() inserts string 'str' at the current cursor posi-	*/
/* Tion in window 'win', and takes any actions as dictated by	*/
/* The characters.						*/
/****************************************************************/

int waddstr(win, str)
WINDOW *win;
char *str;
{
  while (*str) {
	if (waddch(win, *str++) == ERR) return(ERR);
  }
  return(OK);
}
