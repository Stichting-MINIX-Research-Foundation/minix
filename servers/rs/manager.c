/*
 * Changes:
 *   Nov 22, 2009:	added basic live update support  (Cristiano Giuffrida)
 *   Mar 02, 2009:	Extended isolation policies  (Jorrit N. Herder)
 *   Jul 22, 2005:	Created  (Jorrit N. Herder)
 */

#include "inc.h"
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/vm.h>
#include <minix/vm.h>
#include <lib.h>

#include <minix/sysutil.h>

/* Prototypes for internal functions that do the hard work. */
FORWARD _PROTOTYPE( int caller_is_root, (endpoint_t endpoint) );
FORWARD _PROTOTYPE( int caller_can_control, (endpoint_t endpoint,
	char *label) );
FORWARD _PROTOTYPE( int copy_label, (endpoint_t src_e,
	struct rss_label *src_label, char *dst_label, size_t dst_len) );
FORWARD _PROTOTYPE( int start_service, (struct rproc *rp, int flags,
	endpoint_t *ep) );
FORWARD _PROTOTYPE( int stop_service, (struct rproc *rp,int how) );
FORWARD _PROTOTYPE( int fork_nb, (void) );
FORWARD _PROTOTYPE( int read_exec, (struct rproc *rp) );
FORWARD _PROTOTYPE( int share_exec, (struct rproc *rp_src,
	struct rproc *rp_dst) );
FORWARD _PROTOTYPE( void free_slot, (struct rproc *rp) );
FORWARD _PROTOTYPE( void run_script, (struct rproc *rp) );
FORWARD _PROTOTYPE( char *get_next_label, (char *ptr, char *label,
	char *caller_label) );
FORWARD _PROTOTYPE( void add_forward_ipc, (struct rproc *rp,
	struct priv *privp) );
FORWARD _PROTOTYPE( void add_backward_ipc, (struct rproc *rp,
	struct priv *privp) );
FORWARD _PROTOTYPE( void init_privs, (struct rproc *rp, struct priv *privp) );
FORWARD _PROTOTYPE( void init_pci, (struct rproc *rp, int endpoint) );
FORWARD _PROTOTYPE( void update_period, (message *m_ptr) );
FORWARD _PROTOTYPE( void end_update, (clock_t now) );

PRIVATE int shutting_down = FALSE;

/*===========================================================================*
 *				caller_is_root				     *
 *===========================================================================*/
PRIVATE int caller_is_root(endpoint)
endpoint_t endpoint;				/* caller endpoint */
{
  uid_t euid;

  /* Check if caller has root user ID. */
  euid = getnuid(endpoint);
  if (rs_verbose && euid != 0)
  {
	printf("RS: got unauthorized request from endpoint %d\n", endpoint);
  }
  
  return euid == 0;
}

/*===========================================================================*
 *				caller_can_control			     *
 *===========================================================================*/
PRIVATE int caller_can_control(endpoint, label)
endpoint_t endpoint;
char *label;
{
  int control_allowed = 0;
  register struct rproc *rp;
  register struct rprocpub *rpub;
  int c;
  char *progname;

  /* Find name of binary for given label. */
  for (rp = BEG_RPROC_ADDR; rp < END_RPROC_ADDR; rp++) {
	rpub = rp->r_pub;
	if (strcmp(rpub->label, label) == 0) {
		break;
	}
  }
  if (rp == END_RPROC_ADDR) return 0;
  progname = strrchr(rp->r_argv[0], '/');
  if (progname != NULL)
	progname++;
  else
	progname = rp->r_argv[0];

  /* Check if label is listed in caller's isolation policy. */
  for (rp = BEG_RPROC_ADDR; rp < END_RPROC_ADDR; rp++) {
	rpub = rp->r_pub;
	if (rpub->endpoint == endpoint) {
		break;
	}
  }
  if (rp == END_RPROC_ADDR) return 0;
  if (rp->r_nr_control > 0) {
	for (c = 0; c < rp->r_nr_control; c++) {
		if (strcmp(rp->r_control[c], progname) == 0)
			control_allowed = 1;
	}
  }

  if (rs_verbose) {
	printf("RS: allowing %u control over %s via policy: %s\n",
		endpoint, label, control_allowed ? "yes" : "no");
  }
  return control_allowed;
}

/*===========================================================================*
 *				copy_label				     *
 *===========================================================================*/
PRIVATE int copy_label(src_e, src_label, dst_label, dst_len)
endpoint_t src_e;
struct rss_label *src_label;
char *dst_label;
size_t dst_len;
{
  int s, len;

  len = MIN(dst_len-1, src_label->l_len);

  s = sys_datacopy(src_e, (vir_bytes) src_label->l_addr,
	SELF, (vir_bytes) dst_label, len);
  if (s != OK) return s;

  dst_label[len] = 0;

  return OK;
}

/*===========================================================================*
 *				   do_up				     *
 *===========================================================================*/
