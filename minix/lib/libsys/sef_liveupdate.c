#include "syslib.h"
#include <assert.h>
#include <minix/sysutil.h>
#include <minix/rs.h>

/* SEF Live update variables. */
int sef_lu_state;
int __sef_st_before_receive_enabled;
char sef_lu_state_eval[SEF_LU_STATE_EVAL_MAX_LEN];
static int sef_lu_flags;

/* SEF Live update callbacks. */
static struct sef_lu_cbs {
    sef_cb_lu_prepare_t                 sef_cb_lu_prepare;
    sef_cb_lu_state_isvalid_t           sef_cb_lu_state_isvalid;
    sef_cb_lu_state_changed_t           sef_cb_lu_state_changed;
    sef_cb_lu_state_dump_t              sef_cb_lu_state_dump;
    sef_cb_lu_state_save_t              sef_cb_lu_state_save;
    sef_cb_lu_response_t                sef_cb_lu_response;
} sef_lu_cbs = {
    SEF_CB_LU_PREPARE_DEFAULT,
    SEF_CB_LU_STATE_ISVALID_DEFAULT,
    SEF_CB_LU_STATE_CHANGED_DEFAULT,
    SEF_CB_LU_STATE_DUMP_DEFAULT,
    SEF_CB_LU_STATE_SAVE_DEFAULT,
    SEF_CB_LU_RESPONSE_DEFAULT
};

/* SEF Live update prototypes for sef_receive(). */
void do_sef_lu_before_receive(void);
int do_sef_lu_request(message *m_ptr);

/* SEF Live update helpers. */
static void sef_lu_ready(int result);
static void sef_lu_state_change(int state, int flags);
int sef_lu_handle_state_data(endpoint_t src_e, int state,
    cp_grant_id_t state_data_gid);

/* Debug. */
EXTERN char* sef_debug_header(void);
static int sef_lu_debug_cycle = 0;

/* Information about SELF. */
EXTERN endpoint_t sef_self_endpoint;

/*===========================================================================*
 *                         do_sef_lu_before_receive             	     *
 *===========================================================================*/
void do_sef_lu_before_receive(void)
{
/* Handle SEF Live update before receive events. */
  int r;

  assert(sef_lu_state != SEF_LU_STATE_NULL);

  /* Debug. */
#if SEF_LU_DEBUG
  sef_lu_debug_cycle++;
  sef_lu_debug_begin();
  sef_lu_dprint("%s, cycle=%d. Dumping state variables:\n",
      sef_debug_header(), sef_lu_debug_cycle);
  sef_lu_cbs.sef_cb_lu_state_dump(sef_lu_state);
  sef_lu_debug_end();
#endif

  /* Check the state. For SEF_LU_STATE_WORK_FREE/SEF_LU_STATE_UNREACHABLE,
   * we are always/never ready. For SEF_LU_STATE_EVAL, evaluate the expression.
   * For other states, let the callback code handle the event.
   */
  switch(sef_lu_state) {
      case SEF_LU_STATE_WORK_FREE:
          r = OK;
      break;
      case SEF_LU_STATE_UNREACHABLE:
          r = sef_cb_lu_prepare_never_ready(sef_lu_state);
      break;
      case SEF_LU_STATE_PREPARE_CRASH:
          r = sef_cb_lu_prepare_crash(sef_lu_state);
      break;
      case SEF_LU_STATE_EVAL:
          r = sef_cb_lu_prepare_eval(sef_lu_state);
      break;
      default:
          r = sef_lu_cbs.sef_cb_lu_prepare(sef_lu_state);
      break;
  }
  if(r == OK || r != ENOTREADY) {
       sef_lu_ready(r);
  }
}

/*===========================================================================*
 *                               do_sef_lu_request              	     *
 *===========================================================================*/
