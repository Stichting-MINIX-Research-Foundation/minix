/* This file implements entry points for system profiling.
 *
 * The entry points in this file are:
 *   do_sprofile:   start/stop statistical profiling
 *   do_cprofile:   get/reset call profiling tables
 *
 * Changes:
 *   14 Aug, 2006  Created (Rogier Meurs)
 */

#include <minix/config.h>
#include <minix/profile.h>
#include "pm.h"
#include <sys/wait.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include <signal.h>
#include "mproc.h"

/*===========================================================================*
 *				do_sprofile				     *
 *===========================================================================*/
int do_sprofile(void)
{
#if SPROFILE

  int r;

  switch(m_in.m_lc_pm_sprof.action) {

  case PROF_START:
	return sys_sprof(PROF_START, m_in.m_lc_pm_sprof.mem_size,
		m_in.m_lc_pm_sprof.freq, m_in.m_lc_pm_sprof.intr_type, who_e,
		m_in.m_lc_pm_sprof.ctl_ptr, m_in.m_lc_pm_sprof.mem_ptr);

  case PROF_STOP:
	return sys_sprof(PROF_STOP,0,0,0,0,0,0);

  default:
	return EINVAL;
  }

#else
	return ENOSYS;
#endif
}


/*===========================================================================*
 *				do_cprofile				     *
 *===========================================================================*/
int do_cprofile(void)
{
#if CPROFILE

  int r;

  switch(m_in.m_lc_pm_cprof.action) {

  case PROF_GET:
	return sys_cprof(PROF_GET, m_in.m_lc_pm_cprof.mem_size, who_e,
		m_in.m_lc_pm_cprof.ctl_ptr, m_in.m_lc_pm_cprof.mem_ptr);

  case PROF_RESET:
	return sys_cprof(PROF_RESET,0,0,0,0);

  default:
	return EINVAL;
  }

#else
	return ENOSYS;
#endif
}

