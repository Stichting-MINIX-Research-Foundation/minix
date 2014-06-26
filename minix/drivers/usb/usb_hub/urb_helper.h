/*
 * URB formatting related definitions
 */

#ifndef _URB_HELPER_H_
#define _URB_HELPER_H_

#include <ddekit/usb.h>
#include <ddekit/semaphore.h>

/* Possible values for attach_urb_data's buf_type */
/* Both may be used for single URB */
#define URB_BUF_TYPE_DATA 0		/* attached buffer is data buffer */
#define URB_BUF_TYPE_SETUP 1		/* attached buffer is setup structure */

/* Possible values for blocking_urb_submit's check_len */
/* Use URB_SUBMIT_CHECK_LEN when actual data buffer length returned
 * by HCD must match expected length, supplied in attach_urb_data */
#define URB_SUBMIT_CHECK_LEN 0		/* return error on length mismatch */
#define URB_SUBMIT_ALLOW_MISMATCH 1	/* ignore length check */

/* Endpoint configuration related */
#define URB_INVALID_EP (-1)		/* default for unset endpoint */

/*---------------------------*
 *    declared types         *
 *---------------------------*/
/* URB's endpoint configuration */
typedef struct urb_ep_config {

	ddekit_int32_t ep_num;
	ddekit_int32_t direction;
	ddekit_int32_t type;
	ddekit_int32_t max_packet_size;
	ddekit_int32_t interval;
}
urb_ep_config;

/*---------------------------*
 *    declared functions     *
 *---------------------------*/
void init_urb(struct ddekit_usb_urb *, struct ddekit_usb_dev *,
		urb_ep_config *);
void attach_urb_data(struct ddekit_usb_urb *, int, void *, ddekit_uint32_t);
int blocking_urb_submit(struct ddekit_usb_urb *, ddekit_sem_t *, int);

#endif /* !_URB_HELPER_H_ */
