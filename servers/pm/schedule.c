#include "pm.h"
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/config.h>
#include <minix/sysinfo.h>
#include <minix/type.h>
#include <machine/archtypes.h>
#include <lib.h>
#include "mproc.h"
#include "kernel/proc.h" /* for MIN_USER_Q */

PRIVATE timer_t sched_timer;

/*
 * makes a kernel call that schedules the process using the actuall scheduling
 * parameters set for this process
 */
PUBLIC int schedule_process(struct mproc * rmp)
{
	int err;

	if ((err = sys_schedule(rmp->mp_endpoint, rmp->mp_priority,
		rmp->mp_time_slice)) != OK) {
		printf("PM: An error occurred when trying to schedule %s: %d\n",
		rmp->mp_name, err);
	}

	return err;
}

/*===========================================================================*
 *				do_noquantum				     *
 *===========================================================================*/

PUBLIC void do_noquantum(void)
{
	int rv, proc_nr_n;
	register struct mproc *rmp;

	if (pm_isokendpt(m_in.m_source, &proc_nr_n) != OK) {
		printf("PM: WARNING: got an invalid endpoint in OOQ msg %u.\n",
		m_in.m_source);
		return;
	}

	rmp = &mproc[proc_nr_n];
	if (rmp->mp_priority < MIN_USER_Q) {
		rmp->mp_priority += 1; /* lower priority */
	}

	schedule_process(rmp);
}

/*===========================================================================*
 *				overtake_scheduling			     *
 *===========================================================================*/
PUBLIC void overtake_scheduling(void)
{
	struct mproc *trmp;
	int proc_nr;

	tmr_inittimer(&sched_timer);

	for (proc_nr=0, trmp=mproc; proc_nr < NR_PROCS; proc_nr++, trmp++) {
		/* Don't overtake system processes. When the system starts,
		 * this will typically only overtake init, from which other
		 * user space processes will inherit. */
		if (trmp->mp_flags & IN_USE && !(trmp->mp_flags & PRIV_PROC)) {
			if (sys_schedctl(trmp->mp_endpoint))
				printf("PM: Error while overtaking scheduling for %s\n",
					trmp->mp_name);
		}
	}

	pm_set_timer(&sched_timer, 100, balance_queues, 0);
}

/*===========================================================================*
 *				balance_queues				     *
 *===========================================================================*/

PUBLIC void balance_queues(tp)
struct timer *tp;
{
	struct mproc *rmp;
	int proc_nr;
	int rv;

	for (proc_nr=0, rmp=mproc; proc_nr < NR_PROCS; proc_nr++, rmp++) {
		if (rmp->mp_flags & IN_USE) {
			if (rmp->mp_priority > rmp->mp_max_priority) {
				rmp->mp_priority -= 1; /* increase priority */
				schedule_process(rmp);
			}
		}
	}

	pm_set_timer(&sched_timer, 100, balance_queues, 0);
}
