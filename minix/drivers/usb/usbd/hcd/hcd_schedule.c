/*
 * Implementation of HCD URB scheduler
 */

#include <string.h>				/* memset */

#include <usbd/hcd_common.h>
#include <usbd/hcd_ddekit.h>
#include <usbd/hcd_interface.h>
#include <usbd/hcd_schedule.h>
#include <usbd/usbd_common.h>
#include <usbd/usbd_schedule.h>


/*===========================================================================*
 *    Required for scheduling                                                *
 *===========================================================================*/
/* TODO: Like in DDEKit but power of 2 */
#define HCD_MAX_URBS 16

/* TODO: Structure to hold URBs in DDEKit is limited so this is no better
 * (but because of that, there is no need for another malloc) */
static hcd_urb * stored_urb[HCD_MAX_URBS];

/* Number of URBs stored during operation */
static int num_stored_urbs;

/* Scheduler thread */
static hcd_thread * urb_thread;

/* This allows waiting for URB */
static hcd_lock * urb_lock;

/* This allows waiting for completion */
static hcd_lock * handled_lock;

/* Scheduler task */
static void hcd_urb_scheduler_task(void *);

/* Completion callback */
static void hcd_urb_handled(hcd_urb *);

/* Stores URB to be handled */
static int hcd_store_urb(hcd_urb *);

/* Removes stored URB */
static void hcd_remove_urb(hcd_urb *);

/* Gets URB to be handled next (based on priority) */
static hcd_urb * hcd_get_urb(void);


/*===========================================================================*
 *    usbd_init_scheduler                                                    *
 *===========================================================================*/
int
usbd_init_scheduler(void)
{
	DEBUG_DUMP;

	/* Reset everything */
	num_stored_urbs = 0;
	memset(stored_urb, 0, sizeof(stored_urb));

	urb_thread = ddekit_thread_create(hcd_urb_scheduler_task, NULL, "urb");
	if (NULL == urb_thread)
		goto ERR1;

	urb_lock = ddekit_sem_init(0);
	if (NULL == urb_lock)
		goto ERR2;

	handled_lock = ddekit_sem_init(0);
	if (NULL == handled_lock)
		goto ERR3;

	return EXIT_SUCCESS;

	ERR3:
	ddekit_sem_deinit(urb_lock);
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

	ddekit_sem_deinit(urb_lock);

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

	/* Store and check if scheduler should be unlocked */
	if (EXIT_SUCCESS == hcd_store_urb(urb)) {

		/* This is the first stored URB (possibly in a row)
		 * so unlock scheduler */
		if (1 == num_stored_urbs)
			ddekit_sem_up(urb_lock);

		return EXIT_SUCCESS;
	}

	return EXIT_FAILURE;
}


/*===========================================================================*
 *    hcd_unschedule_urb                                                     *
 *===========================================================================*/
void
hcd_unschedule_urb(hcd_urb * urb)
{
	DEBUG_DUMP;

	hcd_remove_urb(urb);
}


/*===========================================================================*
 *    hcd_urb_scheduler_task                                                 *
 *===========================================================================*/
static void
hcd_urb_scheduler_task(void * UNUSED(arg))
{
	hcd_urb * current_urb;

	DEBUG_DUMP;

	for (;;) {
		/* Wait for scheduler to unlock on first URB submit */
		if (0 == num_stored_urbs)
			ddekit_sem_down(urb_lock);

		/* Get URB */
		current_urb = hcd_get_urb();

		/* Check for mismatch */
		USB_ASSERT(NULL != current_urb, "URB missing after URB unlock");

		/* Tell device that this is its URB */
		current_urb->target_device->urb = current_urb;

		/* Start handling URB event */
		hcd_handle_event(current_urb->target_device,
				HCD_EVENT_URB, HCD_UNUSED_VAL);

		/* Wait for completion */
		ddekit_sem_down(handled_lock);

		/* Handled, forget about it */
		current_urb->target_device->urb = NULL;
	}
}


/*===========================================================================*
 *    hcd_urb_handled                                                        *
 *===========================================================================*/
static void
hcd_urb_handled(hcd_urb * urb)
{
	DEBUG_DUMP;

	/* Call completion regardless of status */
	hcd_completion_cb(urb);

	/* Handling completed */
	ddekit_sem_up(handled_lock);
}


/*===========================================================================*
 *    hcd_store_urb                                                          *
 *===========================================================================*/
static int
hcd_store_urb(hcd_urb * urb)
{
	int i;

	DEBUG_DUMP;

	for (i = 0; i < HCD_MAX_URBS; i++) {
		if (NULL == stored_urb[i]) {
			stored_urb[i] = urb;
			num_stored_urbs++;
			return EXIT_SUCCESS;
		}
	}

	USB_MSG("No more free URBs");

	return EXIT_FAILURE;
}

/*===========================================================================*
 *    hcd_remove_urb                                                         *
 *===========================================================================*/
static void
hcd_remove_urb(hcd_urb * urb)
{
	int i;

	DEBUG_DUMP;

	for (i = 0; i < HCD_MAX_URBS; i++) {
		if (urb == stored_urb[i]) {
			stored_urb[i] = NULL;
			num_stored_urbs--;
			return;
		}
	}

	USB_ASSERT(0, "URB to be removed, was never stored");
}

/*===========================================================================*
 *    hcd_get_urb                                                            *
 *===========================================================================*/
static hcd_urb *
hcd_get_urb(void)
{
	static int i = 0;
	int checked;

	DEBUG_DUMP;

	/* TODO: Some priority checking may be here */
	for (checked = 0; checked < HCD_MAX_URBS; checked++) {
		/* To avoid starting from 0 every
		 * time (potential starvation) */
		i = (i + 1) % HCD_MAX_URBS;

		/* When found */
		if (NULL != stored_urb[i])
			return stored_urb[i];
	}

	/* Nothing submitted yet */
	return NULL;
}
