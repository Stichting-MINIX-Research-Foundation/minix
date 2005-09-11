/* This file contains some utility routines for PM.
 *
 * The entry points are:
 *   find_param:	look up a boot monitor parameter
 *   get_free_pid:	get a free process or group id
 *   allowed:		see if an access is permitted
 *   no_sys:		called for invalid system call numbers
 *   panic:		PM has run aground of a fatal error 
 *   tell_fs:		interface to FS
 *   get_mem_map:	get memory map of given process
 *   get_stack_ptr:	get stack pointer of given process	
 *   proc_from_pid:	return process pointer from pid number
 */

#include "pm.h"
#include <sys/stat.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include <fcntl.h>
#include <signal.h>		/* needed only because mproc.h needs it */
#include "mproc.h"
#include "param.h"

#include <minix/config.h>
#include <timers.h>
#include <string.h>
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
 *				allowed					     *
 *===========================================================================*/
PUBLIC int allowed(name_buf, s_buf, mask)
char *name_buf;			/* pointer to file name to be EXECed */
struct stat *s_buf;		/* buffer for doing and returning stat struct*/
int mask;			/* R_BIT, W_BIT, or X_BIT */
{
/* Check to see if file can be accessed.  Return EACCES or ENOENT if the access
 * is prohibited.  If it is legal open the file and return a file descriptor.
 */
  int fd;
  int save_errno;

  /* Use the fact that mask for access() is the same as the permissions mask.
   * E.g., X_BIT in <minix/const.h> is the same as X_OK in <unistd.h> and
   * S_IXOTH in <sys/stat.h>.  tell_fs(DO_CHDIR, ...) has set PM's real ids
   * to the user's effective ids, so access() works right for setuid programs.
   */
  if (access(name_buf, mask) < 0) return(-errno);

  /* The file is accessible but might not be readable.  Make it readable. */
  tell_fs(SETUID, PM_PROC_NR, (int) SUPER_USER, (int) SUPER_USER);

  /* Open the file and fstat it.  Restore the ids early to handle errors. */
  fd = open(name_buf, O_RDONLY | O_NONBLOCK);
  save_errno = errno;		/* open might fail, e.g. from ENFILE */
  tell_fs(SETUID, PM_PROC_NR, (int) mp->mp_effuid, (int) mp->mp_effuid);
  if (fd < 0) return(-save_errno);
  if (fstat(fd, s_buf) < 0) panic(__FILE__,"allowed: fstat failed", NO_NUM);

  /* Only regular files can be executed. */
  if (mask == X_BIT && (s_buf->st_mode & I_TYPE) != I_REGULAR) {
	close(fd);
	return(EACCES);
  }
  return(fd);
}

/*===========================================================================*
 *				no_sys					     *
 *===========================================================================*/
PUBLIC int no_sys()
{
/* A system call number not implemented by PM has been requested. */

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
 * defined constant. The process manager decides to shut down. This results 
 * in a HARD_STOP notification to all system processes to allow local cleanup.
 */
  printf("PM panic (%s): %s", who, mess);
  if (num != NO_NUM) printf(": %d",num);
  printf("\n");
  sys_abort(RBT_PANIC);
}

/*===========================================================================*
 *				tell_fs					     *
 *===========================================================================*/
PUBLIC void tell_fs(what, p1, p2, p3)
int what, p1, p2, p3;
{
/* This routine is only used by PM to inform FS of certain events:
 *      tell_fs(CHDIR, slot, dir, 0)
 *      tell_fs(EXEC, proc, 0, 0)
 *      tell_fs(EXIT, proc, 0, 0)
 *      tell_fs(FORK, parent, child, pid)
 *      tell_fs(SETGID, proc, realgid, effgid)
 *      tell_fs(SETSID, proc, 0, 0)
 *      tell_fs(SETUID, proc, realuid, effuid)
 *      tell_fs(UNPAUSE, proc, signr, 0)
 *      tell_fs(STIME, time, 0, 0)
 */
  message m;

  m.tell_fs_arg1 = p1;
  m.tell_fs_arg2 = p2;
  m.tell_fs_arg3 = p3;
  _taskcall(FS_PROC_NR, what, &m);
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
 *				get_mem_map				     *
 *===========================================================================*/
PUBLIC int get_mem_map(proc_nr, mem_map)
int proc_nr;					/* process to get map of */
struct mem_map *mem_map;			/* put memory map here */
{
  struct proc p;
  int s;

  if ((s=sys_getproc(&p, proc_nr)) != OK)
  	return(s);
  memcpy(mem_map, p.p_memmap, sizeof(p.p_memmap));
  return(OK);
}

/*===========================================================================*
 *				get_stack_ptr				     *
 *===========================================================================*/
PUBLIC int get_stack_ptr(proc_nr, sp)
int proc_nr;					/* process to get sp of */
vir_bytes *sp;					/* put stack pointer here */
{
  struct proc p;
  int s;

  if ((s=sys_getproc(&p, proc_nr)) != OK)
  	return(s);
  *sp = p.p_reg.sp;
  return(OK);
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

