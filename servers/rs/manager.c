/*
 * Changes:
 *   Jul 22, 2005:	Created  (Jorrit N. Herder)
 */

#include "inc.h"
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <minix/dmap.h>
#include <minix/ds.h>
#include <minix/endpoint.h>
#include <minix/rs.h>
#include <lib.h>

#include <timers.h>				/* For priv.h */
#include "../../kernel/priv.h"

/* Allocate variables. */
struct rproc rproc[NR_SYS_PROCS];		/* system process table */
struct rproc *rproc_ptr[NR_PROCS];		/* mapping for fast access */

/* Prototypes for internal functions that do the hard work. */
FORWARD _PROTOTYPE( int start_service, (struct rproc *rp, int flags,
	endpoint_t *ep) );
FORWARD _PROTOTYPE( int stop_service, (struct rproc *rp,int how) );
FORWARD _PROTOTYPE( int fork_nb, (void) );
FORWARD _PROTOTYPE( int read_exec, (struct rproc *rp) );
FORWARD _PROTOTYPE( void run_script, (struct rproc *rp) );
FORWARD _PROTOTYPE( void init_privs, (struct rproc *rp, struct priv *privp) );
FORWARD _PROTOTYPE( void init_pci, (struct rproc *rp, int endpoint) );

PRIVATE int shutting_down = FALSE;

extern int rs_verbose;

/*===========================================================================*
 *					do_up				     *
 *===========================================================================*/
PUBLIC int do_up(m_ptr, do_copy, flags)
message *m_ptr;					/* request message pointer */
int do_copy;					/* keep copy in memory */
int flags;					/* extra flags, if any */
{
/* A request was made to start a new system service. Dismember the request 
 * message and gather all information needed to start the service. Starting
 * is done by a helper routine.
 */
  register struct rproc *rp;			/* system process table */
  int slot_nr;					/* local table entry */
  int arg_count;				/* number of arguments */
  char *cmd_ptr;				/* parse command string */
  char *label;					/* unique name of command */
  enum dev_style dev_style;			/* device style */
  int s;					/* status variable */
  int len;					/* length of string */
  int r;
  endpoint_t ep;				/* new endpoint no. */

printf("RS: in do_up\n");

  /* See if there is a free entry in the table with system processes. */
  for (slot_nr = 0; slot_nr < NR_SYS_PROCS; slot_nr++) {
      rp = &rproc[slot_nr];			/* get pointer to slot */
      if (! rp->r_flags & RS_IN_USE) 		/* check if available */
	  break;
  }

  /* Obtain command name and parameters. This is a space-separated string
   * that looks like "/sbin/service arg1 arg2 ...". Arguments are optional.
   */
  if (m_ptr->RS_CMD_LEN > MAX_COMMAND_LEN) return(E2BIG);
  if (OK!=(s=sys_datacopy(m_ptr->m_source, (vir_bytes) m_ptr->RS_CMD_ADDR, 
  	SELF, (vir_bytes) rp->r_cmd, m_ptr->RS_CMD_LEN))) return(s);
  rp->r_cmd[m_ptr->RS_CMD_LEN] = '\0';		/* ensure it is terminated */
  if (rp->r_cmd[0] != '/') return(EINVAL);	/* insist on absolute path */

  rp->r_script[0]= '\0';

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

  /* Default label for the driver */
  label= strrchr(rp->r_argv[0], '/');
  if (label)
	label++;
  else
	label= rp->r_argv[0];
  len= strlen(label);
  if (len > MAX_LABEL_LEN-1)
	len= MAX_LABEL_LEN-1;	/* truncate name */
  memcpy(rp->r_label, label, len);
  rp->r_label[len]= '\0';
  if(rs_verbose) printf("RS: do_up: using label '%s'\n", rp->r_label);

  rp->r_uid= 0;
  rp->r_nice= 0;

  rp->r_exec= NULL;
  if (do_copy)
  {
	s= read_exec(rp);
	if (s != OK)
		return s;
  }

  /* Initialize some fields. */
  rp->r_period = m_ptr->RS_PERIOD;
  rp->r_dev_nr = m_ptr->RS_DEV_MAJOR;
  rp->r_dev_style = STYLE_DEV; 
  rp->r_restarts = -1; 				/* will be incremented */
  rp->r_set_resources= 0;			/* old style */
  
  /* All information was gathered. Now try to start the system service. */

  r = start_service(rp, flags, &ep);
  m_ptr->RS_ENDPOINT = ep;
  return r;
}


/*===========================================================================*
 *					do_start			     *
 *===========================================================================*/
