#include "kernel/system.h"
#include <minix/endpoint.h>

/*===========================================================================*
 *			          do_schedctl			     *
 *===========================================================================*/
int do_schedctl(struct proc * caller, message * m_ptr)
{
	struct proc *p;
	unsigned flags;
	int priority, quantum, cpu;
	int proc_nr;
	int r;

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
		 * scheduling the process.
		 */
		priority = (int) m_ptr->SCHEDCTL_PRIORITY;
		quantum = (int) m_ptr->SCHEDCTL_QUANTUM;
		cpu = (int) m_ptr->SCHEDCTL_CPU;

		/* Try to schedule the process. */
		if((r = sched_proc(p, priority, quantum, cpu) != OK))
			return r;
		p->p_scheduler = NULL;
	} else {
		/* the caller becomes the scheduler */
		p->p_scheduler = caller;
	}

	return(OK);
}
