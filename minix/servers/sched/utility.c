/* This file contains some utility routines for SCHED.
 *
 * The entry points are:
 *   no_sys:		called for invalid system call numbers
 *   sched_isokendpt:	check the validity of an endpoint
 *   sched_isemtyendpt  check for validity and availability of endpoint slot
 *   accept_message	check whether message is allowed
 */

#include "sched.h"
#include <machine/archtypes.h>
#include <sys/resource.h> /* for PRIO_MAX & PRIO_MIN */
#include "schedproc.h"

/*===========================================================================*
 *				no_sys					     *
 *===========================================================================*/
int no_sys(int who_e, int call_nr)
{
/* A system call number not implemented by PM has been requested. */
  printf("SCHED: in no_sys, call nr %d from %d\n", call_nr, who_e);
  return(ENOSYS);
}


/*===========================================================================*
 *				sched_isokendpt			 	     *
 *===========================================================================*/
int sched_isokendpt(int endpoint, int *proc)
{
	*proc = _ENDPOINT_P(endpoint);
	if (*proc < 0)
		return (EBADEPT); /* Don't schedule tasks */
	if(*proc >= NR_PROCS)
		return (EINVAL);
	if(endpoint != schedproc[*proc].endpoint)
		return (EDEADEPT);
	if(!(schedproc[*proc].flags & IN_USE))
		return (EDEADEPT);
	return (OK);
}

/*===========================================================================*
 *				sched_isemtyendpt		 	     *
 *===========================================================================*/
int sched_isemtyendpt(int endpoint, int *proc)
{
	*proc = _ENDPOINT_P(endpoint);
	if (*proc < 0)
		return (EBADEPT); /* Don't schedule tasks */
	if(*proc >= NR_PROCS)
		return (EINVAL);
	if(schedproc[*proc].flags & IN_USE)
		return (EDEADEPT);
	return (OK);
}

/*===========================================================================*
 *				accept_message				     *
 *===========================================================================*/
int accept_message(message *m_ptr)
{
	/* accept all messages from PM and RS */
	switch (m_ptr->m_source) {

		case PM_PROC_NR:
		case RS_PROC_NR:
			return 1;
			
	}
	
	/* no other messages are allowable */
	return 0;
}