PUBLIC int do_up(m_ptr)
message *m_ptr;					/* request message pointer */
{
/* A request was made to start a new system service. 
 */
  register struct rproc *rp;			/* system process table */
  register struct rprocpub *rpub;		/* public entry */
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
  struct rprocpub *tmp_rpub;
  struct rs_start rs_start;

  /* This call requires special privileges. */
  if (!caller_is_root(m_ptr->m_source)) return(EPERM);

  /* See if there is a free entry in the table with system processes. */
  for (slot_nr = 0; slot_nr < NR_SYS_PROCS; slot_nr++) {
      rp = &rproc[slot_nr];			/* get pointer to slot */
      if (!(rp->r_flags & RS_IN_USE)) 		/* check if available */
	  break;
  }
  if (slot_nr >= NR_SYS_PROCS)
  {
      printf("RS: do_up: system process table full\n");
	return ENOMEM;
  }
  rpub = rp->r_pub;

  /* Ok, there is space. Get the request structure. */
  s= sys_datacopy(m_ptr->m_source, (vir_bytes) m_ptr->RS_CMD_ADDR, 
  	SELF, (vir_bytes) &rs_start, sizeof(rs_start));
  if (s != OK) return(s);

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
  
  /* Process name for the service. */
  cmd_ptr = strrchr(rp->r_argv[0], '/');
  if (cmd_ptr)
  	cmd_ptr++;
  else
  	cmd_ptr= rp->r_argv[0];
  len= strlen(cmd_ptr);
  if (len > RS_MAX_LABEL_LEN-1)
  	len= RS_MAX_LABEL_LEN-1;	/* truncate name */
  memcpy(rpub->proc_name, cmd_ptr, len);
  rpub->proc_name[len]= '\0';
  if(rs_verbose)
      printf("RS: do_up: using proc_name (from binary %s) '%s'\n",
          rp->r_argv[0], rpub->proc_name);

  if(rs_start.rss_label.l_len > 0) {
	/* RS_UP caller has supplied a custom label for this service. */
	int s = copy_label(m_ptr->m_source, &rs_start.rss_label,
		rpub->label, sizeof(rpub->label));
	if(s != OK)
		return s;
        if(rs_verbose)
	  printf("RS: do_up: using label (custom) '%s'\n", rpub->label);
  } else {
	/* Default label for the service. */
	label = rpub->proc_name;
  	len= strlen(label);
  	memcpy(rpub->label, label, len);
  	rpub->label[len]= '\0';
        if(rs_verbose)
          printf("RS: do_up: using label (from proc_name) '%s'\n",
		rpub->label);
  }

  if(rs_start.rss_nr_control > 0) {
	int i, s;
	if (rs_start.rss_nr_control > RS_NR_CONTROL)
	{
		printf("RS: do_up: too many control labels\n");
		return EINVAL;
	}
	for (i=0; i<rs_start.rss_nr_control; i++) {
		s = copy_label(m_ptr->m_source, &rs_start.rss_control[i],
			rp->r_control[i], sizeof(rp->r_control[i]));
		if(s != OK)
			return s;
	}
	rp->r_nr_control = rs_start.rss_nr_control;

	if (rs_verbose) {
		printf("RS: do_up: control labels:");
		for (i=0; i<rp->r_nr_control; i++)
			printf(" %s", rp->r_control[i]);
		printf("\n");
	}
  }

  /* Check for duplicates */
  for (slot_nr = 0; slot_nr < NR_SYS_PROCS; slot_nr++) {
      tmp_rp = &rproc[slot_nr];			/* get pointer to slot */
      if (!(tmp_rp->r_flags & RS_IN_USE)) 	/* check if available */
	  continue;
      if (tmp_rp == rp)
	  continue;				/* Our slot */
      tmp_rpub = tmp_rp->r_pub;
      if (strcmp(tmp_rpub->label, rpub->label) == 0)
      {
	  printf("RS: found duplicate label '%s': slot %d\n",
		rpub->label, slot_nr);
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

  if (rs_start.rss_flags & RSS_IPC_VALID)
  {
	if (rs_start.rss_ipclen+1 > sizeof(rp->r_ipc_list))
	{
		printf("rs: ipc list too long for '%s'\n", rpub->label);
		return EINVAL;
	}
	s=sys_datacopy(m_ptr->m_source, (vir_bytes) rs_start.rss_ipc, 
		SELF, (vir_bytes) rp->r_ipc_list, rs_start.rss_ipclen);
	if (s != OK) return(s);
	rp->r_ipc_list[rs_start.rss_ipclen]= '\0';
  }
  else
	rp->r_ipc_list[0]= '\0';

  rpub->sys_flags = DSRV_SF;
  rp->r_exec= NULL;
  if (rs_start.rss_flags & RSS_COPY) {
	int exst_cpy;
	struct rproc *rp2;
	struct rprocpub *rpub2;
	exst_cpy = 0;
	
	if(rs_start.rss_flags & RSS_REUSE) {
                int i;

                for(i = 0; i < NR_SYS_PROCS; i++) {
                	rp2 = &rproc[i];
                	rpub2 = rproc[i].r_pub;
                        if(strcmp(rpub->proc_name, rpub2->proc_name) == 0 &&
                           (rpub2->sys_flags & SF_USE_COPY)) {
                                /* We have found the same binary that's
                                 * already been copied */
                                 exst_cpy = 1;
                                 break;
                        }
                }
         }                

	if(!exst_cpy)
		s = read_exec(rp);
	else
		s = share_exec(rp, rp2);

	if (s != OK)
		return s;

	rpub->sys_flags |= SF_USE_COPY;
  }

  /* All dynamically created services get the same privilege flags, and
   * allowed traps. Other privilege settings can be specified at runtime.
   * The privilege id is dynamically allocated by the kernel.
   */
  rp->r_priv.s_flags = DSRV_F;           /* privilege flags */
  rp->r_priv.s_trap_mask = DSRV_T;       /* allowed traps */

  /* Copy granted resources */
  if (rs_start.rss_nr_irq > NR_IRQ)
  {
	printf("RS: do_up: too many IRQs requested\n");
	return EINVAL;
  }
  rp->r_priv.s_nr_irq= rs_start.rss_nr_irq;
  for (i= 0; i<rp->r_priv.s_nr_irq; i++)
  {
	rp->r_priv.s_irq_tab[i]= rs_start.rss_irq[i];
	if(rs_verbose)
		printf("RS: do_up: IRQ %d\n", rp->r_priv.s_irq_tab[i]);
  }

  if (rs_start.rss_nr_io > NR_IO_RANGE)
  {
	printf("RS: do_up: too many I/O ranges requested\n");
	return EINVAL;
  }
  rp->r_priv.s_nr_io_range= rs_start.rss_nr_io;
  for (i= 0; i<rp->r_priv.s_nr_io_range; i++)
  {
	rp->r_priv.s_io_tab[i].ior_base= rs_start.rss_io[i].base;
	rp->r_priv.s_io_tab[i].ior_limit=
		rs_start.rss_io[i].base+rs_start.rss_io[i].len-1;
	if(rs_verbose)
	   printf("RS: do_up: I/O [%x..%x]\n",
		rp->r_priv.s_io_tab[i].ior_base,
		rp->r_priv.s_io_tab[i].ior_limit);
  }

  if (rs_start.rss_nr_pci_id > RS_NR_PCI_DEVICE)
  {
	printf("RS: do_up: too many PCI device IDs\n");
	return EINVAL;
  }
  rpub->pci_acl.rsp_nr_device = rs_start.rss_nr_pci_id;
  for (i= 0; i<rpub->pci_acl.rsp_nr_device; i++)
  {
	rpub->pci_acl.rsp_device[i].vid= rs_start.rss_pci_id[i].vid;
	rpub->pci_acl.rsp_device[i].did= rs_start.rss_pci_id[i].did;
	if(rs_verbose)
	   printf("RS: do_up: PCI %04x/%04x\n",
		rpub->pci_acl.rsp_device[i].vid,
		rpub->pci_acl.rsp_device[i].did);
  }
  if (rs_start.rss_nr_pci_class > RS_NR_PCI_CLASS)
  {
	printf("RS: do_up: too many PCI class IDs\n");
	return EINVAL;
  }
  rpub->pci_acl.rsp_nr_class= rs_start.rss_nr_pci_class;
  for (i= 0; i<rpub->pci_acl.rsp_nr_class; i++)
  {
	rpub->pci_acl.rsp_class[i].class= rs_start.rss_pci_class[i].class;
	rpub->pci_acl.rsp_class[i].mask= rs_start.rss_pci_class[i].mask;
	if(rs_verbose)
	    printf("RS: do_up: PCI class %06x mask %06x\n",
		rpub->pci_acl.rsp_class[i].class,
		rpub->pci_acl.rsp_class[i].mask);
  }

  /* Copy 'system' call number bits */
  if (sizeof(rs_start.rss_system[0]) == sizeof(rp->r_call_mask[0]) &&
	sizeof(rs_start.rss_system) == sizeof(rp->r_call_mask))
  {
	for (i= 0; i<RS_SYS_CALL_MASK_SIZE; i++)
		rp->r_call_mask[i]= rs_start.rss_system[i];
  }
  else
  {
	printf(
	"RS: do_up: internal inconsistency: bad size of r_call_mask\n");
	memset(rp->r_call_mask, '\0', sizeof(rp->r_call_mask));
  }

  /* Initialize some fields. */
  rpub->period = rs_start.rss_period;
  rpub->dev_nr = rs_start.rss_major;
  rpub->dev_style = STYLE_DEV; 
  rp->r_restarts = -1; 				/* will be incremented */
  rp->r_set_resources= 1;			/* set resources */

  if (sizeof(rpub->vm_call_mask) == sizeof(rs_start.rss_vm) &&
      sizeof(rpub->vm_call_mask[0]) == sizeof(rs_start.rss_vm[0]))
  {
          int basic_vmc[] =  { VM_BASIC_CALLS, SYS_NULL_C };
	  memcpy(rpub->vm_call_mask, rs_start.rss_vm,
	  	sizeof(rpub->vm_call_mask));
	  fill_call_mask(basic_vmc, NR_VM_CALLS,
	  	rpub->vm_call_mask, VM_RQ_BASE, FALSE);
  }
  else
  {
	  printf("RS: internal inconsistency: bad size of vm_call_mask\n");
	  memset(rpub->vm_call_mask, '\0', sizeof(rpub->vm_call_mask));
  }

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
  register struct rprocpub *rpub;
  size_t len;
  int s, proc;
  char label[RS_MAX_LABEL_LEN];

  /* This call requires special privileges. */
  if (!caller_is_root(m_ptr->m_source)) return(EPERM);

  len= m_ptr->RS_CMD_LEN;
  if (len >= sizeof(label))
	return EINVAL;		/* Too long */

  s= sys_datacopy(m_ptr->m_source, (vir_bytes) m_ptr->RS_CMD_ADDR, 
  	SELF, (vir_bytes) label, len);
  if (s != OK) return(s);
  label[len]= '\0';

  for (rp=BEG_RPROC_ADDR; rp<END_RPROC_ADDR; rp++) {
      rpub = rp->r_pub;
      if (rp->r_flags & RS_IN_USE && strcmp(rpub->label, label) == 0) {
	/* Core system services should never go down. */
	if (rpub->sys_flags & SF_CORE_SRV) return(EPERM);

	if(rs_verbose)
	  printf("RS: stopping '%s' (%d)\n", label, rp->r_pid);
	stop_service(rp,RS_EXITING);
	if (rp->r_pid == -1)
	{
		/* Process is already gone, release slot. */
		free_slot(rp);
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
  register struct rprocpub *rpub;
  size_t len;
  int s, proc, r;
  char label[RS_MAX_LABEL_LEN];
  endpoint_t ep;

  len= m_ptr->RS_CMD_LEN;
  if (len >= sizeof(label))
	return EINVAL;		/* Too long */

  s= sys_datacopy(m_ptr->m_source, (vir_bytes) m_ptr->RS_CMD_ADDR, 
  	SELF, (vir_bytes) label, len);
  if (s != OK) return(s);
  label[len]= '\0';

  /* This call requires special privileges. */
  if (! (caller_can_control(m_ptr->m_source, label) ||
		caller_is_root(m_ptr->m_source))) {
	return(EPERM);
  }

  for (rp=BEG_RPROC_ADDR; rp<END_RPROC_ADDR; rp++) {
      rpub = rp->r_pub;
      if ((rp->r_flags & RS_IN_USE) && strcmp(rpub->label, label) == 0) {
	  if(rs_verbose) printf("RS: restarting '%s' (%d)\n", label, rp->r_pid);
	  if (rp->r_pid >= 0)
	  {
		if(rs_verbose)
		  printf("RS: do_restart: '%s' is (still) running, pid = %d\n",
			rp->r_pid);
		return EBUSY;
	  }
	  rp->r_flags &= ~(RS_REFRESHING|RS_NOPINGREPLY);
	  r = start_service(rp, 0, &ep);	
	  if (r != OK) printf("do_restart: start_service failed: %d\n", r);
	  m_ptr->RS_ENDPOINT = ep;
	  return(r);
      }
  }
  if(rs_verbose) {
      printf("RS: do_restart: '%s' not found\n", label);
  }
  
  return(ESRCH);
}


/*===========================================================================*
 *				do_refresh				     *
 *===========================================================================*/
PUBLIC int do_refresh(message *m_ptr)
{
  register struct rproc *rp;
  register struct rprocpub *rpub;
  size_t len;
  int s;
  char label[RS_MAX_LABEL_LEN];

  len= m_ptr->RS_CMD_LEN;
  if (len >= sizeof(label))
	return EINVAL;		/* Too long */

  s= sys_datacopy(m_ptr->m_source, (vir_bytes) m_ptr->RS_CMD_ADDR, 
  	SELF, (vir_bytes) label, len);
  if (s != OK) return(s);
  label[len]= '\0';

  /* This call requires special privileges. */
  if (! (caller_can_control(m_ptr->m_source, label) ||
		caller_is_root(m_ptr->m_source))) {
	return(EPERM);
  }

  for (rp=BEG_RPROC_ADDR; rp<END_RPROC_ADDR; rp++) {
      rpub = rp->r_pub;
      if (rp->r_flags & RS_IN_USE && strcmp(rpub->label, label) == 0) {
          /* Only system processes not including RS can refresh. */
          if(!(rp->r_priv.s_flags & SYS_PROC) || rpub->endpoint == RS_PROC_NR) {
              return EPERM;
          }

          if(rs_verbose) {
              printf("RS: refreshing %s (%d)\n", rpub->label, rp->r_pid);
          }
	  stop_service(rp,RS_REFRESHING);
	  return(OK);
      }
  }
  if(rs_verbose) {
      printf("RS: do_refresh: '%s' not found\n", label);
  }
  
  return(ESRCH);
}

/*===========================================================================*
 *				do_shutdown				     *
 *===========================================================================*/
PUBLIC int do_shutdown(message *m_ptr)
{
  /* This call requires special privileges. */
  if (m_ptr != NULL && !caller_is_root(m_ptr->m_source)) return(EPERM);

  /* Set flag so that RS server knows services shouldn't be restarted. */
  shutting_down = TRUE;
  return(OK);
}


/*===========================================================================*
 *				do_init_ready				     *
 *===========================================================================*/
PUBLIC int do_init_ready(message *m_ptr)
{
  int who_p;
  struct rproc *rp;
  struct rprocpub *rpub;
  int result;

  who_p = _ENDPOINT_P(m_ptr->m_source);
  rp = rproc_ptr[who_p];
  rpub = rp->r_pub;
  result = m_ptr->RS_INIT_RESULT;

  /* Make sure the originating service was requested to initialize. */
  if(! (rp->r_flags & RS_INITIALIZING) ) {
      if(rs_verbose) {
          printf("RS: do_init_ready: got unexpected init ready msg from %d\n",
              m_ptr->m_source);
      }
      return(EINVAL);
  }

  /* Mark the slot as no longer initializing. */
  rp->r_flags &= ~RS_INITIALIZING;
  rp->r_check_tm = 0;
  getuptime(&rp->r_alive_tm);

  /* Check if something went wrong and the service failed to init.
   * In that case, kill it and make sure it won't be restarted.
   */
  if(result != OK) {
      if(rs_verbose)
          printf("RS: initialization failed for service %d: %d\n",
              rpub->endpoint, result);
      rp->r_flags |= RS_EXITING;
      kill(rp->r_pid, SIGKILL);
  }
  else {
      if(rs_verbose)
          printf("RS: initialization succeeded for service %d\n",
              rpub->endpoint);
  }

  return(EDONTREPLY);
}

/*===========================================================================*
 *				do_update				     *
 *===========================================================================*/
PUBLIC int do_update(message *m_ptr)
{
  register struct rproc *rp;
  register struct rprocpub *rpub;
  size_t len;
  int s;
  char label[RS_MAX_LABEL_LEN];
  int lu_state;
  int prepare_maxtime;

  /* Retrieve label. */
  len= m_ptr->RS_CMD_LEN;
  if (len >= sizeof(label))
      return EINVAL;		/* Too long */
  s= sys_datacopy(m_ptr->m_source, (vir_bytes) m_ptr->RS_CMD_ADDR, 
      SELF, (vir_bytes) label, len);
  if (s != OK) return(s);
  label[len]= '\0';

  /* This call requires special privileges. */
  if (! (caller_can_control(m_ptr->m_source, label) ||
      caller_is_root(m_ptr->m_source))) {
      return(EPERM);
  }

  /* Retrieve live update state. */
  lu_state = m_ptr->RS_LU_STATE;
  if(lu_state == SEF_LU_STATE_NULL) {
      return(EINVAL);
  }

  /* Retrieve prepare max time. */
  prepare_maxtime = m_ptr->RS_LU_PREPARE_MAXTIME;
  if(prepare_maxtime) {
      if(prepare_maxtime < 0 || prepare_maxtime > RS_MAX_PREPARE_MAXTIME) {
          return(EINVAL);
      }
  }
  else {
      prepare_maxtime = RS_DEFAULT_PREPARE_MAXTIME;
  }

  /* Make sure we are not already updating. */
  if(rupdate.flags & RS_UPDATING) {
      if(rs_verbose) {
	  printf("RS: do_update: an update is already in progress");
      }
      return(EBUSY);
  }

  /* Try to start the update process. */
  for (rp=BEG_RPROC_ADDR; rp<END_RPROC_ADDR; rp++) {
      rpub = rp->r_pub;
      if (rp->r_flags & RS_IN_USE && strcmp(rpub->label, label) == 0) {
          /* Only system processes not including RS can update. */
          if(!(rp->r_priv.s_flags & SYS_PROC) || rpub->endpoint == RS_PROC_NR) {
              return EPERM;
          }

          if(rs_verbose) {
	      printf("RS: updating %s (%d)\n", rpub->label, rp->r_pid);
	  }

	  rp->r_flags |= RS_UPDATING;
	  rupdate.flags |= RS_UPDATING;
	  getuptime(&rupdate.prepare_tm);
          rupdate.prepare_maxtime = prepare_maxtime;
	  rupdate.rp = rp;

	  m_ptr->m_type = RS_LU_PREPARE;
	  asynsend(rpub->endpoint, m_ptr);  /* request to update */

	  return(OK);
      }
  }
  if(rs_verbose) {
      printf("RS: do_update: '%s' not found\n", label);
  }

  return(ESRCH);
}

/*===========================================================================*
 *				do_upd_ready				     *
 *===========================================================================*/
PUBLIC int do_upd_ready(message *m_ptr)
{
  register struct rproc *rp;
  int who_p;
  clock_t now = m_ptr->NOTIFY_TIMESTAMP;
  int result;

  who_p = _ENDPOINT_P(m_ptr->m_source);
  rp = rproc_ptr[who_p];
  result = m_ptr->RS_LU_RESULT;

  /* Make sure the originating service was requested to prepare for update. */
  if(! (rp->r_flags & RS_UPDATING) ) {
      if(rs_verbose) {
          printf("RS: do_upd_ready: got unexpected update ready msg from %d\n",
              m_ptr->m_source);
      }
      return(EINVAL);
  }

  /* Check if something went wrong and the service failed to prepare
   * for the update. In that case, end the update process.
   */
  if(result != OK) {
      end_update(now);
      switch(result) {
          case EACCES:
              printf("RS: update failed: %s\n",
                  "service does not support live update");
          break;

          case EINVAL:
              printf("RS: update failed: %s\n",
                  "service does not support the required state");
          break;

          case EBUSY:
              printf("RS: update failed: %s\n",
                  "service is not able to prepare for the update now");
          break;

          case EGENERIC:
              printf("RS: update failed: %s\n",
                  "a generic error occurred while preparing for the update");
          break;

          default:
              printf("RS: update failed: %s (%d)\n",
                  "an unknown error occurred while preparing for the update\n",
                  result);
          break;
      }

      return(OK);
  }

  /* Kill the process now and mark it for refresh, the new version will
   * be automatically restarted.
   */
  rp->r_flags &= ~RS_EXITING;
  rp->r_flags |= RS_REFRESHING;
  kill(rp->r_pid, SIGKILL);

  return(EDONTREPLY);
}

/*===========================================================================*
 *				update_period				     *
 *===========================================================================*/
PRIVATE void update_period(message *m_ptr)
{
  clock_t now = m_ptr->NOTIFY_TIMESTAMP;
  short has_update_timed_out;
  message m;
  struct rprocpub *rpub;

  rpub = rupdate.rp->r_pub;

  /* See if a timeout has occurred. */
  has_update_timed_out = (now - rupdate.prepare_tm > rupdate.prepare_maxtime);

  /* If an update timed out, end the update process and notify the service. */
  if(has_update_timed_out) {
      end_update(now);
      printf("RS: update failed: maximum prepare time reached\n");

      /* Prepare cancel request. */
      m.m_type = RS_LU_PREPARE;
      m.RS_LU_STATE = SEF_LU_STATE_NULL;
      asynsend(rpub->endpoint, &m);
  }
}

/*===========================================================================*
 *				end_update				     *
 *===========================================================================*/
PRIVATE void end_update(clock_t now)
{
  /* End the update process and mark the affected service as no longer under
   * update. Eventual late ready to update message (if any) will simply be
   * ignored and the service can continue executing.
   * We reset the check timestamp, so that if the service has a period a status
   * request will be forced in the next period.
   */
  rupdate.flags &= ~RS_UPDATING;
  rupdate.rp->r_flags &= ~RS_UPDATING;
  rupdate.rp->r_check_tm = 0;
}

/*===========================================================================*
 *				do_exit					     *
 *===========================================================================*/
PUBLIC void do_exit(message *m_ptr)
{
  register struct rproc *rp;
  register struct rprocpub *rpub;
  pid_t exit_pid;
  int exit_status, r, slot_nr;
  endpoint_t ep;
  clock_t now = m_ptr->NOTIFY_TIMESTAMP;

  if(rs_verbose)
     printf("RS: got SIGCHLD signal, doing wait to get exited child.\n");

  /* See which child exited and what the exit status is. This is done in a
   * loop because multiple children may have exited, all reported by one 
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
			break;	/* No data */
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
		rp->r_flags |= RS_EXITING;
	}

      /* Search the system process table to see who exited. 
       * This should always succeed. 
       */
      for (rp=BEG_RPROC_ADDR; rp<END_RPROC_ADDR; rp++) {
          rpub = rp->r_pub;
          if ((rp->r_flags & RS_IN_USE) && rp->r_pid == exit_pid) {
	      int proc;
	      proc = _ENDPOINT_P(rpub->endpoint);

              rproc_ptr[proc] = NULL;		/* invalidate */
	      rp->r_pid= -1;

	      /* If PCI properties are set, inform the PCI driver. */
              if(rpub->pci_acl.rsp_nr_device || rpub->pci_acl.rsp_nr_class) {
                  pci_del_acl(rpub->endpoint);
              }

              if ((rp->r_flags & RS_EXITING) || shutting_down) {
		  /* No reply sent to RS_DOWN yet. */
		  if(rp->r_flags & RS_LATEREPLY) {
			message rsm;
			rsm.m_type = OK;
			send(rp->r_caller, &rsm);
		  }

		  /* Release slot. */
		  free_slot(rp);
	      }
	      else if(rp->r_flags & RS_REFRESHING) {
		      short is_updating = rp->r_flags & RS_UPDATING;

		      /* Refresh */
		      if (rp->r_script[0] != '\0')
			run_script(rp);
		      else {
		        start_service(rp, 0, &ep); /* direct restart */
			if(m_ptr)
		  		m_ptr->RS_ENDPOINT = ep;
		      }

		      /* If updating, end the update process. */
		      if(is_updating) {
                          end_update(now);
                          printf("RS: update succeeded\n");
		      }
	      }
	      else {
                  /* Determine what to do. If this is the first unexpected 
		   * exit, immediately restart this service. Otherwise use
		   * a binary exponential backoff.
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

		  if (rp->r_script[0] != '\0') {
			if(rs_verbose)
				printf("RS: running restart script for %s\n",
					rp->r_cmd);
		      run_script(rp);
		  } else if (rp->r_restarts > 0) {
		      printf("RS: restarting %s, restarts %d\n",
				rp->r_cmd, rp->r_backoff);
		      rp->r_backoff = 1 << MIN(rp->r_restarts,(BACKOFF_BITS-2));
		      rp->r_backoff = MIN(rp->r_backoff,MAX_BACKOFF); 
		      if ((rpub->sys_flags & SF_USE_COPY) && rp->r_backoff > 1)
			rp->r_backoff= 1;
		  }
		  else {
		      printf("RS: restarting %s\n", rp->r_cmd);
		      start_service(rp, 0, &ep);	/* direct restart */
		      if(m_ptr)
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
  register struct rprocpub *rpub;
  clock_t now = m_ptr->NOTIFY_TIMESTAMP;
  int s;
  endpoint_t ep;
  long period;

  /* If an update is in progress, check its status. */
  if(rupdate.flags & RS_UPDATING) {
      update_period(m_ptr);
  }

  /* Search system services table. Only check slots that are in use and not
   * updating.
   */
  for (rp=BEG_RPROC_ADDR; rp<END_RPROC_ADDR; rp++) {
      rpub = rp->r_pub;
      if ((rp->r_flags & RS_IN_USE) && !(rp->r_flags & RS_UPDATING)) {

          /* Compute period. */
          period = rpub->period;
          if(rp->r_flags & RS_INITIALIZING) {
              period = RS_INIT_T;
          }

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
	  else if (period > 0) {

	      /* Check if an answer to a status request is still pending. If 
	       * the service didn't respond within time, kill it to simulate 
	       * a crash. The failure will be detected and the service will 
	       * be restarted automatically.
	       */
              if (rp->r_alive_tm < rp->r_check_tm) { 
	          if (now - rp->r_alive_tm > 2*period &&
		      rp->r_pid > 0 && !(rp->r_flags & RS_NOPINGREPLY)) { 
		      if(rs_verbose)
                           printf("RS: service %d reported late\n",
				rpub->endpoint); 
		      rp->r_flags |= RS_NOPINGREPLY;
		      if(rp->r_flags & RS_INITIALIZING) {
                           rp->r_flags |= RS_EXITING;   /* don't restart */
		      }
                      kill(rp->r_pid, SIGKILL);		/* simulate crash */
		  }
	      }

	      /* No answer pending. Check if a period expired since the last
	       * check and, if so request the system service's status.
	       */
	      else if (now - rp->r_check_tm > rpub->period) {
#if 0
		if(rs_verbose)
                  printf("RS: status request sent to %d\n", rpub->endpoint);
#endif
		  notify(rpub->endpoint);		/* request status */
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
  int s, use_copy, slot_nr, init_type;
  bitchunk_t *vm_mask;
  message m;
  char * null_env = NULL;
  struct rprocpub *rpub;

  rpub = rp->r_pub;
  use_copy= (rpub->sys_flags & SF_USE_COPY);

  /* See if we are not using a copy but we do need one to start the service. */
  if(!use_copy && (rpub->sys_flags & SF_NEED_COPY)) {
	printf("RS: unable to start service %s without an in-memory copy\n",
	    rpub->label);
	free_slot(rp);
	return(EPERM);
  }

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
       * e.g., because the root file system cannot be read, try to strip off
       * the path, and see if the command is in RS' current working dir.
       */
      nice(rp->r_nice);		/* Nice before setuid, to allow negative
				 * nice values.
				 */
      setuid(rp->r_uid);
      cpf_reload();			/* Tell kernel about grant table  */
      if (!use_copy)
      {
	execve(rp->r_argv[0], rp->r_argv, &null_env);	/* POSIX execute */
	file_only = strrchr(rp->r_argv[0], '/') + 1;
	execve(file_only, rp->r_argv, &null_env);	/* POSIX execute */
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

  /* Regardless of any following failures, there is now a child process.
   * Update the system process table that is maintained by the RS server.
   */
  child_proc_nr_n = _ENDPOINT_P(child_proc_nr_e);
  rp->r_flags = RS_IN_USE | flags;		/* mark slot in use */
  rp->r_restarts += 1;				/* raise nr of restarts */
  rpub->endpoint = child_proc_nr_e;		/* set child details */
  rp->r_pid = child_pid;
  rp->r_check_tm = 0;				/* not checked yet */
  getuptime(&rp->r_alive_tm); 			/* currently alive */
  rp->r_stop_tm = 0;				/* not exiting yet */
  rp->r_backoff = 0;				/* not to be restarted */
  rproc_ptr[child_proc_nr_n] = rp;		/* mapping for fast access */
  rpub->in_use = TRUE;				/* public entry is now in use */

  /* If any of the calls below fail, the RS_EXITING flag is set. This implies
   * that the process will be removed from RS's process table once it has
   * terminated. The assumption is that it is not useful to try to restart the
   * process later in these failure cases.
   */

  if (use_copy)
  {
	extern char **environ;

	/* Copy the executable image into the child process. If this call
	 * fails, the child process may or may not be killed already. If it is
	 * not killed, it's blocked because of NO_PRIV. Kill it now either way.
	 */
	s = dev_execve(child_proc_nr_e, rp->r_exec, rp->r_exec_len, rp->r_argv,
		environ);
	if (s != OK) {
		report("RS", "dev_execve call failed", s);
		kill(child_pid, SIGKILL);
		rp->r_flags |= RS_EXITING;	/* don't try again */
		return(s);
	}
  }

  /* Set resources when asked to. */
  if (rp->r_set_resources)
  {
	/* Initialize privilege structure. */
	init_privs(rp, &rp->r_priv);

	/* Tell VM about allowed calls. */
	vm_mask = &rpub->vm_call_mask[0];
	if ((s = vm_set_priv(child_proc_nr_e, vm_mask)) < 0) {
	    report("RS", "vm_set_priv call failed", s);
	    kill(child_pid, SIGKILL);
	    rp->r_flags |= RS_EXITING;
	    return (s);
	}
  }

  /* Set and synch the privilege structure for the new service. */
  if ((s = sys_privctl(child_proc_nr_e, SYS_PRIV_SET_SYS, &rp->r_priv)) != OK
      || (s = sys_getpriv(&rp->r_priv, child_proc_nr_e)) != OK) {
      report("RS","unable to set privileges", s);
      kill(child_pid, SIGKILL);				/* kill the service */
      rp->r_flags |= RS_EXITING;			/* expect exit */
      return(s);					/* return error */
  }

  /* If PCI properties are set, inform the PCI driver about the new service. */
  if(rpub->pci_acl.rsp_nr_device || rpub->pci_acl.rsp_nr_class) {
      init_pci(rp, child_proc_nr_e);
  }

  /* Publish the new system service. */
  s = publish_service(rp);
  if (s != OK) {
	printf("RS: warning: publish_service failed: %d\n", s);
  }

  /* Allow the service to run.
   * XXX: We should let the service run/init only after publishing information
   * about the new system service, but this is not currently possible due to
   * the blocking nature of mapdriver() that expects the service to be running.
   * The current solution is not race-free. This hack can go once service
   * publishing is made fully asynchronous in RS.
   */
  if ((s = sys_privctl(child_proc_nr_e, SYS_PRIV_ALLOW, NULL)) != OK) {
      report("RS","unable to allow the service to run", s);
      kill(child_pid, SIGKILL);				/* kill the service */
      rp->r_flags |= RS_EXITING;			/* expect exit */
      return(s);					/* return error */
  }

  /* Initialize service. */
  init_type = rp->r_restarts > 0 ? SEF_INIT_RESTART : SEF_INIT_FRESH;
  if((s = init_service(rp, init_type)) != OK) {
      panic("RS", "unable to initialize service", s);
  }

  /* The purpose of non-blocking forks is to avoid involving VFS in the forking
   * process, because VFS may be blocked on a sendrec() to a MFS that is
   * waiting for a endpoint update for a dead driver. We have just published
   * that update, but VFS may still be blocked. As a result, VFS may not yet
   * have received PM's fork message. Hence, if we call mapdriver()
   * immediately, VFS may not know about the process and thus refuse to add the
   * driver entry. The following temporary hack works around this by forcing
   * blocking communication from PM to VFS. Once VFS has been made non-blocking
   * towards MFS instances, this hack and the entire fork_nb() call can go.
   */
  if (use_copy)
	setuid(0);

  /* Map the new service. */
  if (rpub->dev_nr > 0) {				/* set driver map */
      if ((s=mapdriver(rpub->label,
	      rpub->dev_nr, rpub->dev_style, !!use_copy /* force */)) < 0) {
          report("RS", "couldn't map driver (continuing)", errno);
      }
  }

  if(rs_verbose)
      printf("RS: started '%s', major %d, pid %d, endpoint %d, proc %d\n", 
          rp->r_cmd, rpub->dev_nr, child_pid,
	  child_proc_nr_e, child_proc_nr_n);

  /* The system service now has been successfully started. The only thing
   * that can go wrong now, is that execution fails at the child. If that's
   * the case, the child will exit. 
   */
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

  /* This call requires special privileges. */
  if (!caller_is_root(m_ptr->m_source)) return(EPERM);

  switch(m_ptr->m1_i1) {
  case SI_PROC_TAB:
  	src_addr = (vir_bytes) rproc;
  	len = sizeof(struct rproc) * NR_SYS_PROCS;
  	break; 
  case SI_PROCPUB_TAB:
  	src_addr = (vir_bytes) rprocpub;
  	len = sizeof(struct rprocpub) * NR_SYS_PROCS;
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

/*===========================================================================*
 *				 fork_nb				     *
 *===========================================================================*/
PRIVATE pid_t fork_nb()
{
  message m;

  return(_syscall(PM_PROC_NR, FORK_NB, &m));
}

/*===========================================================================*
 *				share_exec				     *
 *===========================================================================*/
PRIVATE int share_exec(rp_dst, rp_src)
struct rproc *rp_dst, *rp_src;
{
  struct rprocpub *rpub_src;
  struct rprocpub *rpub_dst;

  rpub_src = rp_src->r_pub;
  rpub_dst = rp_dst->r_pub;

  if(rs_verbose) {
      printf("RS: share_exec: sharing exec image from %s to %s\n",
          rpub_src->label, rpub_dst->label);
  }

  /* Share exec image from rp_src to rp_dst. */
  rp_dst->r_exec_len = rp_src->r_exec_len;
  rp_dst->r_exec = rp_src->r_exec;

  return OK;
}

/*===========================================================================*
 *				read_exec				     *
 *===========================================================================*/
PRIVATE int read_exec(rp)
struct rproc *rp;
{
  int e, r, fd;
  char *e_name;
  struct stat sb;

  e_name= rp->r_argv[0];
  if(rs_verbose) {
      printf("RS: read_exec: copying exec image from: %s\n", e_name);
  }

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
 *				free_slot				     *
 *===========================================================================*/
PRIVATE void free_slot(rp)
struct rproc *rp;
{
  int slot_nr, has_shared_exec;
  struct rprocpub *rpub;
  struct rproc *other_rp;

  rpub = rp->r_pub;

  /* Free memory if necessary. */
  if(rpub->sys_flags & SF_USE_COPY) {
      /* Search for some other slot sharing the same exec image. */
      has_shared_exec = FALSE;
      for (slot_nr = 0; slot_nr < NR_SYS_PROCS; slot_nr++) {
          other_rp = &rproc[slot_nr];		/* get pointer to slot */
          if (other_rp->r_flags & RS_IN_USE && other_rp != rp
              && other_rp->r_exec == rp->r_exec) {  /* found! */
              has_shared_exec = TRUE;
          }
      }

      /* If nobody uses our copy of the exec image, we can get rid of it. */
      if(!has_shared_exec) {
          if(rs_verbose) {
              printf("RS: free_slot: free exec image from %s\n",
                  rpub->label);
          }
          free(rp->r_exec);
          rp->r_exec = NULL;
          rp->r_exec_len = 0;
      }
  }

  /* Mark slot as no longer in use.. */
  rp->r_flags = 0;
  rpub->in_use = FALSE;
  rproc_ptr[_ENDPOINT_P(rpub->endpoint)] = NULL;
}

/*===========================================================================*
 *				run_script				     *
 *===========================================================================*/
PRIVATE void run_script(rp)
struct rproc *rp;
{
	int r, endpoint;
	pid_t pid;
	char *reason;
	char incarnation_str[20];	/* Enough for a counter? */
	char *envp[1] = { NULL };
	struct rprocpub *rpub;

	rpub = rp->r_pub;
	if (rp->r_flags & RS_REFRESHING)
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
			rpub->label);
		return;
	}
	sprintf(incarnation_str, "%d", rp->r_restarts);

 	if(rs_verbose) {
	  printf("RS: calling script '%s'\n", rp->r_script);
	  printf("RS: sevice name: '%s'\n", rpub->label);
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
		execle(rp->r_script, rp->r_script, rpub->label, reason,
			incarnation_str, NULL, envp);
		printf("RS: run_script: execl '%s' failed: %s\n",
			rp->r_script, strerror(errno));
		exit(1);
	default:
		/* Set the privilege structure for the child process. */
		endpoint = getnprocnr(pid);
		if ((r = sys_privctl(endpoint, SYS_PRIV_SET_USER, NULL))
			!= OK) {
			printf("RS: run_script: can't set privileges: %d\n",r);
		}
		/* Allow the service to run. */
		if ((r = sys_privctl(endpoint, SYS_PRIV_ALLOW, NULL)) != OK) {
			printf("RS: run_script: process can't run: %d\n",r);
		}
		/* Do not wait for the child */
		break;
	}
}


/*===========================================================================*
 *				get_next_label				     *
 *===========================================================================*/
PRIVATE char *get_next_label(ptr, label, caller_label)
char *ptr;
char *label;
char *caller_label;
{
	/* Get the next label from the list of (IPC) labels.
	 */
	char *p, *q;
	size_t len;

	for (p= ptr; p[0] != '\0'; p= q)
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
		if (len > RS_MAX_LABEL_LEN)
		{
			printf(
	"rs:get_next_label: bad ipc list entry '.*s' for %s: too long\n",
				len, p, caller_label);
			continue;
		}
		memcpy(label, p, len);
		label[len]= '\0';

		return q; /* found another */
	}

	return NULL; /* done */
}

/*===========================================================================*
 *				add_forward_ipc				     *
 *===========================================================================*/
PRIVATE void add_forward_ipc(rp, privp)
struct rproc *rp;
struct priv *privp;
{
	/* Add IPC send permissions to a process based on that process's IPC
	 * list.
	 */
	char label[RS_MAX_LABEL_LEN+1], *p;
	struct rproc *tmp_rp;
	struct rprocpub *tmp_rpub;
	endpoint_t endpoint;
	int r;
	int slot_nr, priv_id;
	struct priv priv;
	struct rprocpub *rpub;

	rpub = rp->r_pub;
	p = rp->r_ipc_list;

	while ((p = get_next_label(p, label, rpub->label)) != NULL) {

		if (strcmp(label, "SYSTEM") == 0)
			endpoint= SYSTEM;
		else if (strcmp(label, "USER") == 0)
			endpoint= INIT_PROC_NR; /* all user procs */
		else if (strcmp(label, "PM") == 0)
			endpoint= PM_PROC_NR;
		else if (strcmp(label, "VFS") == 0)
			endpoint= FS_PROC_NR;
		else if (strcmp(label, "RS") == 0)
			endpoint= RS_PROC_NR;
		else if (strcmp(label, "LOG") == 0)
			endpoint= LOG_PROC_NR;
		else if (strcmp(label, "TTY") == 0)
			endpoint= TTY_PROC_NR;
		else if (strcmp(label, "DS") == 0)
			endpoint= DS_PROC_NR;
		else if (strcmp(label, "VM") == 0)
			endpoint= VM_PROC_NR;
		else
		{
			/* Try to find process */
			for (slot_nr = 0; slot_nr < NR_SYS_PROCS;
				slot_nr++)
			{
				tmp_rp = &rproc[slot_nr];
				if (!(tmp_rp->r_flags & RS_IN_USE))
					continue;
				tmp_rpub = tmp_rp->r_pub;
				if (strcmp(tmp_rpub->label, label) == 0)
					break;
			}
			if (slot_nr >= NR_SYS_PROCS)
			{
				if (rs_verbose)
					printf(
			"add_forward_ipc: unable to find '%s'\n", label);
				continue;
			}
			endpoint= tmp_rpub->endpoint;
		}

		if ((r = sys_getpriv(&priv, endpoint)) < 0)
		{
			printf(
		"add_forward_ipc: unable to get priv_id for '%s': %d\n",
				label, r);
			continue;
		}
		priv_id= priv.s_id;
		set_sys_bit(privp->s_ipc_to, priv_id);
	}
}


/*===========================================================================*
 *				add_backward_ipc			     *
 *===========================================================================*/
PRIVATE void add_backward_ipc(rp, privp)
struct rproc *rp;
struct priv *privp;
{
	/* Add IPC send permissions to a process based on other processes' IPC
	 * lists. This is enough to allow each such two processes to talk to
	 * each other, as the kernel guarantees send mask symmetry. We need to
	 * add these permissions now because the current process may not yet
	 * have existed at the time that the other process was initialized.
	 */
	char label[RS_MAX_LABEL_LEN+1], *p;
	struct rproc *rrp;
	struct rprocpub *rrpub;
	int priv_id, found;

	for (rrp=BEG_RPROC_ADDR; rrp<END_RPROC_ADDR; rrp++) {
		if (!(rrp->r_flags & RS_IN_USE))
			continue;

		/* If an IPC target list was provided for the process being
		 * checked here, make sure that the label of the new process
		 * is in that process's list.
		 */
		if (rrp->r_ipc_list[0]) {
			found = 0;

			rrpub = rrp->r_pub;
			p = rrp->r_ipc_list;

			while ((p = get_next_label(p, label,
				rrpub->label)) != NULL) {
				if (!strcmp(rrpub->label, label)) {
					found = 1;
					break;
				}
			}

			if (!found)
				continue;
		}

		priv_id= rrp->r_priv.s_id;

		set_sys_bit(privp->s_ipc_to, priv_id);
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
		src_bit, call_nr;
	unsigned long mask;

	/* Clear s_k_call_mask */
	memset(privp->s_k_call_mask, '\0', sizeof(privp->s_k_call_mask));

	src_bits_per_word= 8*sizeof(rp->r_call_mask[0]);
	dst_bits_per_word= 8*sizeof(privp->s_k_call_mask[0]);
	for (src_word= 0; src_word < RS_SYS_CALL_MASK_SIZE; src_word++)
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
			if (dst_word >= SYS_CALL_MASK_SIZE)
			{
				printf(
				"RS: init_privs: call number %d doesn't fit\n",
					call_nr);
			}
			privp->s_k_call_mask[dst_word] |= mask;
		}
	}

	/* Clear s_ipc_to */
	memset(&privp->s_ipc_to, '\0', sizeof(privp->s_ipc_to));

	if (strlen(rp->r_ipc_list) != 0)
	{
		add_forward_ipc(rp, privp);
		add_backward_ipc(rp, privp);

	}
	else
	{
		for (i= 0; i<NR_SYS_PROCS; i++)
		{
			if (i != USER_PRIV_ID)
				set_sys_bit(privp->s_ipc_to, i);
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
	/* Inform the PCI driver about the new service. */
	size_t len;
	int i, r;
	struct rs_pci rs_pci;
	struct rprocpub *rpub;

	rpub = rp->r_pub;
	rs_pci = rpub->pci_acl;
	strcpy(rs_pci.rsp_label, rpub->label);
	rs_pci.rsp_endpoint= endpoint;

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

/*===========================================================================*
 *				do_lookup				     *
 *===========================================================================*/
PUBLIC int do_lookup(m_ptr)
message *m_ptr;
{
	static char namebuf[100];
	int len, r;
	struct rproc *rrp;
	struct rprocpub *rrpub;

	len = m_ptr->RS_NAME_LEN;

	if(len < 2 || len >= sizeof(namebuf)) {
		printf("RS: len too weird (%d)\n", len);
		return EINVAL;
	}

	if((r=sys_vircopy(m_ptr->m_source, D, (vir_bytes) m_ptr->RS_NAME,
		SELF, D, (vir_bytes) namebuf, len)) != OK) {
		printf("RS: name copy failed\n");
		return r;

	}

	namebuf[len] = '\0';

	for (rrp=BEG_RPROC_ADDR; rrp<END_RPROC_ADDR; rrp++) {
		if (!(rrp->r_flags & RS_IN_USE))
			continue;
		rrpub = rrp->r_pub;
		if (!strcmp(rrpub->label, namebuf)) {
			m_ptr->RS_ENDPOINT = rrpub->endpoint;
			return OK;
		}
	}

	return ESRCH;
}

