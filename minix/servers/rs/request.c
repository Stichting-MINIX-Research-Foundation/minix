/*
 * Changes:
 *   Jan 22, 2010:  Created  (Cristiano Giuffrida)
 */

#include "inc.h"

#include "kernel/proc.h"

static int check_request(struct rs_start *rs_start);

/*===========================================================================*
 *				   do_up				     *
 *===========================================================================*/
int do_up(m_ptr)
message *m_ptr;					/* request message pointer */
{
/* A request was made to start a new system service. */
  struct rproc *rp;
  struct rprocpub *rpub;
  int r;
  struct rs_start rs_start;
  int noblock;

  /* Check if the call can be allowed. */
  if((r = check_call_permission(m_ptr->m_source, RS_UP, NULL)) != OK)
      return r;

  /* Allocate a new system service slot. */
  r = alloc_slot(&rp);
  if(r != OK) {
      printf("RS: do_up: unable to allocate a new slot: %d\n", r);
      return r;
  }
  rpub = rp->r_pub;

  /* Copy the request structure. */
  r = copy_rs_start(m_ptr->m_source, m_ptr->m_rs_req.addr, &rs_start);
  if (r != OK) {
      return r;
  }
  r = check_request(&rs_start);
  if (r != OK) {
      return r;
  }
  noblock = (rs_start.rss_flags & RSS_NOBLOCK);

  /* Initialize the slot as requested. */
  r = init_slot(rp, &rs_start, m_ptr->m_source);
  if(r != OK) {
      printf("RS: do_up: unable to init the new slot: %d\n", r);
      return r;
  }

  /* Check for duplicates */
  if(lookup_slot_by_label(rpub->label)) {
      printf("RS: service with the same label '%s' already exists\n",
          rpub->label);
      return EBUSY;
  }
  if(rpub->dev_nr>0 && lookup_slot_by_dev_nr(rpub->dev_nr)) {
      printf("RS: service with the same device number %d already exists\n",
          rpub->dev_nr);
      return EBUSY;
  }

  /* All information was gathered. Now try to start the system service. */
  r = start_service(rp);
  if(r != OK) {
      return r;
  }

  /* Unblock the caller immediately if requested. */
  if(noblock) {
      return OK;
  }

  /* Late reply - send a reply when service completes initialization. */
  rp->r_flags |= RS_LATEREPLY;
  rp->r_caller = m_ptr->m_source;
  rp->r_caller_request = RS_UP;

  return EDONTREPLY;
}

/*===========================================================================*
 *				do_down					     *
 *===========================================================================*/
int do_down(message *m_ptr)
{
  register struct rproc *rp;
  int s;
  char label[RS_MAX_LABEL_LEN];

  /* Copy label. */
  s = copy_label(m_ptr->m_source, m_ptr->m_rs_req.addr,
      m_ptr->m_rs_req.len, label, sizeof(label));
  if(s != OK) {
      return s;
  }

  /* Lookup slot by label. */
  rp = lookup_slot_by_label(label);
  if(!rp) {
      if(rs_verbose)
          printf("RS: do_down: service '%s' not found\n", label);
      return(ESRCH);
  }

  /* Check if the call can be allowed. */
  if((s = check_call_permission(m_ptr->m_source, RS_DOWN, rp)) != OK)
      return s;

  /* Stop service. */
  if (rp->r_flags & RS_TERMINATED) {
        /* A recovery script is requesting us to bring down the service.
         * The service is already gone, simply perform cleanup.
         */
        if(rs_verbose)
            printf("RS: recovery script performs service down...\n");
  	unpublish_service(rp);
  	cleanup_service(rp);
    	return(OK);
  }
  stop_service(rp,RS_EXITING);

  /* Late reply - send a reply when service dies. */
  rp->r_flags |= RS_LATEREPLY;
  rp->r_caller = m_ptr->m_source;
  rp->r_caller_request = RS_DOWN;

  return EDONTREPLY;
}

/*===========================================================================*
 *				do_restart				     *
 *===========================================================================*/
