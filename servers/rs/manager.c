/*
 * Changes:
 *   Nov 22, 2009:	added basic live update support  (Cristiano Giuffrida)
 *   Mar 02, 2009:	Extended isolation policies  (Jorrit N. Herder)
 *   Jul 22, 2005:	Created  (Jorrit N. Herder)
 */

#include <paths.h>

#include "inc.h"

#include "kernel/proc.h"

/*===========================================================================*
 *				caller_is_root				     *
 *===========================================================================*/
static int caller_is_root(endpoint)
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
static int caller_can_control(endpoint, target_rp)
endpoint_t endpoint;
struct rproc *target_rp;
{
  int control_allowed = 0;
  register struct rproc *rp;
  register struct rprocpub *rpub;
  char *proc_name;
  int c;

  proc_name = target_rp->r_pub->proc_name;

  /* Check if label is listed in caller's isolation policy. */
  for (rp = BEG_RPROC_ADDR; rp < END_RPROC_ADDR; rp++) {
	if (!(rp->r_flags & RS_IN_USE))
		continue;

	rpub = rp->r_pub;
	if (rpub->endpoint == endpoint) {
		break;
	}
  }
  if (rp == END_RPROC_ADDR) return 0;

  for (c = 0; c < rp->r_nr_control; c++) {
	if (strcmp(rp->r_control[c], proc_name) == 0) {
		control_allowed = 1;
		break;
	}
  }

  if (rs_verbose) 
	printf("RS: allowing %u control over %s via policy: %s\n",
		endpoint, target_rp->r_pub->label,
		control_allowed ? "yes" : "no");

  return control_allowed;
}

/*===========================================================================*
 *			     check_call_permission			     *
 *===========================================================================*/
int check_call_permission(caller, call, rp)
endpoint_t caller;
int call;
struct rproc *rp;
{
/* Check if the caller has permission to execute a particular call. */
  struct rprocpub *rpub;
  int call_allowed;

  /* Caller should be either root or have control privileges. */
  call_allowed = caller_is_root(caller);
  if(rp) {
      call_allowed |= caller_can_control(caller, rp);
  }
  if(!call_allowed) {
      return EPERM;
  }

  if(rp) {
      rpub = rp->r_pub;

      /* Only allow RS_EDIT if the target is a user process. */
      if(!(rp->r_priv.s_flags & SYS_PROC)) {
          if(call != RS_EDIT) return EPERM;
      }

      /* Disallow the call if another call is in progress for the service. */
      if((rp->r_flags & RS_LATEREPLY)
          || (rp->r_flags & RS_INITIALIZING) || (rp->r_flags & RS_UPDATING)) {
          return EBUSY;
      }

      /* Only allow RS_DOWN and RS_RESTART if the service has terminated. */
      if(rp->r_flags & RS_TERMINATED) {
          if(call != RS_DOWN && call != RS_RESTART) return EPERM;
      }

      /* Disallow RS_DOWN for core system services. */
      if (rpub->sys_flags & SF_CORE_SRV) {
          if(call == RS_DOWN) return EPERM;
      }
  }

  return OK;
}

/*===========================================================================*
 *				copy_rs_start				     *
 *===========================================================================*/
int copy_rs_start(src_e, src_rs_start, dst_rs_start)
endpoint_t src_e;
char *src_rs_start;
struct rs_start *dst_rs_start;
{
  int r;

  r = sys_datacopy(src_e, (vir_bytes) src_rs_start, 
  	SELF, (vir_bytes) dst_rs_start, sizeof(struct rs_start));

  return r;
}

/*===========================================================================*
 *				copy_label				     *
 *===========================================================================*/
int copy_label(src_e, src_label, src_len, dst_label, dst_len)
endpoint_t src_e;
char *src_label;
size_t src_len;
char *dst_label;
size_t dst_len;
{
  int s, len;

  len = MIN(dst_len-1, src_len);

  s = sys_datacopy(src_e, (vir_bytes) src_label,
	SELF, (vir_bytes) dst_label, len);
  if (s != OK) return s;

  dst_label[len] = 0;

  return OK;
}

/*===========================================================================*
 *			        build_cmd_dep				     *
 *===========================================================================*/
void build_cmd_dep(struct rproc *rp)
{
  struct rprocpub *rpub;
  int arg_count;
  int len;
  char *cmd_ptr;

  rpub = rp->r_pub;

  /* Build argument vector to be passed to execute call. The format of the
   * arguments vector is: path, arguments, NULL. 
   */
  strcpy(rp->r_args, rp->r_cmd);		/* copy raw command */
  arg_count = 0;				/* initialize arg count */
  rp->r_argv[arg_count++] = rp->r_args;		/* start with path */
  cmd_ptr = rp->r_args;				/* do some parsing */ 
  while(*cmd_ptr != '\0') {			/* stop at end of string */
      if (*cmd_ptr == ' ') {			/* next argument */
          *cmd_ptr = '\0';			/* terminate previous */
	  while (*++cmd_ptr == ' ') ; 		/* skip spaces */
	  if (*cmd_ptr == '\0') break;		/* no arg following */
	  /* There are ARGV_ELEMENTS elements; must leave one for null */
	  if (arg_count>=ARGV_ELEMENTS-1) {	/* arg vector full */
		printf("RS: build_cmd_dep: too many args\n");
	  	break;
	  }
	  assert(arg_count < ARGV_ELEMENTS);
          rp->r_argv[arg_count++] = cmd_ptr;	/* add to arg vector */
      }
      cmd_ptr ++;				/* continue parsing */
  }
  assert(arg_count < ARGV_ELEMENTS);
  rp->r_argv[arg_count] = NULL;			/* end with NULL pointer */
  rp->r_argc = arg_count;
  
  /* Build process name. */
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
}

/*===========================================================================*
 *				 srv_fork				     *
 *===========================================================================*/
pid_t srv_fork(uid_t reuid, gid_t regid)
{
  message m;

  m.m1_i1 = (int) reuid;
  m.m1_i2 = (int) regid;
  return _syscall(PM_PROC_NR, SRV_FORK, &m);
}

/*===========================================================================*
 *				 srv_kill				     *
 *===========================================================================*/
int srv_kill(pid_t pid, int sig)
{
  message m;

  m.m1_i1 = pid;
  m.m1_i2 = sig;
  return(_syscall(PM_PROC_NR, SRV_KILL, &m));
}

/*===========================================================================*
 *				 srv_update				     *
 *===========================================================================*/
int srv_update(endpoint_t src_e, endpoint_t dst_e)
{
  int r;

  /* Ask VM to swap the slots of the two processes and tell the kernel to
   * do the same. If VM is the service being updated, only perform the kernel
   * part of the call. The new instance of VM will do the rest at
   * initialization time.
   */
  if(src_e != VM_PROC_NR) {
      r = vm_update(src_e, dst_e);
  }
  else {
      r = sys_update(src_e, dst_e);
  }

  return r;
}

/*===========================================================================*
 *				update_period				     *
 *===========================================================================*/
void update_period(message *m_ptr)
{
  clock_t now = m_ptr->NOTIFY_TIMESTAMP;
  short has_update_timed_out;
  message m;
  struct rprocpub *rpub;

  rpub = rupdate.rp->r_pub;

  /* See if a timeout has occurred. */
  has_update_timed_out = (now - rupdate.prepare_tm > rupdate.prepare_maxtime);

  /* If an update timed out, end the update process and notify
   * the old version that the update has been canceled. From now on, the old
   * version will continue executing.
   */
  if(has_update_timed_out) {
      printf("RS: update failed: maximum prepare time reached\n");
      end_update(EINTR, RS_DONTREPLY);

      /* Prepare cancel request. */
      m.m_type = RS_LU_PREPARE;
      m.RS_LU_STATE = SEF_LU_STATE_NULL;
      if(rpub->endpoint == RS_PROC_NR) {
          /* RS can process the request directly. */
          do_sef_lu_request(&m);
      }
      else {
          /* Send request message to the system service. */
          asynsend(rpub->endpoint, &m);
      }
  }
}

/*===========================================================================*
 *				end_update				     *
 *===========================================================================*/
