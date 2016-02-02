/* Code left here for historical purposes only. TODO: move into libnetdriver */

#include "rtl8139.h"

/* State management variables. */
EXTERN re_t re_state;

/* Custom states definition. */
#define RL_STATE_READ_PROTOCOL_FREE     (SEF_LU_STATE_CUSTOM_BASE + 0)
#define RL_STATE_WRITE_PROTOCOL_FREE    (SEF_LU_STATE_CUSTOM_BASE + 1)
#define RL_STATE_IS_CUSTOM(s) \
    ((s) >= RL_STATE_READ_PROTOCOL_FREE && (s) <= RL_STATE_WRITE_PROTOCOL_FREE)

/* State management helpers. */
static int is_reading;
static int is_writing;

static void load_state_info(void)
{
  re_t *rep;

  /* Check if we are reading or writing. */
  rep = &re_state;

  is_reading = !!(rep->re_flags & REF_READING);
  is_writing = !!(rep->re_flags & REF_SEND_AVAIL);
}

/*===========================================================================*
 *       			 sef_cb_lu_prepare 	 	             *
 *===========================================================================*/
int sef_cb_lu_prepare(int state)
{
  int is_ready;

  /* Load state information. */
  load_state_info();

  /* Check if we are ready for the target state. */
  is_ready = FALSE;
  switch(state) {
      /* Standard states. */
      case SEF_LU_STATE_REQUEST_FREE:
          is_ready = TRUE;
      break;

      case SEF_LU_STATE_PROTOCOL_FREE:
          is_ready = (!is_reading && !is_writing);
      break;

      /* Custom states. */
      case RL_STATE_READ_PROTOCOL_FREE:
          is_ready = (!is_reading);
      break;

      case RL_STATE_WRITE_PROTOCOL_FREE:
          is_ready = (!is_writing);
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
  return SEF_LU_STATE_IS_STANDARD(state) || RL_STATE_IS_CUSTOM(state);
}

/*===========================================================================*
 *      		   sef_cb_lu_state_dump         	             *
 *===========================================================================*/
void sef_cb_lu_state_dump(int state)
{
  /* Load state information. */
  load_state_info();

  sef_lu_dprint("rtl8139: live update state = %d\n", state);
  sef_lu_dprint("rtl8139: is_reading = %d\n", is_reading);
  sef_lu_dprint("rtl8139: is_writing = %d\n", is_writing);

  sef_lu_dprint("rtl8139: SEF_LU_STATE_WORK_FREE(%d) reached = %d\n", 
      SEF_LU_STATE_WORK_FREE, TRUE);
  sef_lu_dprint("rtl8139: SEF_LU_STATE_REQUEST_FREE(%d) reached = %d\n", 
      SEF_LU_STATE_REQUEST_FREE, TRUE);
  sef_lu_dprint("rtl8139: SEF_LU_STATE_PROTOCOL_FREE(%d) reached = %d\n", 
      SEF_LU_STATE_PROTOCOL_FREE, (!is_reading && !is_writing));
  sef_lu_dprint("rtl8139: RL_STATE_READ_PROTOCOL_FREE(%d) reached = %d\n", 
      RL_STATE_READ_PROTOCOL_FREE, (!is_reading));
  sef_lu_dprint("rtl8139: RL_STATE_WRITE_PROTOCOL_FREE(%d) reached = %d\n", 
      RL_STATE_WRITE_PROTOCOL_FREE, (!is_writing));
}

