#include <minix/audio_fw.h>

/* State management variables. */
EXTERN int is_status_msg_expected;
/*
 * - From audio_fw.h:
 * EXTERN drv_t drv;
 * EXTERN sub_dev_t sub_dev[];
*/

/* State management helpers */
static int is_read_pending;
static int is_write_pending;
static void load_state_info(void)
{
  int i, dma_mode, found_pending;

  /* Check if reads or writes are pending. */
  is_read_pending = FALSE;
  is_write_pending = FALSE;
  found_pending = FALSE;
  for (i = 0; i < drv.NrOfSubDevices && !found_pending; i++) {
      if(sub_dev[i].RevivePending) {
          dma_mode = sub_dev[i].DmaMode;

          if(dma_mode == DEV_READ_S) {
              is_read_pending = TRUE;
          }
          else if (dma_mode == DEV_WRITE_S){
              is_write_pending = TRUE;
          }
      }

      found_pending = (is_read_pending && is_write_pending);
  }
}

/* Custom states definition. */
#define AUDIO_STATE_READ_REQUEST_FREE   (SEF_LU_STATE_CUSTOM_BASE + 0)
#define AUDIO_STATE_WRITE_REQUEST_FREE  (SEF_LU_STATE_CUSTOM_BASE + 1)
#define AUDIO_STATE_IS_CUSTOM(s) \
  ((s) >= AUDIO_STATE_READ_REQUEST_FREE && (s) <=AUDIO_STATE_WRITE_REQUEST_FREE)

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
          is_ready = (!is_read_pending && !is_write_pending);
      break;

      case SEF_LU_STATE_PROTOCOL_FREE:
          is_ready = (!is_read_pending && !is_write_pending
              && !is_status_msg_expected);
      break;

      /* Custom states. */
      case AUDIO_STATE_READ_REQUEST_FREE:
          is_ready = (!is_read_pending);
      break;

      case AUDIO_STATE_WRITE_REQUEST_FREE:
          is_ready = (!is_write_pending);
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
  return SEF_LU_STATE_IS_STANDARD(state) || AUDIO_STATE_IS_CUSTOM(state);
}

/*===========================================================================*
 *      		   sef_cb_lu_state_dump         	             *
 *===========================================================================*/
void sef_cb_lu_state_dump(int state)
{
  /* Load state information. */
  load_state_info();

  sef_lu_dprint("audio: live update state = %d\n", state);
  sef_lu_dprint("audio: is_status_msg_expected = %d\n",
      is_status_msg_expected);
  sef_lu_dprint("audio: is_read_pending = %d\n", is_read_pending);
  sef_lu_dprint("audio: is_write_pending = %d\n", is_write_pending);

  sef_lu_dprint("audio: SEF_LU_STATE_WORK_FREE(%d) reached = %d\n", 
      SEF_LU_STATE_WORK_FREE, TRUE);
  sef_lu_dprint("audio: SEF_LU_STATE_REQUEST_FREE(%d) reached = %d\n", 
      SEF_LU_STATE_REQUEST_FREE, (!is_read_pending && !is_write_pending));
  sef_lu_dprint("audio: SEF_LU_STATE_PROTOCOL_FREE(%d) reached = %d\n", 
      SEF_LU_STATE_PROTOCOL_FREE, (!is_read_pending && !is_write_pending
      && !is_status_msg_expected));
  sef_lu_dprint("audio: AUDIO_STATE_READ_REQUEST_FREE(%d) reached = %d\n", 
      AUDIO_STATE_READ_REQUEST_FREE, (!is_read_pending));
  sef_lu_dprint("audio: AUDIO_STATE_WRITE_REQUEST_FREE(%d) reached = %d\n", 
      AUDIO_STATE_WRITE_REQUEST_FREE, (!is_write_pending));
}

