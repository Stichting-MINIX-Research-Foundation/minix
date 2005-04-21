#include <curses.h>
#include "curspriv.h"

/****************************************************************/
/* Wgetstr(win,str) reads in a string (terminated by \n or \r)	*/
/* To the buffer pointed to by 'str', and displays the input	*/
/* In window 'win'. The user's erase and kill characters are	*/
/* Active.							*/
/****************************************************************/

int wgetstr(win, str)
WINDOW *win;
char *str;
{
  while ((*str = wgetch(win)) != ERR && *str != '\n') str++;
  if (*str == ERR) {
	*str = '\0';
	return ERR;
  }
  *str = '\0';
  return OK;
}
