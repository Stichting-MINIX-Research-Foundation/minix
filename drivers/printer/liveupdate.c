#include <minix/drivers.h>

/* State management variables. */
EXTERN int writing;
EXTERN int is_status_msg_expected;

/* Custom states definition. */
#define PR_STATE_WRITE_PROTOCOL_FREE    (SEF_LU_STATE_CUSTOM_BASE + 0)
#define PR_STATE_IS_CUSTOM(s)   ((s) == PR_STATE_WRITE_PROTOCOL_FREE)

/*===========================================================================*
 *       			 sef_cb_lu_prepare 	 	             *
 *===========================================================================*/
int sef_cb_lu_prepare(int state)
{
  int is_ready;

  /* Check if we are ready for the target state. */
  is_ready = FALSE;
  switch(state) {
      /* Standard states. */
      case SEF_LU_STATE_REQUEST_FREE:
          is_ready = TRUE;
      break;

      case SEF_LU_STATE_PROTOCOL_FREE:
          is_ready = (!writing && !is_status_msg_expected);
      break;

      /* Custom states. */
      case PR_STATE_WRITE_PROTOCOL_FREE:
          is_ready = (!writing);
      break;
  }

  /* Tell SEF if we are ready. */
  return is_ready ? OK : ENOTREADY;
}

/*===========================================================================*
 *      		  sef_cb_lu_state_isvalid		             *
 *===========================================================================*/
int sef_cb_lu_state_isvalid(int state)
{
  return SEF_LU_STATE_IS_STANDARD(state) || PR_STATE_IS_CUSTOM(state);
}

/*===========================================================================*
 *      		   sef_cb_lu_state_dump         	             *
 *===========================================================================*/
void sef_cb_lu_state_dump(int state)
{
  sef_lu_dprint("printer: live update state = %d\n", state);
  sef_lu_dprint("printer: writing = %d\n", writing);
  sef_lu_dprint("printer: is_status_msg_expected = %d\n",
      is_status_msg_expected);

  sef_lu_dprint("printer: SEF_LU_STATE_WORK_FREE(%d) reached = %d\n", 
      SEF_LU_STATE_WORK_FREE, TRUE);
  sef_lu_dprint("printer: SEF_LU_STATE_REQUEST_FREE(%d) reached = %d\n", 
      SEF_LU_STATE_REQUEST_FREE, TRUE);
  sef_lu_dprint("printer: SEF_LU_STATE_PROTOCOL_FREE(%d) reached = %d\n", 
      SEF_LU_STATE_PROTOCOL_FREE, (!writing && !is_status_msg_expected));
  sef_lu_dprint("printer: PR_STATE_WRITE_PROTOCOL_FREE(%d) reached = %d\n", 
      PR_STATE_WRITE_PROTOCOL_FREE, (!writing));
}

