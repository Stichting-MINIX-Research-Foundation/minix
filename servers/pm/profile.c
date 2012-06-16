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
#include "param.h"

/*===========================================================================*
 *				do_sprofile				     *
 *===========================================================================*/
int do_sprofile(void)
{
#if SPROFILE

  int r;

  switch(m_in.PROF_ACTION) {

  case PROF_START:
	return sys_sprof(PROF_START, m_in.PROF_MEM_SIZE, m_in.PROF_FREQ,
			m_in.PROF_INTR_TYPE,
			who_e, m_in.PROF_CTL_PTR, m_in.PROF_MEM_PTR);

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

  switch(m_in.PROF_ACTION) {

  case PROF_GET:
	return sys_cprof(PROF_GET, m_in.PROF_MEM_SIZE, who_e,
		m_in.PROF_CTL_PTR, m_in.PROF_MEM_PTR);

  case PROF_RESET:
	return sys_cprof(PROF_RESET,0,0,0,0);

  default:
	return EINVAL;
  }

#else
	return ENOSYS;
#endif
}

