/*
 * Changes:
 *   Jul 22, 2005:	Created  (Jorrit N. Herder)
 */

#include "rs.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <minix/dmap.h>

/* Allocate variables. */
struct rproc rproc[NR_SYS_PROCS];		/* system process table */
struct rproc *rproc_ptr[NR_PROCS];		/* mapping for fast access */
int nr_in_use; 					/* number of services */
extern int errno;				/* error status */

/* Prototypes for internal functions that do the hard work. */
FORWARD _PROTOTYPE( int start_service, (struct rproc *rp) );
FORWARD _PROTOTYPE( int stop_service, (struct rproc *rp) );

PRIVATE int shutting_down = FALSE;

#define EXEC_FAILED	49			/* recognizable status */

/*===========================================================================*
 *				do_start				     *
 *===========================================================================*/
PUBLIC int do_start(m_ptr)
message *m_ptr;					/* request message pointer */
{
/* A request was made to start a new system service. Dismember the request 
 * message and gather all information needed to start the service. Starting
 * is done by a helper routine.
 */
  register struct rproc *rp;			/* system process table */
  int slot_nr;					/* local table entry */
  int arg_count;				/* number of arguments */
  char *cmd_ptr;				/* parse command string */
  enum dev_style dev_style;			/* device style */
  int s;					/* status variable */

  /* See if there is a free entry in the table with system processes. */
  if (nr_in_use >= NR_SYS_PROCS) return(EAGAIN); 
  for (slot_nr = 0; slot_nr < NR_SYS_PROCS; slot_nr++) {
      rp = &rproc[slot_nr];			/* get pointer to slot */
      if (! rp->r_flags & IN_USE) 		/* check if available */
	  break;
  }
  nr_in_use ++;					/* update administration */

  /* Obtain command name and parameters. This is a space-separated string
   * that looks like "/sbin/service arg1 arg2 ...". Arguments are optional.
   */
  if (m_ptr->SRV_CMD_LEN > MAX_COMMAND_LEN) return(E2BIG);
  if (OK!=(s=sys_datacopy(m_ptr->m_source, (vir_bytes) m_ptr->SRV_CMD_ADDR, 
  	SELF, (vir_bytes) rp->r_cmd, m_ptr->SRV_CMD_LEN))) return(s);
  rp->r_cmd[m_ptr->SRV_CMD_LEN] = '\0';		/* ensure it is terminated */
  if (rp->r_cmd[0] != '/') return(EINVAL);	/* insist on absolute path */

  /* Build argument vector to be passed to execute call. The format of the
   * arguments vector is: path, arguments, NULL. 
   */
  arg_count = 0;				/* initialize arg count */
  rp->r_argv[arg_count++] = rp->r_cmd;		/* start with path */
  cmd_ptr = rp->r_cmd;				/* do some parsing */ 
  while(*cmd_ptr != '\0') {			/* stop at end of string */
      if (*cmd_ptr == ' ') {			/* next argument */
          *cmd_ptr = '\0';			/* terminate previous */
	  while (*++cmd_ptr == ' ') ; 		/* skip spaces */
	  if (*cmd_ptr == '\0') break;		/* no arg following */
	  if (arg_count>MAX_NR_ARGS+1) break;	/* arg vector full */
          rp->r_argv[arg_count++] = cmd_ptr;	/* add to arg vector */
      }
      cmd_ptr ++;				/* continue parsing */
  }
  rp->r_argv[arg_count] = NULL;			/* end with NULL pointer */
  rp->r_argc = arg_count;

  /* Check if a heartbeat period was given. */
  rp->r_period = m_ptr->SRV_PERIOD;
  rp->r_dev_nr = m_ptr->SRV_DEV_MAJOR;
  rp->r_dev_style = STYLE_DEV; 
  
  /* All information was gathered. Now try to start the system service. */
  return(start_service(rp));
}


/*===========================================================================*
 *				do_stop					     *
 *===========================================================================*/
PUBLIC int do_stop(message *m_ptr)
{
  register struct rproc *rp;
  pid_t pid = (pid_t) m_ptr->SRV_PID;

  for (rp=BEG_RPROC_ADDR; rp<END_RPROC_ADDR; rp++) {
      if (rp->r_flags & IN_USE && rp->r_pid == pid) {
	  printf("stopping %d (%d)\n", pid, m_ptr->SRV_PID);
	  stop_service(rp);
	  return(OK);
      }
  }
  printf("not found %d (%d)\n", pid, m_ptr->SRV_PID);
  return(ESRCH);
}


