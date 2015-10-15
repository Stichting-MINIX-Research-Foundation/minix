#include "common.h"

#include <ddekit/minix/msg_queue.h>
#include <ddekit/panic.h>
#include <ddekit/printf.h>
#include <ddekit/usb.h>
#include <minix/safecopies.h>
#include <minix/usb.h>
#include <minix/usb_ch9.h>
#include <minix/devman.h>

#define MAX_URBS    10

#define DRIVER_UNUSED 0
#define DRIVER_ACTIVE 1
#define DRIVER_BOUND  2

#if 0
#define DEBUG_MSG(fmt, ...) ddekit_printf("%s : "fmt"\n", __func__, ##__VA_ARGS__ ) 
#else
#define DEBUG_MSG(fmt, ...)
#endif

#undef DDEBUG
#define DDEBUG 0
#include "debug.h"

#define MAX_DEVS 256
#define MAX_DRIVERS 256
#define OK 0

#define INVAL_DEV (-1)

struct my_context {
	unsigned urb_id;
	struct ddekit_usb_urb *d_urb;
	struct usb_urb *mx_urb;
	struct minix_usb_driver *drv;
	gid_t gid;
};

struct minix_usb_driver {
	endpoint_t ep;                       /* address of the client */

	int status;                          /* In what state is the client? */

	int dev;                          /* which device is this driver handling */
	unsigned interfaces;                 /* which interfaces of the device the
	                                        driver is handling */
	
	struct ddekit_usb_urb *urbs[MAX_URBS]; /* pending urbs */

	unsigned long urb_id;			     /* generation of driver_local urb_ids */
};

struct minix_usb_device {
	struct ddekit_usb_dev *dev;
	unsigned int interfaces; 
};

static struct minix_usb_driver *find_driver(endpoint_t ep);
static struct minix_usb_driver *find_unused_driver(void);
static int add_to_pending_urbs(struct minix_usb_driver *drv, struct
	ddekit_usb_urb *urb);
static int remove_from_pending_urbs(struct minix_usb_driver *drv,
	struct ddekit_usb_urb *urb);
static struct ddekit_usb_urb * find_pending_urb(struct minix_usb_driver
	*drv, unsigned urb_id);
static void register_driver(message *msg);
static struct ddekit_usb_urb *ddekit_usb_urb_from_mx_urb(struct usb_urb
	*mx_urb);
static void submit_urb(message *msg);
static void cancle_urb(message *msg);
static void get_info(message *msg);
static void completion_callback(void *priv);

static void prepare_devman_usbdev(struct ddekit_usb_dev * dev, int
	dev_id, unsigned int interfaces, struct devman_usb_dev *dudev);
static void device_disconnect_callback(struct ddekit_usb_dev * dev);
static int add_acl(int dev_id, unsigned interfaces, endpoint_t ep);
static int del_acl(int dev_id, unsigned interaces, endpoint_t ep);
static int handle_msg(message *msg);
static void _ddekit_usb_thread();
static void device_connect_callback(struct ddekit_usb_dev * dev,
	unsigned int interfaces);

char *_ddekit_usb_get_manufacturer(struct ddekit_usb_dev *ddev);
char *_ddekit_usb_get_product(struct ddekit_usb_dev *ddev);
char *_ddekit_usb_get_serial(struct ddekit_usb_dev *ddev);
usb_device_descriptor_t *_ddekit_usb_get_device_desc(struct
	ddekit_usb_dev *ddev);
usb_interface_descriptor_t *_ddekit_usb_get_interface_desc(struct
	ddekit_usb_dev *ddev, int inum);


static ddekit_usb_malloc_fn my_malloc;
static ddekit_usb_free_fn my_free;
static struct minix_usb_driver gbl_drivers[MAX_DRIVERS];
static struct minix_usb_device _devices[MAX_DEVS];

static struct ddekit_usb_driver my_driver = {
	.completion = completion_callback,
	.connect    = device_connect_callback,
	.disconnect = device_disconnect_callback,
};


/*****************************************************************************
 *         find_driver                                                       *
 ****************************************************************************/
