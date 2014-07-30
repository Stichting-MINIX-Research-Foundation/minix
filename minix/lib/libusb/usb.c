#include <minix/config.h>
#include <minix/const.h>
#include <minix/usb.h>
#include <minix/com.h>
#include <minix/safecopies.h>
#include <minix/sysutil.h>
#include <minix/ds.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

static struct usb_urb * pending_urbs = NULL;
static endpoint_t hcd_ep;

static void _usb_urb_complete(struct usb_driver *ud, long urb_id);

/*****************************************************************************
 *         usb_send_urb                                                      *
 ****************************************************************************/
int usb_send_urb(struct usb_urb* urb)
{
	message msg;
	int res;
	cp_grant_id_t gid;
	if (urb == NULL) {
		return EINVAL;
	}

	if (hcd_ep == 0) {
		return EINVAL;
	}

	/* setup grant */
	gid = cpf_grant_direct(hcd_ep,(vir_bytes) &urb->dev_id,
	         urb->urb_size - sizeof(void*),CPF_WRITE|CPF_READ);
	
	if (gid == -1) {
		printf("usb_send_urb: grant failed: "
		      "cpf_grant_direct(%d,%p,%d)\n", hcd_ep, urb, urb->urb_size);
		return EINVAL;
	}
	
	urb->gid = gid; 
	
	/* prepare message */
	msg.m_type         = USB_RQ_SEND_URB;
	msg.USB_GRANT_ID   = gid;
	msg.USB_GRANT_SIZE = urb->urb_size-sizeof(void*);
		
	/* send message */
	res = ipc_sendrec(hcd_ep, &msg);

	if (res != 0) {
		panic("usb_send_urb: could not talk to hcd: %d", res);
	}
	
	if (msg.m_type != USB_REPLY) {
		panic("usb_send_urb: got illegal response from hcd: %d", msg.m_type);
	}

	if (msg.USB_RESULT != 0) {
		panic("usb_send_urb: hcd could not enqueue URB: %ld", msg.USB_RESULT);
	}
	
	/* everything ok, add urb to pending_urbs */
	urb->urb_id = msg.USB_URB_ID;
	urb->next = pending_urbs;
	pending_urbs = urb;
	
	/* done. */

	/* (The HCD will send us a message when the URB is completed.) */

	return res;
}

/*****************************************************************************
 *         usb_cancle_urb                                                    *
 ****************************************************************************/
int usb_cancle_urb(struct usb_urb* urb)
{
	int res;
	message msg;

	if (urb == NULL) {
		panic("usb_send_urb: urb == NULL!");
	}
	
	if (urb->urb_id == USB_INVALID_URB_ID) {
		return EINVAL;
	}

	/* prepare message */
	msg.m_type       = USB_RQ_CANCEL_URB;
	msg.USB_URB_ID = urb->urb_id;

	/* send message */
	res = ipc_sendrec(hcd_ep, &msg);
	
	if (res != 0) {
		panic("usb_cancle_urb: could not talk to hcd: %d", res);
	}

	if (msg.m_type != USB_REPLY) {
		panic("usb_cancle_urb: got illegal response from hcd: %d", msg.m_type);
	}

	if (msg.USB_RESULT != 0) {
		panic("usb_cancle_urb: got illegal response from hcd: %d", msg.m_type);
	}
	
	res = msg.USB_RESULT;

	/* done. */
	return res; 
}

/*****************************************************************************
 *         usb_init                                                          *
 ****************************************************************************/
int usb_init(char *name) 
{
	int res;
	message msg;

	/* get the endpoint of the HCD */
	res = ds_retrieve_label_endpt("usbd", &hcd_ep);

	if (res != 0) {
		panic("usb_init: ds_retrieve_label_endpt failed for 'usb': %d", res);
	}

	msg.m_type = USB_RQ_INIT;

	strncpy(msg.USB_RB_INIT_NAME, name, M_PATH_STRING_MAX);
	
	res = ipc_sendrec(hcd_ep, &msg);

	if (res != 0) {
		panic("usb_init: can't talk to USB: %d", res);
	}

	if (msg.m_type != USB_REPLY) {
		panic("usb_init: bad reply from USB: %d", msg.m_type);
	}

	if (msg.USB_RESULT != 0 ) {
		panic("usb_init: init failed: %ld", msg.USB_RESULT);
	}

	return 0;
}

/*****************************************************************************
 *      _usb_urb_complete                                                    *
 ****************************************************************************/
static void _usb_urb_complete(struct usb_driver *ud, long urb_id)
{
	/* find the corresponding URB in the urb_pending list. */
	struct usb_urb * urb = NULL;
	if (pending_urbs != NULL) {
		if (pending_urbs->urb_id == urb_id) {
			urb = pending_urbs;
			pending_urbs = urb->next;
		} else {
			struct usb_urb *u = pending_urbs;
			while (u->next) {
				if (u->next->urb_id == urb_id) {
					urb       = u->next;
					u->next   = u->next->next;
					urb->next = NULL;
					break;
				}
				u = u->next;
			}
		}
	}

	/* Did we find a URB? */
	if (urb != NULL) {
		/* revoke grant */
		cpf_revoke(urb->gid);
		/* call completion handler */
#if 0
		dump_urb(urb);
#endif
		ud->urb_completion(urb);
	} else {
		printf("WARN: _usb_urb_complete: did not find URB with ID %ld", urb_id);
	}
}

/*****************************************************************************
 *         usb_handle_msg                                                    *
 ****************************************************************************/
int usb_handle_msg(struct usb_driver *ud, message *msg)
{
	/* 
	 * we expect kind of messages:
	 *  - new usb device
	 *  - removed device
	 *  - URB completed 
	 * 
	 * NOTE: the hcd driver doesn't expect replies for these messages.
	 */

	if (!ud) {
		return -1;
	}

	switch(msg->m_type) {
		case USB_COMPLETE_URB:
			_usb_urb_complete(ud, msg->USB_URB_ID);
			return 0;
		case USB_ANNOUCE_DEV:
			ud->connect_device(msg->USB_DEV_ID, msg->USB_INTERFACES);
			return 0;
		case USB_WITHDRAW_DEV:
			ud->disconnect_device(msg->USB_DEV_ID);
			return 0;
		default:
			panic("usb_handle_msg: bogus message from USB");
	}
}


/*****************************************************************************
 *         usb_send_info                                                     *
 *****************************************************************************/
int
usb_send_info(long info_type, long info_value)
{
	int res;
	message msg;

	/* Prepare message */
	msg.m_type		= USB_RQ_SEND_INFO;
	msg.USB_INFO_TYPE	= info_type;
	msg.USB_INFO_VALUE	= info_value;

	/* Send/receive message */
	res = ipc_sendrec(hcd_ep, &msg);

	if (res != 0)
		panic("usb_send_info: could not talk to HCD: %d", res);

	if (msg.m_type != USB_REPLY)
		panic("usb_send_info: got illegal response from HCD: %d", msg.m_type);

	if (msg.USB_RESULT != 0)
		panic("usb_send_info: got illegal response from HCD: %d", msg.m_type);

	return msg.USB_RESULT;
}
