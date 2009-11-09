/****************************************************************/
/* Overlay() and overwrite() functions of the PCcurses package	*/
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
/* Overlay() overwrites 'win1' upon 'win2', with origins alig-	*/
/* Ned. Overlay is transparent; blanks from 'win1' are not	*/
/* Copied to 'win2'.						*/
/****************************************************************/
void overlay(win1, win2)
WINDOW *win1, *win2;
{
  int *minchng;
  int *maxchng;
  int *w1ptr;
  int *w2ptr;
  int attrs;
  int col;
  int line;
  int last_line;
  int last_col;

  last_col = min(win1->_maxx, win2->_maxx);
  last_line = min(win1->_maxy, win2->_maxy);
  attrs = win2->_attrs & ATR_MSK;
  minchng = win2->_minchng;
  maxchng = win2->_maxchng;

  for (line = 0; line <= last_line; line++) {
	register short fc, lc = 0;
	w1ptr = win1->_line[line];
	w2ptr = win2->_line[line];
	fc = _NO_CHANGE;
	for (col = 0; col <= last_col; col++) {
		if ((*w1ptr & CHR_MSK) != ' ') {
			*w2ptr = (*w1ptr & CHR_MSK) | attrs;
			if (fc == _NO_CHANGE) fc = col;
			lc = col;
		}
		w1ptr++;
		w2ptr++;
	}

	if (*minchng == _NO_CHANGE) {
		*minchng = fc;
		*maxchng = lc;
	} else if (fc != _NO_CHANGE) {
		if (fc < *minchng) *minchng = fc;
		if (lc > *maxchng) *maxchng = lc;
	}
	minchng++;
	maxchng++;
  }				/* for */
}				/* overlay */

/****************************************************************/
/* Overwrite() overwrites 'win1' upon 'win2', with origins	*/
/* Aligned. Overwrite is non-transparent; blanks from 'win1'	*/
/* Are copied to 'win2'.					*/
/****************************************************************/
void overwrite(win1, win2)
WINDOW *win1, *win2;
{
  int *minchng;
  int *maxchng;
  int *w1ptr;
  int *w2ptr;
  int attrs;
  int col;
  int line;
  int last_line;
  int last_col;

  last_col = min(win1->_maxx, win2->_maxx);
  last_line = min(win1->_maxy, win2->_maxy);
  attrs = win2->_attrs & ATR_MSK;
  minchng = win2->_minchng;
  maxchng = win2->_maxchng;

  for (line = 0; line <= last_line; line++) {
	register short fc, lc = 0;

	w1ptr = win1->_line[line];
	w2ptr = win2->_line[line];
	fc = _NO_CHANGE;

	for (col = 0; col <= last_col; col++) {
		if ((*w1ptr & CHR_MSK) != (*w2ptr & CHR_MSK)) {
			*w2ptr = (*w1ptr & CHR_MSK) | attrs;

			if (fc == _NO_CHANGE) fc = col;
			lc = col;
		}
		w1ptr++;
		w2ptr++;
	}			/* for */

	if (*minchng == _NO_CHANGE) {
		*minchng = fc;
		*maxchng = lc;
	} else if (fc != _NO_CHANGE) {
		if (fc < *minchng) *minchng = fc;
		if (lc > *maxchng) *maxchng = lc;
	}
	minchng++;
	maxchng++;
  }
}
