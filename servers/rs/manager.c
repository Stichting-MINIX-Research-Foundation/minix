/*
 * Changes:
 *   Jul 22, 2005:	Created  (Jorrit N. Herder)
 */

#include "inc.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <minix/dmap.h>
#include <minix/endpoint.h>

/* Allocate variables. */
struct rproc rproc[NR_SYS_PROCS];		/* system process table */
struct rproc *rproc_ptr[NR_PROCS];		/* mapping for fast access */
int nr_in_use; 					/* number of services */
extern int errno;				/* error status */

/* Prototypes for internal functions that do the hard work. */
FORWARD _PROTOTYPE( int start_service, (struct rproc *rp) );
FORWARD _PROTOTYPE( int stop_service, (struct rproc *rp,int how) );

PRIVATE int shutting_down = FALSE;

#define EXEC_FAILED	49			/* recognizable status */

/*===========================================================================*
 *					do_up				     *
 *===========================================================================*/
PUBLIC int do_up(m_ptr)
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
      if (! rp->r_flags & RS_IN_USE) 		/* check if available */
	  break;
  }
  nr_in_use ++;					/* update administration */

  /* Obtain command name and parameters. This is a space-separated string
   * that looks like "/sbin/service arg1 arg2 ...". Arguments are optional.
   */
  if (m_ptr->RS_CMD_LEN > MAX_COMMAND_LEN) return(E2BIG);
  if (OK!=(s=sys_datacopy(m_ptr->m_source, (vir_bytes) m_ptr->RS_CMD_ADDR, 
  	SELF, (vir_bytes) rp->r_cmd, m_ptr->RS_CMD_LEN))) return(s);
  rp->r_cmd[m_ptr->RS_CMD_LEN] = '\0';		/* ensure it is terminated */
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

  /* Initialize some fields. */
  rp->r_period = m_ptr->RS_PERIOD;
  rp->r_dev_nr = m_ptr->RS_DEV_MAJOR;
  rp->r_dev_style = STYLE_DEV; 
  rp->r_restarts = -1; 				/* will be incremented */
  
  /* All information was gathered. Now try to start the system service. */
  return(start_service(rp));
}


/*===========================================================================*
 *				do_down					     *
 *===========================================================================*/
PUBLIC int do_down(message *m_ptr)
{
  register struct rproc *rp;
  pid_t pid = (pid_t) m_ptr->RS_PID;

  for (rp=BEG_RPROC_ADDR; rp<END_RPROC_ADDR; rp++) {
      if (rp->r_flags & RS_IN_USE && rp->r_pid == pid) {
#if VERBOSE
	  printf("stopping %d (%d)\n", pid, m_ptr->RS_PID);
#endif
	  stop_service(rp,RS_EXITING);
	  return(OK);
      }
  }
#if VERBOSE
  printf("not found %d (%d)\n", pid, m_ptr->RS_PID);
#endif
  return(ESRCH);
}


/*===========================================================================*
 *				do_refresh				     *
 *===========================================================================*/
PUBLIC int do_refresh(message *m_ptr)
{
  register struct rproc *rp;
  pid_t pid = (pid_t) m_ptr->RS_PID;

  for (rp=BEG_RPROC_ADDR; rp<END_RPROC_ADDR; rp++) {
      if (rp->r_flags & RS_IN_USE && rp->r_pid == pid) {
#if VERBOSE
	  printf("refreshing %d (%d)\n", pid, m_ptr->RS_PID);
#endif
	  stop_service(rp,RS_REFRESHING);
	  return(OK);
      }
  }
#if VERBOSE
  printf("not found %d (%d)\n", pid, m_ptr->RS_PID);
#endif
  return(ESRCH);
}

/*===========================================================================*
 *				do_rescue				     *
 *===========================================================================*/
