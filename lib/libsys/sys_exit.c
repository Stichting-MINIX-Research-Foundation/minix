#include "syslib.h"

/*===========================================================================*
 *                                sys_exit			     	     *
 *===========================================================================*/
int sys_exit()
{
/* A system process requests to exit. */
  message m;

  return(_kernel_call(SYS_EXIT, &m));
}
