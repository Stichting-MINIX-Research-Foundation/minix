#include "kernel/system.h"
#include <minix/endpoint.h> // Kept

// Added kernel headers
#include <minix/kernel_types.h> // For k_errno_t or similar if error codes are mapped, and uint32_t
#include <klib/include/kprintf.h>
#include <klib/include/kstring.h>
#include <klib/include/kmemory.h>


/*===========================================================================*
 *			          do_schedctl			     *
 *===========================================================================*/
int do_schedctl(struct proc * caller, message * m_ptr)
{
	struct proc *p;
	uint32_t flags; // uint32_t might need definition from kernel_types.h
	int priority, quantum, cpu;
	int proc_nr;
	int r;

	/* check parameter validity */
	flags = m_ptr->m_lsys_krn_schedctl.flags;
	if (flags & ~SCHEDCTL_FLAG_KERNEL) {
		kprintf_stub("do_schedctl: flags 0x%x invalid, caller=%d\n", // MODIFIED
			flags, caller - proc);
		return EINVAL; // EINVAL might be undefined
	}

	if (!isokendpt(m_ptr->m_lsys_krn_schedctl.endpoint, &proc_nr))
		return EINVAL; // EINVAL might be undefined

	p = proc_addr(proc_nr);

	if ((flags & SCHEDCTL_FLAG_KERNEL) == SCHEDCTL_FLAG_KERNEL) {
		/* the kernel becomes the scheduler and starts 
		 * scheduling the process.
		 */
		priority = m_ptr->m_lsys_krn_schedctl.priority;
		quantum = m_ptr->m_lsys_krn_schedctl.quantum;
		cpu = m_ptr->m_lsys_krn_schedctl.cpu;

		/* Try to schedule the process. */
		if((r = sched_proc(p, priority, quantum, cpu, FALSE)) != OK)
			return r;
		p->p_scheduler = NULL; // NULL might be undefined
	} else {
		/* the caller becomes the scheduler */
		p->p_scheduler = caller;
	}

	return(OK);
}