PUBLIC int do_rescue(message *m_ptr)
{
  char rescue_dir[MAX_RESCUE_DIR_LEN];
  int s;

  /* Copy rescue directory from user. */
  if (m_ptr->RS_CMD_LEN > MAX_RESCUE_DIR_LEN) return(E2BIG);
  if (OK!=(s=sys_datacopy(m_ptr->m_source, (vir_bytes) m_ptr->RS_CMD_ADDR, 
  	SELF, (vir_bytes) rescue_dir, m_ptr->RS_CMD_LEN))) return(s);
  rescue_dir[m_ptr->RS_CMD_LEN] = '\0';		/* ensure it is terminated */
  if (rescue_dir[0] != '/') return(EINVAL);	/* insist on absolute path */

  /* Change RS' directory to the rescue directory. Provided that the needed
   * binaries are in the rescue dir, this makes recovery possible even if the 
   * (root) file system is no longer available, because no directory lookups
   * are required. Thus if an absolute path fails, we can try to strip the 
   * path an see if the command is in the rescue dir. 
   */
  if (chdir(rescue_dir) != 0) return(errno);
  return(OK);
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
      printf("RS: proc %d, pid %d, ", rp->r_proc_nr_e, exit_pid); 
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
          if ((rp->r_flags & RS_IN_USE) && rp->r_pid == exit_pid) {
	      int proc;
	      proc = _ENDPOINT_P(rp->r_proc_nr_e);

              rproc_ptr[proc] = NULL;		/* invalidate */

              if ((rp->r_flags & RS_EXITING) || shutting_down) {
		  rp->r_flags = 0;			/* release slot */
		  rproc_ptr[proc] = NULL;
	      }
	      else if(rp->r_flags & RS_REFRESHING) {
		      rp->r_restarts = -1;		/* reset counter */
		      start_service(rp);		/* direct restart */
	      }
              else if (WIFEXITED(exit_status) &&
		      WEXITSTATUS(exit_status) == EXEC_FAILED) {
		  rp->r_flags = 0;			/* release slot */
              }
	      else {
#if VERBOSE
		  printf("Unexpected exit. Restarting %s\n", rp->r_cmd);
#endif
                  /* Determine what to do. If this is the first unexpected 
		   * exit, immediately restart this service. Otherwise use
		   * a binary exponetial backoff.
		   */
                  if (rp->r_restarts > 0) {
		      rp->r_backoff = 1 << MIN(rp->r_restarts,(BACKOFF_BITS-1));
		      rp->r_backoff = MIN(rp->r_backoff,MAX_BACKOFF); 
		  }
		  else {
		      start_service(rp);		/* direct restart */
		  }
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
      if (rp->r_flags & RS_IN_USE) {

          /* If the service is to be revived (because it repeatedly exited, 
	   * and was not directly restarted), the binary backoff field is  
	   * greater than zero. 
	   */
	  if (rp->r_backoff > 0) {
              rp->r_backoff -= 1;
	      if (rp->r_backoff == 0) {
		  start_service(rp);
	      }
	  }

	  /* If the service was signaled with a SIGTERM and fails to respond,
	   * kill the system service with a SIGKILL signal.
	   */
	  else if (rp->r_stop_tm > 0 && now - rp->r_stop_tm > 2*RS_DELTA_T
	   && rp->r_pid > 0) {
              kill(rp->r_pid, SIGKILL);		/* terminate */
	  }
	
	  /* There seems to be no special conditions. If the service has a 
	   * period assigned check its status. 
	   */
	  else if (rp->r_period > 0) {

	      /* Check if an answer to a status request is still pending. If 
	       * the driver didn't respond within time, kill it to simulate 
	       * a crash. The failure will be detected and the service will 
	       * be restarted automatically.
	       */
              if (rp->r_alive_tm < rp->r_check_tm) { 
	          if (now - rp->r_alive_tm > 2*rp->r_period &&
		      rp->r_pid > 0) { 
#if VERBOSE
                      printf("RS: service %d reported late\n", rp->r_proc_nr_e); 
#endif
                      kill(rp->r_pid, SIGKILL);		/* simulate crash */
		  }
	      }

	      /* No answer pending. Check if a period expired since the last
	       * check and, if so request the system service's status.
	       */
	      else if (now - rp->r_check_tm > rp->r_period) {
#if VERBOSE
                  printf("RS: status request sent to %d\n", rp->r_proc_nr_e); 
#endif
		  notify(rp->r_proc_nr_e);		/* request status */
		  rp->r_check_tm = now;			/* mark time */
              }
          }
      }
  }

  /* Reschedule a synchronous alarm for the next period. */
  if (OK != (s=sys_setalarm(RS_DELTA_T, 0)))
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
  int child_proc_nr_e, child_proc_nr_n;		/* child process slot */
  pid_t child_pid;				/* child's process id */
  char *file_only;
  int s;
  message m;

  /* Now fork and branch for parent and child process (and check for error). */
  child_pid = fork();
  switch(child_pid) {					/* see fork(2) */
  case -1:						/* fork failed */
      report("RS", "warning, fork() failed", errno);	/* shouldn't happen */
      return(errno);					/* return error */

  case 0:						/* child process */
      /* Try to execute the binary that has an absolute path. If this fails, 
       * e.g., because the root file system cannot be read, try to strip of
       * the path, and see if the command is in RS' current working dir.
       */
      execve(rp->r_argv[0], rp->r_argv, NULL);		/* POSIX execute */
      file_only = strrchr(rp->r_argv[0], '/') + 1;
      execve(file_only, rp->r_argv, NULL);		/* POSIX execute */
      printf("RS: exec failed for %s: %d\n", rp->r_argv[0], errno);
      exit(EXEC_FAILED);				/* terminate child */

  default:						/* parent process */
      child_proc_nr_e = getnprocnr(child_pid);		/* get child slot */ 
      break;						/* continue below */
  }

  /* Only the parent process (the RS server) gets to this point. The child
   * is still inhibited from running because it's privilege structure is
   * not yet set. First try to set the device driver mapping at the FS.
   */
  if (rp->r_dev_nr > 0) {				/* set driver map */
      if ((s=mapdriver(child_proc_nr_e, rp->r_dev_nr, rp->r_dev_style)) < 0) {
          report("RS", "couldn't map driver", errno);
          rp->r_flags |= RS_EXITING;			/* expect exit */
	  if(child_pid > 0) kill(child_pid, SIGKILL);	/* kill driver */
	  else report("RS", "didn't kill pid", child_pid);
	  return(s);					/* return error */
      }
  }

  /* The device driver mapping has been set, or the service was not a driver.
   * Now, set the privilege structure for the child process to let is run.
   * This should succeed: we tested number in use above.
   */
  if ((s = sys_privctl(child_proc_nr_e, SYS_PRIV_INIT, 0, NULL)) < 0) {
      report("RS","call to SYSTEM failed", s);		/* to let child run */
      rp->r_flags |= RS_EXITING;			/* expect exit */
      if(child_pid > 0) kill(child_pid, SIGKILL);	/* kill driver */
      else report("RS", "didn't kill pid", child_pid);
      return(s);					/* return error */
  }

#if VERBOSE
      printf("RS: started '%s', major %d, pid %d, endpoint %d, proc %d\n", 
          rp->r_cmd, rp->r_dev_nr, child_pid,
	  child_proc_nr_e, child_proc_nr_n);
#endif

  /* The system service now has been successfully started. Update the rest
   * of the system process table that is maintain by the RS server. The only 
   * thing that can go wrong now, is that execution fails at the child. If 
   * that's the case, the child will exit. 
   */
  child_proc_nr_n = _ENDPOINT_P(child_proc_nr_e);
  rp->r_flags = RS_IN_USE;			/* mark slot in use */
  rp->r_restarts += 1;				/* raise nr of restarts */
  rp->r_proc_nr_e = child_proc_nr_e;		/* set child details */
  rp->r_pid = child_pid;
  rp->r_check_tm = 0;				/* not check yet */
  getuptime(&rp->r_alive_tm); 			/* currently alive */
  rp->r_stop_tm = 0;				/* not exiting yet */
  rproc_ptr[child_proc_nr_n] = rp;		/* mapping for fast access */
  return(OK);
}

/*===========================================================================*
 *				stop_service				     *
 *===========================================================================*/
PRIVATE int stop_service(rp,how)
struct rproc *rp;
int how;
{
  /* Try to stop the system service. First send a SIGTERM signal to ask the
   * system service to terminate. If the service didn't install a signal 
   * handler, it will be killed. If it did and ignores the signal, we'll
   * find out because we record the time here and send a SIGKILL.
   */
#if VERBOSE
  printf("RS tries to stop %s (pid %d)\n", rp->r_cmd, rp->r_pid);
#endif

  rp->r_flags |= how;				/* what to on exit? */
  if(rp->r_pid > 0) kill(rp->r_pid, SIGTERM);	/* first try friendly */
  else report("RS", "didn't kill pid", rp->r_pid);
  getuptime(&rp->r_stop_tm); 			/* record current time */
}


/*===========================================================================*
 *				do_getsysinfo				     *
 *===========================================================================*/
PUBLIC int do_getsysinfo(m_ptr)
message *m_ptr;
{
  vir_bytes src_addr, dst_addr;
  int dst_proc;
  size_t len;
  int s;

  switch(m_ptr->m1_i1) {
  case SI_PROC_TAB:
  	src_addr = (vir_bytes) rproc;
  	len = sizeof(struct rproc) * NR_SYS_PROCS;
  	break; 
  default:
  	return(EINVAL);
  }

  dst_proc = m_ptr->m_source;
  dst_addr = (vir_bytes) m_ptr->m1_p1;
  if (OK != (s=sys_datacopy(SELF, src_addr, dst_proc, dst_addr, len)))
  	return(s);
  return(OK);
}

