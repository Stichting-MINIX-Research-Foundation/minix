/****************************************************************/
/* Wsetscrreg() routine of the PCcurses package			*/
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
/* Wsetscrreg() set the scrolling region of window 'win' to in-	*/
/* Clude all lines between 'top' and 'bottom'.			*/
/****************************************************************/

int wsetscrreg(win, top, bottom)
WINDOW *win;
int top;
int bottom;
{
  if ((0 <= top) &&
      (top <= win->_cury)
      &&
      (win->_cury <= bottom)
      &&
      (bottom <= win->_maxy)
	) {
	win->_regtop = top;
	win->_regbottom = bottom;
	return(OK);
  }

   /* If */ 
  else
	return(ERR);
}				/* wsetscrreg */

/****************************************************************/
/* Setscrreg() set the scrolling region of stdscr to include	*/
/* All lines between 'top' and 'bottom'.			*/
/****************************************************************/

int setscrreg(top, bottom)
int top;
int bottom;
{
  return(wsetscrreg(stdscr, top, bottom));
}				/* setscrreg */
