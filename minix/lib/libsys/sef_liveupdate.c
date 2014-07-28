#include "syslib.h"
#include <assert.h>
#include <minix/sysutil.h>

/* SEF Live update variables. */
static int sef_lu_state;

/* SEF Live update callbacks. */
static struct sef_cbs {
    sef_cb_lu_prepare_t                 sef_cb_lu_prepare;
    sef_cb_lu_state_isvalid_t           sef_cb_lu_state_isvalid;
    sef_cb_lu_state_changed_t           sef_cb_lu_state_changed;
    sef_cb_lu_state_dump_t              sef_cb_lu_state_dump;
    sef_cb_lu_state_save_t              sef_cb_lu_state_save;
    sef_cb_lu_response_t                sef_cb_lu_response;
} sef_cbs = {
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

/* Debug. */
EXTERN char* sef_debug_header(void);
static int sef_lu_debug_cycle = 0;

/* Information about SELF. */
EXTERN endpoint_t sef_self_endpoint;
EXTERN int sef_self_first_receive_done;

/*===========================================================================*
 *                         do_sef_lu_before_receive             	     *
 *===========================================================================*/
void do_sef_lu_before_receive(void)
{
/* Handle SEF Live update before receive events. */
  int r;

  /* Initialize on first receive. */
  if(!sef_self_first_receive_done) {
      sef_lu_state = SEF_LU_STATE_NULL;
  }

  /* Nothing to do if we are not preparing for a live update. */
  if(sef_lu_state == SEF_LU_STATE_NULL) {
      return;
  }

  /* Debug. */
#if SEF_LU_DEBUG
  sef_lu_debug_cycle++;
  sef_lu_debug_begin();
  sef_lu_dprint("%s, cycle=%d. Dumping state variables:\n",
      sef_debug_header(), sef_lu_debug_cycle);
  sef_cbs.sef_cb_lu_state_dump(sef_lu_state);
  sef_lu_debug_end();
#endif

  /* Let the callback code handle the event.
   * For SEF_LU_STATE_WORK_FREE, we're always ready, tell immediately.
   */
  r = OK;
  if(sef_lu_state != SEF_LU_STATE_WORK_FREE) {
      r = sef_cbs.sef_cb_lu_prepare(sef_lu_state);
  }
  if(r == OK) {
      sef_lu_ready(OK);
  }
}

/*===========================================================================*
 *                               do_sef_lu_request              	     *
 *===========================================================================*/
int do_sef_lu_request(message *m_ptr)
{
/* Handle a SEF Live update request. */
  int state, old_state, is_valid_state;

  sef_lu_debug_cycle = 0;
  old_state = sef_lu_state;
  state = m_ptr->m_rs_update.state;

  /* Deal with prepare cancel requests first. */
  is_valid_state = (state == SEF_LU_STATE_NULL);

  /* Otherwise only accept live update requests with a valid state. */
  is_valid_state = is_valid_state || sef_cbs.sef_cb_lu_state_isvalid(state);
  if(!is_valid_state) {
      if(sef_cbs.sef_cb_lu_state_isvalid == SEF_CB_LU_STATE_ISVALID_NULL) {
          sef_lu_ready(ENOSYS);
      }
      else {
          sef_lu_ready(EINVAL);
      }
  }
  else {
      /* Set the new live update state. */
      sef_lu_state = state;

      /* If the live update state changed, let the callback code
       * handle the rest.
       */
      if(old_state != sef_lu_state) {
          sef_cbs.sef_cb_lu_state_changed(old_state, sef_lu_state);
      }
  }

  /* Return OK not to let anybody else intercept the request. */
  return(OK);
}

/*===========================================================================*
 *				  sef_lu_ready				     *
 *===========================================================================*/
static void sef_lu_ready(int result)
{
  message m;
  int old_state, r;

#if SEF_LU_DEBUG
  sef_lu_debug_begin();
  sef_lu_dprint("%s, cycle=%d. Ready to update with result: %d%s\n",
      sef_debug_header(), sef_lu_debug_cycle,
      result, (result == OK ? "(OK)" : ""));
  sef_lu_debug_end();
#endif

  /* If result is OK, let the callback code save
   * any state that must be carried over to the new version.
   */
  if(result == OK) {
      r = sef_cbs.sef_cb_lu_state_save(sef_lu_state);
      if(r != OK) {
          /* Abort update if callback returned error. */
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
  r = sef_cbs.sef_cb_lu_response(&m);

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
  old_state = sef_lu_state;
  sef_lu_state = SEF_LU_STATE_NULL;
  if(old_state != sef_lu_state) {
      sef_cbs.sef_cb_lu_state_changed(old_state, sef_lu_state);
  }
}

/*===========================================================================*
 *                            sef_setcb_lu_prepare                           *
 *===========================================================================*/
void sef_setcb_lu_prepare(sef_cb_lu_prepare_t cb)
{
  assert(cb != NULL);
  sef_cbs.sef_cb_lu_prepare = cb;
}

/*===========================================================================*
 *                         sef_setcb_lu_state_isvalid                        *
 *===========================================================================*/
void sef_setcb_lu_state_isvalid(sef_cb_lu_state_isvalid_t cb)
{
  assert(cb != NULL);
  sef_cbs.sef_cb_lu_state_isvalid = cb;
}

/*===========================================================================*
 *                         sef_setcb_lu_state_changed                        *
 *===========================================================================*/
void sef_setcb_lu_state_changed(sef_cb_lu_state_changed_t cb)
{
  assert(cb != NULL);
  sef_cbs.sef_cb_lu_state_changed = cb;
}

/*===========================================================================*
 *                          sef_setcb_lu_state_dump                          *
 *===========================================================================*/
void sef_setcb_lu_state_dump(sef_cb_lu_state_dump_t cb)
{
  assert(cb != NULL);
  sef_cbs.sef_cb_lu_state_dump = cb;
}

/*===========================================================================*
 *                          sef_setcb_lu_state_save                           *
 *===========================================================================*/
void sef_setcb_lu_state_save(sef_cb_lu_state_save_t cb)
{
  assert(cb != NULL);
  sef_cbs.sef_cb_lu_state_save = cb;
}

/*===========================================================================*
 *                          sef_setcb_lu_response                            *
 *===========================================================================*/
void sef_setcb_lu_response(sef_cb_lu_response_t cb)
{
  assert(cb != NULL);
  sef_cbs.sef_cb_lu_response = cb;
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
int sef_cb_lu_state_isvalid_null(int UNUSED(state))
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
int sef_cb_lu_state_save_null(int UNUSED(result))
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
  panic("Simulating a crash at update prepare time...");

  return OK;
}

/*===========================================================================*
 *      	      sef_cb_lu_state_isvalid_standard                       *
 *===========================================================================*/
int sef_cb_lu_state_isvalid_standard(int state)
{
  return SEF_LU_STATE_IS_STANDARD(state);
}

/*===========================================================================*
 *      	      sef_cb_lu_state_isvalid_workfree                       *
 *===========================================================================*/
int sef_cb_lu_state_isvalid_workfree(int state)
{
  return (state == SEF_LU_STATE_WORK_FREE);
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