int do_restart(message *m_ptr)
{
  struct rproc *rp;
  int s, r;
  char label[RS_MAX_LABEL_LEN];
  char script[MAX_SCRIPT_LEN];

  /* Copy label. */
  s = copy_label(m_ptr->m_source, m_ptr->m_rs_req.addr,
      m_ptr->m_rs_req.len, label, sizeof(label));
  if(s != OK) {
      return s;
  }

  /* Lookup slot by label. */
  rp = lookup_slot_by_label(label);
  if(!rp) {
      if(rs_verbose)
          printf("RS: do_restart: service '%s' not found\n", label);
      return(ESRCH);
  }

  /* Check if the call can be allowed. */
  if((r = check_call_permission(m_ptr->m_source, RS_RESTART, rp)) != OK)
      return r;

  /* We can only be asked to restart a service from a recovery script. */
  if (! (rp->r_flags & RS_TERMINATED) ) {
      if(rs_verbose)
          printf("RS: %s is still running\n", srv_to_string(rp));
      return EBUSY;
  }

  if(rs_verbose)
      printf("RS: recovery script performs service restart...\n");

  /* Restart the service, but make sure we don't call the script again. */
  strcpy(script, rp->r_script);
  rp->r_script[0] = '\0';
  restart_service(rp);
  strcpy(rp->r_script, script);

  return OK;
}

/*===========================================================================*
 *				do_clone				     *
 *===========================================================================*/
int do_clone(message *m_ptr)
{
  struct rproc *rp;
  struct rprocpub *rpub;
  int s, r;
  char label[RS_MAX_LABEL_LEN];

  /* Copy label. */
  s = copy_label(m_ptr->m_source, m_ptr->m_rs_req.addr,
      m_ptr->m_rs_req.len, label, sizeof(label));
  if(s != OK) {
      return s;
  }

  /* Lookup slot by label. */
  rp = lookup_slot_by_label(label);
  if(!rp) {
      if(rs_verbose)
          printf("RS: do_clone: service '%s' not found\n", label);
      return(ESRCH);
  }
  rpub = rp->r_pub;

  /* Check if the call can be allowed. */
  if((r = check_call_permission(m_ptr->m_source, RS_CLONE, rp)) != OK)
      return r;

  /* Don't clone if a replica is already available. */
  if(rp->r_next_rp) {
      return EEXIST;
  }

  /* Clone the service as requested. */
  rpub->sys_flags |= SF_USE_REPL;
  if ((r = clone_service(rp, RST_SYS_PROC)) != OK) {
      rpub->sys_flags &= ~SF_USE_REPL;
      return r;
  }

  return OK;
}

/*===========================================================================*
 *				    do_edit				     *
 *===========================================================================*/
int do_edit(message *m_ptr)
{
  struct rproc *rp;
  struct rprocpub *rpub;
  struct rs_start rs_start;
  int r;
  char label[RS_MAX_LABEL_LEN];

  /* Copy the request structure. */
  r = copy_rs_start(m_ptr->m_source, m_ptr->m_rs_req.addr, &rs_start);
  if (r != OK) {
      return r;
  }

  /* Copy label. */
  r = copy_label(m_ptr->m_source, rs_start.rss_label.l_addr,
      rs_start.rss_label.l_len, label, sizeof(label));
  if(r != OK) {
      return r;
  }

  /* Lookup slot by label. */
  rp = lookup_slot_by_label(label);
  if(!rp) {
      if(rs_verbose)
          printf("RS: do_edit: service '%s' not found\n", label);
      return ESRCH;
  }
  rpub = rp->r_pub;

  /* Check if the call can be allowed. */
  if((r = check_call_permission(m_ptr->m_source, RS_EDIT, rp)) != OK)
      return r;

  if(rs_verbose)
      printf("RS: %s edits settings\n", srv_to_string(rp));

  /* Synch the privilege structure with the kernel. */
  if ((r = sys_getpriv(&rp->r_priv, rpub->endpoint)) != OK) {
      printf("RS: do_edit: unable to synch privilege structure: %d\n", r);
      return r;
  }

  /* Tell scheduler this process is finished */
  if ((r = sched_stop(rp->r_scheduler, rpub->endpoint)) != OK) {
      printf("RS: do_edit: scheduler won't give up process: %d\n", r);
      return r;
  }

  /* Edit the slot as requested. */
  if((r = edit_slot(rp, &rs_start, m_ptr->m_source)) != OK) {
      printf("RS: do_edit: unable to edit the existing slot: %d\n", r);
      return r;
  }

  /* Update privilege structure. */
  r = sys_privctl(rpub->endpoint, SYS_PRIV_UPDATE_SYS, &rp->r_priv);
  if(r != OK) {
      printf("RS: do_edit: unable to update privilege structure: %d\n", r);
      return r;
  }

  /* Update VM calls. */
  if ((r = vm_set_priv(rpub->endpoint, &rpub->vm_call_mask[0],
    !!(rp->r_priv.s_flags & SYS_PROC))) != OK) {
      printf("RS: do_edit: failed: %d\n", r);
      return r;
  }

  /* Reinitialize scheduling. */
  if ((r = sched_init_proc(rp)) != OK) {
      printf("RS: do_edit: unable to reinitialize scheduling: %d\n", r);
      return r;
  }

  /* Cleanup old replicas and create a new one, if necessary. */
  if(rpub->sys_flags & SF_USE_REPL) {
      if(rp->r_next_rp) {
          cleanup_service(rp->r_next_rp);
          rp->r_next_rp = NULL;
      }
      if ((r = clone_service(rp, RST_SYS_PROC)) != OK) {
          printf("RS: warning: unable to clone %s\n", srv_to_string(rp));
      }
  }

  return OK;
}