int do_sef_lu_request(message *m_ptr)
{
/* Handle a SEF Live update request. */
  int r, state, flags, is_valid_state;
  cp_grant_id_t rs_state_data_gid;

  sef_lu_debug_cycle = 0;
  state = m_ptr->m_rs_update.state;
  flags = m_ptr->m_rs_update.flags;
  rs_state_data_gid = m_ptr->m_rs_update.state_data_gid;

  /* Deal with prepare cancel requests first, where no reply is requested. */
  if(state == SEF_LU_STATE_NULL) {
      sef_lu_state_change(SEF_LU_STATE_NULL, 0);
      return OK;
  }

  /* Check if we are already busy. */
  if(sef_lu_state != SEF_LU_STATE_NULL) {
      sef_lu_ready(EBUSY);
      return OK;
  }

  /* Otherwise only accept live update requests with a valid state. */
  is_valid_state = SEF_LU_ALWAYS_ALLOW_DEBUG_STATES && SEF_LU_STATE_IS_DEBUG(state);
  is_valid_state = is_valid_state || sef_lu_cbs.sef_cb_lu_state_isvalid(state, flags);
  if(!is_valid_state) {
      if(sef_lu_cbs.sef_cb_lu_state_isvalid == SEF_CB_LU_STATE_ISVALID_DEFAULT) {
          sef_lu_ready(ENOSYS);
      }
      else {
          sef_lu_ready(EINVAL);
      }
      return OK;
  }

  /* Handle additional state data (if any). */
  r = sef_lu_handle_state_data(m_ptr->m_source, state, rs_state_data_gid);
  if(r != OK) {
      sef_lu_ready(r);
      return OK;
  }

  /* Set the new live update state. */
  sef_lu_state_change(state, flags);


  /* Return OK not to let anybody else intercept the request. */
  return(OK);
}

/*===========================================================================*
 *				  sef_lu_ready				     *
 *===========================================================================*/
static void sef_lu_ready(int result)
{
  message m;
  int r=EINVAL;

#if SEF_LU_DEBUG
  sef_lu_debug_begin();
  sef_lu_dprint("%s, cycle=%d. Ready to update with result: %d%s\n",
      sef_debug_header(), sef_lu_debug_cycle,
      result, (result == OK ? "(OK)" : ""));
  sef_lu_debug_end();
#endif

  /* If result is OK, let the callback code cleanup and save
   * any state that must be carried over to the new version.
   */
  if(result == OK) {
      r = sef_llvm_state_cleanup();
      if(r == OK) {
          r = sef_lu_cbs.sef_cb_lu_state_save(sef_lu_state, sef_lu_flags);
      }
      if(r != OK) {
          /* Abort update in case of error. */
          result = r;
      }
  }

  /* Let the callback code produce a live update response and block.
   * We should get beyond this point only if either result is an error or
   * something else goes wrong in the callback code.
   */
  m.m_source = sef_self_endpoint;
  m.m_type = RS_LU_PREPARE;
  m.m_rs_update.state = sef_lu_state;
  m.m_rs_update.result = result;
  r = sef_lu_cbs.sef_cb_lu_response(&m);

#if SEF_LU_DEBUG
  sef_lu_debug_begin();
  sef_lu_dprint("%s, cycle=%d. The %s aborted the update with result %d!\n",
      sef_debug_header(), sef_lu_debug_cycle,
      (result == OK ? "server" : "client"),
      (result == OK ? r : result)); /* EINTR if update was canceled. */
  sef_lu_debug_end();
#endif

  /* Something went wrong. Update was aborted and we didn't get updated.
   * Restore things back to normal and continue executing.
   */
  sef_lu_state_change(SEF_LU_STATE_NULL, 0);

  /* Transfer of asynsend tables during live update is messy at best. The
   * general idea is that the asynsend table is preserved during live update,
   * so that messages never get lost. That means that 1) the new instance
   * takes over the table from the old instance upon live update, and 2) the
   * old instance takes over the table on rollback. Case 1 is not atomic:
   * the new instance starts with no asynsend table, and after swapping slots,
   * the old instance's table will no longer be looked at by the kernel. The
   * new instance copies over the table from the old instance, and then calls
   * senda_reload() to tell the kernel about the new location of the otherwise
   * preserved table. Case 2 is different: the old instance cannot copy the
   * table from the new instance, and so the kernel does that part, based on
   * the table provided through the new instance's senda_reload(). However, if
   * the new instance never got to the senda_reload() call, then the kernel
   * also would not have been able to deliver any messages, and so the old
   * instance's table can still be used as is. Now the problem. Because case 1
   * is not atomic, there is a small window during which other processes may
   * attempt to receive a message, based on the fact that their s_asyn_pending
   * mask in the kernel has a bit set for the process being updated. Failing
   * to find a matching message in the yet-missing table of the new process,
   * the kernel will unset the s_asyn_pending bit. Now, normally the bit would
   * be set again through the new instance's senda_reload() call. However, if
   * the new instance rolls back instead, the old instance will have a message
   * for the other process, but its s_asyn_pending bit will not be set. Thus,
   * the message will never be delivered unless we call senda_reload() here.
   * XXX TODO: the story is even more complicated, because based on the above
   * story, copying back the table should never be necessary and never happen.
   * My logs show it does happen for at least RS, which may indicate RS sends
   * asynchronous messages in its initialization code.. -dcvmoole
   */
  senda_reload();
}

