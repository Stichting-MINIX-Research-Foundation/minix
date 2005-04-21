#include <string.h>
#include <curses.h>
#include "curspriv.h"

static char printscanbuf[513];	/* buffer used during I/O */

/****************************************************************/
/* Wprintw(win,fmt,args) does a printf() in window 'win'.	*/
/****************************************************************/
int wprintw(WINDOW *win, const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  vsprintf(printscanbuf, fmt, args);
  if (waddstr(win, printscanbuf) == ERR) return(ERR);
  return(strlen(printscanbuf));
}

/****************************************************************/
/* Printw(fmt,args) does a printf() in stdscr.			*/
/****************************************************************/
int printw(const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  vsprintf(printscanbuf, fmt, args);
  if (waddstr(stdscr, printscanbuf) == ERR) return(ERR);
  return(strlen(printscanbuf));
}				/* printw */

/****************************************************************/
/* Mvprintw(fmt,args) moves the stdscr cursor to a new posi-	*/
/* tion, then does a printf() in stdscr.			*/
/****************************************************************/
int mvprintw(int y, int x, const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  if (wmove(stdscr, y, x) == ERR) return(ERR);
  vsprintf(printscanbuf, fmt, args);
  if (waddstr(stdscr, printscanbuf) == ERR) return(ERR);
  return(strlen(printscanbuf));
}

/****************************************************************/
/* Mvwprintw(win,fmt,args) moves the window 'win's cursor to	*/
/* A new position, then does a printf() in window 'win'.	*/
/****************************************************************/
int mvwprintw(WINDOW *win, int y, int x, const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  if (wmove(win, y, x) == ERR) return(ERR);
  vsprintf(printscanbuf, fmt, args);
  if (waddstr(win, printscanbuf) == ERR) return(ERR);
  return(strlen(printscanbuf));
}				/* mvwprintw */

/****************************************************************/
/* Wscanw(win,fmt,args) gets a string via window 'win', then	*/
/* Scans the string using format 'fmt' to extract the values	*/
/* And put them in the variables pointed to the arguments.	*/
/****************************************************************/
int wscanw(WINDOW *win, const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  wrefresh(win);		/* set cursor */
  if (wgetstr(win, printscanbuf) == ERR)	/* get string */
	return(ERR);
  return(vsscanf(printscanbuf, fmt, args));
}				/* wscanw */

/****************************************************************/
/* Scanw(fmt,args) gets a string via stdscr, then scans the	*/
/* String using format 'fmt' to extract the values and put them	*/
/* In the variables pointed to the arguments.			*/
/****************************************************************/
int scanw(const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  wrefresh(stdscr);		/* set cursor */
  if (wgetstr(stdscr, printscanbuf) == ERR)	/* get string */
	return(ERR);
  return(vsscanf(printscanbuf, fmt, args));
}				/* scanw */

/****************************************************************/
/* Mvscanw(y,x,fmt,args) moves stdscr's cursor to a new posi-	*/
/* Tion, then gets a string via stdscr and scans the string	*/
/* Using format 'fmt' to extract the values and put them in the	*/
/* Variables pointed to the arguments.				*/
/****************************************************************/
int mvscanw(int y, int x, const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  if (wmove(stdscr, y, x) == ERR) return(ERR);
  wrefresh(stdscr);		/* set cursor */
  if (wgetstr(stdscr, printscanbuf) == ERR)	/* get string */
	return(ERR);
  return(vsscanf(printscanbuf, fmt, args));
}				/* mvscanw */

/****************************************************************/
/* Mvwscanw(win,y,x,fmt,args) moves window 'win's cursor to a	*/
/* New position, then gets a string via 'win' and scans the	*/
/* String using format 'fmt' to extract the values and put them	*/
/* In the variables pointed to the arguments.			*/
/****************************************************************/
int mvwscanw(WINDOW *win, int y, int x, const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  if (wmove(win, y, x) == ERR) return(ERR);
  wrefresh(win);		/* set cursor */
  if (wgetstr(win, printscanbuf) == ERR)	/* get string */
	return(ERR);
  return(vsscanf(printscanbuf, fmt, args));
}				/* mvwscanw */
