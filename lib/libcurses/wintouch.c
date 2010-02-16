/****************************************************************/
/* Touchwin() routine of the PCcurses package			*/
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
/* Touchwin() marks all lines of window 'win' as changed, from	*/
/* The first to the last character on the line.			*/
/****************************************************************/

void touchwin(win)
WINDOW *win;
{
  int y;
  int maxy;
  int maxx;

  maxy = win->_maxy;
  maxx = win->_maxx;

  for (y = 0; y <= maxy; y++) {
	win->_minchng[y] = 0;
	win->_maxchng[y] = maxx;
  }				/* for */
}				/* touchwin */
