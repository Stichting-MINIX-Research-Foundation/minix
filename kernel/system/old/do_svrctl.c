/* The system call implemented in this file:
 *   m_type:	SYS_SVRCTL
 *
 * The parameters for this system call are:
 *    m2_i1:	CTL_PROC_NR 	(process number of caller)	
 *    m2_i2:	CTL_REQUEST	(request type)	
 *    m2_i3:	CTL_MM_PRIV 	(privilege)
 *    m2_l1:    CTL_SEND_MASK   (new send mask to be installed)
 *    m2_l2:    CTL_PROC_TYPE   (new process type)
 *    m2_p1:	CTL_ARG_PTR 	(argument pointer)
 */

#include "../kernel.h"
#include "../system.h"
#include <sys/svrctl.h>
#include "../sendmask.h"

/*===========================================================================*
 *				do_svrctl				     *
 *===========================================================================*/
PUBLIC int do_svrctl(m_ptr)
message *m_ptr;			/* pointer to request message */
{
  register struct proc *rp;
  int proc_nr, priv;
  int request;
  vir_bytes argp;

  /* Extract message parameters. */
  proc_nr = m_ptr->CTL_PROC_NR;
  if (proc_nr == SELF) proc_nr = m_ptr->m_source;
  if (! isokprocn(proc_nr)) return(EINVAL);

  request = m_ptr->CTL_REQUEST;
  priv = m_ptr->CTL_MM_PRIV;
  argp = (vir_bytes) m_ptr->CTL_ARG_PTR;
  rp = proc_addr(proc_nr);

  /* Check if the MM privileges are super user. */
  if (!priv || !isuserp(rp)) 
      return(EPERM);

  /* See what is requested and handle the request. */
  switch (request) {
  case SYSSIGNON: {
	/* Make this process a server. The system processes should be able
	 * to communicate with this new server, so update their send masks
	 * as well.
	 */
  	/* fall through */
  }
  case SYSSENDMASK: {
	rp->p_type = P_SERVER;
	rp->p_sendmask = ALLOW_ALL_MASK;
	send_mask_allow(proc_addr(RTL8139)->p_sendmask, proc_nr);
	send_mask_allow(proc_addr(PM_PROC_NR)->p_sendmask, proc_nr);
	send_mask_allow(proc_addr(FS_PROC_NR)->p_sendmask, proc_nr);
	send_mask_allow(proc_addr(IS_PROC_NR)->p_sendmask, proc_nr);
	send_mask_allow(proc_addr(CLOCK)->p_sendmask, proc_nr);
	send_mask_allow(proc_addr(SYSTASK)->p_sendmask, proc_nr);
	return(OK); 
  }
  default:
	return(EINVAL);
  }
}

