/****************************************************************/
/* Delwin() routine of the PCcurses package.			*/
/*								*/
/****************************************************************/
/* This version of curses is based on ncurses, a curses version	*/
/* Originally written by Pavel Curtis at Cornell University.	*/
/* I have made substantial changes to make it run on IBM PC's,	*/
/* And therefore consider myself free to make it public domain.	*/
/*		Bjorn Larsson (...mcvax!enea!infovax!bl)	*/
/****************************************************************/
/* 1.0:	Release:					870515	*/
/****************************************************************/
/* Modified to run under the MINIX operating system by Don Cope */
/* These changes are also released into the public domain.      */
/* 							900906  */
/****************************************************************/

#include <stdlib.h>
#include <curses.h>
#include "curspriv.h"

/****************************************************************/
/* Delwin() deallocates all data allocated by 'win'. If 'win'	*/
/* Is a subwindow, it uses the original window's lines for sto-	*/
/* Rage, and thus the line arrays are not deallocated.		*/
/****************************************************************/

void delwin(win)
WINDOW *win;
{
  int i;

  if (!(win->_flags & _SUBWIN)) {	/* subwindow uses 'parent's' lines */
	for (i = 0; i <= win->_maxy && win->_line[i]; i++)
		free(win->_line[i]);
  }
  free(win->_minchng);
  free(win->_maxchng);
  free(win->_line);
  free(win);
}				/* delwin */
