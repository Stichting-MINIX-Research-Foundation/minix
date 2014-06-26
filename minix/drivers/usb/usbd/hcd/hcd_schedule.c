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

/* Makes URB schedule enabled */
static int hcd_schedule_urb(hcd_urb *);

/* Makes URB schedule disabled */
static void hcd_unschedule_urb(hcd_urb *);

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

	urb_thread = ddekit_thread_create(hcd_urb_scheduler_task, NULL,
					"scheduler");
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
 *    hcd_schedule_external_urb                                              *
 *===========================================================================*/
int
hcd_schedule_external_urb(hcd_urb * urb)
{
	DEBUG_DUMP;

	return hcd_schedule_urb(urb);
}


/*===========================================================================*
 *    hcd_schedule_internal_urb                                              *
 *===========================================================================*/
int
hcd_schedule_internal_urb(hcd_urb * urb)
{
	DEBUG_DUMP;

	return hcd_schedule_urb(urb);
}


/*===========================================================================*
 *    hcd_schedule_urb                                                       *
 *===========================================================================*/
static int
hcd_schedule_urb(hcd_urb * urb)
{
	DEBUG_DUMP;

	/* Tell URB what to call on completion */
	urb->handled = hcd_urb_handled;

	/* Store and check if scheduler should be unlocked */
	if (EXIT_SUCCESS == hcd_store_urb(urb)) {
		ddekit_sem_up(urb_lock);
		return EXIT_SUCCESS;
	}

	return EXIT_FAILURE;
}


/*===========================================================================*
 *    hcd_unschedule_urb                                                     *
 *===========================================================================*/
static void
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
	hcd_device_state * current_device;
	hcd_urb * current_urb;

	DEBUG_DUMP;

	for (;;) {
		/* Wait for scheduler to unlock on any URB submit */
		ddekit_sem_down(urb_lock);

		/* Get URB */
		current_urb = hcd_get_urb();

		/* Get URB's target device */
		current_device = current_urb->target_device;

		/* Check for mismatch */
		USB_ASSERT(NULL != current_urb, "URB missing after URB unlock");

		/* Check if URB's device is still allocated */
		if (EXIT_SUCCESS == hcd_check_device(current_device)) {
			/* Tell device that this is its URB */
			current_device->urb = current_urb;

			/* Start handling URB event */
			hcd_handle_event(current_device, HCD_EVENT_URB,
					HCD_UNUSED_VAL);

			/* Wait for completion */
			ddekit_sem_down(handled_lock);

			/* TODO: Not enough DDEKit thread priorities
			 * for a better solution */
			/* Yield, to allow unlocking thread, to continue
			 * before next URB is used */
			ddekit_yield();

			/* Makes thread debugging easier */
			USB_DBG("URB handled, scheduler unlocked");
		} else {
			USB_MSG("Device 0x%08X for URB 0x%08X, is unavailable",
				(int)current_device,
				(int)current_urb);
		}
	}
}


/*===========================================================================*
 *    hcd_urb_handled                                                        *
 *===========================================================================*/
static void
hcd_urb_handled(hcd_urb * urb)
{
	DEBUG_DUMP;

	/* This URB will be scheduled no more */
	hcd_unschedule_urb(urb);

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