void end_update(int result, int reply_flag)
{
/* End the update process. There are two possibilities:
 * 1) the update succeeded. In that case, cleanup the old version and mark the
 *    new version as no longer under update.
 * 2) the update failed. In that case, cleanup the new version and mark the old
 *    version as no longer under update. Eventual late ready to update
 *    messages (if any) will simply be ignored and the service can
 *    continue executing. In addition, reset the check timestamp, so that if the
 *    service has a period, a status request will be forced in the next period.
 */
  struct rproc *old_rp, *new_rp, *exiting_rp, *surviving_rp;
  struct rproc **rps;
  int nr_rps, i;

  old_rp = rupdate.rp;
  new_rp = old_rp->r_new_rp;

  if(rs_verbose)
      printf("RS: ending update from %s to %s with result: %d\n",
          srv_to_string(old_rp), srv_to_string(new_rp), result);

  /* Decide which version has to die out and which version has to survive. */
  surviving_rp = (result == OK ? new_rp : old_rp);
  exiting_rp =   (result == OK ? old_rp : new_rp);

  /* End update. */
  rupdate.flags &= ~RS_UPDATING;
  rupdate.rp = NULL;
  old_rp->r_new_rp = NULL;
  new_rp->r_old_rp = NULL;
  old_rp->r_check_tm = 0;

  /* Send a late reply if necessary. */
  late_reply(old_rp, result);

  /* Mark the version that has to survive as no longer updating and
   * reply when asked to.
   */
  surviving_rp->r_flags &= ~RS_UPDATING;
  if(reply_flag == RS_REPLY) {
      message m;
      m.m_type = result;
      reply(surviving_rp->r_pub->endpoint, surviving_rp, &m);
  }

  /* Cleanup the version that has to die out. */
  get_service_instances(exiting_rp, &rps, &nr_rps);
  for(i=0;i<nr_rps;i++) {
      cleanup_service(rps[i]);
  }

  if(rs_verbose)
      printf("RS: %s ended the update\n", srv_to_string(surviving_rp));
}

/*===========================================================================*
 *			     kill_service_debug				     *
 *===========================================================================*/
int kill_service_debug(file, line, rp, errstr, err)
char *file;
int line;
struct rproc *rp;
char *errstr;
int err;
{
/* Crash a system service and don't let it restart. */
  if(errstr && !shutting_down) {
      printf("RS: %s (error %d)\n", errstr, err);
  }
  rp->r_flags |= RS_EXITING;				/* expect exit */
  crash_service_debug(file, line, rp);			/* simulate crash */

  return err;
}

/*===========================================================================*
 *			    crash_service_debug				     *
 *===========================================================================*/
int crash_service_debug(file, line, rp)
char *file;
int line;
struct rproc *rp;
{
/* Simluate a crash in a system service. */
  struct rprocpub *rpub;

  rpub = rp->r_pub;

  if(rs_verbose)
      printf("RS: %s %skilled at %s:%d\n", srv_to_string(rp),
          rp->r_flags & RS_EXITING ? "lethally " : "", file, line);

  /* RS should simply exit() directly. */
  if(rpub->endpoint == RS_PROC_NR) {
      exit(1);
  }

  return sys_kill(rpub->endpoint, SIGKILL);
}

/*===========================================================================*
 *			  cleanup_service_debug				     *
 *===========================================================================*/
void cleanup_service_debug(file, line, rp)
char *file;
int line;
struct rproc *rp;
{
  struct rprocpub *rpub;
  int s;

  rpub = rp->r_pub;

  if(rs_verbose)
      printf("RS: %s cleaned up at %s:%d\n", srv_to_string(rp),
          file, line);

  /* Tell scheduler this process is finished */
  if ((s = sched_stop(rp->r_scheduler, rpub->endpoint)) != OK) {
	printf("RS: warning: scheduler won't give up process: %d\n", s);
  }

  /* Ask PM to exit the service */
  if(rp->r_pid == -1) {
      printf("RS: warning: attempt to kill pid -1!\n");
  }
  else {
      srv_kill(rp->r_pid, SIGKILL);
  }

  /* Free slot, unless we're about to reuse it */
  if (!(rp->r_flags & RS_REINCARNATE))
      free_slot(rp);
}

/*===========================================================================*
 *				create_service				     *
 *===========================================================================*/
int create_service(rp)
struct rproc *rp;
{
/* Create the given system service. */
  int child_proc_nr_e, child_proc_nr_n;		/* child process slot */
  pid_t child_pid;				/* child's process id */
  int s, use_copy, has_replica;
  extern char **environ;
  struct rprocpub *rpub;

  rpub = rp->r_pub;
  use_copy= (rpub->sys_flags & SF_USE_COPY);
  has_replica= (rp->r_old_rp
      || (rp->r_prev_rp && !(rp->r_prev_rp->r_flags & RS_TERMINATED)));

  /* Do we need an existing replica to create the service? */
  if(!has_replica && (rpub->sys_flags & SF_NEED_REPL)) {
      printf("RS: unable to create service '%s' without a replica\n",
          rpub->label);
      free_slot(rp);
      return(EPERM);
  }

  /* Do we need an in-memory copy to create the service? */
  if(!use_copy && (rpub->sys_flags & SF_NEED_COPY)) {
      printf("RS: unable to create service '%s' without an in-memory copy\n",
          rpub->label);
      free_slot(rp);
      return(EPERM);
  }

  /* Do we have a copy or a command to create the service? */
  if(!use_copy && !strcmp(rp->r_cmd, "")) {
      printf("RS: unable to create service '%s' without a copy or command\n",
          rpub->label);
      free_slot(rp);
      return(EPERM);
  }

  /* Now fork and branch for parent and child process (and check for error).
   * After fork()ing, we need to pin RS memory again or pagefaults will occur
   * on future writes.
   */
  if(rs_verbose)
      printf("RS: forking child with srv_fork()...\n");
  child_pid= srv_fork(rp->r_uid, 0);	/* Force group to operator for now */
  if(child_pid == -1) {
      printf("RS: srv_fork() failed (error %d)\n", errno);
      free_slot(rp);
      return(errno);
  }

  /* Get endpoint of the child. */
  child_proc_nr_e = getnprocnr(child_pid);

  /* There is now a child process. Update the system process table. */
  child_proc_nr_n = _ENDPOINT_P(child_proc_nr_e);
  rp->r_flags = RS_IN_USE;			/* mark slot in use */
  rpub->endpoint = child_proc_nr_e;		/* set child endpoint */
  rp->r_pid = child_pid;			/* set child pid */
  rp->r_check_tm = 0;				/* not checked yet */
  getticks(&rp->r_alive_tm); 			/* currently alive */
  rp->r_stop_tm = 0;				/* not exiting yet */
  rp->r_backoff = 0;				/* not to be restarted */
  rproc_ptr[child_proc_nr_n] = rp;		/* mapping for fast access */
  rpub->in_use = TRUE;				/* public entry is now in use */

  /* Set and synch the privilege structure for the new service. */
  if ((s = sys_privctl(child_proc_nr_e, SYS_PRIV_SET_SYS, &rp->r_priv)) != OK
	|| (s = sys_getpriv(&rp->r_priv, child_proc_nr_e)) != OK) {
	printf("RS: unable to set privilege structure: %d\n", s);
	cleanup_service(rp);
	vm_memctl(RS_PROC_NR, VM_RS_MEM_PIN);
	return ENOMEM;
  }

  /* Set the scheduler for this process */
  if ((s = sched_init_proc(rp)) != OK) {
	printf("RS: unable to start scheduling: %d\n", s);
	cleanup_service(rp);
	vm_memctl(RS_PROC_NR, VM_RS_MEM_PIN);
	return s;
  }

  /* Copy the executable image into the child process. If no copy exists,
   * allocate one and free it right after exec completes.
   */
  if(use_copy) {
      if(rs_verbose)
          printf("RS: %s uses an in-memory copy\n",
              srv_to_string(rp));
  }
  else {
      if ((s = read_exec(rp)) != OK) {
          printf("RS: read_exec failed: %d\n", s);
          cleanup_service(rp);
          vm_memctl(RS_PROC_NR, VM_RS_MEM_PIN);
          return s;
      }
  }
  if(rs_verbose)
        printf("RS: execing child with srv_execve()...\n");
  s = srv_execve(child_proc_nr_e, rp->r_exec, rp->r_exec_len, rp->r_argv,
        environ);
  vm_memctl(RS_PROC_NR, VM_RS_MEM_PIN);
  if (s != OK) {
        printf("RS: srv_execve failed: %d\n", s);
        cleanup_service(rp);
        return s;
  }
  if(!use_copy) {
        free_exec(rp);
  }

  /* If this is a VM instance, let VM know now. */
  if(rp->r_priv.s_flags & VM_SYS_PROC) {
      if(rs_verbose)
          printf("RS: informing VM of instance %s\n", srv_to_string(rp));

      s = vm_memctl(rpub->endpoint, VM_RS_MEM_MAKE_VM);
      if(s != OK) {
          printf("vm_memctl failed: %d\n", s);
          cleanup_service(rp);
          return s;
      }
  }