static struct minix_usb_driver *find_driver(endpoint_t ep) 
{
	int i;
	for (i = 0; i < MAX_DRIVERS; i++ ){
		if (gbl_drivers[i].ep == ep) {
			return &gbl_drivers[i];
		}
	}
	return NULL;
}

/*****************************************************************************
 *         find_unused_driver                                                *
 ****************************************************************************/
static struct minix_usb_driver *find_unused_driver() 
{
	int i;
	for (i = 0; i < MAX_DRIVERS; i++ ){
		if (gbl_drivers[i].status == DRIVER_UNUSED) {
			return &gbl_drivers[i];
		}
	}
	return NULL;
}

/*****************************************************************************
 *         add_to_pending_urbs                                               *
 ****************************************************************************/
static int add_to_pending_urbs(struct minix_usb_driver *drv,
                               struct ddekit_usb_urb *urb)
{
	int i;

	for (i = 0; i < MAX_URBS; i++) {
		if (drv->urbs[i] == NULL) {
			drv->urbs[i] = urb;
			return 0;
		}
	}

	return -1;
}

/*****************************************************************************
 *         remove_from_pending_urbs                                          *
 ****************************************************************************/
static int remove_from_pending_urbs(struct minix_usb_driver *drv,
                               struct ddekit_usb_urb *urb)
{
	int i;

	for (i = 0; i < MAX_URBS; i++) {
		if (drv->urbs[i] == urb) {
			drv->urbs[i] = NULL;
			return 0;
		}
	}

	return -1;
}

/*****************************************************************************
 *         find_pending_urb                                                  *
 ****************************************************************************/
static struct ddekit_usb_urb * find_pending_urb(struct minix_usb_driver *drv,
                                                unsigned urb_id) 
{
	int i;

	for (i = 0; i < MAX_URBS; i++) {
		if (((struct my_context*)drv->urbs[i]->priv)->urb_id == urb_id) {
				return drv->urbs[i];
		}
	}

	return NULL;
}

/*****************************************************************************
 *         register_driver                                                   *
 ****************************************************************************/
static void register_driver(message *msg)
{
	endpoint_t ep = msg->m_source;
	struct minix_usb_driver *drv;

	msg->m_type=USB_REPLY;

	if ( (drv = find_driver(ep)) != NULL) {
		msg->m_type = USB_REPLY;
		msg->USB_RESULT = OK;
		ipc_send(ep,msg);
	} else {
		msg->m_type = USB_REPLY;
		msg->USB_RESULT = EPERM;
		ipc_send(ep,msg);
		return; 
	}
	
	DEBUG_MSG("DRIVER %d registered \n"
	              "Announcing device %d, interfaces 0x%x\n",
				  ep,
				  drv->dev,
				  drv->interfaces);

	/* hand out the device */
	msg->m_type = USB_ANNOUCE_DEV;
	msg->USB_DEV_ID     = drv->dev;
	msg->USB_INTERFACES = drv->interfaces;
	ipc_send(ep, msg);
}

/*****************************************************************************
 *         deregister_driver                                                 *
 ****************************************************************************/
static void deregister_driver(message *msg)
{
	endpoint_t ep = msg->m_source;
	
	struct minix_usb_driver *drv;

	msg->m_type=USB_REPLY;

	if ( (drv = find_driver(ep)) == NULL) {
		DEBUG_MSG("Non-registered driver tries to unregister.");
		return; 
	} else {
		/* do not accept requests for this client anymore! */
		drv->status = DRIVER_UNUSED;

		msg->USB_RESULT = 0;
		asynsend3(ep, msg, AMF_NOREPLY);
	}
}

/*****************************************************************************
 *         ddekit_usb_urb_from_mx_urb                                        *
 ****************************************************************************/
static struct ddekit_usb_urb *ddekit_usb_urb_from_mx_urb(struct usb_urb *mx_urb)
{
	/*
	 * A helper function that generates (allocates and initializes)
	 * a ddekit_usb_urb.
	 */

	struct ddekit_usb_urb *d_urb = (struct ddekit_usb_urb *)
		my_malloc(sizeof(struct ddekit_usb_urb));

	if (d_urb == NULL) {
		return NULL;
	}

