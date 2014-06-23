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
	ddekit_int32_t urb_type, ddekit_int32_t urb_endpoint,
	ddekit_int32_t urb_direction)
{
	MASS_DEBUG_DUMP;

	/* Sanity checks */
	assert(NULL != urb);
	assert(NULL != dev);
	assert((DDEKIT_USB_TRANSFER_BLK == urb_type) ||
		(DDEKIT_USB_TRANSFER_CTL == urb_type) ||
		(DDEKIT_USB_TRANSFER_INT == urb_type) ||
		(DDEKIT_USB_TRANSFER_ISO == urb_type));
	assert(urb_endpoint < 16);
	assert((DDEKIT_USB_IN == urb_direction) ||
		(DDEKIT_USB_OUT == urb_direction));

	/* Clear block first */
	memset(urb, 0, sizeof(*urb));

	/* Set supplied values */
	urb->dev = dev;
	urb->type = urb_type;
	urb->endpoint = urb_endpoint;
	urb->direction = urb_direction;
}


/*===========================================================================*
 *    attach_urb_data                                                        *
 *===========================================================================*/
void
attach_urb_data(struct ddekit_usb_urb * urb, int buf_type,
		void * buf, ddekit_uint32_t buf_len)
{
	MASS_DEBUG_DUMP;

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
	MASS_DEBUG_DUMP;

	assert(NULL != urb);
	assert(NULL != sem);
	assert((check_len == URB_SUBMIT_CHECK_LEN) ||
		(check_len == URB_SUBMIT_ALLOW_MISMATCH));

	/* Submit and block until semaphore gets up */
	if (ddekit_usb_submit_urb(urb)) {
		MASS_MSG("Submitting DDEKit URB failed");
		return EXIT_FAILURE;
	} else {
		/* Submitting succeeded so block and wait for reply */
		ddekit_sem_down(sem);

		/* Check for DDEKit status first */
		if (urb->status) {
			MASS_MSG("Invalid DDEKit URB status");
			return EXIT_FAILURE;
		} else {
			if (URB_SUBMIT_CHECK_LEN == check_len) {
				/* Compare lengths */
				if (urb->actual_length != urb->size) {
					MASS_MSG("URB different than expected");
					return EXIT_FAILURE;
				}
			}

			return EXIT_SUCCESS;
		}
	}
}
