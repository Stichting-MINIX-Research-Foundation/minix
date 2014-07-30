/*
 * Implementation of HCD URB scheduler
 */

#include <usbd/hcd_common.h>
#include <usbd/hcd_ddekit.h>
#include <usbd/hcd_interface.h>
#include <usbd/hcd_schedule.h>
#include <usbd/usbd_common.h>
#include <usbd/usbd_schedule.h>


/*===========================================================================*
 *    Required for scheduling                                                *
 *===========================================================================*/
/* Scheduler thread */
static hcd_thread * urb_thread;

/* TODO: This will soon become URB list */
static hcd_urb * current_urb;

/* This allows waiting for completion */
static hcd_lock * handled_lock;

/* Scheduler task */
static void hcd_urb_scheduler_task(void *);

/* Completion callback */
static void hcd_urb_handled(void);


/*===========================================================================*
 *    usbd_init_scheduler                                                    *
 *===========================================================================*/
int
usbd_init_scheduler(void)
{
	DEBUG_DUMP;

	urb_thread = ddekit_thread_create(hcd_urb_scheduler_task, NULL, "URB");
	if (NULL == urb_thread)
		goto ERR1;

	handled_lock = ddekit_sem_init(0);
	if (NULL == handled_lock)
		goto ERR2;

	return EXIT_SUCCESS;

	ERR2:
	ddekit_thread_terminate(urb_thread);
	ERR1:
	return EXIT_FAILURE;
}


/*===========================================================================*
 *    usbd_deinit_scheduler                                                  *
 *===========================================================================*/
void
usbd_deinit_scheduler(void)
{
	DEBUG_DUMP;

	ddekit_sem_deinit(handled_lock);

	ddekit_thread_terminate(urb_thread);
}


/*===========================================================================*
 *    hcd_schedule_urb                                                       *
 *===========================================================================*/
int
hcd_schedule_urb(hcd_urb * urb)
{
	DEBUG_DUMP;

	/* Tell URB what to call on completion */
	urb->handled = hcd_urb_handled;

	/* TODO: Proper list handling */
	current_urb = urb;

	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    hcd_unschedule_urb                                                     *
 *===========================================================================*/
void
hcd_unschedule_urb(hcd_urb * urb)
{
	DEBUG_DUMP;

	/* TODO: Proper list handling */
	((void)urb);
	current_urb = NULL;
}


/*===========================================================================*
 *    hcd_urb_scheduler_task                                                 *
 *===========================================================================*/
static void
hcd_urb_scheduler_task(void * UNUSED(arg))
{
	DEBUG_DUMP;

	for (;;) {
		if (NULL != current_urb) {
			/* Tell device that this is it's URB */
			current_urb->target_device->urb = current_urb;

			/* Start handling URB event */
			hcd_handle_event(current_urb->target_device,
					HCD_EVENT_URB, HCD_UNUSED_VAL);

			/* Wait for completion */
			ddekit_sem_down(handled_lock);

			/* Call completion regardless of status */
			hcd_completion_cb(current_urb);
		}

		/* TODO: Temporary, poor scheduling with forced sleep */
		ddekit_thread_msleep(50);
	}
}


/*===========================================================================*
 *    hcd_urb_handled                                                        *
 *===========================================================================*/
static void
hcd_urb_handled(void)
{
	DEBUG_DUMP;

	/* Handling completed */
	ddekit_sem_up(handled_lock);
}
