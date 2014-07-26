/* This file contains some utility routines for RS.
 *
 * Changes:
 *   Nov 22, 2009: Created    (Cristiano Giuffrida)
 */

#include "inc.h"

#include <assert.h>
#include <minix/sched.h>
#include "kernel/proc.h"

/*===========================================================================*
 *				 init_service				     *
 *===========================================================================*/
int init_service(rp, type)
struct rproc *rp;				/* pointer to process slot */
int type;					/* type of initialization */
{
  int r;
  message m;
  struct rprocpub *rpub;
  endpoint_t old_endpoint;

  rpub = rp->r_pub;

  rp->r_flags |= RS_INITIALIZING;              /* now initializing */
  rp->r_check_tm = rp->r_alive_tm + 1;         /* expect reply within period */

  /* In case of RS initialization, we are done. */
  if(rp->r_priv.s_flags & ROOT_SYS_PROC) {
      return OK;
  }

  /* Determine the old endpoint if this is a new instance. */
  old_endpoint = NONE;
  if(rp->r_old_rp) {
      old_endpoint = rp->r_old_rp->r_pub->endpoint;
  }
  else if(rp->r_prev_rp) {
      old_endpoint = rp->r_prev_rp->r_pub->endpoint;
  }

  /* Send initialization message. */
  m.m_type = RS_INIT;
  m.m_rs_init.type = type;
  m.m_rs_init.rproctab_gid = rinit.rproctab_gid;
  m.m_rs_init.old_endpoint = old_endpoint;
  r = asynsend(rpub->endpoint, &m);

  return r;
}

/*===========================================================================*
 *			      fill_send_mask                                 *
 *===========================================================================*/
void fill_send_mask(send_mask, set_bits)
sys_map_t *send_mask;		/* the send mask to fill in */
int set_bits;			/* TRUE sets all bits, FALSE clears all bits */
{
/* Fill in a send mask. */
  int i;

  for (i = 0; i < NR_SYS_PROCS; i++) {
	if (set_bits)
		set_sys_bit(*send_mask, i);
	else
		unset_sys_bit(*send_mask, i);
  }
}

/*===========================================================================*
 *			      fill_call_mask                                 *
 *===========================================================================*/
void fill_call_mask(calls, tot_nr_calls, call_mask, call_base, is_init)
int *calls;                     /* the unordered set of calls */
int tot_nr_calls;               /* the total number of calls */
bitchunk_t *call_mask;          /* the call mask to fill in */
int call_base;                  /* the base offset for the calls */
int is_init;                    /* set when initializing a call mask */
{
/* Fill a call mask from an unordered set of calls. */
  int i;
  int call_mask_size, nr_calls;

  call_mask_size = BITMAP_CHUNKS(tot_nr_calls);

  /* Count the number of calls to fill in. */
  nr_calls = 0;
  for(i=0; calls[i] != NULL_C; i++) {
      nr_calls++;
  }

  /* See if all calls are allowed and call mask must be completely filled. */
  if(nr_calls == 1 && calls[0] == ALL_C) {
      for(i=0; i < call_mask_size; i++) {
          call_mask[i] = (~0);
      }
  }
  else {
      /* When initializing, reset the mask first. */
      if(is_init) {
          for(i=0; i < call_mask_size; i++) {
              call_mask[i] = 0;
          }
      }
      /* Enter calls bit by bit. */
      for(i=0; i < nr_calls; i++) {
          SET_BIT(call_mask, calls[i] - call_base);
      }
  }
}

/*===========================================================================*
 *			     srv_to_string				     *
 *===========================================================================*/
char* srv_to_string(rp)
struct rproc *rp;			/* pointer to process slot */
{
  struct rprocpub *rpub;
  int slot_nr;
  char *srv_string;
  static char srv_string_pool[3][RS_MAX_LABEL_LEN + (DEBUG ? 256 : 64)];
  static int srv_string_pool_index = 0;

  rpub = rp->r_pub;
  slot_nr = rp - rproc;
  srv_string = srv_string_pool[srv_string_pool_index];
  srv_string_pool_index = (srv_string_pool_index + 1) % 3;

#define srv_str(cmd) ((cmd) == NULL || (cmd)[0] == '\0' ? "_" : (cmd))
#define srv_ep_str(rp) (itoa((rp)->r_pub->endpoint))
#define srv_active_str(rp) ((rp)->r_flags & RS_ACTIVE ? "*" : " ")
#define srv_version_str(rp) ((rp)->r_new_rp || (rp)->r_next_rp ? "-" : \
    ((rp)->r_old_rp || (rp)->r_prev_rp ? "+" : " "))

