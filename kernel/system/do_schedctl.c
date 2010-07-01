#include "kernel/system.h"
#include <minix/endpoint.h>

/*===========================================================================*
 *			          do_schedctl			     *
 *===========================================================================*/
PUBLIC int do_schedctl(struct proc * caller, message * m_ptr)
{
	struct proc *p;
	unsigned flags;
	int proc_nr;

	/* Only system processes can change process schedulers */
	if (! (priv(caller)->s_flags & SYS_PROC))
		return(EPERM);

	/* check parameter validity */
	flags = (unsigned) m_ptr->SCHEDCTL_FLAGS;
	if (flags & ~SCHEDCTL_FLAG_KERNEL) {
		printf("do_schedctl: flags 0x%x invalid, caller=%d\n", 
			flags, caller - proc);
		return EINVAL;
	}

	if (!isokendpt(m_ptr->SCHEDCTL_ENDPOINT, &proc_nr))
		return EINVAL;

	p = proc_addr(proc_nr);

	if ((flags & SCHEDCTL_FLAG_KERNEL) == SCHEDCTL_FLAG_KERNEL) {
		/* the kernel becomes the scheduler and starts 
		 * scheduling the process; RTS_NO_QUANTUM which was 
		 * previously set by sys_fork is removed
		 */
		p->p_scheduler = NULL;
		RTS_UNSET(p, RTS_NO_QUANTUM);
	} else {
		/* the caller becomes the scheduler */
		p->p_scheduler = caller;
	}

	return(OK);
}
