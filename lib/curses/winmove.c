/****************************************************************/
/* Mvwin() routine of the PCcurses package			*/
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

#include <curses.h>
#include "curspriv.h"

/****************************************************************/
/* Mvwin() moves window 'win' to position (begx, begy) on the	*/
/* Screen.							*/
/****************************************************************/

int mvwin(win, begy, begx)
WINDOW *win;
int begy, begx;
{
  if ((begy + win->_maxy) > (LINES - 1) || (begx + win->_maxx) > (COLS - 1))
	return(ERR);
  win->_begy = begy;
  win->_begx = begx;
  touchwin(win);
  return(OK);
}				/* mvwin */