/*===========================================================================*
 *				do_refresh				     *
 *===========================================================================*/
int do_refresh(message *m_ptr)
{
  register struct rproc *rp;
  int s;
  char label[RS_MAX_LABEL_LEN];

  /* Copy label. */
  s = copy_label(m_ptr->m_source, m_ptr->m_rs_req.addr,
      m_ptr->m_rs_req.len, label, sizeof(label));
  if(s != OK) {
      return s;
  }

  /* Lookup slot by label. */
  rp = lookup_slot_by_label(label);
  if(!rp) {
      if(rs_verbose)
          printf("RS: do_refresh: service '%s' not found\n", label);
      return(ESRCH);
  }

  /* Check if the call can be allowed. */
  if((s = check_call_permission(m_ptr->m_source, RS_REFRESH, rp)) != OK)
      return s;

  /* Refresh service. */
  if(rs_verbose)
      printf("RS: %s refreshing\n", srv_to_string(rp));
  stop_service(rp,RS_REFRESHING);

  return OK;
}

/*===========================================================================*
 *				do_shutdown				     *
 *===========================================================================*/
int do_shutdown(message *m_ptr)
{
  int slot_nr;
  struct rproc *rp;
  int r;

  /* Check if the call can be allowed. */
  if (m_ptr != NULL) {
      if((r = check_call_permission(m_ptr->m_source, RS_SHUTDOWN, NULL)) != OK)
          return r;
  }

  if(rs_verbose)
      printf("RS: shutting down...\n");

  /* Set flag to tell RS we are shutting down. */
  shutting_down = TRUE;

  /* Don't restart dead services. */
  for (slot_nr = 0; slot_nr < NR_SYS_PROCS; slot_nr++) {
      rp = &rproc[slot_nr];
      if (rp->r_flags & RS_IN_USE) {
          rp->r_flags |= RS_EXITING;
      }
  }
  return(OK);
}

/*===========================================================================*
 *				do_init_ready				     *
 *===========================================================================*/
