/* The kernel call implemented in this file:
 *   m_type:	SYS_GETRUSAGE
 *
 * The parameters for this kernel call are:
 *    m1_i1:	RU_ENDPT	(process to get rusage)
 *    m1_p1:	RU_RUSAGE_PTR	(pointer to struct rusage)
 */

#include "kernel/system.h"
#include "kernel/proc.h"

#include <minix/endpoint.h>
#include <sys/resource.h>
/*===========================================================================*
 *				do_getrusage				     *
 *===========================================================================*/
int do_getrusage(struct proc * caller, message * msg)
{
	struct proc *target = NULL;
	int target_slot = 0;
	struct rusage r_usage;
	u64_t usec;

	if (!isokendpt(msg->RU_ENDPT, &target_slot))
		return EINVAL;

	target = proc_addr(target_slot);
	if (isemptyp(target))
		return EINVAL;

	if (data_copy(msg->m_source, (vir_bytes) msg->RU_RUSAGE_ADDR, KERNEL,
		(vir_bytes) &r_usage, (vir_bytes) sizeof(struct rusage)) != OK)
		return EFAULT;

	usec = target->p_user_time * 1000000 / system_hz;
	r_usage.ru_utime.tv_sec = usec / 1000000;
	r_usage.ru_utime.tv_usec = usec % 100000;
	usec = target->p_sys_time * 1000000 / system_hz;
	r_usage.ru_stime.tv_sec = usec / 1000000;
	r_usage.ru_stime.tv_usec = usec % 100000;
	r_usage.ru_msgsnd = target->p_message_sent;
	r_usage.ru_msgrcv = target->p_message_received;
	r_usage.ru_nsignals = target->p_signal_received;
	r_usage.ru_nvcsw = target->p_voluntary_context_switch;
	r_usage.ru_nivcsw = target->p_involuntary_context_switch;

	/* Get the mcontext structure into our address space.  */
	return data_copy(KERNEL, (vir_bytes) &r_usage, msg->m_source,
		(vir_bytes) msg->RU_RUSAGE_ADDR,
		(vir_bytes) sizeof(struct rusage));
}