/*===========================================================================*
 *				do_shutdown				     *
 *===========================================================================*/
PUBLIC int do_shutdown(message *m_ptr)
{
  /* Set flag so that RS server knows services shouldn't be restarted. */
  shutting_down = TRUE;
  return(OK);
}

/*===========================================================================*
 *				do_exit					     *
 *===========================================================================*/
PUBLIC void do_exit(message *m_ptr)
{
  register struct rproc *rp;
  pid_t exit_pid;
  int exit_status;

#if VERBOSE
  printf("RS: got SIGCHLD signal, doing wait to get exited child.\n");
#endif

  /* See which child exited and what the exit status is. This is done in a
   * loop because multiple childs may have exited, all reported by one 
   * SIGCHLD signal. The WNOHANG options is used to prevent blocking if, 
   * somehow, no exited child can be found. 
   */
  while ( (exit_pid = waitpid(-1, &exit_status, WNOHANG)) != 0 ) {

#if VERBOSE
      printf("RS: proc %d, pid %d,", rp->r_proc_nr, exit_pid); 
      if (WIFSIGNALED(exit_status)) {
          printf("killed, signal number %d\n", WTERMSIG(exit_status));
      } 
      else if (WIFEXITED(exit_status)) {
          printf("normal exit, status %d\n", WEXITSTATUS(exit_status));
      }
#endif

      /* Search the system process table to see who exited. 
       * This should always succeed. 
       */
      for (rp=BEG_RPROC_ADDR; rp<END_RPROC_ADDR; rp++) {
          if ((rp->r_flags & IN_USE) && rp->r_pid == exit_pid) {

	      printf("Slot found!\n");
              rproc_ptr[rp->r_proc_nr] = NULL;		/* invalidate */

              if ((rp->r_flags & EXIT_PENDING) || shutting_down) {
		  printf("Expected exit. Doing nothing.\n");
		  rp->r_flags = 0;			/* release slot */
		  rproc_ptr[rp->r_proc_nr] = NULL;
	      }
              else if (WIFEXITED(exit_status) &&
		      WEXITSTATUS(exit_status) == EXEC_FAILED) {
		  printf("Exit because EXEC() failed. Doing nothing.\n");
		  rp->r_flags = 0;			/* release slot */
              }
	      else {
		  printf("Unexpected exit. Restarting %s\n", rp->r_cmd);
		  start_service(rp);			/* restart */
              }
	      break;
	  }
      }
  }
}

/*===========================================================================*
 *				do_period				     *
 *===========================================================================*/
PUBLIC void do_period(m_ptr)
message *m_ptr;
{
  register struct rproc *rp;
  clock_t now = m_ptr->NOTIFY_TIMESTAMP;
  int s;

  /* Search system services table. Only check slots that are in use. */
  for (rp=BEG_RPROC_ADDR; rp<END_RPROC_ADDR; rp++) {
      if (rp->r_flags & IN_USE) {
	
	  /* If the service has a period assigned check its status. */
	  if (rp->r_period > 0) {

	      /* Check if an answer to a status request is still pending. If 
	       * the driver didn't respond within time, kill it to simulate 
	       * a crash. The failure will be detected and the service will 
	       * be restarted automatically.
	       */
              if (rp->r_alive_tm < rp->r_check_tm) { 
	          if (now - rp->r_alive_tm > 2*rp->r_period) { 
#if VERBOSE
                      printf("RS: service %d reported late\n", rp->r_proc_nr); 
#endif
                      kill(rp->r_pid, SIGKILL);		/* simulate crash */
		  }
	      }

	      /* No answer pending. Check if a period expired since the last
	       * check and, if so request the system service's status.
	       */
	      else if (now - rp->r_check_tm > rp->r_period) {
#if VERBOSE
                  printf("RS: status request sent to %d\n", rp->r_proc_nr); 
#endif
		  notify(rp->r_proc_nr);		/* request status */
		  rp->r_check_tm = now;			/* mark time */
              }
          }

	  /* If the service was signaled with a SIGTERM and fails to respond,
	   * kill the system service with a SIGKILL signal.
	   */
	  if (rp->r_stop_tm > 0 && now - rp->r_stop_tm > 2*HZ) {
              kill(rp->r_pid, SIGKILL);		/* terminate */
	  }
      }
  }

  /* Reschedule a synchronous alarm for the next period. */
  if (OK != (s=sys_setalarm(HZ, 0)))
      panic("RS", "couldn't set alarm", s);
}


