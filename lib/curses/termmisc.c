#include <curses.h>
#include "curspriv.h"

/* Static variables or saving terminal modes */

int fixterm()
{
  return(OK);
}				/* fixterm */

int resetterm()
{
  return(OK);
}

int saveoldterm()
{
  return(OK);
}				/* saveoldterm */

int saveterm()
{
  return(OK);
}				/* saveterm */

int baudrate()
{
  return(19200);
}				/* baudrate */

/****************************************************************/
/* Erasechar(), killchar() returns std MSDOS erase chars.	*/
/****************************************************************/

int erasechar()
{
  return(_DCCHAR);		/* character delete char */
}				/* erasechar */

int killchar()
{
  return(_DLCHAR);		/* line delete char */
}				/* killchar */

/****************************************************************/
/* Savetty() and resetty() saves and restores the terminal I/O	*/
/* Settings.							*/
/****************************************************************/

int savetty()
{
  return(OK);
}				/* savetty */

/****************************************************************/
/* Setupterm() sets up the terminal. On a PC, it is always suc-	*/
/* Cessful, and returns 1.					*/
/****************************************************************/

int setupterm()
{
  return(1);
}				/* setupterm */