int do_init_ready(message *m_ptr)
{
  int who_p;
  message m;
  struct rproc *rp;
  struct rprocpub *rpub;
  int result, is_rs;
  int r;

  is_rs = (m_ptr->m_source == RS_PROC_NR);
  who_p = _ENDPOINT_P(m_ptr->m_source);
  result = m_ptr->m_rs_init.result;

  /* Check for RS failing initialization first. */
  if(is_rs && result != OK) {
      return result;
  }

  rp = rproc_ptr[who_p];
  rpub = rp->r_pub;

  /* Make sure the originating service was requested to initialize. */
  if(! (rp->r_flags & RS_INITIALIZING) ) {
      if(rs_verbose)
          printf("RS: do_init_ready: got unexpected init ready msg from %d\n",
              m_ptr->m_source);
      return EINVAL;
  }

  /* Check if something went wrong and the service failed to init.
   * In that case, kill the service.
   */
  if(result != OK) {
      if(rs_verbose)
          printf("RS: %s initialization error: %s\n", srv_to_string(rp),
              init_strerror(result));
      if (result == ERESTART)
          rp->r_flags |= RS_REINCARNATE;
      crash_service(rp); /* simulate crash */
      return EDONTREPLY;
  }

  /* Mark the slot as no longer initializing. */
  rp->r_flags &= ~RS_INITIALIZING;
  rp->r_check_tm = 0;
  getticks(&rp->r_alive_tm);

  /* Reply and unblock the service before doing anything else. */
  m.m_type = OK;
  reply(rpub->endpoint, rp, &m);

  /* See if a late reply has to be sent. */
  late_reply(rp, OK);

  if(rs_verbose)
      printf("RS: %s initialized\n", srv_to_string(rp));

  /* If the service has completed initialization after a live
   * update, end the update now.
   */
  if(rp->r_flags & RS_UPDATING) {
      printf("RS: update succeeded\n");
      end_update(OK, RS_DONTREPLY);
  }

  /* If the service has completed initialization after a crash
   * make the new instance active and cleanup the old replica.
   */
  if(rp->r_prev_rp) {
      cleanup_service(rp->r_prev_rp);
      rp->r_prev_rp = NULL;
      rp->r_restarts += 1;

      if(rs_verbose)
          printf("RS: %s completed restart\n", srv_to_string(rp));
  }

  /* If we must keep a replica of this system service, create it now. */
  if(rpub->sys_flags & SF_USE_REPL) {
      if ((r = clone_service(rp, RST_SYS_PROC)) != OK) {
          printf("RS: warning: unable to clone %s\n", srv_to_string(rp));
      }
  }

  return is_rs ? OK : EDONTREPLY; /* return what the caller expects */
}

/*===========================================================================*
 *				do_update				     *
 *===========================================================================*/
