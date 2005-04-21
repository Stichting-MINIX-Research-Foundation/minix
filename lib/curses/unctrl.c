/****************************************************************/
/* Unctrl() routines of the PCcurses package			*/
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

static char strbuf[3] = {0, 0, 0};

/****************************************************************/
/* Unctrl() returns a char pointer to a string corresponding to	*/
/* Argument character 'c'.					*/
/****************************************************************/

char *unctrl(c)
char c;
{
  int ic = c;
  ic &= 0xff;

  if ((ic >= ' ') && (ic != 0x7f)) {	/* normal characters */
	strbuf[0] = ic;
	strbuf[1] = '\0';
	return(strbuf);
  }				/* if */
  strbuf[0] = '^';		/* '^' prefix */
  if (c == 0x7f)		/* DEL */
	strbuf[1] = '?';
  else				/* other control */
	strbuf[1] = ic + '@';
  return(strbuf);
}				/* unctrl */
