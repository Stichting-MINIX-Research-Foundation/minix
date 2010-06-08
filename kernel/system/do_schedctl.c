#include "kernel/system.h"
#include <minix/endpoint.h>

/*===========================================================================*
 *			          do_schedctl			     *
 *===========================================================================*/
PUBLIC int do_schedctl(struct proc * caller, message * m_ptr)
{
	struct proc *p;
	int proc_nr;

	/* Only system processes can change process schedulers */
	if (! (priv(caller)->s_flags & SYS_PROC))
		return(EPERM);

	if (!isokendpt(m_ptr->SCHEDULING_ENDPOINT, &proc_nr))
		return EINVAL;

	p = proc_addr(proc_nr);
	p->p_scheduler = caller;

	return(OK);
}