  /* Tell VM about allowed calls. */
  if ((s = vm_set_priv(rpub->endpoint, &rpub->vm_call_mask[0])) != OK) {
      printf("RS: vm_set_priv failed: %d\n", s);
      cleanup_service(rp);
      return s;
  }

  if(rs_verbose)
      printf("RS: %s created\n", srv_to_string(rp));

  return OK;
}

/*===========================================================================*
 *				clone_service				     *
 *===========================================================================*/
int clone_service(rp, instance_flag)
struct rproc *rp;
int instance_flag;
{
/* Clone the given system service instance. */
  struct rproc *replica_rp;
  struct rprocpub *replica_rpub;
  struct rproc **rp_link;
  struct rproc **replica_link;
  struct rproc *rs_rp;
  int rs_flags;
  int r;

  if(rs_verbose)
      printf("RS: creating a replica for %s\n", srv_to_string(rp));

  /* Clone slot. */
  if((r = clone_slot(rp, &replica_rp)) != OK) {
      return r;
  }
  replica_rpub = replica_rp->r_pub;

  /* Clone is a live updated or restarted service instance? */
  if(instance_flag == LU_SYS_PROC) {
      rp_link = &rp->r_new_rp;
      replica_link = &replica_rp->r_old_rp;
  }
  else {
      rp_link = &rp->r_next_rp;
      replica_link = &replica_rp->r_prev_rp;
  }
  replica_rp->r_priv.s_flags |= instance_flag;

  /* Link the two slots. */
  *rp_link = replica_rp;
  *replica_link = rp;

  /* Create a new replica of the service. */
  r = create_service(replica_rp);
  if(r != OK) {
      *rp_link = NULL;
      return r;
  }

  /* If this instance is for restarting RS, set up a backup signal manager. */
  rs_flags = (ROOT_SYS_PROC | RST_SYS_PROC);
  if((replica_rp->r_priv.s_flags & rs_flags) == rs_flags) {
      rs_rp = rproc_ptr[_ENDPOINT_P(RS_PROC_NR)];

      /* Update signal managers. */
      r = update_sig_mgrs(rs_rp, SELF, replica_rpub->endpoint);
      if(r == OK) {
          r = update_sig_mgrs(replica_rp, SELF, NONE);
      }
      if(r != OK) {
          *rp_link = NULL;
          return kill_service(replica_rp, "update_sig_mgrs failed", r);
      }
  }

  return OK;
}

/*===========================================================================*
 *				publish_service				     *
 *===========================================================================*/
int publish_service(rp)
struct rproc *rp;				/* pointer to service slot */
{
/* Publish a service. */
  int r;
  struct rprocpub *rpub;
  struct rs_pci pci_acl;
  message m;
  endpoint_t ep;

  rpub = rp->r_pub;

  /* Register label with DS. */
  r = ds_publish_label(rpub->label, rpub->endpoint, DSF_OVERWRITE);
  if (r != OK) {
      return kill_service(rp, "ds_publish_label call failed", r);
  }

  /* If the service is a driver, map it. */
  if (rpub->dev_nr > 0) {
      /* The purpose of non-blocking forks is to avoid involving VFS in the
       * forking process, because VFS may be blocked on a sendrec() to a MFS
       * that is waiting for a endpoint update for a dead driver. We have just
       * published that update, but VFS may still be blocked. As a result, VFS
       * may not yet have received PM's fork message. Hence, if we call
       * mapdriver() immediately, VFS may not know about the process and thus
       * refuse to add the driver entry. The following temporary hack works
       * around this by forcing blocking communication from PM to VFS. Once VFS
       * has been made non-blocking towards MFS instances, this hack and the
       * big part of srv_fork() can go.
       */
      setuid(0);

      if (mapdriver(rpub->label, rpub->dev_nr, rpub->dev_style,
          rpub->dev_flags) != OK) {
          return kill_service(rp, "couldn't map driver", errno);
      }
  }

#if USE_PCI
  /* If PCI properties are set, inform the PCI driver about the new service. */
  if(rpub->pci_acl.rsp_nr_device || rpub->pci_acl.rsp_nr_class) {
      pci_acl = rpub->pci_acl;
      strcpy(pci_acl.rsp_label, rpub->label);
      pci_acl.rsp_endpoint= rpub->endpoint;

      r = pci_set_acl(&pci_acl);
      if (r != OK) {
          return kill_service(rp, "pci_set_acl call failed", r);
      }
  }
#endif /* USE_PCI */

  if (rpub->devman_id != 0) {
	  r = ds_retrieve_label_endpt("devman",&ep);
	  
	  if (r != OK) {
		return kill_service(rp, "devman not running?", r);
	  }
	  m.m_type = DEVMAN_BIND;
	  m.DEVMAN_ENDPOINT  = rpub->endpoint;
	  m.DEVMAN_DEVICE_ID = rpub->devman_id;
	  r = sendrec(ep, &m);
	  if (r != OK || m.DEVMAN_RESULT != OK) {
		 return kill_service(rp, "devman bind device failed", r);
	  }
  }

  if(rs_verbose)
      printf("RS: %s published\n", srv_to_string(rp));

  return OK;
}

/*===========================================================================*
 *			      unpublish_service				     *
 *===========================================================================*/
int unpublish_service(rp)
struct rproc *rp;				/* pointer to service slot */
{
/* Unpublish a service. */
  struct rprocpub *rpub;
  int r, result;
  message m;
  endpoint_t ep;


  rpub = rp->r_pub;
  result = OK;

  /* Unregister label with DS. */
  r = ds_delete_label(rpub->label);
  if (r != OK && !shutting_down) {
     printf("RS: ds_delete_label call failed (error %d)\n", r);
     result = r;
  }

  /* No need to inform VFS and VM, cleanup is done on exit automatically. */

#if USE_PCI
  /* If PCI properties are set, inform the PCI driver. */
  if(rpub->pci_acl.rsp_nr_device || rpub->pci_acl.rsp_nr_class) {
      r = pci_del_acl(rpub->endpoint);
      if (r != OK && !shutting_down) {
          printf("RS: pci_del_acl call failed (error %d)\n", r);
          result = r;
      }
  }
#endif /* USE_PCI */

  if (rpub->devman_id != 0) {
	  r = ds_retrieve_label_endpt("devman",&ep);
  
	  if (r != OK) {
		printf("RS: devman not running?");
	  } else {
		m.m_type = DEVMAN_UNBIND;
		m.DEVMAN_ENDPOINT  = rpub->endpoint;
		m.DEVMAN_DEVICE_ID = rpub->devman_id;
		r = sendrec(ep, &m);

		if (r != OK || m.DEVMAN_RESULT != OK) {
			 printf("RS: devman unbind device failed");
		}
	  }
  }

  if(rs_verbose)
      printf("RS: %s unpublished\n", srv_to_string(rp));

  return result;
}

/*===========================================================================*
 *				run_service				     *
 *===========================================================================*/
int run_service(rp, init_type)
struct rproc *rp;
int init_type;
{
/* Let a newly created service run. */
  struct rprocpub *rpub;
  int s;

  rpub = rp->r_pub;

  /* Allow the service to run. */
  if ((s = sys_privctl(rpub->endpoint, SYS_PRIV_ALLOW, NULL)) != OK) {
      return kill_service(rp, "unable to allow the service to run",s);
  }

  /* Initialize service. */
  if((s = init_service(rp, init_type)) != OK) {
      return kill_service(rp, "unable to initialize service", s);
  }

  if(rs_verbose)
      printf("RS: %s allowed to run\n", srv_to_string(rp));

  return OK;
}

/*===========================================================================*
 *				start_service				     *
 *===========================================================================*/
int start_service(rp)
struct rproc *rp;
{
/* Start a system service. */
  int r, init_type;
  struct rprocpub *rpub;

  rpub = rp->r_pub;

  /* Create and make active. */
  r = create_service(rp);
  if(r != OK) {
      return r;
  }
  activate_service(rp, NULL);

  /* Publish service properties. */
  r = publish_service(rp);
  if (r != OK) {
      return r;
  }

  /* Run. */
  init_type = SEF_INIT_FRESH;
  r = run_service(rp, init_type);
  if(r != OK) {
      return r;
  }

  if(rs_verbose)
      printf("RS: %s started with major %d\n", srv_to_string(rp),
          rpub->dev_nr);

  return OK;
}

/*===========================================================================*
 *				stop_service				     *
 *===========================================================================*/