PUBLIC int do_start(m_ptr)
message *m_ptr;					/* request message pointer */
{
/* A request was made to start a new system service. 
 */
  register struct rproc *rp;			/* system process table */
  int slot_nr;					/* local table entry */
  int arg_count;				/* number of arguments */
  char *cmd_ptr;				/* parse command string */
  char *label;					/* unique name of command */
  enum dev_style dev_style;			/* device style */
  int s;					/* status variable */
  int len;					/* length of string */
  int i;
  int r;
  endpoint_t ep;
  struct rproc *tmp_rp;
  struct rs_start rs_start;

  /* Get the request structure */
  s= sys_datacopy(m_ptr->m_source, (vir_bytes) m_ptr->RS_CMD_ADDR, 
  	SELF, (vir_bytes) &rs_start, sizeof(rs_start));
  if (s != OK) return(s);

  /* See if there is a free entry in the table with system processes. */
  for (slot_nr = 0; slot_nr < NR_SYS_PROCS; slot_nr++) {
      rp = &rproc[slot_nr];			/* get pointer to slot */
      if (!(rp->r_flags & RS_IN_USE)) 		/* check if available */
	  break;
  }
  if (slot_nr >= NR_SYS_PROCS)
  {
	printf("rs`do_start: driver table full\n");
	return ENOMEM;
  }

  /* Obtain command name and parameters. This is a space-separated string
   * that looks like "/sbin/service arg1 arg2 ...". Arguments are optional.
   */
  if (rs_start.rss_cmdlen > MAX_COMMAND_LEN-1) return(E2BIG);
  s=sys_datacopy(m_ptr->m_source, (vir_bytes) rs_start.rss_cmd, 
  	SELF, (vir_bytes) rp->r_cmd, rs_start.rss_cmdlen);
  if (s != OK) return(s);
  rp->r_cmd[rs_start.rss_cmdlen] = '\0';	/* ensure it is terminated */
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

  if(rs_start.rss_label) {
	int len;
	/* RS_START caller has supplied a custom label for this driver. */
	len = MIN(sizeof(rp->r_label)-1, rs_start.rss_labellen);
        s=sys_datacopy(m_ptr->m_source, (vir_bytes) rs_start.rss_label,
        	SELF, (vir_bytes) rp->r_label, len);
	if(s != OK)
		return s;
	rp->r_label[len] = '\0';
        if(rs_verbose)
	  printf("RS: do_start: using label (custom) '%s'\n", rp->r_label);
  } else {
	/* Default label for the driver. */
	label= strrchr(rp->r_argv[0], '/');
	if (label)
		label++;
	else
		label= rp->r_argv[0];
  	len= strlen(label);
  	if (len > MAX_LABEL_LEN-1)
		len= MAX_LABEL_LEN-1;	/* truncate name */
  	memcpy(rp->r_label, label, len);
  	rp->r_label[len]= '\0';
        if(rs_verbose)
          printf("RS: do_start: using label (from binary %s) '%s'\n",
		rp->r_argv[0], rp->r_label);
  }

  /* Check for duplicates */
  for (slot_nr = 0; slot_nr < NR_SYS_PROCS; slot_nr++) {
      tmp_rp = &rproc[slot_nr];			/* get pointer to slot */
      if (!(tmp_rp->r_flags & RS_IN_USE)) 	/* check if available */
	  continue;
      if (tmp_rp == rp)
	  continue;				/* Our slot */
      if (strcmp(tmp_rp->r_label, rp->r_label) == 0)
      {
	  printf("RS: found duplicate label '%s': slot %d\n",
		rp->r_label, slot_nr);
	  return EBUSY;
      }
  }

  rp->r_script[0]= '\0';
  if (rs_start.rss_scriptlen > MAX_SCRIPT_LEN-1) return(E2BIG);
  if (rs_start.rss_script != NULL)
  {
	  s=sys_datacopy(m_ptr->m_source, (vir_bytes) rs_start.rss_script, 
		SELF, (vir_bytes) rp->r_script, rs_start.rss_scriptlen);
	  if (s != OK) return(s);
	  rp->r_script[rs_start.rss_scriptlen] = '\0';
  }
  rp->r_uid= rs_start.rss_uid;
  rp->r_nice= rs_start.rss_nice;

  if (rs_start.rss_flags & RF_IPC_VALID)
  {
	if (rs_start.rss_ipclen+1 > sizeof(rp->r_ipc_list))
	{
		printf("rs: ipc list too long for '%s'\n", rp->r_label);
		return EINVAL;
	}
	s=sys_datacopy(m_ptr->m_source, (vir_bytes) rs_start.rss_ipc, 
		SELF, (vir_bytes) rp->r_ipc_list, rs_start.rss_ipclen);
	if (s != OK) return(s);
	rp->r_ipc_list[rs_start.rss_ipclen]= '\0';
  }
  else
	rp->r_ipc_list[0]= '\0';

  rp->r_exec= NULL;
  if (rs_start.rss_flags & RF_COPY)
  {
	s= read_exec(rp);
	if (s != OK)
		return s;
  }

  /* Copy granted resources */
  if (rs_start.rss_nr_irq > NR_IRQ)
  {
	printf("RS: do_start: too many IRQs requested\n");
	return EINVAL;
  }
  rp->r_priv.s_nr_irq= rs_start.rss_nr_irq;
  for (i= 0; i<rp->r_priv.s_nr_irq; i++)
  {
	rp->r_priv.s_irq_tab[i]= rs_start.rss_irq[i];
	if(rs_verbose)
		printf("RS: do_start: IRQ %d\n", rp->r_priv.s_irq_tab[i]);
  }

  if (rs_start.rss_nr_io > NR_IO_RANGE)
  {
	printf("RS: do_start: too many I/O ranges requested\n");
	return EINVAL;
  }
  rp->r_priv.s_nr_io_range= rs_start.rss_nr_io;
  for (i= 0; i<rp->r_priv.s_nr_io_range; i++)
  {
	rp->r_priv.s_io_tab[i].ior_base= rs_start.rss_io[i].base;
	rp->r_priv.s_io_tab[i].ior_limit=
		rs_start.rss_io[i].base+rs_start.rss_io[i].len-1;
#if 0
	if(rs_verbose)
	   printf("RS: do_start: I/O [%x..%x]\n",
		rp->r_priv.s_io_tab[i].ior_base,
		rp->r_priv.s_io_tab[i].ior_limit);
#endif
  }

  if (rs_start.rss_nr_pci_id > MAX_NR_PCI_ID)
  {
	printf("RS: do_start: too many PCI device IDs\n");
	return EINVAL;
  }
  rp->r_nr_pci_id= rs_start.rss_nr_pci_id;
  for (i= 0; i<rp->r_nr_pci_id; i++)
  {
	rp->r_pci_id[i].vid= rs_start.rss_pci_id[i].vid;
	rp->r_pci_id[i].did= rs_start.rss_pci_id[i].did;
	if(rs_verbose)
	   printf("RS: do_start: PCI %04x/%04x\n",
		rp->r_pci_id[i].vid, rp->r_pci_id[i].did);
  }
  if (rs_start.rss_nr_pci_class > MAX_NR_PCI_CLASS)
  {
	printf("RS: do_start: too many PCI class IDs\n");
	return EINVAL;
  }
  rp->r_nr_pci_class= rs_start.rss_nr_pci_class;
  for (i= 0; i<rp->r_nr_pci_class; i++)
  {
	rp->r_pci_class[i].class= rs_start.rss_pci_class[i].class;
	rp->r_pci_class[i].mask= rs_start.rss_pci_class[i].mask;
	if(rs_verbose)
	    printf("RS: do_start: PCI class %06x mask %06x\n",
		rp->r_pci_class[i].class, rp->r_pci_class[i].mask);
  }

  /* Copy 'system' call number bits */
  if (sizeof(rs_start.rss_system[0]) == sizeof(rp->r_call_mask[0]) &&
	sizeof(rs_start.rss_system) == sizeof(rp->r_call_mask))
  {
	for (i= 0; i<RSS_NR_SYSTEM; i++)
		rp->r_call_mask[i]= rs_start.rss_system[i];
  }
  else
  {
	printf(
	"RS: do_start: internal inconsistency: bad size of r_call_mask\n");
	memset(rp->r_call_mask, '\0', sizeof(rp->r_call_mask));
  }

  /* Initialize some fields. */
  rp->r_period = rs_start.rss_period;
  rp->r_dev_nr = rs_start.rss_major;
  rp->r_dev_style = STYLE_DEV; 
  rp->r_restarts = -1; 				/* will be incremented */
  rp->r_set_resources= 1;			/* new style, enforece
						 * I/O resources
						 */
  
  /* All information was gathered. Now try to start the system service. */
  r = start_service(rp, 0, &ep);
  m_ptr->RS_ENDPOINT = ep;
  return r;
}


