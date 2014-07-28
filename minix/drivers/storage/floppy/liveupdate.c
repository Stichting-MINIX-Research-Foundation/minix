#include "floppy.h"

/* State management variables. */
EXTERN u16_t f_busy;
EXTERN int motor_status;
EXTERN unsigned f_drive;
EXTERN int last_was_write;
#define BSY_IO      1   /* busy doing I/O */

/* State management helpers. */
#define IS_REQUEST_PENDING(b)           ((b) == BSY_IO)
#define IS_READ_PENDING(b, c) \
    (IS_REQUEST_PENDING((b)) && (c) == DEV_GATHER_S)
#define IS_WRITE_PENDING(b, c) \
    (IS_REQUEST_PENDING((b)) && (c) == DEV_SCATTER_S)
#define IS_MOTOR_RUNNING(s, d)          ((s) & (1 << (d)))

/* Custom states definition. */
#define FL_STATE_MOTOR_OFF              (SEF_LU_STATE_CUSTOM_BASE + 0)
#define FL_STATE_IS_CUSTOM(s)   ((s) == FL_STATE_MOTOR_OFF)

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
          is_ready = (!IS_REQUEST_PENDING(f_busy));
      break;

      /* Custom states. */
      case FL_STATE_MOTOR_OFF:
          is_ready = (!IS_REQUEST_PENDING(f_busy)
              && !IS_MOTOR_RUNNING(motor_status, f_drive));
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
  return SEF_LU_STATE_IS_STANDARD(state) || FL_STATE_IS_CUSTOM(state);
}

/*===========================================================================*
 *      		   sef_cb_lu_state_dump         	             *
 *===========================================================================*/
void sef_cb_lu_state_dump(int state)
{
  sef_lu_dprint("floppy: live update state = %d\n", state);
  sef_lu_dprint("floppy: f_busy = %d\n", f_busy);
  sef_lu_dprint("floppy: motor_status = 0x%02X\n", motor_status);
  sef_lu_dprint("floppy: f_drive = %d\n", f_drive);
  sef_lu_dprint("floppy: last_was_write = %d\n", last_was_write);

  sef_lu_dprint("floppy: SEF_LU_STATE_WORK_FREE(%d) reached = %d\n",
      SEF_LU_STATE_WORK_FREE, TRUE);
  sef_lu_dprint("floppy: SEF_LU_STATE_REQUEST_FREE(%d) reached = %d\n",
      SEF_LU_STATE_REQUEST_FREE, (!IS_REQUEST_PENDING(f_busy)));
  sef_lu_dprint("floppy: SEF_LU_STATE_PROTOCOL_FREE(%d) reached = %d\n",
      SEF_LU_STATE_PROTOCOL_FREE, (!IS_REQUEST_PENDING(f_busy)));
  sef_lu_dprint("floppy: FL_STATE_MOTOR_OFF(%d) reached = %d\n",
      FL_STATE_MOTOR_OFF, (!IS_REQUEST_PENDING(f_busy)
      && !IS_MOTOR_RUNNING(motor_status, f_drive)));
}