#if DEBUG
  sprintf(srv_string, "service '%s'%s%s(slot %d, ep %d, pid %d, cmd %s, script %s, proc %s, major %d, flags 0x%03x, sys_flags 0x%02x)",
      rpub->label, srv_active_str(rp), srv_version_str(rp),
      slot_nr, rpub->endpoint, rp->r_pid, srv_str(rp->r_cmd),
      srv_str(rp->r_script), srv_str(rpub->proc_name), rpub->dev_nr,
      rp->r_flags, rpub->sys_flags);
#else
  sprintf(srv_string, "service '%s'%s%s(slot %d, ep %d, pid %d)",
      rpub->label, srv_active_str(rp), srv_version_str(rp),
      slot_nr, rpub->endpoint, rp->r_pid);
#endif

  return srv_string;
}

/*===========================================================================*
 *				reply					     *
 *===========================================================================*/
void reply(who, rp, m_ptr)
endpoint_t who;                        	/* replyee */
struct rproc *rp;                       /* replyee slot (if any) */
message *m_ptr;                         /* reply message */
{
  int r;				/* send status */

  /* No need to actually reply to RS */
  if(who == RS_PROC_NR) {
      return;
  }

  if(rs_verbose && rp)
      printf("RS: %s being replied to\n", srv_to_string(rp));

  r = ipc_sendnb(who, m_ptr);		/* send the message */
  if (r != OK)
      printf("RS: unable to send reply to %d: %d\n", who, r);
}

/*===========================================================================*
 *			      late_reply				     *
 *===========================================================================*/
void late_reply(rp, code)
struct rproc *rp;				/* pointer to process slot */
int code;					/* status code */
{
/* If a caller is waiting for a reply, unblock it. */
  if(rp->r_flags & RS_LATEREPLY) {
      message m;
      m.m_type = code;
      if(rs_verbose)
          printf("RS: %s late reply %d to %d for request %d\n",
              srv_to_string(rp), code, rp->r_caller, rp->r_caller_request);

      reply(rp->r_caller, NULL, &m);
      rp->r_flags &= ~RS_LATEREPLY;
  }
}

/*===========================================================================*
 *				rs_isokendpt			 	     *
 *===========================================================================*/
int rs_isokendpt(endpoint_t endpoint, int *proc)
{
	*proc = _ENDPOINT_P(endpoint);
	if(*proc < -NR_TASKS || *proc >= NR_PROCS)
		return EINVAL;

	return OK;
}

/*===========================================================================*
 *				sched_init_proc			 	     *
 *===========================================================================*/
int sched_init_proc(struct rproc *rp)
{
  int s;
  int is_usr_proc;

  /* Make sure user processes have no scheduler. PM deals with them. */
  is_usr_proc = !(rp->r_priv.s_flags & SYS_PROC);
  if(is_usr_proc) assert(rp->r_scheduler == NONE);
  if(!is_usr_proc) assert(rp->r_scheduler != NONE);

  /* Start scheduling for the given process. */
  if ((s = sched_start(rp->r_scheduler, rp->r_pub->endpoint, 
      RS_PROC_NR, rp->r_priority, rp->r_quantum, rp->r_cpu,
      &rp->r_scheduler)) != OK) {
      return s;
  }

  return s;
}

/*===========================================================================*
 *				update_sig_mgrs			 	     *
 *===========================================================================*/
int update_sig_mgrs(struct rproc *rp, endpoint_t sig_mgr,
	endpoint_t bak_sig_mgr)
{
  int r;
  struct rprocpub *rpub;

  rpub = rp->r_pub;

  if(rs_verbose)
      printf("RS: %s updates signal managers: %d%s / %d\n", srv_to_string(rp),
          sig_mgr == SELF ? rpub->endpoint : sig_mgr,
          sig_mgr == SELF ? "(SELF)" : "",
          bak_sig_mgr == NONE ? -1 : bak_sig_mgr);

  /* Synch privilege structure with the kernel. */
  if ((r = sys_getpriv(&rp->r_priv, rpub->endpoint)) != OK) {
      printf("unable to synch privilege structure: %d", r);
      return r;
  }

  /* Set signal managers. */
  rp->r_priv.s_sig_mgr = sig_mgr;
  rp->r_priv.s_bak_sig_mgr = bak_sig_mgr;

  /* Update privilege structure. */
  r = sys_privctl(rpub->endpoint, SYS_PRIV_UPDATE_SYS, &rp->r_priv);
  if(r != OK) {
      printf("unable to update privilege structure: %d", r);
      return r;
  }

  return OK;
}