/*===========================================================================*
 *				do_down					     *
 *===========================================================================*/
PUBLIC int do_down(message *m_ptr)
{
  register struct rproc *rp;
  size_t len;
  int s, proc;
  char label[MAX_LABEL_LEN];

  len= m_ptr->RS_CMD_LEN;
  if (len >= sizeof(label))
	return EINVAL;		/* Too long */

  s= sys_datacopy(m_ptr->m_source, (vir_bytes) m_ptr->RS_CMD_ADDR, 
  	SELF, (vir_bytes) label, len);
  if (s != OK) return(s);
  label[len]= '\0';

  for (rp=BEG_RPROC_ADDR; rp<END_RPROC_ADDR; rp++) {
      if (rp->r_flags & RS_IN_USE && strcmp(rp->r_label, label) == 0) {
	if(rs_verbose)
	  printf("RS: stopping '%s' (%d)\n", label, rp->r_pid);
	  stop_service(rp,RS_EXITING);
	  if (rp->r_pid == -1)
	  {
		/* Process is already gone */
		rp->r_flags = 0;			/* release slot */
		if (rp->r_exec)
		{
			free(rp->r_exec);
			rp->r_exec= NULL;
		}
		proc = _ENDPOINT_P(rp->r_proc_nr_e);
		rproc_ptr[proc] = NULL;
	  	return(OK);
	  }

	  /* Late reply - send a reply when process dies. */
	  rp->r_flags |= RS_LATEREPLY;
	  rp->r_caller = m_ptr->m_source;
	  return EDONTREPLY;
      }
  }
  if(rs_verbose) printf("RS: do_down: '%s' not found\n", label);
  return(ESRCH);
}