int do_update(message *m_ptr)
{
  struct rproc *rp;
  struct rproc *new_rp;
  struct rprocpub *rpub;
  struct rs_start rs_start;
  int noblock, do_self_update;
  int s;
  char label[RS_MAX_LABEL_LEN];
  int lu_state;
  int prepare_maxtime;

  /* Copy the request structure. */
  s = copy_rs_start(m_ptr->m_source, m_ptr->m_rs_req.addr, &rs_start);
  if (s != OK) {
      return s;
  }
  noblock = (rs_start.rss_flags & RSS_NOBLOCK);
  do_self_update = (rs_start.rss_flags & RSS_SELF_LU);
  s = check_request(&rs_start);
  if (s != OK) {
      return s;
  }

  /* Copy label. */
  s = copy_label(m_ptr->m_source, rs_start.rss_label.l_addr,
      rs_start.rss_label.l_len, label, sizeof(label));
  if(s != OK) {
      return s;
  }

  /* Lookup slot by label. */
  rp = lookup_slot_by_label(label);
  if(!rp) {
      if(rs_verbose)
          printf("RS: do_update: service '%s' not found\n", label);
      return ESRCH;
  }
  rpub = rp->r_pub;

  /* Check if the call can be allowed. */
  if((s = check_call_permission(m_ptr->m_source, RS_UPDATE, rp)) != OK)
      return s;

  /* Retrieve live update state. */
  lu_state = m_ptr->m_rs_update.state;
  if(lu_state == SEF_LU_STATE_NULL) {
      return(EINVAL);
  }

  /* Retrieve prepare max time. */
  prepare_maxtime = m_ptr->m_rs_update.prepare_maxtime;
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
      if(rs_verbose)
	  printf("RS: do_update: an update is already in progress\n");
      return EBUSY;
  }

  /* A self update live updates a service instance into a replica, a regular
   * update live updates a service instance into a new version, as specified
   * by the given binary.
   */
  if(do_self_update) {
      if(rs_verbose)
          printf("RS: %s performs self update\n", srv_to_string(rp));

      /* Clone the system service and use the replica as the new version. */
      s = clone_service(rp, LU_SYS_PROC);
      if(s != OK) {
          printf("RS: do_update: unable to clone service: %d\n", s);
          return s;
      }
  }
  else {
      if(rs_verbose)
          printf("RS: %s performs regular update\n", srv_to_string(rp));

      /* Allocate a system service slot for the new version. */
      s = alloc_slot(&new_rp);
      if(s != OK) {
          printf("RS: do_update: unable to allocate a new slot: %d\n", s);
          return s;
      }

      /* Initialize the slot as requested. */
      s = init_slot(new_rp, &rs_start, m_ptr->m_source);
      if(s != OK) {
          printf("RS: do_update: unable to init the new slot: %d\n", s);
          return s;
      }

      /* Let the new version inherit defaults from the old one. */
      inherit_service_defaults(rp, new_rp);

      /* Link the two versions. */
      rp->r_new_rp = new_rp;
      new_rp->r_old_rp = rp;

      /* Create new version of the service but don't let it run. */
      new_rp->r_priv.s_flags |= LU_SYS_PROC;
      s = create_service(new_rp);
      if(s != OK) {
          printf("RS: do_update: unable to create a new service: %d\n", s);
          return s;
      }
  }

  /* Mark both versions as updating. */
  rp->r_flags |= RS_UPDATING;
  rp->r_new_rp->r_flags |= RS_UPDATING;
  rupdate.flags |= RS_UPDATING;
  getticks(&rupdate.prepare_tm);
  rupdate.prepare_maxtime = prepare_maxtime;
  rupdate.rp = rp;

  if(rs_verbose)
    printf("RS: %s updating\n", srv_to_string(rp));

  /* If RS is updating, set up signal managers for the new instance.
   * The current RS instance must be made the backup signal manager to
   * support rollback in case of a crash during initialization.
   */
  if(rp->r_priv.s_flags & ROOT_SYS_PROC) {
      new_rp = rp->r_new_rp;

      s = update_sig_mgrs(new_rp, SELF, new_rp->r_pub->endpoint);
      if(s != OK) {
          cleanup_service(new_rp);
          return s;
      }
  }

  if(noblock) {
      /* Unblock the caller immediately if requested. */
      m_ptr->m_type = OK;
      reply(m_ptr->m_source, NULL, m_ptr);
  }
  else {
      /* Send a reply when the new version completes initialization. */
      rp->r_flags |= RS_LATEREPLY;
      rp->r_caller = m_ptr->m_source;
      rp->r_caller_request = RS_UPDATE;
  }

  /* Request to update. */
  m_ptr->m_type = RS_LU_PREPARE;
  if(rpub->endpoint == RS_PROC_NR) {
      /* RS can process the request directly. */
      do_sef_lu_request(m_ptr);
  }
  else {
      /* Send request message to the system service. */
      asynsend3(rpub->endpoint, m_ptr, AMF_NOREPLY);
  }

  return EDONTREPLY;
}

/*===========================================================================*
 *				do_upd_ready				     *
 *===========================================================================*/
int do_upd_ready(message *m_ptr)
{
  struct rproc *rp, *old_rp, *new_rp;
  int who_p;
  int result;
  int is_rs;
  int r;

  who_p = _ENDPOINT_P(m_ptr->m_source);
  rp = rproc_ptr[who_p];
  result = m_ptr->m_rs_update.result;
  is_rs = (m_ptr->m_source == RS_PROC_NR);

  /* Make sure the originating service was requested to prepare for update. */
  if(rp != rupdate.rp) {
      if(rs_verbose)
          printf("RS: do_upd_ready: got unexpected update ready msg from %d\n",
              m_ptr->m_source);
      return EINVAL;
  }

  /* Check if something went wrong and the service failed to prepare
   * for the update. In that case, end the update process. The old version will
   * be replied to and continue executing.
   */
  if(result != OK) {
      end_update(result, RS_REPLY);

      printf("RS: update failed: %s\n", lu_strerror(result));
      return is_rs ? result : EDONTREPLY; /* return what the caller expects */
  }

  old_rp = rp;
  new_rp = rp->r_new_rp;

  /* If RS itself is updating, yield control to the new version immediately. */
  if(is_rs) {
      r = init_service(new_rp, SEF_INIT_LU);
      if(r != OK) {
          panic("unable to initialize the new RS instance: %d", r);
      }
      r = sys_privctl(new_rp->r_pub->endpoint, SYS_PRIV_YIELD, NULL);
      if(r != OK) {
          panic("unable to yield control to the new RS instance: %d", r);
      }
      /* If we get this far, the new version failed to initialize. Rollback. */
      r = srv_update(RS_PROC_NR, new_rp->r_pub->endpoint);
      assert(r == OK); /* can't fail */
      end_update(ERESTART, RS_REPLY);
      return ERESTART;
  }

  /* Perform the update. */
  r = update_service(&old_rp, &new_rp, RS_SWAP);
  if(r != OK) {
      end_update(r, RS_REPLY);
      printf("RS: update failed: error %d\n", r);
      return EDONTREPLY;
  }

  /* Let the new version run. */
  r = run_service(new_rp, SEF_INIT_LU);
  if(r != OK) {
      /* Something went wrong. Rollback. */
      r = update_service(&new_rp, &old_rp, RS_SWAP);
      assert(r == OK); /* can't fail */
      end_update(r, RS_REPLY);
      printf("RS: update failed: error %d\n", r);
      return EDONTREPLY;
  }

  return EDONTREPLY;
}

