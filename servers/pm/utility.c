/* This file contains some utility routines for PM.
 *
 * The entry points are:
 *   find_param:	look up a boot monitor parameter
 *   get_free_pid:	get a free process or group id
 *   allowed:		see if an access is permitted
 *   no_sys:		called for invalid system call numbers
 *   panic:		PM has run aground of a fatal error 
 *   get_stack_ptr:	get stack pointer of given process	
 *   proc_from_pid:	return process pointer from pid number
 */

#include "pm.h"
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
#include <archconst.h>
#include <archtypes.h>
#include "../../kernel/const.h"
#include "../../kernel/config.h"
#include "../../kernel/type.h"
#include "../../kernel/proc.h"

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
 *				panic					     *
 *===========================================================================*/
PUBLIC void panic(who, mess, num)
char *who;			/* who caused the panic */
char *mess;			/* panic message string */
int num;			/* number to go with it */
{
/* An unrecoverable error has occurred.  Panics are caused when an internal
 * inconsistency is detected, e.g., a programming error or illegal value of a
 * defined constant. The process manager decides to exit.
 */
  printf("PM panic (%s): %s", who, mess);
  if (num != NO_NUM) printf(": %d",num);
  printf("\n");
   
  /* Exit PM. */
  sys_exit(SELF);
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
 *				proc_from_pid				     *
 *===========================================================================*/
PUBLIC int proc_from_pid(mp_pid)
pid_t mp_pid;
{
	int rmp;

	for (rmp = 0; rmp < NR_PROCS; rmp++)
		if (mproc[rmp].mp_pid == mp_pid)
			return rmp;

	return -1;
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
		return EDEADSRCDST;
	if(*proc >= 0 && !(mproc[*proc].mp_flags & IN_USE))
		return EDEADSRCDST;
	return OK;
}

