#include "syslib.h"

/*===========================================================================*
 *                              sys_exit			     	     *
 *===========================================================================*/
PUBLIC int sys_exit(int status)
{
/* A server wants to exit. The exit status is passed on, possibly a panic.
 */
  message m;
  m.EXIT_STATUS = status;
  return(_taskcall(SYSTASK, SYS_EXIT, &m));
  /* NOT REACHED */
}

