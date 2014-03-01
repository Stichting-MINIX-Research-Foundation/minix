#include "at_wini.h"

/* State management variables. */
EXTERN int w_command;

/* State management helpers. */
#define IS_REQUEST_PENDING(c)           ((c) != CMD_IDLE)
#define IS_READ_PENDING(c)              ((c) == CMD_READ \
    || (c) == CMD_READ_EXT || (c) == CMD_READ_DMA || (c) == CMD_READ_DMA_EXT)
#define IS_WRITE_PENDING(c)             ((c) == CMD_WRITE \
    || (c) == CMD_WRITE_EXT || (c) == CMD_WRITE_DMA || (c) == CMD_WRITE_DMA_EXT)

/* Custom states definition. */
#define AT_STATE_READ_REQUEST_FREE      (SEF_LU_STATE_CUSTOM_BASE + 0)
#define AT_STATE_WRITE_REQUEST_FREE     (SEF_LU_STATE_CUSTOM_BASE + 1)
#define AT_STATE_IS_CUSTOM(s) \
    ((s) >= AT_STATE_READ_REQUEST_FREE && (s) <= AT_STATE_WRITE_REQUEST_FREE)

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
      case SEF_LU_STATE_PROTOCOL_FREE:
          is_ready = (!IS_REQUEST_PENDING(w_command));
      break;

      /* Custom states. */
      case AT_STATE_READ_REQUEST_FREE:
          is_ready = (!IS_READ_PENDING(w_command));
      break;

      case AT_STATE_WRITE_REQUEST_FREE:
          is_ready = (!IS_WRITE_PENDING(w_command));
      break;
  }

  /* Tell SEF if we are ready. */
  return is_ready ? OK : ENOTREADY;
}

/*===========================================================================*
 *      		  sef_cb_lu_state_isvalid		             *
 *===========================================================================*/
int sef_cb_lu_state_isvalid(int state, int UNUSED(flags))
{
  return SEF_LU_STATE_IS_STANDARD(state) || AT_STATE_IS_CUSTOM(state);
}

/*===========================================================================*
 *      		   sef_cb_lu_state_dump         	             *
 *===========================================================================*/
void sef_cb_lu_state_dump(int state)
{
  sef_lu_dprint("at_wini: live update state = %d\n", state);
  sef_lu_dprint("at_wini: w_command = 0x%02X\n", w_command);

  sef_lu_dprint("at_wini: SEF_LU_STATE_WORK_FREE(%d) reached = %d\n", 
      SEF_LU_STATE_WORK_FREE, TRUE);
  sef_lu_dprint("at_wini: SEF_LU_STATE_REQUEST_FREE(%d) reached = %d\n", 
      SEF_LU_STATE_REQUEST_FREE, (!IS_REQUEST_PENDING(w_command)));
  sef_lu_dprint("at_wini: SEF_LU_STATE_PROTOCOL_FREE(%d) reached = %d\n", 
      SEF_LU_STATE_PROTOCOL_FREE, (!IS_REQUEST_PENDING(w_command)));
  sef_lu_dprint("at_wini: AT_STATE_READ_REQUEST_FREE(%d) reached = %d\n", 
      AT_STATE_READ_REQUEST_FREE, (!IS_READ_PENDING(w_command)));
  sef_lu_dprint("at_wini: AT_STATE_WRITE_REQUEST_FREE(%d) reached = %d\n", 
      AT_STATE_WRITE_REQUEST_FREE, (!IS_WRITE_PENDING(w_command)));
}