void stop_service(struct rproc *rp,int how)
{
  struct rprocpub *rpub;
  int signo;

  rpub = rp->r_pub;

  /* Try to stop the system service. First send a SIGTERM signal to ask the
   * system service to terminate. If the service didn't install a signal 
   * handler, it will be killed. If it did and ignores the signal, we'll
   * find out because we record the time here and send a SIGKILL.
   */
  if(rs_verbose)
      printf("RS: %s signaled with SIGTERM\n", srv_to_string(rp));

  signo = rpub->endpoint != RS_PROC_NR ? SIGTERM : SIGHUP; /* SIGHUP for RS. */

  rp->r_flags |= how;				/* what to on exit? */
  sys_kill(rpub->endpoint, signo);		/* first try friendly */
  getticks(&rp->r_stop_tm); 			/* record current time */
}

/*===========================================================================*
 *				update_service				     *
 *===========================================================================*/
int update_service(src_rpp, dst_rpp, swap_flag)
struct rproc **src_rpp;
struct rproc **dst_rpp;
int swap_flag;
{
/* Update an existing service. */
  int r;
  struct rproc *src_rp;
  struct rproc *dst_rp;
  struct rprocpub *src_rpub;
  struct rprocpub *dst_rpub;
  int pid;
  endpoint_t endpoint;

  src_rp = *src_rpp;
  dst_rp = *dst_rpp;
  src_rpub = src_rp->r_pub;
  dst_rpub = dst_rp->r_pub;

  if(rs_verbose)
      printf("RS: %s updating into %s\n",
          srv_to_string(src_rp), srv_to_string(dst_rp));

  /* Swap the slots of the two processes when asked to. */
  if(swap_flag == RS_SWAP) {
      if((r = srv_update(src_rpub->endpoint, dst_rpub->endpoint)) != OK) {
          return r;
      }
  }

  /* Swap slots here as well. */
  pid = src_rp->r_pid;
  endpoint = src_rpub->endpoint;
  swap_slot(&src_rp, &dst_rp);

  /* Reassign pids and endpoints. */
  src_rp->r_pid = dst_rp->r_pid;
  src_rp->r_pub->endpoint = dst_rp->r_pub->endpoint;
  rproc_ptr[_ENDPOINT_P(src_rp->r_pub->endpoint)] = src_rp;
  dst_rp->r_pid = pid;
  dst_rp->r_pub->endpoint = endpoint;
  rproc_ptr[_ENDPOINT_P(dst_rp->r_pub->endpoint)] = dst_rp;

  /* Adjust input pointers. */
  *src_rpp = src_rp;
  *dst_rpp = dst_rp;

  /* Make the new version active. */
  activate_service(dst_rp, src_rp);

  if(rs_verbose)
      printf("RS: %s updated into %s\n",
          srv_to_string(src_rp), srv_to_string(dst_rp));

  return OK;
}

/*===========================================================================*
 *			      activate_service				     *
 *===========================================================================*/
void activate_service(struct rproc *rp, struct rproc *ex_rp)
{
/* Activate a service instance and deactivate another one if requested. */

  if(ex_rp && (ex_rp->r_flags & RS_ACTIVE) ) {
      ex_rp->r_flags &= ~RS_ACTIVE;
      if(rs_verbose)
          printf("RS: %s becomes inactive\n", srv_to_string(ex_rp));
  }

  if(! (rp->r_flags & RS_ACTIVE) ) {
      rp->r_flags |= RS_ACTIVE;
      if(rs_verbose)
          printf("RS: %s becomes active\n", srv_to_string(rp));
  }
}

/*===========================================================================*
 *			      reincarnate_service			     *
 *===========================================================================*/
void reincarnate_service(struct rproc *rp)
{
/* Restart a service as if it were never started before. */
  struct rprocpub *rpub;
  int i;

  rpub = rp->r_pub;

  rp->r_flags &= RS_IN_USE;
  rp->r_pid = -1;
  rproc_ptr[_ENDPOINT_P(rpub->endpoint)] = NULL;

  /* Restore original IRQ and I/O range tables in the priv struct. This is the
   * only part of the privilege structure that can be modified by processes
   * other than RS itself.
   */
  rp->r_priv.s_nr_irq = rp->r_nr_irq;
  for (i = 0; i < rp->r_nr_irq; i++)
      rp->r_priv.s_irq_tab[i] = rp->r_irq_tab[i];
  rp->r_priv.s_nr_io_range = rp->r_nr_io_range;
  for (i = 0; i < rp->r_nr_io_range; i++)
      rp->r_priv.s_io_tab[i] = rp->r_io_tab[i];

  rp->r_old_rp = NULL;
  rp->r_new_rp = NULL;
  rp->r_prev_rp = NULL;
  rp->r_next_rp = NULL;

  start_service(rp);
}

/*===========================================================================*
 *			      terminate_service				     *
 *===========================================================================*/
void terminate_service(struct rproc *rp)
{
/* Handle a termination event for a system service. */
  struct rproc **rps;
  struct rprocpub *rpub;
  int nr_rps;
  int i, r;

  rpub = rp->r_pub;

  if(rs_verbose)
     printf("RS: %s terminated\n", srv_to_string(rp));

  /* Deal with failures during initialization. */
  if(rp->r_flags & RS_INITIALIZING) {
      if (rpub->sys_flags & SF_NO_BIN_EXP) {
          /* If service was deliberately started with binary exponential offset
	   * disabled, we're going to assume we want to refresh a service upon
	   * failure.
	   */
          if(rs_verbose)
              printf("RS: service '%s' exited during initialization; "
		     "refreshing\n", rpub->label);
          rp->r_flags |= RS_REFRESHING; /* restart initialization. */
      } else {
          if(rs_verbose)
              printf("RS: service '%s' exited during initialization; "
                     "not restarting\n", rpub->label);
          rp->r_flags |= RS_EXITING; /* don't restart. */
      }

      /* If updating, rollback. */
      if(rp->r_flags & RS_UPDATING) {
          struct rproc *old_rp, *new_rp;
          printf("RS: update failed: state transfer failed. Rolling back...\n");
          new_rp = rp;
          old_rp = new_rp->r_old_rp;
          new_rp->r_flags &= ~RS_INITIALIZING;
          r = update_service(&new_rp, &old_rp, RS_SWAP);
          assert(r == OK); /* can't fail */
          end_update(ERESTART, RS_REPLY);
          return;
      }
  }

  if (rp->r_flags & RS_EXITING) {
      /* If a core system service is exiting, we are in trouble. */
      if (rp->r_pub->sys_flags & SF_CORE_SRV && !shutting_down) {
          printf("core system service died: %s\n", srv_to_string(rp));
	  _exit(1);
      }

      /* See if a late reply has to be sent. */
      r = (rp->r_caller_request == RS_DOWN ? OK : EDEADEPT);
      late_reply(rp, r);

      /* Unpublish the service. */
      unpublish_service(rp);

      /* Cleanup all the instances of the service. */
      get_service_instances(rp, &rps, &nr_rps);
      for(i=0;i<nr_rps;i++) {
          cleanup_service(rps[i]);
      }

      /* If the service is reincarnating, its slot has not been cleaned up.
       * Check for this flag now, and attempt to start the service again.
       * If this fails, start_service() itself will perform cleanup.
       */
      if (rp->r_flags & RS_REINCARNATE) {
          reincarnate_service(rp);
      }
  }
  else if(rp->r_flags & RS_REFRESHING) {
      /* Restart service. */
      restart_service(rp);
  }
  else {
      /* If an update is in progress, end it. The old version
       * that just exited will continue executing.
       */
      if(rp->r_flags & RS_UPDATING) {
          end_update(ERESTART, RS_DONTREPLY);
      }

      /* Determine what to do. If this is the first unexpected 
       * exit, immediately restart this service. Otherwise use
       * a binary exponential backoff.
       */
      if (rp->r_restarts > 0) {
          if (!(rpub->sys_flags & SF_NO_BIN_EXP)) {
              rp->r_backoff = 1 << MIN(rp->r_restarts,(BACKOFF_BITS-2));
              rp->r_backoff = MIN(rp->r_backoff,MAX_BACKOFF); 
              if ((rpub->sys_flags & SF_USE_COPY) && rp->r_backoff > 1)
                  rp->r_backoff= 1;
	  }
	  else {
              rp->r_backoff = 1;
	  }
          return;
      }

      /* Restart service. */
      restart_service(rp);
  }
}

/*===========================================================================*
 *				run_script				     *
 *===========================================================================*/
