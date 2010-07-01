/* This file contains some utility routines for PM.
 *
 * The entry points are:
 *   get_free_pid:	get a free process or group id
 *   no_sys:		called for invalid system call numbers
 *   find_param:	look up a boot monitor parameter
 *   find_proc:		return process pointer from pid number
 *   nice_to_priority	convert nice level to priority queue
 *   pm_isokendpt:	check the validity of an endpoint
 *   tell_vfs:		send a request to VFS on behalf of a process
 */

#include "pm.h"
#include <sys/resource.h>
#include <sys/stat.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/endpoint.h>
#include <fcntl.h>
#include <signal.h>		/* needed only because mproc.h needs it */
#include "mproc.h"
#include "param.h"

#include <minix/config.h>
#include <timers.h>
#include <string.h>
#include <machine/archtypes.h>
#include "kernel/const.h"
#include "kernel/config.h"
#include "kernel/type.h"
#include "kernel/proc.h"

#define munmap _munmap
#define munmap_text _munmap_text
#include <sys/mman.h>
#undef munmap
#undef munmap_text

/*===========================================================================*
 *				get_free_pid				     *
 *===========================================================================*/
PUBLIC pid_t get_free_pid()
{
  static pid_t next_pid = INIT_PID + 1;		/* next pid to be assigned */
  register struct mproc *rmp;			/* check process table */
  int t;					/* zero if pid still free */

  /* Find a free pid for the child and put it in the table. */
  do {
	t = 0;			
	next_pid = (next_pid < NR_PIDS ? next_pid + 1 : INIT_PID + 1);
	for (rmp = &mproc[0]; rmp < &mproc[NR_PROCS]; rmp++)
		if (rmp->mp_pid == next_pid || rmp->mp_procgrp == next_pid) {
			t = 1;
			break;
		}
  } while (t);					/* 't' = 0 means pid free */
  return(next_pid);
}


/*===========================================================================*
 *				no_sys					     *
 *===========================================================================*/
PUBLIC int no_sys()
{
/* A system call number not implemented by PM has been requested. */
  printf("PM: in no_sys, call nr %d from %d\n", call_nr, who_e);
  return(ENOSYS);
}

/*===========================================================================*
 *				find_param				     *
 *===========================================================================*/
PUBLIC char *find_param(name)
const char *name;
{
  register const char *namep;
  register char *envp;

  for (envp = (char *) monitor_params; *envp != 0;) {
	for (namep = name; *namep != 0 && *namep == *envp; namep++, envp++)
		;
	if (*namep == '\0' && *envp == '=') 
		return(envp + 1);
	while (*envp++ != 0)
		;
  }
  return(NULL);
}

/*===========================================================================*
 *				find_proc  				     *
 *===========================================================================*/
PUBLIC struct mproc *find_proc(lpid)
pid_t lpid;
{
  register struct mproc *rmp;

  for (rmp = &mproc[0]; rmp < &mproc[NR_PROCS]; rmp++)
	if ((rmp->mp_flags & IN_USE) && rmp->mp_pid == lpid)
		return(rmp);

  return(NULL);
}

/*===========================================================================*
 *				nice_to_priority			     *
 *===========================================================================*/
PUBLIC int nice_to_priority(int nice, unsigned* new_q)
{
	if (nice < PRIO_MIN || nice > PRIO_MAX) return(EINVAL);

	*new_q = MAX_USER_Q + (nice-PRIO_MIN) * (MIN_USER_Q-MAX_USER_Q+1) /
	    (PRIO_MAX-PRIO_MIN+1);
	if (*new_q < MAX_USER_Q) *new_q = MAX_USER_Q;	/* shouldn't happen */
	if (*new_q > MIN_USER_Q) *new_q = MIN_USER_Q;	/* shouldn't happen */

	return (OK);
}

/*===========================================================================*
 *				pm_isokendpt			 	     *
 *===========================================================================*/
PUBLIC int pm_isokendpt(int endpoint, int *proc)
{
	*proc = _ENDPOINT_P(endpoint);
	if(*proc < -NR_TASKS || *proc >= NR_PROCS)
		return EINVAL;
	if(*proc >= 0 && endpoint != mproc[*proc].mp_endpoint)
		return EDEADEPT;
	if(*proc >= 0 && !(mproc[*proc].mp_flags & IN_USE))
		return EDEADEPT;
	return OK;
}

/*===========================================================================*
 *				tell_vfs			 	     *
 *===========================================================================*/
PUBLIC void tell_vfs(rmp, m_ptr)
struct mproc *rmp;
message *m_ptr;
{
/* Send a request to VFS, without blocking.
 */
  int r;

  if (rmp->mp_flags & VFS_CALL)
	panic("tell_vfs: not idle: %d", m_ptr->m_type);

  r = asynsend3(VFS_PROC_NR, m_ptr, AMF_NOREPLY);
  if (r != OK)
  	panic("unable to send to VFS: %d", r);

  rmp->mp_flags |= VFS_CALL;
}

int unmap_ok = 0;

PUBLIC int munmap(void *addrstart, vir_bytes len)
{
	if(!unmap_ok) 
		return ENOSYS;

	return _munmap(addrstart, len);
}

PUBLIC int munmap_text(void *addrstart, vir_bytes len)
{
	if(!unmap_ok)
		return ENOSYS;

	return _munmap_text(addrstart, len);

}