/*===========================================================================*
 *				do_period				     *
 *===========================================================================*/
void do_period(m_ptr)
message *m_ptr;
{
  register struct rproc *rp;
  register struct rprocpub *rpub;
  clock_t now = m_ptr->m_notify.timestamp;
  int s;
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
      if ((rp->r_flags & RS_ACTIVE) && !(rp->r_flags & RS_UPDATING)) {

          /* Compute period. */
          period = rp->r_period;
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
		  restart_service(rp);
	      }
	  }

	  /* If the service was signaled with a SIGTERM and fails to respond,
	   * kill the system service with a SIGKILL signal.
	   */
	  else if (rp->r_stop_tm > 0 && now - rp->r_stop_tm > 2*RS_DELTA_T
	   && rp->r_pid > 0) {
              rp->r_stop_tm = 0;
              crash_service(rp); /* simulate crash */
	  }

	  /* There seems to be no special conditions. If the service has a 
	   * period assigned check its status. 
	   */
	  else if (period > 0) {

	      /* Check if an answer to a status request is still pending. If 
	       * the service didn't respond within time, kill it to simulate 
	       * a crash. The failure will be detected and the service will 
	       * be restarted automatically. Give the service a free pass if
	       * somebody is initializing. There may be some weird dependencies
	       * if another service is, for example, restarting at the same
	       * time.
	       */
              if (rp->r_alive_tm < rp->r_check_tm) { 
	          if (now - rp->r_alive_tm > 2*period &&
		      rp->r_pid > 0 && !(rp->r_flags & RS_NOPINGREPLY)) { 
		      if(rs_verbose)
                           printf("RS: %s reported late\n", srv_to_string(rp)); 
		      if(lookup_slot_by_flags(RS_INITIALIZING)) {
                           /* Skip for now. */
                           if(rs_verbose)
                               printf("RS: %s gets a free pass\n",
                                   srv_to_string(rp)); 
                           rp->r_alive_tm = now;
                           rp->r_check_tm = now+1;
                           continue;
		      }
		      rp->r_flags |= RS_NOPINGREPLY;
                      crash_service(rp); /* simulate crash */
		  }
	      }

	      /* No answer pending. Check if a period expired since the last
	       * check and, if so request the system service's status.
	       */
	      else if (now - rp->r_check_tm > rp->r_period) {
  		  ipc_notify(rpub->endpoint);		/* request status */
		  rp->r_check_tm = now;			/* mark time */
              }
          }
      }
  }

  /* Reschedule a synchronous alarm for the next period. */
  if (OK != (s=sys_setalarm(RS_DELTA_T, 0)))
      panic("couldn't set alarm: %d", s);
}

/*===========================================================================*
 *			          do_sigchld				     *
 *===========================================================================*/
