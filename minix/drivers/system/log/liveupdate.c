#include "log.h"

/* State management variables. */
#define NR_DEVS     1   /* number of minor devices */
EXTERN struct logdevice logdevices[NR_DEVS];

/* State management helpers. */
static int is_read_pending;
static int is_select_callback_pending;
static void load_state_info(void)
{
  int i, found_pending;
  struct logdevice *log;

  /* Check if reads or select callbacks are pending. */
  is_read_pending = FALSE;
  is_select_callback_pending = FALSE;
  found_pending = FALSE;
  for (i = 0; i < NR_DEVS && !found_pending; i++) {
      log = &logdevices[i];
      if(log->log_source != NONE) {
          is_read_pending = TRUE;
      }
      if(log->log_selected) {
          is_select_callback_pending = TRUE;
      }

      found_pending = (is_read_pending && is_select_callback_pending);
  }
}

/* Custom states definition. */
#define LOG_STATE_SELECT_PROTOCOL_FREE  (SEF_LU_STATE_CUSTOM_BASE + 0)
#define LOG_STATE_IS_CUSTOM(s)  ((s) == LOG_STATE_SELECT_PROTOCOL_FREE)

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
          is_ready = (!is_read_pending);
      break;

      case SEF_LU_STATE_PROTOCOL_FREE:
          is_ready = (!is_read_pending && !is_select_callback_pending);
      break;

      /* Custom states. */
      case LOG_STATE_SELECT_PROTOCOL_FREE:
          is_ready = (!is_select_callback_pending);
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
  return SEF_LU_STATE_IS_STANDARD(state) || LOG_STATE_IS_CUSTOM(state);
}

/*===========================================================================*
 *      		   sef_cb_lu_state_dump         	             *
 *===========================================================================*/
void sef_cb_lu_state_dump(int state)
{
  /* Load state information. */
  load_state_info();

  sef_lu_dprint("log: live update state = %d\n", state);
  sef_lu_dprint("log: is_read_pending = %d\n", is_read_pending);
  sef_lu_dprint("log: is_select_callback_pending = %d\n",
      is_select_callback_pending);

  sef_lu_dprint("log: SEF_LU_STATE_WORK_FREE(%d) reached = %d\n", 
      SEF_LU_STATE_WORK_FREE, TRUE);
  sef_lu_dprint("log: SEF_LU_STATE_REQUEST_FREE(%d) reached = %d\n", 
      SEF_LU_STATE_REQUEST_FREE, (!is_read_pending));
  sef_lu_dprint("log: SEF_LU_STATE_PROTOCOL_FREE(%d) reached = %d\n", 
      SEF_LU_STATE_PROTOCOL_FREE, (!is_read_pending
      && !is_select_callback_pending));
  sef_lu_dprint("log: LOG_STATE_SELECT_PROTOCOL_FREE(%d) reached = %d\n", 
      LOG_STATE_SELECT_PROTOCOL_FREE, (!is_select_callback_pending));
}

