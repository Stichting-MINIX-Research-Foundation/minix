/****************************************************************/
/* Tabsize() routines of the PCcurses package			*/
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
/* Wtabsize(win,ts) sets the tabsize of window 'win' to 'ts',	*/
/* And returns the original value.				*/
/****************************************************************/

int wtabsize(win, ts)
WINDOW *win;
int ts;
{
  int origval;

  origval = win->_tabsize;
  win->_tabsize = ts;
  return(origval);
}				/* wtabsize */

/****************************************************************/
/* Tabsize(ts) sets the tabsize of stdscr to 'ts', and returns	*/
/* The original value.						*/
/****************************************************************/

int tabsize(ts)
int ts;
{
  int origval;

  origval = stdscr->_tabsize;
  stdscr->_tabsize = ts;
  return(origval);
}				/* tabsize */
