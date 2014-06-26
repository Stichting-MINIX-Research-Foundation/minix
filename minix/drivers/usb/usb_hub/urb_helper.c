/*
 * URB formatting related implementation
 */

#include <minix/sysutil.h>			/* panic */
#include <minix/usb.h>				/* struct usb_ctrlrequest */

#include <string.h>				/* memset */
#include <assert.h>

#include "common.h"
#include "urb_helper.h"

/*---------------------------*
 *    defined functions      *
 *---------------------------*/
/*===========================================================================*
 *    init_urb                                                               *
 *===========================================================================*/
void
init_urb(struct ddekit_usb_urb * urb, struct ddekit_usb_dev * dev,
	urb_ep_config * conf)
{
	HUB_DEBUG_DUMP;

	/* Sanity checks */
	assert(NULL != urb);
	assert(NULL != dev);
	assert((DDEKIT_USB_TRANSFER_BLK == conf->type) ||
		(DDEKIT_USB_TRANSFER_CTL == conf->type) ||
		(DDEKIT_USB_TRANSFER_INT == conf->type) ||
		(DDEKIT_USB_TRANSFER_ISO == conf->type));
	assert((conf->ep_num >= 0) && (conf->ep_num < 16));
	assert((DDEKIT_USB_IN == conf->direction) ||
		(DDEKIT_USB_OUT == conf->direction));

	/* Clear block first */
	memset(urb, 0, sizeof(*urb));

	/* Set supplied values */
	urb->dev = dev;
	urb->type = conf->type;
	urb->endpoint = conf->ep_num;
	urb->direction = conf->direction;
	urb->interval = conf->interval;
}


/*===========================================================================*
 *    attach_urb_data                                                        *
 *===========================================================================*/
void
attach_urb_data(struct ddekit_usb_urb * urb, int buf_type,
		void * buf, ddekit_uint32_t buf_len)
{
	HUB_DEBUG_DUMP;

	assert(NULL != urb);
	assert(NULL != buf);

	/* Mutual exclusion */
	if (URB_BUF_TYPE_DATA == buf_type) {
		urb->data = buf;
		urb->size = buf_len;
	} else if ( URB_BUF_TYPE_SETUP == buf_type ) {
		assert(sizeof(struct usb_ctrlrequest) == buf_len);
		urb->setup_packet = buf;
	} else
		panic("Unexpected buffer type!");
}


/*===========================================================================*
 *    blocking_urb_submit                                                    *
 *===========================================================================*/
int
blocking_urb_submit(struct ddekit_usb_urb * urb, ddekit_sem_t * sem,
		int check_len)
{
	HUB_DEBUG_DUMP;

	assert(NULL != urb);
	assert(NULL != sem);
	assert((check_len == URB_SUBMIT_CHECK_LEN) ||
		(check_len == URB_SUBMIT_ALLOW_MISMATCH));

	/* Submit and block until semaphore gets up */
	if (ddekit_usb_submit_urb(urb)) {
		HUB_MSG("Submitting DDEKit URB failed");
		return EXIT_FAILURE;
	} else {
		/* Submitting succeeded so block and wait for reply */
		ddekit_sem_down(sem);

		/* Check for DDEKit status first */
		if (urb->status) {
			HUB_MSG("Invalid DDEKit URB status");
			return EXIT_FAILURE;
		} else {
			if (URB_SUBMIT_CHECK_LEN == check_len) {
				/* Compare lengths */
				if (urb->actual_length != urb->size) {
					HUB_MSG("URB different than expected");
					return EXIT_FAILURE;
				}
			}

			return EXIT_SUCCESS;
		}
	}
}