/*===========================================================================*
 *				do_restart				     *
 *===========================================================================*/
PUBLIC int do_restart(message *m_ptr)
{
  register struct rproc *rp;
  size_t len;
  int s, proc, r;
  char label[MAX_LABEL_LEN];
  endpoint_t ep;

  len= m_ptr->RS_CMD_LEN;
  if (len >= sizeof(label))
	return EINVAL;		/* Too long */

  s= sys_datacopy(m_ptr->m_source, (vir_bytes) m_ptr->RS_CMD_ADDR, 
  	SELF, (vir_bytes) label, len);
  if (s != OK) return(s);
  label[len]= '\0';

  for (rp=BEG_RPROC_ADDR; rp<END_RPROC_ADDR; rp++) {
      if ((rp->r_flags & RS_IN_USE) && strcmp(rp->r_label, label) == 0) {
	  if(rs_verbose) printf("RS: restarting '%s' (%d)\n", label, rp->r_pid);
	  if (rp->r_pid >= 0)
	  {
		if(rs_verbose)
		  printf("RS: do_restart: '%s' is (still) running, pid = %d\n",
			rp->r_pid);
		return EBUSY;
	  }
	  rp->r_flags &= ~(RS_EXITING|RS_REFRESHING|RS_NOPINGREPLY);
	  r = start_service(rp, 0, &ep);	
	  if (r != OK) printf("do_restart: start_service failed: %d\n", r);
	  m_ptr->RS_ENDPOINT = ep;
	  return(r);
      }
  }
#if VERBOSE
  printf("RS: do_restart: '%s' not found\n", label);
#endif
  return(ESRCH);
}


/*===========================================================================*
 *				do_refresh				     *
 *===========================================================================*/