/*===========================================================================*
 *                           sef_lu_state_change                	     *
 *===========================================================================*/
static void sef_lu_state_change(int state, int flags)
{
  int r, old_state;

  old_state = sef_lu_state;
  sef_lu_state = state;
  sef_lu_flags = flags;
  if(sef_lu_state == SEF_LU_STATE_NULL) {
      r = sys_statectl(SYS_STATE_CLEAR_IPC_FILTERS, 0, 0);
      if(r != OK)
          panic("%s:%d: SYS_STATE_CLEAR_IPC_FILTERS failed\n", __func__, __LINE__);
  }
  if(old_state != sef_lu_state) {
      sef_lu_cbs.sef_cb_lu_state_changed(old_state, sef_lu_state);
  }
}

/*===========================================================================*
 *                         sef_lu_handle_state_data             	     *
 *===========================================================================*/
int sef_lu_handle_state_data(endpoint_t src_e,
    int state, cp_grant_id_t state_data_gid)
{
    int r;
    struct rs_state_data rs_state_data;

    if(state_data_gid == GRANT_INVALID) {
        /* SEF_LU_STATE_EVAL requires an eval expression. */
        return state == SEF_LU_STATE_EVAL ? EINVAL : OK;
    }

    r = sys_safecopyfrom(src_e, state_data_gid, 0,
        (vir_bytes) &rs_state_data, sizeof(rs_state_data));
    if(r != OK) {
        return r;
    }
    if(rs_state_data.size != sizeof(rs_state_data)) {
        return E2BIG;
    }
    if(state == SEF_LU_STATE_EVAL) {
        if(rs_state_data.eval_addr && rs_state_data.eval_len) {
            if(rs_state_data.eval_len >= SEF_LU_STATE_EVAL_MAX_LEN) {
                return E2BIG;
            }
            r = sys_safecopyfrom(src_e, rs_state_data.eval_gid, 0,
                (vir_bytes) sef_lu_state_eval, rs_state_data.eval_len);
            if(r != OK) {
                return r;
            }
            sef_lu_state_eval[rs_state_data.eval_len] = '\0';
            r = sef_cb_lu_prepare_eval(SEF_LU_STATE_EVAL);
            if(r != OK && r != ENOTREADY) {
                /* State expression could not be evaluated correctly. */
                return EINVAL;
            }
        }
        else {
            /* SEF_LU_STATE_EVAL requires a valid eval expression. */
            return EINVAL;
        }
    }
    if(rs_state_data.ipcf_els && rs_state_data.ipcf_els_size) {
        ipc_filter_el_t ipc_filter[IPCF_MAX_ELEMENTS];
        size_t ipc_filter_size = sizeof(ipc_filter);
        int num_ipc_filters = rs_state_data.ipcf_els_size / ipc_filter_size;
        int i;
        if(rs_state_data.ipcf_els_size % ipc_filter_size) {
            return E2BIG;
        }
        r = OK;
        for(i=0;i<num_ipc_filters;i++) {
            int num_elements=0;
            r = sys_safecopyfrom(src_e, rs_state_data.ipcf_els_gid, i*ipc_filter_size,
                (vir_bytes) ipc_filter, ipc_filter_size);
            if(r != OK) {
                break;
            }
#if SEF_LU_DEBUG
            sef_lu_debug_begin();
            sef_lu_dprint("%s, Installing ipc filter:\n", sef_debug_header());
#endif
            while(num_elements < IPCF_MAX_ELEMENTS && ipc_filter[num_elements].flags) {
#if SEF_LU_DEBUG
                sef_lu_dprint("el[%d]=(flags=%c%c%c%c, m_source=%d, m_type=%d)",
                  num_elements,
                  (ipc_filter[num_elements].flags & IPCF_MATCH_M_SOURCE) ? 'S' : '-',
                  (ipc_filter[num_elements].flags & IPCF_MATCH_M_TYPE) ? 'T' : '-',
                  (ipc_filter[num_elements].flags & IPCF_EL_BLACKLIST) ? 'B' : '-',
                  (ipc_filter[num_elements].flags & IPCF_EL_WHITELIST) ? 'W' : '-',
                  ipc_filter[num_elements].m_source, ipc_filter[num_elements].m_type);
                sef_lu_dprint("\n");
#endif
                num_elements++;
            }
#if SEF_LU_DEBUG
            sef_lu_debug_end();
#endif
            if(num_elements == 0) {
                r = EINVAL;
                break;
            }
            r = sys_statectl(ipc_filter[0].flags & IPCF_EL_BLACKLIST ? SYS_STATE_ADD_IPC_BL_FILTER : SYS_STATE_ADD_IPC_WL_FILTER,
                ipc_filter, num_elements*sizeof(ipc_filter_el_t));
            if(r != OK) {
                break;
            }
        }
        if(r != OK) {
            sys_statectl(SYS_STATE_CLEAR_IPC_FILTERS, 0, 0);
            return r;
        }
    }
    return OK;
}