	d_urb->type           = mx_urb->type;
	d_urb->direction      = mx_urb->direction;
	d_urb->transfer_flags = mx_urb->transfer_flags;
	d_urb->size           = mx_urb->size;
	d_urb->data           = mx_urb->buffer;
	d_urb->interval       = mx_urb->interval;
	d_urb->endpoint       = mx_urb->endpoint;

	if (d_urb->type == USB_TRANSFER_CTL) {
		d_urb->setup_packet = mx_urb->setup_packet;
	}
	DEBUG_MSG("setup_package at %p", d_urb->setup_packet);

	if (d_urb->type == USB_TRANSFER_ISO) {
		d_urb->iso_desc  = (struct ddekit_usb_iso_packet_desc *)
		    mx_urb->buffer + mx_urb->iso_desc_offset;
		d_urb->number_of_packets = mx_urb->number_of_packets;
	}

	return d_urb;
}

/*****************************************************************************
 *         submit_urb                                                        *
 ****************************************************************************/
static void submit_urb(message *msg)
{
	/*
	 * submit_urb
	 *
	 * Handles a submit_urb from a minix USB device driver. It copies the 
	 * usb_urb structure containing the buffers and generates and tries to
	 * submit a ddekit_usb_urb. The reference to the ddekit_usb_urb is stored
	 * in the driver structure in order to be able to cancle the URB on the
	 * clients request.
	 */
	endpoint_t ep = msg->m_source;
	struct minix_usb_driver *drv;

	/* find driver */
	if ( (drv = find_driver(ep)) == NULL) {
		DEBUG_MSG("Non-registered driver tries to send URB.");
		return; 
	} else {

		int res;
		struct my_context *ctx = NULL;
		struct ddekit_usb_urb *d_urb = NULL;
		
		struct usb_urb *mx_urb  = (struct usb_urb*) 
		    my_malloc(msg->USB_GRANT_SIZE+sizeof(void *));

		if (mx_urb == NULL) {
			DEBUG_MSG("Can't allocat mem for mx_urb.");
			res = ENOMEM;
			goto out;
		}

		/* copy in URB */
		res = sys_safecopyfrom(ep, msg->USB_GRANT_ID, 0,
		    (vir_bytes) &mx_urb->dev_id, msg->USB_GRANT_SIZE);

		if (res != 0) {
			DEBUG_MSG("sys_safecopyfrom failed ");
			my_free(mx_urb);
			res = EINVAL;
			goto out;
		}
		
		DEBUG_MSG("URB type: %d", mx_urb->type);
		/* check if urb is valid */
		if (mx_urb->dev_id >= MAX_DEVS || mx_urb->dev_id < 0) {
			DEBUG_MSG("Bogus device ID.");
			res = EINVAL;
			goto out;
		}
		
		/* create ddekit_usb_urb */
		d_urb = ddekit_usb_urb_from_mx_urb(mx_urb);
		d_urb->dev = _devices[drv->dev].dev;
		/* submit urb */

		if (!d_urb) {
			res = ENOMEM;
			goto out;
		}
		
		ctx = my_malloc(sizeof(struct my_context));

		if(!ctx) {
			res = ENOMEM;
			goto out;
		}

		ctx->drv       = drv;
		ctx->urb_id    = drv->urb_id++;
		mx_urb->urb_id = ctx->urb_id;
		ctx->mx_urb    = mx_urb;
		ctx->d_urb     = d_urb;
		ctx->gid       = msg->USB_GRANT_ID;
		
		DEBUG_MSG("ctx: %p, urb_id: %d, d_urb: %p, mx_urb: %p, drv: %d, gid: %d ",
		    ctx, ctx->urb_id, ctx->d_urb, ctx->mx_urb, ctx->drv, ctx->gid);

		d_urb->priv = ctx;

		res = add_to_pending_urbs(drv, d_urb);
	
		if (res == 0) {
			DEBUG_MSG("submitting urb...");
			res = ddekit_usb_submit_urb(d_urb);
			if(res) {
				DEBUG_MSG("submitting urb failed (err: %d)", res);
				remove_from_pending_urbs(drv, d_urb);
			}
		}
	
out:		
		/* reply */
		msg->m_type     = USB_REPLY;
		msg->USB_URB_ID = mx_urb->urb_id;
		msg->USB_RESULT = res;

		if(res != 0) {
		
			if (mx_urb != NULL) {
				my_free(mx_urb);
			}
			if (ctx != NULL) {
				my_free(ctx);
			}

			if (d_urb != NULL) {
				my_free(d_urb);
			}
			
		}

		/* send reply */
		ipc_send(ep, msg);
	}
}


