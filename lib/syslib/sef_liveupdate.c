#include "syslib.h"
#include <assert.h>
#include <minix/sysutil.h>

/* SEF Live update variables. */
PRIVATE int sef_lu_state = SEF_LU_STATE_NULL;

/* SEF Live update callbacks. */
PRIVATE struct sef_cbs {
    sef_cb_lu_prepare_t                 sef_cb_lu_prepare;
    sef_cb_lu_state_isvalid_t           sef_cb_lu_state_isvalid;
    sef_cb_lu_state_changed_t           sef_cb_lu_state_changed;
    sef_cb_lu_state_dump_t              sef_cb_lu_state_dump;
    sef_cb_lu_ready_pre_t               sef_cb_lu_ready_pre;
} sef_cbs = {
    SEF_CB_LU_PREPARE_DEFAULT,
    SEF_CB_LU_STATE_ISVALID_DEFAULT,
    SEF_CB_LU_STATE_CHANGED_DEFAULT,
    SEF_CB_LU_STATE_DUMP_DEFAULT,
    SEF_CB_LU_READY_PRE_DEFAULT,
};

/* SEF Live update prototypes for sef_receive(). */
PUBLIC _PROTOTYPE( void do_sef_lu_before_receive, (void) );
PUBLIC _PROTOTYPE( int do_sef_lu_request, (message *m_ptr) );

/* Debug. */
EXTERN _PROTOTYPE( char* sef_debug_header, (void) );
PRIVATE int sef_lu_debug_cycle = 0;

/*===========================================================================*
 *                         do_sef_lu_before_receive             	     *
 *===========================================================================*/
PUBLIC void do_sef_lu_before_receive()
{
/* Handle SEF Live update before receive events. */

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
  if(sef_lu_state == SEF_LU_STATE_WORK_FREE) {
      sef_lu_ready(OK);
  }
  else {
      sef_cbs.sef_cb_lu_prepare(sef_lu_state);
  }
}

/*===========================================================================*
 *                               do_sef_lu_request              	     *
 *===========================================================================*/
PUBLIC int do_sef_lu_request(message *m_ptr)
{
/* Handle a SEF Live update request. */
  int old_state, is_valid_state;

  sef_lu_debug_cycle = 0;
  old_state = sef_lu_state;

  /* Only accept live update requests with a valid state. */
  is_valid_state = sef_cbs.sef_cb_lu_state_isvalid(m_ptr->RS_LU_STATE);
  if(!is_valid_state) {
      sef_lu_ready(EINVAL);
  }
  else {
      /* Set the new live update state. */
      sef_lu_state = m_ptr->RS_LU_STATE;

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
PUBLIC void sef_lu_ready(int result)
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

  /* Let the callback code perform any pre-ready operations. */
  r = sef_cbs.sef_cb_lu_ready_pre(result);
  if(r != OK) {
      /* Abort update if callback returned error. */
      result = r;
  }
  else {
      /* Inform RS that we're ready with the given result. */
      m.m_type = RS_LU_PREPARE;
      m.RS_LU_STATE = sef_lu_state;
      m.RS_LU_RESULT = result;
      r = sendrec(RS_PROC_NR, &m);
      if ( r != OK) {
          panic("SEF", "sendrec failed", r);
      }
  }

#if SEF_LU_DEBUG
  sef_lu_debug_begin();
  sef_lu_dprint("%s, cycle=%d. The %s aborted the update!\n",
      sef_debug_header(), sef_lu_debug_cycle,
      (result == OK ? "server" : "client"));
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
PUBLIC void sef_setcb_lu_prepare(sef_cb_lu_prepare_t cb)
{
  assert(cb != NULL);
  sef_cbs.sef_cb_lu_prepare = cb;
}

/*===========================================================================*
 *                         sef_setcb_lu_state_isvalid                        *
 *===========================================================================*/
PUBLIC void sef_setcb_lu_state_isvalid(sef_cb_lu_state_isvalid_t cb)
{
  assert(cb != NULL);
  sef_cbs.sef_cb_lu_state_isvalid = cb;
}

/*===========================================================================*
 *                         sef_setcb_lu_state_changed                        *
 *===========================================================================*/
PUBLIC void sef_setcb_lu_state_changed(sef_cb_lu_state_changed_t cb)
{
  assert(cb != NULL);
  sef_cbs.sef_cb_lu_state_changed = cb;
}

/*===========================================================================*
 *                          sef_setcb_lu_state_dump                          *
 *===========================================================================*/
PUBLIC void sef_setcb_lu_state_dump(sef_cb_lu_state_dump_t cb)
{
  assert(cb != NULL);
  sef_cbs.sef_cb_lu_state_dump = cb;
}

/*===========================================================================*
 *                          sef_setcb_lu_ready_pre                           *
 *===========================================================================*/
PUBLIC void sef_setcb_lu_ready_pre(sef_cb_lu_ready_pre_t cb)
{
  assert(cb != NULL);
  sef_cbs.sef_cb_lu_ready_pre = cb;
}

/*===========================================================================*
 *      	            sef_cb_lu_prepare_null 	 	             *
 *===========================================================================*/
PUBLIC void sef_cb_lu_prepare_null(int state)
{
}

/*===========================================================================*
 *      	         sef_cb_lu_state_isvalid_null		             *
 *===========================================================================*/
PUBLIC int sef_cb_lu_state_isvalid_null(int state)
{
  return FALSE;
}

/*===========================================================================*
 *                       sef_cb_lu_state_changed_null        		     *
 *===========================================================================*/
PUBLIC void sef_cb_lu_state_changed_null(int old_state, int state)
{
}

/*===========================================================================*
 *                       sef_cb_lu_state_dump_null        		     *
 *===========================================================================*/
PUBLIC void sef_cb_lu_state_dump_null(int state)
{
  sef_lu_dprint("NULL\n");
}

/*===========================================================================*
 *                       sef_cb_lu_ready_pre_null        		     *
 *===========================================================================*/
PUBLIC int sef_cb_lu_ready_pre_null(int result)
{
  return(OK);
}

/*===========================================================================*
 *      	       sef_cb_lu_prepare_always_ready	                     *
 *===========================================================================*/
PUBLIC void sef_cb_lu_prepare_always_ready(int state)
{
  sef_lu_ready(OK);
}

/*===========================================================================*
 *      	      sef_cb_lu_state_isvalid_standard                       *
 *===========================================================================*/
PUBLIC int sef_cb_lu_state_isvalid_standard(int state)
{
  return SEF_LU_STATE_IS_STANDARD(state);
}

