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

#include "sm.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

extern int errno;

#define EXEC_FAILED	49		/* recognizable exit status */

/*===========================================================================*
 *				   do_start				     *
 *===========================================================================*/
PUBLIC int do_start(message *m_ptr)
{
  pid_t child_pid;
  char command[255] = "/usr/sbin/is";

  /* Obtain command name and parameters. */
  

  /* Now try to execute the new system service. */
  child_pid = fork();	/* normal POSIX fork */
  switch(child_pid) {
  case 0:		/* child process, start system service */
      execve(command, NULL, NULL);			/* POSIX exec */
      report("SM", "warning, exec() failed", errno);	/* shouldn't happen */
      exit(EXEC_FAILED);				/* terminate child */
      break;
  case -1:		/* fork failed, report error */
      report("SM", "warning, fork() failed", errno);	/* shouldn't happen */
      return(errno);
  default:		/* parent process */
      report("SM", "new process forked, pid", child_pid);
      							/* update tables */
  }
  return(OK);
}


/*===========================================================================*
 *				   do_stop				     *
 *===========================================================================*/
PUBLIC int do_stop(message *m_ptr)
{
  return(ENOSYS);
}

/*===========================================================================*
 *				   do_exit				     *
 *===========================================================================*/
PUBLIC int do_exit(message *m_ptr)
{
  pid_t exit_pid;
  int exit_status;

  printf("SM: got SIGCHLD signal, doing wait to get exited child.\n");

  /* See which child exited and what the exit status is. This is done in a
   * loop because multiple childs may have exited, all reported by one 
   * SIGCHLD signal. The WNOHANG options is used to prevent blocking if, 
   * somehow, no exited child can be found. 
   */
  while ( (exit_pid = waitpid(-1, &exit_status, WNOHANG)) != 0 ) {

  	printf("SM: pid %d,", exit_pid); 
  	if (WIFSIGNALED(exit_status)) {
  		printf("killed, signal number %d\n", WTERMSIG(exit_status));
  	} else if (WIFEXITED(exit_status)) {
  		printf("normal exit, status %d\n", WEXITSTATUS(exit_status));
  	}
  }
  return(OK);
}


