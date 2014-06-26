#include "common.h"
#include <ddekit/usb.h>
#include <ddekit/memory.h>
#include <ddekit/minix/msg_queue.h>
#include <minix/usb.h>

struct ddekit_usb_dev {
	int id;
	unsigned int interfaces;
	void *data;
	struct ddekit_usb_dev *next;
	struct ddekit_usb_dev *prev;
};

struct ddekit_usb_dev dev_list_head = {
	.next = &dev_list_head,
	.prev = &dev_list_head,
};

static struct ddekit_usb_driver *d_usb_driver;

static void _ddekit_usb_completion(struct usb_urb *mx);
static void _ddekit_usb_connect( unsigned int dev_id, unsigned int
	interfaces);
static void _ddekit_usb_disconnect(unsigned dev_id);

struct usb_driver mx_usb_driver = {
	.urb_completion = _ddekit_usb_completion,
	.connect_device = _ddekit_usb_connect,
	.disconnect_device = _ddekit_usb_disconnect
};

/*****************************************************************************
 *         _ddekit_usb_completion                                            *
 ****************************************************************************/
static void _ddekit_usb_completion(struct usb_urb *mx_urb)
{

	struct ddekit_usb_urb *d_urb = (struct ddekit_usb_urb *) mx_urb->priv;

	/* XXX: copy stuff back into d_urb */

	d_urb->status         = mx_urb->status;
	d_urb->error_count    = mx_urb->interval;
	d_urb->transfer_flags = mx_urb->error_count;
	d_urb->actual_length  = mx_urb->actual_length;
	d_urb->ddekit_priv    = NULL;

	if (mx_urb->type == USB_TRANSFER_CTL) {
		memcpy(d_urb->setup_packet, mx_urb->setup_packet, 8);
	}

	if (mx_urb->type == USB_TRANSFER_ISO) {
		d_urb->start_frame = mx_urb->start_frame;

		memcpy(d_urb->iso_desc, mx_urb->buffer + d_urb->size,
		       d_urb->number_of_packets * sizeof(struct usb_iso_packet_desc));
	}

	memcpy(d_urb->data, mx_urb->buffer, d_urb->size);

	/* free mx_urb */
	ddekit_simple_free(mx_urb);

	/* 'give back' URB */

	d_usb_driver->completion(d_urb->priv);
}


/*****************************************************************************
 *         _ddekit_usb_connect                                               *
 ****************************************************************************/
static void _ddekit_usb_connect(unsigned int dev_id, unsigned int interfaces)
{
	struct ddekit_usb_dev *d_dev = (struct ddekit_usb_dev *)
		ddekit_simple_malloc(sizeof(struct ddekit_usb_dev));

	d_dev->data       = NULL;
	d_dev->id         = dev_id;
	d_dev->interfaces = interfaces;

	/* add to list */

	d_dev->next = dev_list_head.next; 
	d_dev->prev = &dev_list_head;

	dev_list_head.next = d_dev; 
	d_dev->next->prev = d_dev;
	d_usb_driver->connect(d_dev, interfaces);
}

/*****************************************************************************
 *         _ddekit_usb_disconnect                                            *
 ****************************************************************************/
void _ddekit_usb_disconnect(unsigned dev_id)
{
	/* find dev */
	struct ddekit_usb_dev *it;
	struct ddekit_usb_dev *d_dev = NULL;


	for (it = dev_list_head.next; it != &dev_list_head; it= it->next) {
		if (it->id == dev_id) {
			d_dev = it;
			break;
		}
	}

	if (d_dev == NULL) {
		return;
	}

	d_usb_driver->disconnect(d_dev);
}

/*****************************************************************************
 *         ddekit_usb_dev_set_data                                           *
 ****************************************************************************/
int ddekit_usb_dev_set_data(struct ddekit_usb_dev *dev, void *data)
{
	dev->data = data;
	return 0;
}

/*****************************************************************************
 *         ddekit_usb_dev_get_data                                           *
 ****************************************************************************/
void *ddekit_usb_dev_get_data(struct ddekit_usb_dev *dev)
{
	return dev->data;
}

/*****************************************************************************
 *         ddekit_usb_submit_urb                                             *
 ****************************************************************************/
int ddekit_usb_submit_urb(struct ddekit_usb_urb *d_urb) 
{
	int res;
	unsigned urb_size = USB_URBSIZE(d_urb->size, d_urb->number_of_packets);
	/* create mx urb out of d_urb */
	struct usb_urb *mx_urb = (struct usb_urb*) 
	    ddekit_simple_malloc(urb_size);
	mx_urb->urb_size = urb_size;

	mx_urb->dev_id = d_urb->dev->id;
	mx_urb->type = d_urb->type;
	mx_urb->endpoint = d_urb->endpoint;
	mx_urb->direction = d_urb->direction;
	mx_urb->interval = d_urb->interval;
	mx_urb->transfer_flags = d_urb->transfer_flags;
	mx_urb->size = d_urb->size;
	mx_urb->priv = d_urb;

	if (mx_urb->type == USB_TRANSFER_CTL) {
		memcpy(mx_urb->setup_packet, d_urb->setup_packet, 8);
	}

	if (mx_urb->type == USB_TRANSFER_ISO) {
		mx_urb->number_of_packets = d_urb->number_of_packets;
		mx_urb->start_frame = d_urb->start_frame;
		memcpy(mx_urb->buffer + d_urb->size, d_urb->iso_desc,
		    d_urb->number_of_packets * sizeof(struct usb_iso_packet_desc));
	}
	memcpy(mx_urb->buffer, d_urb->data, d_urb->size);

	d_urb->ddekit_priv = mx_urb;

	/* submit mx_urb */
	res = usb_send_urb(mx_urb);
	return res;
}

/*****************************************************************************
 *         ddekit_usb_cancle_urb                                             *
 ****************************************************************************/
int ddekit_usb_cancle_urb(struct ddekit_usb_urb *d_urb)
{
	int res;

	/* get the associated mx_urb */
	struct usb_urb *mx_urb = (struct usb_urb *) d_urb->ddekit_priv;

	res = usb_cancle_urb(mx_urb);

	return res;
}


/*****************************************************************************
 *         ddekit_usb_info                                                   *
 *****************************************************************************/
long
ddekit_usb_info(struct ddekit_usb_dev * UNUSED(dev), long type, long value)
{
	return usb_send_info(type, value);
}


static void _ddekit_usb_thread()
{ 
	struct ddekit_minix_msg_q *mq = ddekit_minix_create_msg_q(USB_BASE, 
	                                    USB_BASE + 0x1000);
	message m;
	int ipc_status;

	while (1) {
		ddekit_minix_rcv(mq, &m, &ipc_status);
		usb_handle_msg(&mx_usb_driver, &m);
	}

}

/*****************************************************************************
 *         ddekit_usb_init                                             *
 ****************************************************************************/
int ddekit_usb_init
(struct ddekit_usb_driver *drv,
 ddekit_usb_malloc_fn     *unused,
 ddekit_usb_free_fn       *_unused) 
{
	/* start usb_thread */
	d_usb_driver =  drv;
	usb_init("dde");
	_ddekit_usb_thread();
	return 0;
}

