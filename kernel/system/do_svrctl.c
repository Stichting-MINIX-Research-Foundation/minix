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

#include "../system.h"
#include "../ipc.h"
#include <sys/svrctl.h>

#if USE_SVRCTL

/* NOTE: this call will radically change! */

/*===========================================================================*
 *				do_svrctl				     *
 *===========================================================================*/
PUBLIC int do_svrctl(m_ptr)
message *m_ptr;			/* pointer to request message */
{
  register struct proc *rp;
  register struct priv *sp;
  int proc_nr, rights;
  int request;
  vir_bytes argp;

  /* Extract message parameters. */
  proc_nr = m_ptr->CTL_PROC_NR;
  if (proc_nr == SELF) proc_nr = m_ptr->m_source;
  if (! isokprocn(proc_nr)) return(EINVAL);

  request = m_ptr->CTL_REQUEST;
  rights = m_ptr->CTL_MM_PRIV;
  argp = (vir_bytes) m_ptr->CTL_ARG_PTR;
  rp = proc_addr(proc_nr);

  /* Check if the PM privileges are super user. */
  if (!rights || !isuserp(rp)) 
      return(EPERM);

  /* See what is requested and handle the request. */
  switch (request) {
  case SYSSIGNON: {
	/* Make this process a server. The system processes should be able
	 * to communicate with this new server, so update their send masks
	 * as well.
	 */
	
	/* Find a new system privileges structure for this process. */
	for (sp=BEG_PRIV_ADDR; sp< END_PRIV_ADDR; sp++) 
		if (sp->s_proc_nr == NONE) break;
	if (sp->s_proc_nr != NONE) return(ENOSPC);

	/* Now update the process' privileges as requested. */
	rp->p_priv = sp;			/* assign to process */
	rp->p_priv->s_proc_nr = proc_nr(rp);	/* set association */
	rp->p_priv->s_call_mask = SYSTEM_CALL_MASK;
	return(OK);
  }
  default:
	return(EINVAL);
  }
}

#endif /* USE_SVRCTL */