/*
 * cancle_urb
 * 
 * Cancels the submission of an URB identified by a URB_id
 */
/*****************************************************************************
 *         cancle_urb                                                        *
 ****************************************************************************/
static void cancle_urb(message *msg)
{
	endpoint_t ep = msg->m_source;

	struct minix_usb_driver *drv;

	msg->USB_RESULT = -1;
	msg->m_type = USB_REPLY;

	/* find driver */
	if ( (drv = find_driver(ep)) == NULL) {
		DEBUG_MSG("Non-registered driver tries to cancel URB.");
		return; 
	} else {
		struct ddekit_usb_urb *d_urb = NULL;

		d_urb = find_pending_urb(drv, msg->USB_URB_ID);

		if (d_urb != NULL) {
			ddekit_usb_cancle_urb(d_urb);
			msg->USB_RESULT = 0;
		} else {
			DEBUG_MSG("No URB to cancle");
			msg->USB_RESULT = ENODEV;
		}
	}

	ipc_send(ep, msg);
}


/*****************************************************************************
 *         get_info                                                          *
 *****************************************************************************/
static void
get_info(message * msg)
{
	struct minix_usb_driver * drv;
	endpoint_t ep;
	long info_type;
	long info_value;

	/* Read */
	ep		= msg->m_source;
	info_type	= msg->USB_INFO_TYPE;
	info_value	= msg->USB_INFO_VALUE;

	/* Reuse as reply */
	msg->m_type	= USB_REPLY;
	msg->USB_RESULT	= -1;

	/* Try and find driver first */
	if (NULL == (drv = find_driver(ep)))
		ddekit_printf("Non-registered driver tries to send info");
	else
		/* Route info to device */
		msg->USB_RESULT = ddekit_usb_info(_devices[drv->dev].dev,
						info_type, info_value);

	/* Reply */
	ipc_send(ep, msg);
}


/*****************************************************************************
 *         completion_callback                                               *
 ****************************************************************************/
static void completion_callback(void *priv)
{
	/*
	 * completion_callback
	 *
	 * This is called by the DDE side. Here the data is copied back to 
	 * the driver and a message is send to inform the driver about the 
	 * completion.
	 */
	message msg;
	int res;
	struct my_context *ctx       = (struct my_context *)priv;
	struct usb_urb *mx_urb       = ctx->mx_urb;
	struct ddekit_usb_urb *d_urb = ctx->d_urb;
	struct minix_usb_driver *drv = ctx->drv;

	DEBUG_MSG("ctx: %p, urb_id: %d, d_urb: %p, mx_urb: %p, drv: %d, gid: %d ",
		    ctx, ctx->urb_id, ctx->d_urb, ctx->mx_urb, ctx->drv, ctx->gid);

	/* update data in minix URB */
	mx_urb->status          = d_urb->status;
	mx_urb->actual_length   = d_urb->actual_length;
	mx_urb->error_count     = d_urb->error_count; 
	mx_urb->transfer_flags  = d_urb->transfer_flags;

	remove_from_pending_urbs(drv, d_urb);

	/* copy out URB */
	res = sys_safecopyto(drv->ep, ctx->gid, 0,
	    (vir_bytes) ((char*)mx_urb) + sizeof(void*),
		mx_urb->urb_size - sizeof(void*));

	if (res != 0) {
		DEBUG_MSG("Copy out failed: %d", res);
		DEBUG_MSG(" URB ID: %d, Grant-ID: %d, Grant-size: %d", ctx->urb_id,
		            ctx->gid, mx_urb->urb_size);
	}

	/* send message to client */
	msg.m_type     = USB_COMPLETE_URB;
	msg.USB_URB_ID = ctx->urb_id;
	asynsend3(drv->ep, &msg, AMF_NOREPLY);

	/* free stuff */
	my_free(ctx);
	my_free(mx_urb);
	my_free(d_urb);
}


