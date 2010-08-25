#include "sb16.h"

/* State management variables. */
EXTERN int is_processing;
EXTERN int is_status_msg_expected;

/* Custom states definition. */
#define SB16_STATE_PROCESSING_PROTOCOL_FREE (SEF_LU_STATE_CUSTOM_BASE + 0)
#define SB16_STATE_IS_CUSTOM(s) \
    ((s) == SB16_STATE_PROCESSING_PROTOCOL_FREE)

/*===========================================================================*
 *       		 sef_cb_lu_prepare 	 	                     *
 *===========================================================================*/
PUBLIC int sef_cb_lu_prepare(int state)
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
          is_ready = (!is_processing && !is_status_msg_expected);
      break;

      /* Custom states. */
      case SB16_STATE_PROCESSING_PROTOCOL_FREE:
          is_ready = (!is_processing);
      break;
  }

  /* Tell SEF if we are ready. */
  return is_ready ? OK : ENOTREADY;
}

/*===========================================================================*
 *      		  sef_cb_lu_state_isvalid		             *
 *===========================================================================*/
PUBLIC int sef_cb_lu_state_isvalid(int state)
{
  return SEF_LU_STATE_IS_STANDARD(state) || SB16_STATE_IS_CUSTOM(state);
}

/*===========================================================================*
 *      		   sef_cb_lu_state_dump         	 	     *
 *===========================================================================*/
PUBLIC void sef_cb_lu_state_dump(int state)
{
  sef_lu_dprint("sb16: live update state = %d\n", state);
  sef_lu_dprint("sb16: is_processing = %d\n", is_processing);
  sef_lu_dprint("sb16: is_status_msg_expected = %d\n",
      is_status_msg_expected);

  sef_lu_dprint("sb16: SEF_LU_STATE_WORK_FREE(%d) reached = %d\n", 
      SEF_LU_STATE_WORK_FREE, TRUE);
  sef_lu_dprint("sb16: SEF_LU_STATE_REQUEST_FREE(%d) reached = %d\n", 
      SEF_LU_STATE_REQUEST_FREE, TRUE);
  sef_lu_dprint("sb16: SEF_LU_STATE_PROTOCOL_FREE(%d) reached = %d\n", 
      SEF_LU_STATE_PROTOCOL_FREE, (!is_processing && !is_status_msg_expected));
  sef_lu_dprint("sb16: SB16_STATE_PROCESSING_PROTOCOL_FREE(%d) reached = %d\n", 
      SB16_STATE_PROCESSING_PROTOCOL_FREE, (!is_processing));
}