static int run_script(struct rproc *rp)
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
	else reason= "terminated";
	sprintf(incarnation_str, "%d", rp->r_restarts);

 	if(rs_verbose) {
		printf("RS: %s:\n", srv_to_string(rp));
		printf("RS:     calling script '%s'\n", rp->r_script);
		printf("RS:     reason: '%s'\n", reason);
		printf("RS:     incarnation: '%s'\n", incarnation_str);
	}

	pid= fork();
	switch(pid)
	{
	case -1:
		return kill_service(rp, "unable to fork script", errno);
	case 0:
		execle(_PATH_BSHELL, "sh", rp->r_script, rpub->label, reason,
			incarnation_str, (char*) NULL, envp);
		printf("RS: run_script: execl '%s' failed: %s\n",
			rp->r_script, strerror(errno));
		exit(1);
	default:
		/* Set the privilege structure for the child process. */
		endpoint = getnprocnr(pid);
		if ((r = sys_privctl(endpoint, SYS_PRIV_SET_USER, NULL))
			!= OK) {
			return kill_service(rp,"can't set script privileges",r);
		}
		/* Allow the script to run. */
		if ((r = sys_privctl(endpoint, SYS_PRIV_ALLOW, NULL)) != OK) {
			return kill_service(rp,"can't let the script run",r);
		}
		/* Pin RS memory again after fork()ing. */
		vm_memctl(RS_PROC_NR, VM_RS_MEM_PIN);
	}
	return OK;
}

/*===========================================================================*
 *			      restart_service				     *
 *===========================================================================*/
void restart_service(struct rproc *rp)
{
/* Restart service via a recovery script or directly. */
  struct rproc *replica_rp;
  int r;

  /* See if a late reply has to be sent. */
  late_reply(rp, OK);

  /* This hack disables restarting of file servers, which at the moment always
   * cause VFS to hang indefinitely. As soon as VFS no longer blocks on calls
   * to file servers, this exception can be removed again.
   */
  if (!strncmp(rp->r_pub->label, "fs_", 3)) {
      kill_service(rp, "file servers cannot be restarted yet", ENOSYS);
      return;
  }

  /* Run a recovery script if available. */
  if (rp->r_script[0] != '\0') {
      run_script(rp);
      return;
  }

  /* Restart directly. We need a replica if not already available. */
  if(rp->r_next_rp == NULL) {
      /* Create the replica. */
      r = clone_service(rp, RST_SYS_PROC);
      if(r != OK) {
          kill_service(rp, "unable to clone service", r);
          return;
      }
  }
  replica_rp = rp->r_next_rp;

  /* Update the service into the replica. */
  r = update_service(&rp, &replica_rp, RS_SWAP);
  if(r != OK) {
      kill_service(rp, "unable to update into new replica", r);
      return;
  }

  /* Let the new replica run. */
  r = run_service(replica_rp, SEF_INIT_RESTART);
  if(r != OK) {
      kill_service(rp, "unable to let the replica run", r);
      return;
  }

  if(rs_verbose)
      printf("RS: %s restarted into %s\n",
          srv_to_string(rp), srv_to_string(replica_rp));
}

/*===========================================================================*
 *		         inherit_service_defaults			     *
 *===========================================================================*/
void inherit_service_defaults(def_rp, rp)
struct rproc *def_rp;
struct rproc *rp;
{
  struct rprocpub *def_rpub;
  struct rprocpub *rpub;

  def_rpub = def_rp->r_pub;
  rpub = rp->r_pub;

  /* Device and PCI settings. These properties cannot change. */
  rpub->dev_flags = def_rpub->dev_flags;
  rpub->dev_nr = def_rpub->dev_nr;
  rpub->dev_style = def_rpub->dev_style;
  rpub->dev_style2 = def_rpub->dev_style2;
  rpub->pci_acl = def_rpub->pci_acl;

  /* Immutable system and privilege flags. */
  rpub->sys_flags &= ~IMM_SF;
  rpub->sys_flags |= (def_rpub->sys_flags & IMM_SF);
  rp->r_priv.s_flags &= ~IMM_F;
  rp->r_priv.s_flags |= (def_rp->r_priv.s_flags & IMM_F);

  /* Allowed traps. They cannot change. */
  rp->r_priv.s_trap_mask = def_rp->r_priv.s_trap_mask;
}

/*===========================================================================*
 *		           get_service_instances			     *
 *===========================================================================*/
void get_service_instances(rp, rps, length)
struct rproc *rp;
struct rproc ***rps;
int *length;
{
/* Retrieve all the service instances of a given service. */
  static struct rproc *instances[5];
  int nr_instances;

  nr_instances = 0;
  instances[nr_instances++] = rp;
  if(rp->r_prev_rp) instances[nr_instances++] = rp->r_prev_rp;
  if(rp->r_next_rp) instances[nr_instances++] = rp->r_next_rp;
  if(rp->r_old_rp) instances[nr_instances++] = rp->r_old_rp;
  if(rp->r_new_rp) instances[nr_instances++] = rp->r_new_rp;

  *rps = instances;
  *length = nr_instances;
}

/*===========================================================================*
 *				share_exec				     *
 *===========================================================================*/
void share_exec(rp_dst, rp_src)
struct rproc *rp_dst, *rp_src;
{
  if(rs_verbose)
      printf("RS: %s shares exec image with %s\n",
          srv_to_string(rp_dst), srv_to_string(rp_src));

  /* Share exec image from rp_src to rp_dst. */
  rp_dst->r_exec_len = rp_src->r_exec_len;
  rp_dst->r_exec = rp_src->r_exec;
}

/*===========================================================================*
 *				read_exec				     *
 *===========================================================================*/
int read_exec(rp)
struct rproc *rp;
{
  int e, r, fd;
  char *e_name;
  struct stat sb;

  e_name= rp->r_argv[0];
  if(rs_verbose)
      printf("RS: service '%s' reads exec image from: %s\n", rp->r_pub->label,
          e_name);

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

  free_exec(rp);

  if (r >= 0)
      return EIO;
  else
      return -e;
}

/*===========================================================================*
 *				free_exec				     *
 *===========================================================================*/
void free_exec(rp)
struct rproc *rp;
{
/* Free an exec image. */
  int slot_nr, has_shared_exec;
  struct rproc *other_rp;

  /* Search for some other slot sharing the same exec image. */
  has_shared_exec = FALSE;
  for (slot_nr = 0; slot_nr < NR_SYS_PROCS; slot_nr++) {
      other_rp = &rproc[slot_nr];		/* get pointer to slot */
      if (other_rp->r_flags & RS_IN_USE && other_rp != rp
          && other_rp->r_exec == rp->r_exec) {  /* found! */
          has_shared_exec = TRUE;
          break;
      }
  }

  /* If nobody uses our copy of the exec image, we can try to get rid of it. */
  if(!has_shared_exec) {
      if(rs_verbose)
          printf("RS: %s frees exec image\n", srv_to_string(rp));
      free(rp->r_exec);
  }
  else {
      if(rs_verbose)
          printf("RS: %s no longer sharing exec image with %s\n",
              srv_to_string(rp), srv_to_string(other_rp));
  }
  rp->r_exec = NULL;
  rp->r_exec_len = 0;
}

/*===========================================================================*
 *				 edit_slot				     *
 *===========================================================================*/