void do_sigchld()
{
/* PM informed us that there are dead children to cleanup. Go get them. */
  pid_t pid;
  int status;
  struct rproc *rp;
  struct rproc **rps;
  int i, nr_rps;

  if(rs_verbose)
     printf("RS: got SIGCHLD signal, cleaning up dead children\n");

  while ( (pid = waitpid(-1, &status, WNOHANG)) != 0 ) {
      rp = lookup_slot_by_pid(pid);
      if(rp != NULL) {

          if(rs_verbose)
              printf("RS: %s exited via another signal manager\n",
                  srv_to_string(rp));

          /* The slot is still there. This means RS is not the signal
           * manager assigned to the process. Ignore the event but
           * free slots for all the service instances and send a late
           * reply if necessary.
           */
          get_service_instances(rp, &rps, &nr_rps);
          for(i=0;i<nr_rps;i++) {
              if(rupdate.flags & RS_UPDATING) {
                  rupdate.flags &= ~RS_UPDATING;
              }
              free_slot(rps[i]);
          }
      }
  }
}

/*===========================================================================*
 *				do_getsysinfo				     *
 *===========================================================================*/
int do_getsysinfo(m_ptr)
message *m_ptr;
{
  vir_bytes src_addr, dst_addr;
  int dst_proc;
  size_t len;
  int s;

  /* Check if the call can be allowed. */
  if((s = check_call_permission(m_ptr->m_source, 0, NULL)) != OK)
      return s;

  switch(m_ptr->m_lsys_getsysinfo.what) {
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

  if (len != m_ptr->m_lsys_getsysinfo.size)
	return(EINVAL);

  dst_proc = m_ptr->m_source;
  dst_addr = m_ptr->m_lsys_getsysinfo.where;
  return sys_datacopy(SELF, src_addr, dst_proc, dst_addr, len);
}

/*===========================================================================*
 *				do_lookup				     *
 *===========================================================================*/
int do_lookup(m_ptr)
message *m_ptr;
{
	static char namebuf[100];
	int len, r;
	struct rproc *rrp;
	struct rprocpub *rrpub;

	len = m_ptr->m_rs_req.name_len;

	if(len < 2 || len >= sizeof(namebuf)) {
		printf("RS: len too weird (%d)\n", len);
		return EINVAL;
	}

	if((r=sys_datacopy(m_ptr->m_source, (vir_bytes) m_ptr->m_rs_req.name,
		SELF, (vir_bytes) namebuf, len)) != OK) {
		printf("RS: name copy failed\n");
		return r;

	}

	namebuf[len] = '\0';

	rrp = lookup_slot_by_label(namebuf);
	if(!rrp) {
		return ESRCH;
	}
	rrpub = rrp->r_pub;
	m_ptr->m_rs_req.endpoint = rrpub->endpoint;

	return OK;
}

/*===========================================================================*
 *				   check_request			     *
 *===========================================================================*/
static int check_request(struct rs_start *rs_start)
{
  /* Verify scheduling parameters */
  if (rs_start->rss_scheduler != KERNEL && 
	(rs_start->rss_scheduler < 0 || 
	rs_start->rss_scheduler > LAST_SPECIAL_PROC_NR)) {
	printf("RS: check_request: invalid scheduler %d\n", 
		rs_start->rss_scheduler);
	return EINVAL;
  }
  if (rs_start->rss_priority >= NR_SCHED_QUEUES) {
	printf("RS: check_request: priority %u out of range\n", 
		rs_start->rss_priority);
	return EINVAL;
  }
  if (rs_start->rss_quantum <= 0) {
	printf("RS: check_request: quantum %u out of range\n", 
		rs_start->rss_quantum);
	return EINVAL;
  }

  if (rs_start->rss_cpu == RS_CPU_BSP)
	  rs_start->rss_cpu = machine.bsp_id;
  else if (rs_start->rss_cpu == RS_CPU_DEFAULT) {
	  /* keep the default value */
  } else if (rs_start->rss_cpu < 0)
	  return EINVAL;
  else if (rs_start->rss_cpu > machine.processors_count) {
	  printf("RS: cpu number %d out of range 0-%d, using BSP\n",
			  rs_start->rss_cpu, machine.processors_count);
	  rs_start->rss_cpu = machine.bsp_id;
  }

  /* Verify signal manager. */
  if (rs_start->rss_sigmgr != SELF && 
	(rs_start->rss_sigmgr < 0 || 
	rs_start->rss_sigmgr > LAST_SPECIAL_PROC_NR)) {
	printf("RS: check_request: invalid signal manager %d\n", 
		rs_start->rss_sigmgr);
	return EINVAL;
  }

  return OK;
}

