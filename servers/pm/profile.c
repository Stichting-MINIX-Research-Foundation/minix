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

#if SPROFILE || CPROFILE
FORWARD _PROTOTYPE( int check_addrs, (int info_size)               );
#endif

/*===========================================================================*
 *				do_sprofile				     *
 *===========================================================================*/
PUBLIC int do_sprofile(void)
{
#if SPROFILE

  int r;

  switch(m_in.PROF_ACTION) {

  case PROF_START:
	if ((r = check_addrs(sizeof(sprof_info_inst)))) /* check pointers */
		return r;

	return sys_sprof(PROF_START, m_in.PROF_MEM_SIZE, m_in.PROF_FREQ,
			who_e, m_in.PROF_CTL_PTR, m_in.PROF_MEM_PTR);

  case PROF_STOP:
	return sys_sprof(PROF_STOP,0,0,0,0,0);

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
PUBLIC int do_cprofile(void)
{
#if CPROFILE

  int r;

  switch(m_in.PROF_ACTION) {

  case PROF_GET:
	if (r = check_addrs(sizeof(cprof_info_inst))) /* check user pointers */
		return r;

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


#if SPROFILE || CPROFILE

/*===========================================================================*
 *				check_addrs				     *
 *===========================================================================*/
PRIVATE int check_addrs(info_size)
int info_size;
{
  int r;
  phys_bytes p;

  /* Check if supplied pointers point into user process. */
  if ((r = sys_umap(who_e, VM_D, (vir_bytes) m_in.PROF_CTL_PTR,
	 					 1, &p)) != OK) {
	printf("PM: PROFILE: umap failed for process %d\n", who_e);
	return r;                                    
  }  

  if ((r =sys_umap(who_e, VM_D, (vir_bytes) m_in.PROF_MEM_PTR,
 					 1, &p)) != OK) {
	printf("PM: PROFILE: umap failed for process %d\n", who_e);
	return r;                                    
  }  
  return 0;
}

#endif