int edit_slot(rp, rs_start, source)
struct rproc *rp;
struct rs_start *rs_start;
endpoint_t source;
{
/* Edit a given slot to override existing settings. */
  struct rprocpub *rpub;
  char *label;
  int len;
  int s, i;
  int basic_kc[] =  { SYS_BASIC_CALLS, NULL_C };
  int basic_vmc[] =  { VM_BASIC_CALLS, NULL_C };

  rpub = rp->r_pub;

  /* Update IPC target list. */
  if (rs_start->rss_ipclen==0 || rs_start->rss_ipclen+1>sizeof(rp->r_ipc_list)){
      printf("RS: edit_slot: ipc list empty or long for '%s'\n", rpub->label);
      return EINVAL;
  }
  s=sys_datacopy(source, (vir_bytes) rs_start->rss_ipc, 
      SELF, (vir_bytes) rp->r_ipc_list, rs_start->rss_ipclen);
  if (s != OK) return(s);
  rp->r_ipc_list[rs_start->rss_ipclen]= '\0';

  /* Update IRQs. */
  if(rs_start->rss_nr_irq == RSS_IRQ_ALL) {
      rs_start->rss_nr_irq = 0;
  }
  else {
      rp->r_priv.s_flags |= CHECK_IRQ;
  }
  if (rs_start->rss_nr_irq > NR_IRQ) {
      printf("RS: edit_slot: too many IRQs requested\n");
      return EINVAL;
  }
  rp->r_nr_irq= rp->r_priv.s_nr_irq= rs_start->rss_nr_irq;
  for (i= 0; i<rp->r_priv.s_nr_irq; i++) {
      rp->r_irq_tab[i]= rp->r_priv.s_irq_tab[i]= rs_start->rss_irq[i];
      if(rs_verbose)
          printf("RS: edit_slot: IRQ %d\n", rp->r_priv.s_irq_tab[i]);
  }

  /* Update I/O ranges. */
  if(rs_start->rss_nr_io == RSS_IO_ALL) {
      rs_start->rss_nr_io = 0;
  }
  else {
      rp->r_priv.s_flags |= CHECK_IO_PORT;
  }
  if (rs_start->rss_nr_io > NR_IO_RANGE) {
      printf("RS: edit_slot: too many I/O ranges requested\n");
      return EINVAL;
  }
  rp->r_nr_io_range= rp->r_priv.s_nr_io_range= rs_start->rss_nr_io;
  for (i= 0; i<rp->r_priv.s_nr_io_range; i++) {
      rp->r_priv.s_io_tab[i].ior_base= rs_start->rss_io[i].base;
      rp->r_priv.s_io_tab[i].ior_limit=
          rs_start->rss_io[i].base+rs_start->rss_io[i].len-1;
      rp->r_io_tab[i] = rp->r_priv.s_io_tab[i];
      if(rs_verbose)
          printf("RS: edit_slot: I/O [%x..%x]\n",
              rp->r_priv.s_io_tab[i].ior_base,
              rp->r_priv.s_io_tab[i].ior_limit);
  }

  /* Update kernel call mask. Inherit basic kernel calls when asked to. */
  memcpy(rp->r_priv.s_k_call_mask, rs_start->rss_system,
      sizeof(rp->r_priv.s_k_call_mask));
  if(rs_start->rss_flags & RSS_SYS_BASIC_CALLS) {
      fill_call_mask(basic_kc, NR_SYS_CALLS,
          rp->r_priv.s_k_call_mask, KERNEL_CALL, FALSE);
  }

  /* Update VM call mask. Inherit basic VM calls. */
  memcpy(rpub->vm_call_mask, rs_start->rss_vm,
      sizeof(rpub->vm_call_mask));
  if(rs_start->rss_flags & RSS_VM_BASIC_CALLS) {
      fill_call_mask(basic_vmc, NR_VM_CALLS,
          rpub->vm_call_mask, VM_RQ_BASE, FALSE);
  }

  /* Update control labels. */
  if(rs_start->rss_nr_control > 0) {
      int i, s;
      if (rs_start->rss_nr_control > RS_NR_CONTROL) {
          printf("RS: edit_slot: too many control labels\n");
          return EINVAL;
      }
      for (i=0; i<rs_start->rss_nr_control; i++) {
          s = copy_label(source, rs_start->rss_control[i].l_addr,
              rs_start->rss_control[i].l_len, rp->r_control[i],
              sizeof(rp->r_control[i]));
          if(s != OK)
              return s;
      }
      rp->r_nr_control = rs_start->rss_nr_control;

      if (rs_verbose) {
          printf("RS: edit_slot: control labels:");
          for (i=0; i<rp->r_nr_control; i++)
              printf(" %s", rp->r_control[i]);
          printf("\n");
      }
  }

  /* Update signal manager. */
  rp->r_priv.s_sig_mgr = rs_start->rss_sigmgr;

  /* Update scheduling properties if possible. */
  if(rp->r_scheduler != NONE) {
      rp->r_scheduler = rs_start->rss_scheduler;
      rp->r_priority = rs_start->rss_priority;
      rp->r_quantum = rs_start->rss_quantum;
      rp->r_cpu = rs_start->rss_cpu;
  }

  /* Update command and arguments. */
  if (rs_start->rss_cmdlen > MAX_COMMAND_LEN-1) return(E2BIG);
  s=sys_datacopy(source, (vir_bytes) rs_start->rss_cmd, 
      SELF, (vir_bytes) rp->r_cmd, rs_start->rss_cmdlen);
  if (s != OK) return(s);
  rp->r_cmd[rs_start->rss_cmdlen] = '\0';	/* ensure it is terminated */
  if (rp->r_cmd[0] != '/') return(EINVAL);	/* insist on absolute path */

  /* Build cmd dependencies: argv and program name. */
  build_cmd_dep(rp);

  /* Update label if not already set. */
  if(!strcmp(rpub->label, "")) {
      if(rs_start->rss_label.l_len > 0) {
          /* RS_UP caller has supplied a custom label for this service. */
          int s = copy_label(source, rs_start->rss_label.l_addr,
              rs_start->rss_label.l_len, rpub->label, sizeof(rpub->label));
          if(s != OK)
              return s;
          if(rs_verbose)
              printf("RS: edit_slot: using label (custom) '%s'\n", rpub->label);
      } else {
          /* Default label for the service. */
          label = rpub->proc_name;
          len= strlen(label);
          memcpy(rpub->label, label, len);
          rpub->label[len]= '\0';
          if(rs_verbose)
              printf("RS: edit_slot: using label (from proc_name) '%s'\n",
                  rpub->label);
      }
  }

  /* Update recovery script. */
  if (rs_start->rss_scriptlen > MAX_SCRIPT_LEN-1) return(E2BIG);
  if (rs_start->rss_script != NULL && !(rpub->sys_flags & SF_CORE_SRV)) {
      s=sys_datacopy(source, (vir_bytes) rs_start->rss_script, 
          SELF, (vir_bytes) rp->r_script, rs_start->rss_scriptlen);
      if (s != OK) return(s);
      rp->r_script[rs_start->rss_scriptlen] = '\0';
  }

  /* Update system flags and in-memory copy. */
  if ((rs_start->rss_flags & RSS_COPY) && !(rpub->sys_flags & SF_USE_COPY)) {
      int exst_cpy;
      struct rproc *rp2;
      struct rprocpub *rpub2;
      exst_cpy = 0;

      if(rs_start->rss_flags & RSS_REUSE) {
          int i;

          for(i = 0; i < NR_SYS_PROCS; i++) {
              rp2 = &rproc[i];
              if (!(rp2->r_flags & RS_IN_USE)) {
              	  continue;
              }
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

      s = OK;
      if(!exst_cpy)
          s = read_exec(rp);
      else
          share_exec(rp, rp2);

      if (s != OK)
          return s;

      rpub->sys_flags |= SF_USE_COPY;
  }
  if (rs_start->rss_flags & RSS_REPLICA) {
      rpub->sys_flags |= SF_USE_REPL;
  }
  if (rs_start->rss_flags & RSS_NO_BIN_EXP) {
      rpub->sys_flags |= SF_NO_BIN_EXP;
  }

  /* Update period. */
  if(rpub->endpoint != RS_PROC_NR) {
      rp->r_period = rs_start->rss_period;
  }

  /* (Re)initialize privilege settings. */
  init_privs(rp, &rp->r_priv);

  return OK;
}

/*===========================================================================*
 *				 init_slot				     *
 *===========================================================================*/
int init_slot(rp, rs_start, source)
struct rproc *rp;
struct rs_start *rs_start;
endpoint_t source;
{
/* Initialize a slot as requested by the client. */
  struct rprocpub *rpub;
  int i;

  rpub = rp->r_pub;

  /* All dynamically created services get the same sys and privilege flags, and
   * allowed traps. Other privilege settings can be specified at runtime. The
   * privilege id is dynamically allocated by the kernel.
   */
  rpub->sys_flags = DSRV_SF;             /* system flags */
  rp->r_priv.s_flags = DSRV_F;           /* privilege flags */
  rp->r_priv.s_trap_mask = DSRV_T;       /* allowed traps */
  rp->r_priv.s_bak_sig_mgr = NONE;       /* backup signal manager */

  /* Initialize uid. */
  rp->r_uid= rs_start->rss_uid;

  /* Initialize device driver settings. */
  rpub->dev_flags = DSRV_DF;
  rpub->dev_nr = rs_start->rss_major;
  rpub->dev_style = rs_start->rss_dev_style;
  rpub->devman_id = rs_start->devman_id;
  if(rpub->dev_nr && !IS_DEV_STYLE(rs_start->rss_dev_style)) {
      printf("RS: init_slot: bad device style\n");
      return EINVAL;
  }
  rpub->dev_style2 = STYLE_NDEV;

  /* Initialize pci settings. */
  if (rs_start->rss_nr_pci_id > RS_NR_PCI_DEVICE) {
      printf("RS: init_slot: too many PCI device IDs\n");
      return EINVAL;
  }
  rpub->pci_acl.rsp_nr_device = rs_start->rss_nr_pci_id;
  for (i= 0; i<rpub->pci_acl.rsp_nr_device; i++) {
      rpub->pci_acl.rsp_device[i].vid= rs_start->rss_pci_id[i].vid;
      rpub->pci_acl.rsp_device[i].did= rs_start->rss_pci_id[i].did;
      if(rs_verbose)
          printf("RS: init_slot: PCI %04x/%04x\n",
              rpub->pci_acl.rsp_device[i].vid,
              rpub->pci_acl.rsp_device[i].did);
  }
  if (rs_start->rss_nr_pci_class > RS_NR_PCI_CLASS) {
      printf("RS: init_slot: too many PCI class IDs\n");
      return EINVAL;
  }
  rpub->pci_acl.rsp_nr_class= rs_start->rss_nr_pci_class;
  for (i= 0; i<rpub->pci_acl.rsp_nr_class; i++) {
      rpub->pci_acl.rsp_class[i].pciclass=rs_start->rss_pci_class[i].pciclass;
      rpub->pci_acl.rsp_class[i].mask= rs_start->rss_pci_class[i].mask;
      if(rs_verbose)
          printf("RS: init_slot: PCI class %06x mask %06x\n",
              (unsigned int) rpub->pci_acl.rsp_class[i].pciclass,
              (unsigned int) rpub->pci_acl.rsp_class[i].mask);
  }

  /* Initialize some fields. */
  rp->r_restarts = 0; 				/* no restarts yet */
  rp->r_old_rp = NULL;			        /* no old version yet */
  rp->r_new_rp = NULL;			        /* no new version yet */
  rp->r_prev_rp = NULL;			        /* no prev replica yet */
  rp->r_next_rp = NULL;			        /* no next replica yet */
  rp->r_exec = NULL;                            /* no in-memory copy yet */
  rp->r_exec_len = 0;
  rp->r_script[0]= '\0';                        /* no recovery script yet */
  rpub->label[0]= '\0';                         /* no label yet */
  rp->r_scheduler = -1;                         /* no scheduler yet */
  rp->r_priv.s_sig_mgr = -1;                    /* no signal manager yet */

  /* Initialize editable slot settings. */
  return edit_slot(rp, rs_start, source);
}

/*===========================================================================*
 *				clone_slot				     *
 *===========================================================================*/
int clone_slot(rp, clone_rpp)
struct rproc *rp;
struct rproc **clone_rpp;
{
  int r;
  struct rproc *clone_rp;
  struct rprocpub *rpub, *clone_rpub;

  /* Allocate a system service slot for the clone. */
  r = alloc_slot(&clone_rp);
  if(r != OK) {
      printf("RS: clone_slot: unable to allocate a new slot: %d\n", r);
      return r;
  }

  rpub = rp->r_pub;
  clone_rpub = clone_rp->r_pub;

  /* Synch the privilege structure of the source with the kernel. */
  if ((r = sys_getpriv(&(rp->r_priv), rpub->endpoint)) != OK) {
      panic("unable to synch privilege structure: %d", r);
  }

  /* Shallow copy. */
  *clone_rp = *rp;
  *clone_rpub = *rpub;

  /* Deep copy. */
  clone_rp->r_flags &= ~RS_ACTIVE; /* the clone is not active yet */
  clone_rp->r_pid = -1;            /* no pid yet */
  clone_rpub->endpoint = -1;       /* no endpoint yet */
  clone_rp->r_pub = clone_rpub;    /* restore pointer to public entry */
  build_cmd_dep(clone_rp);         /* rebuild cmd dependencies */
  if(clone_rpub->sys_flags & SF_USE_COPY) {
      share_exec(clone_rp, rp);        /* share exec image */
  }
  clone_rp->r_old_rp = NULL;	   /* no old version yet */
  clone_rp->r_new_rp = NULL;	   /* no new version yet */
  clone_rp->r_prev_rp = NULL;	   /* no prev replica yet */
  clone_rp->r_next_rp = NULL;	   /* no next replica yet */

  /* Force dynamic privilege id. */
  clone_rp->r_priv.s_flags |= DYN_PRIV_ID;

  /* Clear instance flags. */
  clone_rp->r_priv.s_flags &= ~(LU_SYS_PROC | RST_SYS_PROC);

  *clone_rpp = clone_rp;
  return OK;
}

/*===========================================================================*
 *			    swap_slot_pointer				     *
 *===========================================================================*/
static void swap_slot_pointer(struct rproc **rpp, struct rproc *src_rp,
    struct rproc *dst_rp)
{
  if(*rpp == src_rp) {
      *rpp = dst_rp;
  }
  else if(*rpp == dst_rp) {
      *rpp = src_rp;
  }
}

/*===========================================================================*
 *				swap_slot				     *
 *===========================================================================*/
void swap_slot(src_rpp, dst_rpp)
struct rproc **src_rpp;
struct rproc **dst_rpp;
{
/* Swap two service slots. */
  struct rproc *src_rp;
  struct rproc *dst_rp;
  struct rprocpub *src_rpub;
  struct rprocpub *dst_rpub;
  struct rproc orig_src_rproc, orig_dst_rproc;
  struct rprocpub orig_src_rprocpub, orig_dst_rprocpub;

  src_rp = *src_rpp;
  dst_rp = *dst_rpp;
  src_rpub = src_rp->r_pub;
  dst_rpub = dst_rp->r_pub;

  /* Save existing data first. */
  orig_src_rproc = *src_rp;
  orig_src_rprocpub = *src_rpub;
  orig_dst_rproc = *dst_rp;
  orig_dst_rprocpub = *dst_rpub;

  /* Swap slots. */
  *src_rp = orig_dst_rproc;
  *src_rpub = orig_dst_rprocpub;
  *dst_rp = orig_src_rproc;
  *dst_rpub = orig_src_rprocpub;

  /* Restore public entries. */
  src_rp->r_pub = orig_src_rproc.r_pub;
  dst_rp->r_pub = orig_dst_rproc.r_pub;

  /* Rebuild command dependencies. */
  build_cmd_dep(src_rp);
  build_cmd_dep(dst_rp);

  /* Swap local slot pointers. */
  swap_slot_pointer(&src_rp->r_prev_rp, src_rp, dst_rp);
  swap_slot_pointer(&src_rp->r_next_rp, src_rp, dst_rp);
  swap_slot_pointer(&src_rp->r_old_rp, src_rp, dst_rp);
  swap_slot_pointer(&src_rp->r_new_rp, src_rp, dst_rp);
  swap_slot_pointer(&dst_rp->r_prev_rp, src_rp, dst_rp);
  swap_slot_pointer(&dst_rp->r_next_rp, src_rp, dst_rp);
  swap_slot_pointer(&dst_rp->r_old_rp, src_rp, dst_rp);
  swap_slot_pointer(&dst_rp->r_new_rp, src_rp, dst_rp);

  /* Swap global slot pointers. */
  swap_slot_pointer(&rupdate.rp, src_rp, dst_rp);
  swap_slot_pointer(&rproc_ptr[_ENDPOINT_P(src_rp->r_pub->endpoint)],
      src_rp, dst_rp);
  swap_slot_pointer(&rproc_ptr[_ENDPOINT_P(dst_rp->r_pub->endpoint)],
      src_rp, dst_rp);

  /* Adjust input pointers. */
  *src_rpp = dst_rp;
  *dst_rpp = src_rp;
}

/*===========================================================================*
 *			   lookup_slot_by_label				     *
 *===========================================================================*/
struct rproc* lookup_slot_by_label(char *label)
{
/* Lookup a service slot matching the given label. */
  int slot_nr;
  struct rproc *rp;
  struct rprocpub *rpub;

  for (slot_nr = 0; slot_nr < NR_SYS_PROCS; slot_nr++) {
      rp = &rproc[slot_nr];
      if (!(rp->r_flags & RS_ACTIVE)) {
          continue;
      }
      rpub = rp->r_pub;
      if (strcmp(rpub->label, label) == 0) {
          return rp;
      }
  }

  return NULL;
}

/*===========================================================================*
 *			   lookup_slot_by_pid				     *
 *===========================================================================*/
struct rproc* lookup_slot_by_pid(pid_t pid)
{
/* Lookup a service slot matching the given pid. */
  int slot_nr;
  struct rproc *rp;

  if(pid < 0) {
      return NULL;
  }

  for (slot_nr = 0; slot_nr < NR_SYS_PROCS; slot_nr++) {
      rp = &rproc[slot_nr];
      if (!(rp->r_flags & RS_IN_USE)) {
          continue;
      }
      if (rp->r_pid == pid) {
          return rp;
      }
  }

  return NULL;
}

/*===========================================================================*
 *			   lookup_slot_by_dev_nr			     *
 *===========================================================================*/
struct rproc* lookup_slot_by_dev_nr(dev_t dev_nr)
{
/* Lookup a service slot matching the given device number. */
  int slot_nr;
  struct rproc *rp;
  struct rprocpub *rpub;

  if(dev_nr <= 0) {
      return NULL;
  }

  for (slot_nr = 0; slot_nr < NR_SYS_PROCS; slot_nr++) {
      rp = &rproc[slot_nr];
      rpub = rp->r_pub;
      if (!(rp->r_flags & RS_IN_USE)) {
          continue;
      }
      if (rpub->dev_nr == dev_nr) {
          return rp;
      }
  }

  return NULL;
}

/*===========================================================================*
 *			   lookup_slot_by_flags				     *
 *===========================================================================*/
struct rproc* lookup_slot_by_flags(int flags)
{
/* Lookup a service slot matching the given flags. */
  int slot_nr;
  struct rproc *rp;

  if(!flags) {
      return NULL;
  }

  for (slot_nr = 0; slot_nr < NR_SYS_PROCS; slot_nr++) {
      rp = &rproc[slot_nr];
      if (!(rp->r_flags & RS_IN_USE)) {
          continue;
      }
      if (rp->r_flags & flags) {
          return rp;
      }
  }

  return NULL;
}

/*===========================================================================*
 *				alloc_slot				     *
 *===========================================================================*/
int alloc_slot(rpp)
struct rproc **rpp;
{
/* Alloc a new system service slot. */
  int slot_nr;

  for (slot_nr = 0; slot_nr < NR_SYS_PROCS; slot_nr++) {
      *rpp = &rproc[slot_nr];			/* get pointer to slot */
      if (!((*rpp)->r_flags & RS_IN_USE)) 	/* check if available */
	  break;
  }
  if (slot_nr >= NR_SYS_PROCS) {
	return ENOMEM;
  }

  return OK;
}

/*===========================================================================*
 *				free_slot				     *
 *===========================================================================*/
void free_slot(rp)
struct rproc *rp;
{
/* Free a system service slot. */
  struct rprocpub *rpub;

  rpub = rp->r_pub;

  /* Send a late reply if there is any pending. */
  late_reply(rp, OK);

  /* Free memory if necessary. */
  if(rpub->sys_flags & SF_USE_COPY) {
      free_exec(rp);
  }

  /* Mark slot as no longer in use.. */
  rp->r_flags = 0;
  rp->r_pid = -1;
  rpub->in_use = FALSE;
  rproc_ptr[_ENDPOINT_P(rpub->endpoint)] = NULL;
}


/*===========================================================================*
 *				get_next_name				     *
 *===========================================================================*/
static char *get_next_name(ptr, name, caller_label)
char *ptr;
char *name;
char *caller_label;
{
	/* Get the next name from the list of (IPC) program names.
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
	"rs:get_next_name: bad ipc list entry '%.*s' for %s: too long\n",
				len, p, caller_label);
			continue;
		}
		memcpy(name, p, len);
		name[len]= '\0';

		return q; /* found another */
	}

	return NULL; /* done */
}

/*===========================================================================*
 *				add_forward_ipc				     *
 *===========================================================================*/
void add_forward_ipc(rp, privp)
struct rproc *rp;
struct priv *privp;
{
	/* Add IPC send permissions to a process based on that process's IPC
	 * list.
	 */
	char name[RS_MAX_LABEL_LEN+1], *p;
	struct rproc *rrp;
	endpoint_t endpoint;
	int r;
	int priv_id;
	struct priv priv;
	struct rprocpub *rpub;

	rpub = rp->r_pub;
	p = rp->r_ipc_list;

	while ((p = get_next_name(p, name, rpub->label)) != NULL) {

		if (strcmp(name, "SYSTEM") == 0)
			endpoint= SYSTEM;
		else if (strcmp(name, "USER") == 0)
			endpoint= INIT_PROC_NR; /* all user procs */
		else
		{
			/* Set a privilege bit for every process matching the
			 * given process name. It is perfectly fine if this
			 * loop does not find any matches, as the target
			 * process(es) may not have been started yet. See
			 * add_backward_ipc() below.
			 */
			for (rrp=BEG_RPROC_ADDR; rrp<END_RPROC_ADDR; rrp++) {
				if (!(rrp->r_flags & RS_IN_USE))
					continue;

				if (!strcmp(rrp->r_pub->proc_name, name)) {
#if PRIV_DEBUG
					printf("  RS: add_forward_ipc: setting"
						" sendto bit for %d...\n",
						rrp->r_pub->endpoint);
#endif

					priv_id= rrp->r_priv.s_id;
					set_sys_bit(privp->s_ipc_to, priv_id);
				}
			}

			continue;
		}

		/* This code only applies to the exception cases. */
		if ((r = sys_getpriv(&priv, endpoint)) < 0)
		{
			printf(
		"add_forward_ipc: unable to get priv_id for '%s': %d\n",
				name, r);
			continue;
		}

#if PRIV_DEBUG
		printf("  RS: add_forward_ipc: setting sendto bit for %d...\n",
			endpoint);
#endif
		priv_id= priv.s_id;
		set_sys_bit(privp->s_ipc_to, priv_id);
	}
}


/*===========================================================================*
 *				add_backward_ipc			     *
 *===========================================================================*/
void add_backward_ipc(rp, privp)
struct rproc *rp;
struct priv *privp;
{
	/* Add IPC send permissions to a process based on other processes' IPC
	 * lists. This is enough to allow each such two processes to talk to
	 * each other, as the kernel guarantees send mask symmetry. We need to
	 * add these permissions now because the current process may not yet
	 * have existed at the time that the other process was initialized.
	 */
	char name[RS_MAX_LABEL_LEN+1], *p;
	struct rproc *rrp;
	struct rprocpub *rrpub;
	char *proc_name;
	int priv_id, is_ipc_all, is_ipc_all_sys;

	proc_name = rp->r_pub->proc_name;

	for (rrp=BEG_RPROC_ADDR; rrp<END_RPROC_ADDR; rrp++) {
		if (!(rrp->r_flags & RS_IN_USE))
			continue;

		if (!rrp->r_ipc_list[0])
			continue;

		/* If the process being checked is set to allow IPC to all
		 * other processes, or for all other system processes and the
		 * target process is a system process, add a permission bit.
		 */
		rrpub = rrp->r_pub;

		is_ipc_all = !strcmp(rrp->r_ipc_list, RSS_IPC_ALL);
		is_ipc_all_sys = !strcmp(rrp->r_ipc_list, RSS_IPC_ALL_SYS);

		if (is_ipc_all ||
			(is_ipc_all_sys && (privp->s_flags & SYS_PROC))) {
#if PRIV_DEBUG
			printf("  RS: add_backward_ipc: setting sendto bit "
				"for %d...\n", rrpub->endpoint);
#endif
			priv_id= rrp->r_priv.s_id;
			set_sys_bit(privp->s_ipc_to, priv_id);

			continue;
		}

		/* An IPC target list was provided for the process being
		 * checked here. Make sure that the name of the new process
		 * is in that process's list. There may be multiple matches.
		 */
		p = rrp->r_ipc_list;

		while ((p = get_next_name(p, name, rrpub->label)) != NULL) {
			if (!strcmp(proc_name, name)) {
#if PRIV_DEBUG
				printf("  RS: add_backward_ipc: setting sendto"
					" bit for %d...\n",
					rrpub->endpoint);
#endif
				priv_id= rrp->r_priv.s_id;
				set_sys_bit(privp->s_ipc_to, priv_id);
			}
		}
	}
}


/*===========================================================================*
 *				init_privs				     *
 *===========================================================================*/
void init_privs(rp, privp)
struct rproc *rp;
struct priv *privp;
{
	int i;
	int is_ipc_all, is_ipc_all_sys;

	/* Clear s_ipc_to */
	fill_send_mask(&privp->s_ipc_to, FALSE);

	is_ipc_all = !strcmp(rp->r_ipc_list, RSS_IPC_ALL);
	is_ipc_all_sys = !strcmp(rp->r_ipc_list, RSS_IPC_ALL_SYS);

#if PRIV_DEBUG
	printf("  RS: init_privs: ipc list is '%s'...\n", rp->r_ipc_list);
#endif

	if (!is_ipc_all && !is_ipc_all_sys)
	{
		add_forward_ipc(rp, privp);
		add_backward_ipc(rp, privp);

	}
	else
	{
		for (i= 0; i<NR_SYS_PROCS; i++)
		{
			if (is_ipc_all || i != USER_PRIV_ID)
				set_sys_bit(privp->s_ipc_to, i);
		}
	}
}

