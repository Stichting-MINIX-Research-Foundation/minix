/* This file contains procedures to manage the system processes.
 *
 * The entry points into this file are
 *   do_start:
 *   do_stop:
 *   do_exit:   	a child of this server exited
 *
 * Changes:
 *   Jul 22, 2005:	Created  (Jorrit N. Herder)
 */

#include "rs.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <minix/dmap.h>

extern int errno;

#define EXEC_FAILED	49		/* arbitrary, recognizable status */
#define MAX_PATH_LEN    256		/* maximum path string length */
#define MAX_ARGS_LEN    4096		/* maximum argument string length */
#define MAX_ARG_COUNT   1		/* parsed arguments count */

PRIVATE char command[MAX_PATH_LEN+1];
PRIVATE char arg_buf[MAX_ARGS_LEN+1];

/*===========================================================================*
 *				do_start				     *
 *===========================================================================*/
PUBLIC int do_start(message *m_ptr)
{
  message m;
  int child_proc_nr;
  int major_nr;
  enum dev_style dev_style;
  pid_t child_pid;
  char *args[MAX_ARG_COUNT+1];
  int s;

  /* Obtain command name and parameters. */
  if (m_ptr->SRV_PATH_LEN > MAX_PATH_LEN) return(E2BIG);
  if (OK != (s=sys_datacopy(m_ptr->m_source, (vir_bytes) m_ptr->SRV_PATH_ADDR, 
  	SELF, (vir_bytes) command, m_ptr->SRV_PATH_LEN))) return(s);
  command[m_ptr->SRV_PATH_LEN] = '\0';
  if (command[0] != '/') return(EINVAL);

  args[0] = command;
  if (m_ptr->SRV_ARGS_LEN > 0) {
      if (m_ptr->SRV_ARGS_LEN > MAX_ARGS_LEN) return(E2BIG);
      if (OK != (s=sys_datacopy(m_ptr->m_source, (vir_bytes) m_ptr->SRV_ARGS_ADDR, 
  	SELF, (vir_bytes) arg_buf, m_ptr->SRV_ARGS_LEN))) return(s);
      arg_buf[m_ptr->SRV_ARGS_LEN] = '\0';
      args[1] = &arg_buf[0];
      args[2] = NULL;
  } else {
      args[1] = NULL;
  }
  
  /* Now try to execute the new system service. Fork a new process. The child
   * process will be inhibited from running by the NO_PRIV flag. Only let the
   * child run once its privileges have been set by the parent.
   */
  if ((s = _taskcall(PM_PROC_NR, FORK, &m)) < 0)	/* use raw interface */
  	report("SM", "_taskcall to PM failed", s);	/* to get both */
  child_pid = m.m_type;					/* - child's pid */
  child_proc_nr = m.PR_PROC_NR;				/* - process nr */

  /* Now branch for parent and child process, and check for error. */
  switch(child_pid) {					/* see fork(2) */
  case 0:						/* child process */
      execve(command, args, NULL);			/* POSIX exec */
      report("SM", "warning, exec() failed", errno);	/* shouldn't happen */
      exit(EXEC_FAILED);				/* terminate child */
      break;
  case -1:						/* fork failed */
      report("SM", "warning, fork() failed", errno);	/* shouldn't happen */
      return(errno);
  default:						/* parent process */
      if ((major_nr = m_ptr->SRV_DEV_MAJOR) > 0) {	/* set driver map */
          dev_style = STYLE_DEV;
          if ((s=mapdriver(child_proc_nr, major_nr, dev_style)) < 0) {
	     
#if VERBOSE
      printf("SM: '%s %s', major %d, pid %d, proc_nr %d", 
          command, arg_buf, major_nr, child_pid, child_proc_nr);
#endif
             report("SM", "couldn't map driver", errno);
          }
      }
      if ((s = _taskcall(SYSTEM, SYS_PRIVCTL, &m)) < 0) /* set privileges */
          report("SM", "_taskcall to SYSTEM failed", s); /* to let child run */
#if VERBOSE
      printf("SM: started '%s %s', major %d, pid %d, proc_nr %d", 
          command, arg_buf, major_nr, child_pid, child_proc_nr);
#endif
      							/* update tables */
  }
  return(OK);
}


/*===========================================================================*
 *				do_stop					     *
 *===========================================================================*/
PUBLIC int do_stop(message *m_ptr)
{
  return(ENOSYS);
}

/*===========================================================================*
 *				do_exit					     *
 *===========================================================================*/
PUBLIC int do_exit(message *m_ptr)
{
  pid_t exit_pid;
  int exit_status;

#if VERBOSE
  printf("SM: got SIGCHLD signal, doing wait to get exited child.\n");
#endif

  /* See which child exited and what the exit status is. This is done in a
   * loop because multiple childs may have exited, all reported by one 
   * SIGCHLD signal. The WNOHANG options is used to prevent blocking if, 
   * somehow, no exited child can be found. 
   */
  while ( (exit_pid = waitpid(-1, &exit_status, WNOHANG)) != 0 ) {

#if VERBOSE
  	printf("SM: pid %d,", exit_pid); 
  	if (WIFSIGNALED(exit_status)) {
  		printf("killed, signal number %d\n", WTERMSIG(exit_status));
  	} else if (WIFEXITED(exit_status)) {
  		printf("normal exit, status %d\n", WEXITSTATUS(exit_status));
  	}
#endif

  }
  return(OK);
}


