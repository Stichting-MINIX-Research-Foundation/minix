#include "kernel/system.h"
#include <minix/endpoint.h> // Kept
#include "kernel/clock.h"

// Added kernel headers
#include <minix/kernel_types.h> // For k_errno_t or similar if error codes are mapped
#include <klib/include/kprintf.h>
#include <klib/include/kstring.h>
#include <klib/include/kmemory.h>


/*===========================================================================*
 *				do_schedule				     *
 *===========================================================================*/
int do_schedule(struct proc * caller, message * m_ptr)
{
	struct proc *p;
	int proc_nr;
	int priority, quantum, cpu, niced;

	if (!isokendpt(m_ptr->m_lsys_krn_schedule.endpoint, &proc_nr))
		return EINVAL; // EINVAL might be undefined

	p = proc_addr(proc_nr);

	/* Only this process' scheduler can schedule it */
	if (caller != p->p_scheduler)
		return(EPERM); // EPERM might be undefined

	/* Try to schedule the process. */
	priority = m_ptr->m_lsys_krn_schedule.priority;
	quantum = m_ptr->m_lsys_krn_schedule.quantum;
	cpu = m_ptr->m_lsys_krn_schedule.cpu;
	niced = !!(m_ptr->m_lsys_krn_schedule.niced);

	return sched_proc(p, priority, quantum, cpu, niced);
}
