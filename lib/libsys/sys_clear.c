#include "syslib.h"

/*===========================================================================*
 *                                sys_clear			     	     *
 *===========================================================================*/
int sys_clear(proc_ep)
endpoint_t proc_ep;			/* which process has exited */
{
/* A process has exited. PM tells the kernel.
 */
  message m;

  m.m_lsys_krn_sys_clear.endpt = proc_ep;
  return(_kernel_call(SYS_CLEAR, &m));
}
