#include "kernel/system.h"
#include <minix/endpoint.h>
#include "kernel/clock.h"

/*===========================================================================*
 *				do_schedule				     *
 *===========================================================================*/
PUBLIC int do_schedule(struct proc * caller, message * m_ptr)
{
	struct proc *p;
	int proc_nr;

	if (!isokendpt(m_ptr->SCHEDULING_ENDPOINT, &proc_nr))
		return EINVAL;

	p = proc_addr(_ENDPOINT_P(m_ptr->SCHEDULING_ENDPOINT));

	/* Only this process' scheduler can schedule it */
	if (caller != p->p_scheduler)
		return(EPERM);

	/* Make sure the priority number given is within the allowed range.*/
	if (m_ptr->SCHEDULING_PRIORITY < TASK_Q ||
		m_ptr->SCHEDULING_PRIORITY > NR_SCHED_QUEUES)
		return(EINVAL);

	/* In some cases, we might be rescheduling a runnable process. In such
	 * a case (i.e. if we are updating the priority) we set the NO_QUANTUM
	 * flag before the generic unset to dequeue/enqueue the process
	 */
	if (proc_is_runnable(p))
		RTS_SET(p, RTS_NO_QUANTUM);

	/* Clear the scheduling bit and enqueue the process */
	p->p_priority = m_ptr->SCHEDULING_PRIORITY;
	p->p_quantum_size_ms = m_ptr->SCHEDULING_QUANTUM;
	p->p_cpu_time_left = ms_2_cpu_time(m_ptr->SCHEDULING_QUANTUM);

	RTS_UNSET(p, RTS_NO_QUANTUM);

	return(OK);
}