PUBLIC int do_refresh(message *m_ptr)
{
  register struct rproc *rp;
  size_t len;
  int s;
  char label[MAX_LABEL_LEN];

  len= m_ptr->RS_CMD_LEN;
  if (len >= sizeof(label))
	return EINVAL;		/* Too long */

  s= sys_datacopy(m_ptr->m_source, (vir_bytes) m_ptr->RS_CMD_ADDR, 
  	SELF, (vir_bytes) label, len);
  if (s != OK) return(s);
  label[len]= '\0';

  for (rp=BEG_RPROC_ADDR; rp<END_RPROC_ADDR; rp++) {
      if (rp->r_flags & RS_IN_USE && strcmp(rp->r_label, label) == 0) {
#if VERBOSE
	  printf("RS: refreshing %s (%d)\n", rp->r_label, rp->r_pid);
#endif
	  stop_service(rp,RS_REFRESHING);
	  return(OK);
      }
  }
#if VERBOSE
  printf("RS: do_refresh: '%s' not found\n", label);
#endif
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
  int exit_status, r, slot_nr;
  endpoint_t ep;

  if(rs_verbose)
     printf("RS: got SIGCHLD signal, doing wait to get exited child.\n");

  /* See which child exited and what the exit status is. This is done in a
   * loop because multiple childs may have exited, all reported by one 
   * SIGCHLD signal. The WNOHANG options is used to prevent blocking if, 
   * somehow, no exited child can be found. 
   */
  while ( (exit_pid = waitpid(-1, &exit_status, WNOHANG)) != 0 ) {

    if(rs_verbose) {
#if 0
      printf("RS: pid %d, ", exit_pid); 
#endif
      if (WIFSIGNALED(exit_status)) {
#if 0
          printf("killed, signal number %d\n", WTERMSIG(exit_status));
#endif
      } 
      else if (WIFEXITED(exit_status)) {
#if 0
          printf("normal exit, status %d\n", WEXITSTATUS(exit_status));
#endif
      }
    }

	/* Read from the exec pipe */
	for (;;)
	{
		r= read(exec_pipe[0], &slot_nr, sizeof(slot_nr));
		if (r == -1)
		{
			if (errno == -EAGAIN)	/* Negative error defines */
				break;	/* No data */
			panic("RS", "do_exit: read from exec pipe failed",
				errno);
		}
		if (r != sizeof(slot_nr))
		{
			panic("RS", "do_exit: unaligned read from exec pipe",
				r);
		}
		printf("do_exit: got slot %d\n", slot_nr);
		if (slot_nr < 0 || slot_nr >= NR_SYS_PROCS)
		{
			panic("RS", "do_exit: bad slot number from exec pipe",
				slot_nr);
		}
		rp= &rproc[slot_nr];
		rp->r_flags |= RS_EXECFAILED;
	}

      /* Search the system process table to see who exited. 
       * This should always succeed. 
       */
      for (rp=BEG_RPROC_ADDR; rp<END_RPROC_ADDR; rp++) {
          if ((rp->r_flags & RS_IN_USE) && rp->r_pid == exit_pid) {
	      int proc;
	      proc = _ENDPOINT_P(rp->r_proc_nr_e);

              rproc_ptr[proc] = NULL;		/* invalidate */
	      rp->r_pid= -1;

	      pci_del_acl(rp->r_proc_nr_e);	/* Ignore errors */

              if ((rp->r_flags & RS_EXITING) || shutting_down) {
		  /* No reply sent to RS_DOWN yet. */
		  if(rp->r_flags & RS_LATEREPLY) {
			message rsm;
			rsm.m_type = OK;
			send(rp->r_caller, &rsm);
		  }

		  /* Release slot. */
		  rp->r_flags = 0;
		  if (rp->r_exec)
		  {
			free(rp->r_exec);
			rp->r_exec= NULL;
		  }
		  rproc_ptr[proc] = NULL;

	      }
	      else if(rp->r_flags & RS_REFRESHING) {
		      rp->r_restarts = -1;		/* reset counter */
		      if (rp->r_script[0] != '\0')
			run_script(rp);
		      else {
		        start_service(rp, 0, &ep); /* direct restart */
	  		m_ptr->RS_ENDPOINT = ep;
		      }
	      }
              else if (rp->r_flags & RS_EXECFAILED) {
		  rp->r_flags = 0;			/* release slot */
              }
	      else {
		if(rs_verbose)
		  printf("RS: unexpected exit. Restarting %s\n", rp->r_cmd);
                  /* Determine what to do. If this is the first unexpected 
		   * exit, immediately restart this service. Otherwise use
		   * a binary exponetial backoff.
		   */
#if 0
rp->r_restarts= 0;
#endif
		  if (WIFSIGNALED(exit_status)) {
			switch(WTERMSIG(exit_status))
			{
			case SIGKILL:	rp->r_flags |= RS_KILLED; break;
			default: 	rp->r_flags |= RS_SIGNALED; break;
			}
		  } 
		  else
			rp->r_flags |= RS_CRASHED;

		  if (rp->r_script[0] != '\0')
		      run_script(rp);
		  else if (rp->r_restarts > 0) {
		      rp->r_backoff = 1 << MIN(rp->r_restarts,(BACKOFF_BITS-2));
		      rp->r_backoff = MIN(rp->r_backoff,MAX_BACKOFF); 
		      if (rp->r_exec != NULL && rp->r_backoff > 1)
			rp->r_backoff= 1;
		  }
		  else {
		      start_service(rp, 0, &ep);	/* direct restart */
	  	      m_ptr->RS_ENDPOINT = ep;
			/* Do this even if no I/O happens with the ioctl, in
			 * order to disambiguate requests with DEV_IOCTL_S.
			 */
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
  endpoint_t ep;

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
		  start_service(rp, 0, &ep);
	  	  m_ptr->RS_ENDPOINT = ep;
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
		      rp->r_pid > 0 && !(rp->r_flags & RS_NOPINGREPLY)) { 
		      if(rs_verbose)
                           printf("RS: service %d reported late\n",
				rp->r_proc_nr_e); 
		      rp->r_flags |= RS_NOPINGREPLY;
                      kill(rp->r_pid, SIGKILL);		/* simulate crash */
		  }
	      }

	      /* No answer pending. Check if a period expired since the last
	       * check and, if so request the system service's status.
	       */
	      else if (now - rp->r_check_tm > rp->r_period) {
		if(rs_verbose)
                  printf("RS: status request sent to %d\n", rp->r_proc_nr_e); 
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
PRIVATE int start_service(rp, flags, endpoint)
struct rproc *rp;
int flags;
endpoint_t *endpoint;
{
/* Try to execute the given system service. Fork a new process. The child
 * process will be inhibited from running by the NO_PRIV flag. Only let the
 * child run once its privileges have been set by the parent.
 */
  int child_proc_nr_e, child_proc_nr_n;		/* child process slot */
  pid_t child_pid;				/* child's process id */
  char *file_only;
  int s, use_copy, slot_nr;
  struct priv *privp;
  message m;

  use_copy= (rp->r_exec != NULL);


  /* Now fork and branch for parent and child process (and check for error). */
  if (use_copy) {
  if(rs_verbose) printf("RS: fork_nb..\n");
	child_pid= fork_nb();
  } else {
  if(rs_verbose) printf("RS: fork regular..\n");
	child_pid = fork();
  }

  switch(child_pid) {					/* see fork(2) */
  case -1:						/* fork failed */
      report("RS", "warning, fork() failed", errno);	/* shouldn't happen */
      return(errno);					/* return error */

  case 0:						/* child process */
      /* Try to execute the binary that has an absolute path. If this fails, 
       * e.g., because the root file system cannot be read, try to strip of
       * the path, and see if the command is in RS' current working dir.
       */
      nice(rp->r_nice);		/* Nice before setuid, to allow negative
				 * nice values.
				 */
      setuid(rp->r_uid);
      cpf_reload();			/* Tell kernel about grant table  */
      if (!use_copy)
      {
	execve(rp->r_argv[0], rp->r_argv, NULL);	/* POSIX execute */
	file_only = strrchr(rp->r_argv[0], '/') + 1;
	execve(file_only, rp->r_argv, NULL);		/* POSIX execute */
      }
      printf("RS: exec failed for %s: %d\n", rp->r_argv[0], errno);
      slot_nr= rp-rproc;
      s= write(exec_pipe[1], &slot_nr, sizeof(slot_nr));
      if (s != sizeof(slot_nr))
	printf("RS: write to exec pipe failed: %d/%d\n", s, errno);
      exit(1);						/* terminate child */

  default:						/* parent process */
#if 0
  if(rs_verbose) printf("RS: parent forked, pid %d..\n", child_pid);
#endif
      child_proc_nr_e = getnprocnr(child_pid);		/* get child slot */ 
#if 0
  if(rs_verbose) printf("RS: forked into %d..\n", child_proc_nr_e);
#endif
      break;						/* continue below */
  }

  if (use_copy)
  {
	extern char **environ;
	dev_execve(child_proc_nr_e, rp->r_exec, rp->r_exec_len, rp->r_argv,
		environ);
  }

  privp= NULL;
  if (rp->r_set_resources)
  {
	init_privs(rp, &rp->r_priv);
	privp= &rp->r_priv;

	/* Inform the PCI server about the driver */
	init_pci(rp, child_proc_nr_e);
  }

  /* Set the privilege structure for the child process to let is run.
   * This should succeed: we tested number in use above.
   */
  if ((s = sys_privctl(child_proc_nr_e, SYS_PRIV_INIT, 0, privp)) < 0) {
      report("RS","sys_privctl call failed", s);	/* to let child run */
      rp->r_flags |= RS_EXITING;			/* expect exit */
      if(child_pid > 0) kill(child_pid, SIGKILL);	/* kill driver */
      else report("RS", "didn't kill pid", child_pid);
      return(s);					/* return error */
  }

  s= ds_publish_u32(rp->r_label, child_proc_nr_e);
  if (s != OK)
	printf("RS: start_service: ds_publish_u32 failed: %d\n", s);
 else if(rs_verbose)
	printf("RS: start_service: ds_publish_u32 done: %s -> %d\n", 
  		rp->r_label, child_proc_nr_e);

  if (rp->r_dev_nr > 0) {				/* set driver map */
      if ((s=mapdriver5(rp->r_label, strlen(rp->r_label),
	      rp->r_dev_nr, rp->r_dev_style, !!use_copy /* force */)) < 0) {
          report("RS", "couldn't map driver (continuing)", errno);
#if 0
          rp->r_flags |= RS_EXITING;			/* expect exit */
	  if(child_pid > 0) kill(child_pid, SIGKILL);	/* kill driver */
	  else report("RS", "didn't kill pid", child_pid);
	  return(s);					/* return error */
#endif
      }
  }

  if(rs_verbose)
      printf("RS: started '%s', major %d, pid %d, endpoint %d, proc %d\n", 
          rp->r_cmd, rp->r_dev_nr, child_pid,
	  child_proc_nr_e, child_proc_nr_n);

  /* The system service now has been successfully started. Update the rest
   * of the system process table that is maintain by the RS server. The only 
   * thing that can go wrong now, is that execution fails at the child. If 
   * that's the case, the child will exit. 
   */
  child_proc_nr_n = _ENDPOINT_P(child_proc_nr_e);
  rp->r_flags = RS_IN_USE | flags;		/* mark slot in use */
  rp->r_restarts += 1;				/* raise nr of restarts */
  rp->r_proc_nr_e = child_proc_nr_e;		/* set child details */
  rp->r_pid = child_pid;
  rp->r_check_tm = 0;				/* not check yet */
  getuptime(&rp->r_alive_tm); 			/* currently alive */
  rp->r_stop_tm = 0;				/* not exiting yet */
  rproc_ptr[child_proc_nr_n] = rp;		/* mapping for fast access */

  if(endpoint) *endpoint = child_proc_nr_e;	/* send back child endpoint */

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
  if(rs_verbose) printf("RS tries to stop %s (pid %d)\n", rp->r_cmd, rp->r_pid);

  rp->r_flags |= how;				/* what to on exit? */
  if(rp->r_pid > 0) kill(rp->r_pid, SIGTERM);	/* first try friendly */
  else if(rs_verbose) printf("RS: no process to kill\n");
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

PRIVATE pid_t fork_nb()
{
  message m;

  return(_syscall(PM_PROC_NR, FORK_NB, &m));
}

PRIVATE int read_exec(rp)
struct rproc *rp;
{
	int e, r, fd;
	char *e_name;
	struct stat sb;

	e_name= rp->r_argv[0];
	r= stat(e_name, &sb);
	if (r != 0)
		return -errno;

	fd= open(e_name, O_RDONLY);
	if (fd == -1)
		return -errno;

	rp->r_exec_len= sb.st_size;
	rp->r_exec= malloc(rp->r_exec_len);
	if (rp->r_exec == NULL)
	{
		printf("RS: read_exec: unable to allocate %d bytes\n",
			rp->r_exec_len);
		close(fd);
		return ENOMEM;
	}

	r= read(fd, rp->r_exec, rp->r_exec_len);
	e= errno;
	close(fd);
	if (r == rp->r_exec_len)
		return OK;

	printf("RS: read_exec: read failed %d, errno %d\n", r, e);

	free(rp->r_exec);
	rp->r_exec= NULL;

	if (r >= 0)
		return EIO;
	else
		return -e;
}

/*===========================================================================*
 *				run_script				     *
 *===========================================================================*/
PRIVATE void run_script(rp)
struct rproc *rp;
{
	int r, proc_nr_e;
	pid_t pid;
	char *reason;
	char incarnation_str[20];	/* Enough for a counter? */

	if (rp->r_flags & RS_EXITING)
		reason= "exit";
	else if (rp->r_flags & RS_REFRESHING)
		reason= "restart";
	else if (rp->r_flags & RS_NOPINGREPLY)
		reason= "no-heartbeat";
	else if (rp->r_flags & RS_KILLED)
		reason= "killed";
	else if (rp->r_flags & RS_CRASHED)
		reason= "crashed";
	else if (rp->r_flags & RS_SIGNALED)
		reason= "signaled";
	else
	{
		printf(
		"RS: run_script: can't find reason for termination of '%s'\n",
			rp->r_label);
		return;
	}
	sprintf(incarnation_str, "%d", rp->r_restarts);

 	if(rs_verbose) {
	  printf("RS: calling script '%s'\n", rp->r_script);
	  printf("RS: sevice name: '%s'\n", rp->r_label);
	  printf("RS: reason: '%s'\n", reason);
	  printf("RS: incarnation: '%s'\n", incarnation_str);
	}

	pid= fork();
	switch(pid)
	{
	case -1:	
		printf("RS: run_script: fork failed: %s\n", strerror(errno));
		break;
	case 0:
		execle(rp->r_script, rp->r_script, rp->r_label, reason,
			incarnation_str, NULL, NULL);
		{
			extern int kputc_use_private_grants;
			kputc_use_private_grants= 1;
		}
		printf("RS: run_script: execl '%s' failed: %s\n",
			rp->r_script, strerror(errno));
		exit(1);
	default:
		/* Set the privilege structure for the child process to let it
		 * run.
		 */
		proc_nr_e = getnprocnr(pid);
		r= sys_privctl(proc_nr_e, SYS_PRIV_USER, 0, NULL);
		if (r < 0)
			printf("RS: run_script: sys_privctl call failed: %d\n", r);

		/* Do not wait for the child */
		break;
	}
}


/*===========================================================================*
 *				init_privs				     *
 *===========================================================================*/
PRIVATE void init_privs(rp, privp)
struct rproc *rp;
struct priv *privp;
{
	int i, src_bits_per_word, dst_bits_per_word, src_word, dst_word,
		src_bit, call_nr, chunk, bit, priv_id, slot_nr;
	endpoint_t proc_nr_e;
	size_t len;
	unsigned long mask;
	char *p, *q;
	struct rproc *tmp_rp;
	char label[MAX_LABEL_LEN+1];

	/* Clear s_k_call_mask */
	memset(privp->s_k_call_mask, '\0', sizeof(privp->s_k_call_mask));

	src_bits_per_word= 8*sizeof(rp->r_call_mask[0]);
	dst_bits_per_word= 8*sizeof(privp->s_k_call_mask[0]);
	for (src_word= 0; src_word < MAX_NR_SYSTEM; src_word++)
	{
		for (src_bit= 0; src_bit < src_bits_per_word; src_bit++)
		{
			mask= (1UL << src_bit);
			if (!(rp->r_call_mask[src_word] & mask))
				continue;
			call_nr= src_word*src_bits_per_word+src_bit;
#if 0
			if(rs_verbose)
			  printf("RS: init_privs: system call %d\n", call_nr);
#endif
			dst_word= call_nr / dst_bits_per_word;
			mask= (1UL << (call_nr % dst_bits_per_word));
			if (dst_word >= CALL_MASK_SIZE)
			{
				printf(
				"RS: init_privs: call number %d doesn't fit\n",
					call_nr);
			}
			privp->s_k_call_mask[dst_word] |= mask;
		}
	}

	/* Clear s_ipc_to and s_ipc_sendrec */
	memset(&privp->s_ipc_to, '\0', sizeof(privp->s_ipc_to));
	memset(&privp->s_ipc_sendrec, '\0', sizeof(privp->s_ipc_sendrec));

	if (strlen(rp->r_ipc_list) != 0)
	{
		for (p= rp->r_ipc_list; p[0] != '\0'; p= q)
		{
			/* Skip leading space */
			while (p[0] != '\0' && isspace((unsigned char)p[0]))
				p++;

			/* Find start of next word */
			q= p;
			while (q[0] != '\0' && !isspace((unsigned char)q[0]))
				q++;
			if (q == p)
				continue;
			len= q-p;
			if (len+1 > sizeof(label))
			{
				printf(
		"rs:init_privs: bad ipc list entry '.*s' for %s: too long\n",
					len, p, rp->r_label);
				continue;
			}
			memcpy(label, p, len);
			label[len]= '\0';

			if (strcmp(label, "SYSTEM") == 0)
				proc_nr_e= SYSTEM;
			else if (strcmp(label, "PM") == 0)
				proc_nr_e= PM_PROC_NR;
			else if (strcmp(label, "VFS") == 0)
				proc_nr_e= FS_PROC_NR;
			else if (strcmp(label, "RS") == 0)
				proc_nr_e= RS_PROC_NR;
			else if (strcmp(label, "LOG") == 0)
				proc_nr_e= LOG_PROC_NR;
			else if (strcmp(label, "TTY") == 0)
				proc_nr_e= TTY_PROC_NR;
			else if (strcmp(label, "DS") == 0)
				proc_nr_e= DS_PROC_NR;
			else
			{
				/* Try to find process */
				for (slot_nr = 0; slot_nr < NR_SYS_PROCS;
					slot_nr++)
				{
					tmp_rp = &rproc[slot_nr];
					if (!(tmp_rp->r_flags & RS_IN_USE))
						continue;
					if (strcmp(tmp_rp->r_label, label) == 0)
						break;
				}
				if (slot_nr >= NR_SYS_PROCS)
				{
					printf(
					"init_privs: unable to find '%s'\n",
						label);
					continue;
				}
				proc_nr_e= tmp_rp->r_proc_nr_e;
			}

			priv_id= sys_getprivid(proc_nr_e);
			if (priv_id < 0)
			{
				printf(
			"init_privs: unable to get priv_id for '%s': %d\n",
					label, priv_id);
				continue;
			}
			chunk= (priv_id / (sizeof(bitchunk_t)*8));
			bit= (priv_id % (sizeof(bitchunk_t)*8));
			privp->s_ipc_to.chunk[chunk] |= (1 << bit);
			privp->s_ipc_sendrec.chunk[chunk] |= (1 << bit);
		}
	}
	else
	{
		for (i= 0; i<sizeof(privp->s_ipc_to)*8; i++)
		{
			chunk= (i / (sizeof(bitchunk_t)*8));
			bit= (i % (sizeof(bitchunk_t)*8));
			privp->s_ipc_to.chunk[chunk] |= (1 << bit);
		}
		for (i= 0; i<sizeof(privp->s_ipc_sendrec)*8; i++)
		{
			chunk= (i / (sizeof(bitchunk_t)*8));
			bit= (i % (sizeof(bitchunk_t)*8));
			privp->s_ipc_sendrec.chunk[chunk] |= (1 << bit);
		}
	}
}


/*===========================================================================*
 *				init_pci				     *
 *===========================================================================*/
PRIVATE void init_pci(rp, endpoint)
struct rproc *rp;
int endpoint;
{
	/* Tell the PCI driver about the new driver */
	size_t len;
	int i, r;
	struct rs_pci rs_pci;

	if (strcmp(rp->r_label, "pci") == 0)
	{
		if(rs_verbose)
			printf("RS: init_pci: not when starting 'pci'\n");
		return;
	}

	len= strlen(rp->r_label);
	if (len+1 > sizeof(rs_pci.rsp_label))
	{
		if(rs_verbose)
		  printf("RS: init_pci: label '%s' too long for rsp_label\n",
			rp->r_label);
		return;
	}
	strcpy(rs_pci.rsp_label, rp->r_label);
	rs_pci.rsp_endpoint= endpoint;

	rs_pci.rsp_nr_device= rp->r_nr_pci_id;
	if (rs_pci.rsp_nr_device > RSP_NR_DEVICE)
	{
		printf("RS: init_pci: too many PCI devices (max %d) "
		  "truncating\n",
			RSP_NR_DEVICE);
		rs_pci.rsp_nr_device= RSP_NR_DEVICE;
	}
	for (i= 0; i<rs_pci.rsp_nr_device; i++)
	{
		rs_pci.rsp_device[i].vid= rp->r_pci_id[i].vid;
		rs_pci.rsp_device[i].did= rp->r_pci_id[i].did;
	}

	rs_pci.rsp_nr_class= rp->r_nr_pci_class;
	if (rs_pci.rsp_nr_class > RSP_NR_CLASS)
	{
		printf("RS: init_pci: too many PCI classes "
		   "(max %d) truncating\n",
			RSP_NR_CLASS);
		rs_pci.rsp_nr_class= RSP_NR_CLASS;
	}
	for (i= 0; i<rs_pci.rsp_nr_class; i++)
	{
		rs_pci.rsp_class[i].class= rp->r_pci_class[i].class;
		rs_pci.rsp_class[i].mask= rp->r_pci_class[i].mask;
	}

	if(rs_verbose)
		printf("RS: init_pci: calling pci_set_acl\n");

	r= pci_set_acl(&rs_pci);

	if(rs_verbose)
		printf("RS: init_pci: after pci_set_acl\n");

	if (r != OK)
	{
		printf("RS: init_pci: pci_set_acl failed: %s\n",
			strerror(errno));
		return;
	}
}