/*****************************************************************************
 *         prepare_devman_usbdev                                             *
 ****************************************************************************/
static void prepare_devman_usbdev
(struct ddekit_usb_dev * dev, int dev_id, unsigned int interfaces,
 struct devman_usb_dev *dudev)
{
	int j; 
	int intf_count;
	/* 
	 * currently this is only implemented by stub driver
	 */
	
	usb_device_descriptor_t *desc = _ddekit_usb_get_device_desc(dev);
	
	dudev->manufacturer = _ddekit_usb_get_manufacturer(dev);
	dudev->product = _ddekit_usb_get_product(dev);
	dudev->serial = _ddekit_usb_get_serial(dev);

	dudev->desc = desc;
	
	intf_count = 0;

	for (j=0; j < 32; j++) {
		if (interfaces & (1 << j)) {
				dudev->interfaces[intf_count++].desc =
				   _ddekit_usb_get_interface_desc(dev, j);
		}
	}
	
	dudev->intf_count = intf_count;
	dudev->dev_id     = dev_id;
}

/*****************************************************************************
 *         device_connect_callback                                           *
 ****************************************************************************/
static void 
device_connect_callback
(struct ddekit_usb_dev * dev, unsigned int interfaces) {
	
	int i, res;

	/* add to device list */
	for (i=0; i < MAX_DEVS; i++) {
		if (_devices[i].dev == NULL)
			break;
	}

	if (i >= MAX_DEVS) {
		DEBUG_MSG("Too much devices...");
	} else {
		_devices[i].dev = dev;
		_devices[i].interfaces = (1 << interfaces);
	}

	struct devman_usb_dev *dudev;

	dudev = devman_usb_device_new(i);

	prepare_devman_usbdev(dev, i, interfaces, dudev);
	
	if (dudev == NULL) {
		/* TODO: ERROR */
		printf("ERROR: !");
	}

	ddekit_usb_dev_set_data(dev, dudev);

	res = devman_usb_device_add(dudev);
	
	if (res != 0) {
		/* TODO: Error*/
		printf("ERROR!");
	}
}

/*****************************************************************************
 *         device_disconnect_callback                                        *
 ****************************************************************************/
static void device_disconnect_callback(struct ddekit_usb_dev * dev)
{
	int i;
	
	/* remove ACL entry */
	for (i = 0; i< MAX_DRIVERS; i++) {
		if (gbl_drivers[i].dev != INVAL_DEV
			&& _devices[gbl_drivers[i].dev].dev == dev) {
			struct minix_usb_driver *drv = &gbl_drivers[i];
			drv->ep     = 0;
			drv->status = DRIVER_UNUSED;
			drv->dev    = INVAL_DEV;
		}
	}
	
	for (i=0; i < MAX_DEVS; i++) {
		if (_devices[i].dev == dev) {
			_devices[i].dev = NULL;
			_devices[i].interfaces = 0;
		}
	}


	/* get the devman device */
	struct devman_usb_dev * dudev = NULL;

	dudev = ddekit_usb_dev_get_data(dev);

	if (dudev == NULL) {
		/* TODO: error */
	}

	devman_usb_device_remove(dudev);

	/* free the devman dev */
	devman_usb_device_delete(dudev);
}


/*****************************************************************************
 *         add_acl                                                           *
 ****************************************************************************/
static int add_acl(int dev_id, unsigned interfaces, endpoint_t ep)
{
	/*
	 * This functions binds a specific USB interface to a client.
	 */
	int i;
	struct minix_usb_driver *drv;

	if (_devices[dev_id].dev == NULL) {
		/* if no device with that ID */
		return  ENODEV;
	} 
	
	/* is the device allready given to a client*/
	for (i = 0; i< MAX_DRIVERS; i++) {
		if (gbl_drivers[i].status != DRIVER_UNUSED &&
			gbl_drivers[i].dev == dev_id) {
			printf("devid: %d\n", dev_id);
			return EBUSY;
		}
	}
	
	/* bind device to client */
	drv = find_unused_driver();

	if (drv == NULL) {
		return ENOMEM;
	}
	
	drv->status     = DRIVER_BOUND;
	drv->dev        = dev_id;
	drv->interfaces = 1 << interfaces;
	drv->ep         = ep;
	drv->urb_id     = 0;

	return OK;
}