/*===========================================================================*
 *				start_service				     *
 *===========================================================================*/
PRIVATE int start_service(rp)
struct rproc *rp;
{
/* Try to execute the given system service. Fork a new process. The child
 * process will be inhibited from running by the NO_PRIV flag. Only let the
 * child run once its privileges have been set by the parent.
 */
  int child_proc_nr;				/* child process slot */
  pid_t child_pid;				/* child's process id */
  int s;
  message m;

  /* Now fork and branch for parent and child process (and check for error). */
  child_pid = fork();
  switch(child_pid) {					/* see fork(2) */
  case -1:						/* fork failed */
      report("RS", "warning, fork() failed", errno);	/* shouldn't happen */
      return(errno);					/* return error */

  case 0:						/* child process */
      execve(rp->r_argv[0], rp->r_argv, NULL);		/* POSIX execute */
      report("RS", "warning, exec() failed", errno);	/* shouldn't happen */
      exit(EXEC_FAILED);				/* terminate child */

  default:						/* parent process */
      child_proc_nr = getnprocnr(child_pid);		/* get child slot */ 
      break;						/* continue below */
  }

  /* Only the parent process (the RS server) gets to this point. The child
   * is still inhibited from running because it's privilege structure is
   * not yet set. First try to set the device driver mapping at the FS.
   */
  if (rp->r_dev_nr > 0) {				/* set driver map */
      if ((s=mapdriver(child_proc_nr, rp->r_dev_nr, rp->r_dev_style)) < 0) {
          report("RS", "couldn't map driver", errno);
          kill(child_pid, SIGKILL);			/* kill driver */
          rp->r_flags |= EXIT_PENDING;			/* expect exit */
	  return(s);					/* return error */
      }
  }

  /* The device driver mapping has been set, or the service was not a driver.
   * Now, set the privilege structure for the child process to let is run.
   * This should succeed: we tested number in use above.
   */
  m.PR_PROC_NR = child_proc_nr;
  if ((s = _taskcall(SYSTEM, SYS_PRIVCTL, &m)) < 0) { 	/* set privileges */
      report("RS","call to SYSTEM failed", s);		/* to let child run */
      kill(child_pid, SIGKILL);				/* kill driver */
      rp->r_flags |= EXIT_PENDING;			/* expect exit */
      return(s);					/* return error */
  }

#if VERBOSE
      printf("RS: started '%s', major %d, pid %d, proc_nr %d\n", 
          rp->r_cmd, rp->r_dev_nr, child_pid, child_proc_nr);
#endif

  /* The system service now has been successfully started. Update the rest
   * of the system process table that is maintain by the RS server. The only 
   * thing that can go wrong now, is that execution fails at the child. If 
   * that's the case, the child will exit. 
   */
  rp->r_flags = IN_USE;				/* mark slot in use */
  rp->r_proc_nr = child_proc_nr;		/* set child details */
  rp->r_pid = child_pid;
  rp->r_check_tm = 0;				/* not check yet */
  getuptime(&rp->r_alive_tm); 			/* currently alive */
  rp->r_stop_tm = 0;				/* not exiting yet */
  rproc_ptr[child_proc_nr] = rp;		/* mapping for fast access */
  return(OK);
}

/*===========================================================================*
 *				stop_service				     *
 *===========================================================================*/
PRIVATE int stop_service(rp)
struct rproc *rp;
{
  printf("RS tries to stop %s (pid %d)\n", rp->r_cmd, rp->r_pid);
  /* Try to stop the system service. First send a SIGTERM signal to ask the
   * system service to terminate. If the service didn't install a signal 
   * handler, it will be killed. If it did and ignores the signal, we'll
   * find out because we record the time here and send a SIGKILL.
   */
  rp->r_flags |= EXIT_PENDING;			/* expect exit */
  kill(rp->r_pid, SIGTERM);			/* first try friendly */
  getuptime(&rp->r_stop_tm); 			/* record current time */
}
