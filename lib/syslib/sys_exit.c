#include "syslib.h"

/*===========================================================================*
 *                                sys_exit			     	     *
 *===========================================================================*/
PUBLIC int sys_exit(proc_ep)
endpoint_t proc_ep;			/* which process has exited */
{
/* A process has exited. PM tells the kernel. In addition this call can be
 * used by system processes to directly exit without passing through the
 * PM. This should be used with care to prevent inconsistent PM tables. 
 */
  message m;

  m.PR_ENDPT = proc_ep;
  return(_taskcall(SYSTASK, SYS_EXIT, &m));
}