/*===========================================================================*
 *                            sef_setcb_lu_prepare                           *
 *===========================================================================*/
void sef_setcb_lu_prepare(sef_cb_lu_prepare_t cb)
{
  assert(cb != NULL);
  sef_lu_cbs.sef_cb_lu_prepare = cb;
}

/*===========================================================================*
 *                         sef_setcb_lu_state_isvalid                        *
 *===========================================================================*/
void sef_setcb_lu_state_isvalid(sef_cb_lu_state_isvalid_t cb)
{
  assert(cb != NULL);
  sef_lu_cbs.sef_cb_lu_state_isvalid = cb;
}

/*===========================================================================*
 *                         sef_setcb_lu_state_changed                        *
 *===========================================================================*/
void sef_setcb_lu_state_changed(sef_cb_lu_state_changed_t cb)
{
  assert(cb != NULL);
  sef_lu_cbs.sef_cb_lu_state_changed = cb;
}

/*===========================================================================*
 *                          sef_setcb_lu_state_dump                          *
 *===========================================================================*/
void sef_setcb_lu_state_dump(sef_cb_lu_state_dump_t cb)
{
  assert(cb != NULL);
  sef_lu_cbs.sef_cb_lu_state_dump = cb;
}

/*===========================================================================*
 *                          sef_setcb_lu_state_save                           *
 *===========================================================================*/
void sef_setcb_lu_state_save(sef_cb_lu_state_save_t cb)
{
  assert(cb != NULL);
  sef_lu_cbs.sef_cb_lu_state_save = cb;
}

/*===========================================================================*
 *                          sef_setcb_lu_response                            *
 *===========================================================================*/
void sef_setcb_lu_response(sef_cb_lu_response_t cb)
{
  assert(cb != NULL);
  sef_lu_cbs.sef_cb_lu_response = cb;
}

/*===========================================================================*
 *      	            sef_cb_lu_prepare_null 	 	             *
 *===========================================================================*/
int sef_cb_lu_prepare_null(int UNUSED(state))
{
  return ENOTREADY;
}

/*===========================================================================*
 *      	         sef_cb_lu_state_isvalid_null		             *
 *===========================================================================*/
int sef_cb_lu_state_isvalid_null(int UNUSED(state), int UNUSED(flags))
{
  return FALSE;
}

/*===========================================================================*
 *                       sef_cb_lu_state_changed_null        		     *
 *===========================================================================*/
void sef_cb_lu_state_changed_null(int UNUSED(old_state),
   int UNUSED(state))
{
}

/*===========================================================================*
 *                       sef_cb_lu_state_dump_null        		     *
 *===========================================================================*/
void sef_cb_lu_state_dump_null(int UNUSED(state))
{
  sef_lu_dprint("NULL\n");
}

