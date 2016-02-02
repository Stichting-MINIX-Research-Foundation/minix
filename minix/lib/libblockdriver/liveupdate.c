/* This file implements SEF hooks for live update of multithreaded block
 * drivers.
 */

#include <minix/drivers.h>
#include <minix/blockdriver_mt.h>

#include "driver_mt.h"

/*===========================================================================*
 *				sef_cb_lu_prepare			     *
 *===========================================================================*/
static int sef_cb_lu_prepare(int state)
{
/* This function is called to decide whether we can enter the given live
 * update state, and to prepare for such an update. If we are requested to
 * update to a request-free or protocol-free state, make sure there is no work
 * pending or being processed, and shut down all worker threads.
 */

  switch (state) {
  case SEF_LU_STATE_REQUEST_FREE:
  case SEF_LU_STATE_PROTOCOL_FREE:
        if (!blockdriver_mt_is_idle()) {
                printf("libblockdriver(%d): not idle, blocking update\n",
		    sef_self());
                break;
        }

        blockdriver_mt_suspend();

        return OK;
  }

  return ENOTREADY;
}

/*===========================================================================*
 *				sef_cb_lu_state_changed			     *
 *===========================================================================*/
static void sef_cb_lu_state_changed(int old_state, int state)
{
/* This function is called in the old driver instance when the state changes.
 * We use it to resume normal operation after a failed live update.
 */

  if (state != SEF_LU_STATE_NULL)
        return;

  switch (old_state) {
  case SEF_LU_STATE_REQUEST_FREE:
  case SEF_LU_STATE_PROTOCOL_FREE:
        blockdriver_mt_resume();
  }
}

/*===========================================================================*
 *				sef_cb_init_lu				     *
 *===========================================================================*/
static int sef_cb_init_lu(int type, sef_init_info_t *info)
{
/* This function is called in the new driver instance during a live update.
 */
  int r;

  /* Perform regular state transfer. */
  if ((r = SEF_CB_INIT_LU_DEFAULT(type, info)) != OK)
        return r;

  /* Recreate worker threads, if necessary. */
  switch (info->prepare_state) {
  case SEF_LU_STATE_REQUEST_FREE:
  case SEF_LU_STATE_PROTOCOL_FREE:
        blockdriver_mt_resume();
  }

  return OK;
}

/*===========================================================================*
 *				blockdriver_mt_support_lu		     *
 *===========================================================================*/
void blockdriver_mt_support_lu(void)
{
/* Enable suppor for live update of this driver. To be called before
 * sef_startup().
 */

  /* Register live update callbacks. */
  sef_setcb_init_lu(sef_cb_init_lu);
  sef_setcb_lu_prepare(sef_cb_lu_prepare);
  sef_setcb_lu_state_changed(sef_cb_lu_state_changed);
  sef_setcb_lu_state_isvalid(sef_cb_lu_state_isvalid_standard);
}
