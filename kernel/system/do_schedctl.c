#include "kernel/system.h"
#include <signal.h>
#include <sys/sigcontext.h>
#include <minix/endpoint.h>

/*===========================================================================*
 *			          do_schedctl			     *
 *===========================================================================*/
PUBLIC int do_schedctl(struct proc * caller, message * m_ptr)
{
	struct proc *p;

	/* Only system processes can change process schedulers */
	if (! (priv(caller)->s_flags & SYS_PROC))
		return(EPERM);

	p = proc_addr(_ENDPOINT_P(m_ptr->SCHEDULING_ENDPOINT));
	p->p_scheduler = caller;

	return(OK);
}