/*===========================================================================*
 *                       sef_cb_lu_state_save_null        		     *
 *===========================================================================*/
int sef_cb_lu_state_save_null(int UNUSED(result), int UNUSED(flags))
{
  return OK;
}

/*===========================================================================*
 *                       sef_cb_lu_response_null        		     *
 *===========================================================================*/
int sef_cb_lu_response_null(message * UNUSED(m_ptr))
{
  return ENOSYS;
}

/*===========================================================================*
 *      	       sef_cb_lu_prepare_always_ready	                     *
 *===========================================================================*/
int sef_cb_lu_prepare_always_ready(int UNUSED(state))
{
  return OK;
}

/*===========================================================================*
 *      	       sef_cb_lu_prepare_never_ready	                     *
 *===========================================================================*/
int sef_cb_lu_prepare_never_ready(int UNUSED(state))
{
#if SEF_LU_DEBUG
  sef_lu_debug_begin();
  sef_lu_dprint("%s, cycle=%d. Simulating a service never ready to update...\n",
      sef_debug_header(), sef_lu_debug_cycle);
  sef_lu_debug_end();
#endif

  return ENOTREADY;
}

/*===========================================================================*
 *      	         sef_cb_lu_prepare_crash	                     *
 *===========================================================================*/
int sef_cb_lu_prepare_crash(int UNUSED(state))
{
  panic("Simulating a crash at update prepare time...\n");

  return OK;
}

/*===========================================================================*
 *      	         sef_cb_lu_prepare_eval 	                     *
 *===========================================================================*/
int sef_cb_lu_prepare_eval(int UNUSED(state))
{
  char result = 0;
  int ret = sef_llvm_eval_bool(sef_lu_state_eval, &result);

#if SEF_LU_DEBUG
  sef_lu_debug_begin();
  sef_lu_dprint("%s, cycle=%d. Evaluated state expression '%s' with error code %d and result %d\n",
      sef_debug_header(), sef_lu_debug_cycle, sef_lu_state_eval, ret, result);
  sef_lu_debug_end();
#endif

  if(ret < 0) {
      return ret == ENOTREADY ? EINTR : ret;
  }
  return result ? OK : ENOTREADY;
}

/*===========================================================================*
 *      	      sef_cb_lu_state_isvalid_standard                       *
 *===========================================================================*/
int sef_cb_lu_state_isvalid_standard(int state, int UNUSED(flags))
{
  return SEF_LU_STATE_IS_STANDARD(state);
}

/*===========================================================================*
 *      	      sef_cb_lu_state_isvalid_workfree                       *
 *===========================================================================*/
int sef_cb_lu_state_isvalid_workfree(int state, int UNUSED(flags))
{
  return (state == SEF_LU_STATE_WORK_FREE);
}

/*===========================================================================*
 *      	    sef_cb_lu_state_isvalid_workfree_self                    *
 *===========================================================================*/
int sef_cb_lu_state_isvalid_workfree_self(int state, int flags)
{
  return (state == SEF_LU_STATE_WORK_FREE) && (flags & (SEF_LU_SELF|SEF_LU_ASR));
}

/*===========================================================================*
 *      	       sef_cb_lu_state_isvalid_generic                       *
 *===========================================================================*/
int sef_cb_lu_state_isvalid_generic(int state, int flags)
{
  return (state == SEF_LU_STATE_EVAL) || sef_cb_lu_state_isvalid_workfree(state, flags);
}

/*===========================================================================*
 *                       sef_cb_lu_state_dump_eval        		     *
 *===========================================================================*/
void sef_cb_lu_state_dump_eval(int state)
{
  if(state == SEF_LU_STATE_EVAL) {
      sef_llvm_dump_eval(sef_lu_state_eval);
  }
  else {
      return sef_cb_lu_state_dump_null(state);
  }
}

/*===========================================================================*
 *                       sef_cb_lu_response_rs_reply        		     *
 *===========================================================================*/
int sef_cb_lu_response_rs_reply(message *m_ptr)
{
  int r;

  /* Inform RS that we're ready with the given result. */
  r = ipc_sendrec(RS_PROC_NR, m_ptr);
  if ( r != OK) {
      return r;
  }

  return m_ptr->m_type == RS_LU_PREPARE ? EINTR : m_ptr->m_type;
}

