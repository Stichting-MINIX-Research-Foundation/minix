/* The kernel call that is implemented in this file:
 *   m_type:    SYS_SPROF
 *
 * The parameters for this kernel call are:
 *	m_lsys_krn_sys_sprof.action	(start/stop profiling)
 *	m_lsys_krn_sys_sprof.mem_size	(available memory for data)
 *	m_lsys_krn_sys_sprof.freq	(requested sample frequency)
 *	m_lsys_krn_sys_sprof.endpt	(endpoint of caller)
 *	m_lsys_krn_sys_sprof.ctl_ptr	(location of info struct)
 *	m_lsys_krn_sys_sprof.mem_ptr	(location of memory for data)
 *	m_lsys_krn_sys_sprof.intr_type	(interrupt source: RTC/NMI)
 *
 * Changes:
 *   14 Aug, 2006   Created (Rogier Meurs)
 */

#include "kernel/system.h"
#include "kernel/watchdog.h"

#if SPROFILE

/* user address to write info struct */
static vir_bytes sprof_info_addr_vir;

static void clean_seen_flag(void)
{
	int i;

	for (i = 0; i < NR_TASKS + NR_PROCS; i++)
		proc[i].p_misc_flags &= ~MF_SPROF_SEEN;
}

/*===========================================================================*
 *				do_sprofile				     *
 *===========================================================================*/
int do_sprofile(struct proc * caller, message * m_ptr)
{
  int proc_nr;
  int err;

  switch(m_ptr->m_lsys_krn_sys_sprof.action) {

  case PROF_START:
	/* Starting profiling.
	 *
	 * Check if profiling is not already running.  Calculate physical
	 * addresses of user pointers.  Reset counters.  Start CMOS timer.
	 * Turn on profiling.
	 */
	if (sprofiling) {
		printf("SYSTEM: start s-profiling: already started\n");
		return EBUSY;
	}

	/* Test endpoint number. */
	if(!isokendpt(m_ptr->m_lsys_krn_sys_sprof.endpt, &proc_nr))
		return EINVAL;

	/* Set parameters for statistical profiler. */
	sprof_ep = m_ptr->m_lsys_krn_sys_sprof.endpt;
	sprof_info_addr_vir = m_ptr->m_lsys_krn_sys_sprof.ctl_ptr;
	sprof_data_addr_vir = m_ptr->m_lsys_krn_sys_sprof.mem_ptr;

	sprof_info.mem_used = 0;
	sprof_info.total_samples = 0;
	sprof_info.idle_samples = 0;
	sprof_info.system_samples = 0;
	sprof_info.user_samples = 0;

	sprof_mem_size =
		m_ptr->m_lsys_krn_sys_sprof.mem_size < SAMPLE_BUFFER_SIZE ?
		m_ptr->m_lsys_krn_sys_sprof.mem_size : SAMPLE_BUFFER_SIZE;

	switch (sprofiling_type = m_ptr->m_lsys_krn_sys_sprof.intr_type) {
		case PROF_RTC:
			init_profile_clock(m_ptr->m_lsys_krn_sys_sprof.freq);
			break;
		case PROF_NMI:
			err = nmi_watchdog_start_profiling(
				m_ptr->m_lsys_krn_sys_sprof.freq);
			if (err)
				return err;
			break;
		default:
			printf("ERROR : unknown profiling interrupt type\n");
			return EINVAL;
	}
	
	sprofiling = 1;

	clean_seen_flag();

  	return OK;

  case PROF_STOP:
	/* Stopping profiling.
	 *
	 * Check if profiling is indeed running.  Turn off profiling.
	 * Stop CMOS timer.  Copy info struct to user process.
	 */
	if (!sprofiling) {
		printf("SYSTEM: stop s-profiling: not started\n");
		return EBUSY;
	}

	sprofiling = 0;

	switch (sprofiling_type) {
		case PROF_RTC:
			stop_profile_clock();
			break;
		case PROF_NMI:
			nmi_watchdog_stop_profiling();
			break;
	}

	data_copy(KERNEL, (vir_bytes) &sprof_info,
		sprof_ep, sprof_info_addr_vir, sizeof(sprof_info));
	data_copy(KERNEL, (vir_bytes) sprof_sample_buffer,
		sprof_ep, sprof_data_addr_vir, sprof_info.mem_used);

	clean_seen_flag();

  	return OK;

  default:
	return EINVAL;
  }
}

#endif /* SPROFILE */