/*****************************************************************************
 *         del_acl                                                           *
 ****************************************************************************/
static int del_acl(int dev_id, unsigned interfaces, endpoint_t ep)
{
	struct minix_usb_driver *drv;
	int dev, withdraw = 0;
	message msg;

	/* find driver */
	drv = find_driver(ep);
	
	if (drv == NULL) {
		return  ENOENT;
	}

	dev = drv->dev;

	if (drv->status == DRIVER_ACTIVE) {
		withdraw = 1;
	}

	drv->ep    = 0;
	drv->status = DRIVER_UNUSED;
	drv->dev   = INVAL_DEV;
	
	if (withdraw) {
		msg.m_type     = USB_WITHDRAW_DEV;
		msg.USB_DEV_ID = dev;
		asynsend3(ep, &msg, AMF_NOREPLY);
	}

	return 0;
}

/*****************************************************************************
 *         handle_msg                                                        *
 ****************************************************************************/
static int handle_msg(message *msg)
{
	/*
	 * handle_msg 
	 *
	 * The dispatcher for USB related messages.
	 */

	switch(msg->m_type) {
		case USB_RQ_INIT:
			register_driver(msg);
			return 1;
		case USB_RQ_DEINIT:
			deregister_driver(msg);
			return 1;
		case USB_RQ_SEND_URB:
			submit_urb(msg);
			return 1;
		case USB_RQ_CANCEL_URB:
			cancle_urb(msg);
			return 1;
		case USB_RQ_SEND_INFO:
			get_info(msg);
			return 1;
		default:
			return 0;
	}
}

/*****************************************************************************
 *         devman_tread                                                      *
 ****************************************************************************/
static void devman_thread(void *unused)
{
	struct ddekit_minix_msg_q *mq = ddekit_minix_create_msg_q(DEVMAN_BASE,
	    DEVMAN_BASE + 0xff);
	int ipc_status;
	message m;
	while (1) {
		ddekit_minix_rcv(mq, &m, &ipc_status);
		devman_handle_msg(&m);
	}
}

/*****************************************************************************
 *         _ddekit_usb_thread                                                *
 ****************************************************************************/
static void _ddekit_usb_thread(void * unused)
{ 
	struct ddekit_minix_msg_q *mq = ddekit_minix_create_msg_q(USB_BASE,
	    USB_BASE + 0xff);

	message m;
	int ipc_status;

	/* create devman thread */
	ddekit_thread_t * __unused dmth;

	dmth = ddekit_thread_create(devman_thread, NULL, "devman_thread");

	while (1) {
		ddekit_minix_rcv(mq, &m, &ipc_status);
		handle_msg(&m);
	}
}


/*****************************************************************************
 *         bind_cb                                                           *
 ****************************************************************************/
static int bind_cb (struct devman_usb_bind_cb_data *data, endpoint_t ep)
{
	if(data) {
		return add_acl(data->dev_id, data->interface, ep);
	} else {
		printf("warning: missing cb_data!\n");
		return EINVAL;
	}
}

/*****************************************************************************
 *         unbind_cb                                                         *
 ****************************************************************************/
static int unbind_cb (struct devman_usb_bind_cb_data *data, endpoint_t ep)
{
	if(data) {
		return del_acl(data->dev_id, data->interface, ep);
	} else {
		printf("warning: missing cb_data!\n");
		return EINVAL;
	}
}

/*****************************************************************************
 *         ddekit_usb_server_init                                            *
 ****************************************************************************/
void ddekit_usb_server_init()
{
	int i;
	/*
	 * this function has to be called inside the context of an dedicated
	 * DDELinux thread
	 */
	devman_usb_init(bind_cb, unbind_cb);	
	ddekit_usb_init(&my_driver, &my_malloc, &my_free);
	for (i = 0; i< MAX_DRIVERS; i++) {
		gbl_drivers[i].dev = DRIVER_UNUSED;
		gbl_drivers[i].dev = INVAL_DEV;
	}
	_ddekit_usb_thread(NULL);
	
}
