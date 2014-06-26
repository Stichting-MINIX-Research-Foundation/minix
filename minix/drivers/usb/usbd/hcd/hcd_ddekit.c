/*
 * Implementation of DDEkit related calls/data
 */

#include <string.h>				/* memset */

#include <ddekit/usb.h>

#include <usbd/hcd_ddekit.h>
#include <usbd/hcd_interface.h>
#include <usbd/hcd_schedule.h>
#include <usbd/usbd_common.h>


/*===========================================================================*
 *    Local declarations                                                     *
 *===========================================================================*/
/*
 * In this file "struct ddekit_usb_dev" equals "hcd_device_state"
 */
struct ddekit_usb_device_id;
struct ddekit_usb_urb;
struct ddekit_usb_dev;

/* Translates DDEKit UBR to one used by HCD */
static void hcd_decode_urb(hcd_urb *, struct ddekit_usb_urb *);
static void hcd_encode_urb(hcd_urb *, struct ddekit_usb_urb *);

/* HCD's URB create/destroy */
static hcd_urb * hcd_new_urb(void);
static void hcd_free_urb(hcd_urb *);

/* Decodes event from received info */
static void hcd_decode_info(long, long, hcd_event *, hcd_reg1 *);


/*===========================================================================*
 *    Global definitions                                                     *
 *===========================================================================*/
ddekit_usb_completion_cb	completion_cb	= NULL;
ddekit_usb_connect_cb		connect_cb	= NULL;
ddekit_usb_disconnect_cb	disconnect_cb	= NULL;


/*===========================================================================*
 *    Implementation for usb_server.c                                        *
 *===========================================================================*/

/*===========================================================================*
 *    _ddekit_usb_get_manufacturer                                           *
 *===========================================================================*/
char *
_ddekit_usb_get_manufacturer(struct ddekit_usb_dev * ddev)
{
	static const char mfg[] = "UNKNOWN";
	DEBUG_DUMP;
	/* TODO: UNUSED for argument won't work */
	((void)ddev);
	return (char *)mfg;
}


/*===========================================================================*
 *    _ddekit_usb_get_product                                                *
 *===========================================================================*/
char *
_ddekit_usb_get_product(struct ddekit_usb_dev * ddev)
{
	static const char prod[] = "UNKNOWN";
	DEBUG_DUMP;
	/* TODO: UNUSED for argument won't work */
	((void)ddev);
	return (char *)prod;
}


/*===========================================================================*
 *    _ddekit_usb_get_serial                                                 *
 *===========================================================================*/
char *
_ddekit_usb_get_serial(struct ddekit_usb_dev * ddev)
{
	static const char serial[] = "UNKNOWN";
	DEBUG_DUMP;
	/* TODO: UNUSED for argument won't work */
	((void)ddev);
	return (char *)serial;
}


/*===========================================================================*
 *    _ddekit_usb_get_device_desc                                            *
 *===========================================================================*/
struct usb_device_descriptor *
_ddekit_usb_get_device_desc(struct ddekit_usb_dev * ddev)
{
	hcd_device_state * dev;

	DEBUG_DUMP;

	dev = (hcd_device_state *)ddev;

	return (struct usb_device_descriptor *)
		(&(dev->config_tree.descriptor));
}


/*===========================================================================*
 *    _ddekit_usb_get_interface_desc                                         *
 *===========================================================================*/
struct usb_interface_descriptor *
_ddekit_usb_get_interface_desc(struct ddekit_usb_dev * ddev, int inum)
{
	hcd_device_state * dev;

	DEBUG_DUMP;

	dev = (hcd_device_state *)ddev;

	return (struct usb_interface_descriptor *)
		(&(dev->config_tree.interface[inum].descriptor));
}


/*===========================================================================*
 *    Implementation for <ddekit/usb.h>                                      *
 *===========================================================================*/

/*===========================================================================*
 *    ddekit_usb_dev_set_data                                                *
 *===========================================================================*/
int
ddekit_usb_dev_set_data(struct ddekit_usb_dev * dev, void * data)
{
	hcd_device_state * hcd_dev;

	DEBUG_DUMP;

	hcd_dev = (hcd_device_state *)dev;

	hcd_dev->data = data;

	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    ddekit_usb_dev_get_data                                                *
 *===========================================================================*/
void *
ddekit_usb_dev_get_data(struct ddekit_usb_dev * dev)
{
	hcd_device_state * hcd_dev;

	DEBUG_DUMP;

	hcd_dev = (hcd_device_state *)dev;

	return hcd_dev->data;
}


/* TODO: This was in 'ddekit/usb.h' header file, but is not used anywhere */
#if 0
/*===========================================================================*
 *    ddekit_usb_get_device_id                                               *
 *===========================================================================*/
void
ddekit_usb_get_device_id(struct ddekit_usb_dev * dev,
			struct ddekit_usb_device_id * id)
{
	DEBUG_DUMP;
	/* TODO: UNUSED for argument won't work */
	((void)dev);
	((void)id);
	return;
}
#endif


/*===========================================================================*
 *    ddekit_usb_submit_urb                                                  *
 *===========================================================================*/
int
ddekit_usb_submit_urb(struct ddekit_usb_urb * d_urb)
{
	hcd_urb * urb;

	DEBUG_DUMP;

	/* Get new URB */
	urb = hcd_new_urb();

	/* Turn DDEKit URB format to one that is easier to
	 * handle by HCD, also check if URB is valid */
	hcd_decode_urb(urb, d_urb);

	/* Add URB to scheduler */
	return hcd_schedule_external_urb(urb);
}


/*===========================================================================*
 *    ddekit_usb_cancle_urb                                                  *
 *===========================================================================*/
int
ddekit_usb_cancle_urb(struct ddekit_usb_urb * d_urb)
{
	DEBUG_DUMP;
	/* TODO: UNUSED for argument won't work */
	((void)d_urb);
	/* TODO: No driver will require this any time soon */
	USB_ASSERT(0, "URB cancellation not supported");
	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    ddekit_usb_info                                                        *
 *===========================================================================*/
long
ddekit_usb_info(struct ddekit_usb_dev * dev, long type, long value)
{
	hcd_event event;
	hcd_reg1 val;

	DEBUG_DUMP;

	/* Decode event */
	hcd_decode_info(type, value, &event, &val);

	if (HCD_EVENT_INVALID == event) {
		USB_MSG("Invalid info message received");
		return EXIT_FAILURE;
	} else {
		/* Let HCD handle info message */
		hcd_handle_event((hcd_device_state *)dev, event, val);
		return EXIT_SUCCESS;
	}
}


/*===========================================================================*
 *    ddekit_usb_init                                                        *
 *===========================================================================*/
int
ddekit_usb_init(struct ddekit_usb_driver * drv,
		ddekit_usb_malloc_fn * _m,
		ddekit_usb_free_fn * _f)
{
	DEBUG_DUMP;

	completion_cb	= drv->completion;
	connect_cb	= drv->connect;
	disconnect_cb	= drv->disconnect;

	*_m		= malloc;
	*_f		= free;

	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    hcd_connect_cb                                                         *
 *===========================================================================*/
void
hcd_connect_cb(hcd_device_state * dev)
{
	unsigned int if_bitmask;

	DEBUG_DUMP;

	/* TODO: Magic numbers like in ddekit/devman */
	/* Each bit starting from 0, represents valid interface */
	if_bitmask = 0xFFFFFFFF >> (32 - dev->config_tree.num_interfaces);

	USB_DBG("Interfaces %d, mask %08X",
		dev->config_tree.num_interfaces,
		if_bitmask);

	connect_cb((struct ddekit_usb_dev *)dev, (int)if_bitmask);
}


/*===========================================================================*
 *    hcd_disconnect_cb                                                      *
 *===========================================================================*/
void
hcd_disconnect_cb(hcd_device_state * dev)
{
	DEBUG_DUMP;

	disconnect_cb((struct ddekit_usb_dev *)dev);
}


/*===========================================================================*
 *    hcd_completion_cb                                                      *
 *===========================================================================*/
void
hcd_completion_cb(hcd_urb * urb)
{
	struct ddekit_usb_urb * d_urb;

	DEBUG_DUMP;

	/* Recollect original URB */
	d_urb = (struct ddekit_usb_urb *)urb->original_urb;

	/* Original URB will not be NULL if URB
	 * was external (from device driver) */
	if (NULL != d_urb) {
		/* Turn HCD URB format to one handled by DDEKit */
		hcd_encode_urb(urb, d_urb);

		/* No need for this URB anymore */
		hcd_free_urb(urb);

		completion_cb(d_urb->priv);
	}
}


/*===========================================================================*
 *    hcd_decode_urb                                                         *
 *===========================================================================*/
static void
hcd_decode_urb(hcd_urb * urb, struct ddekit_usb_urb * dde_urb)
{
	DEBUG_DUMP;

	/* Remember original */
	urb->original_urb = (void *)dde_urb;

	/* No UBR error initially */
	urb->inout_status = EXIT_SUCCESS;

	/* Check transfer direction */
	switch (dde_urb->direction) {
		case DDEKIT_USB_IN:
			urb->direction = HCD_DIRECTION_IN;
			break;
		case DDEKIT_USB_OUT:
			urb->direction = HCD_DIRECTION_OUT;
			break;
		default:
			USB_MSG("URB direction error");
			goto URB_ERROR;
	}

	/* Check transfer type */
	switch (dde_urb->type) {
		case DDEKIT_USB_TRANSFER_ISO:
			urb->type = HCD_TRANSFER_ISOCHRONOUS;
			break;
		case DDEKIT_USB_TRANSFER_INT:
			urb->type = HCD_TRANSFER_INTERRUPT;
			break;
		case DDEKIT_USB_TRANSFER_CTL:
			urb->type = HCD_TRANSFER_CONTROL;
			break;
		case DDEKIT_USB_TRANSFER_BLK:
			urb->type = HCD_TRANSFER_BULK;
			break;
		default:
			USB_MSG("URB type error");
			goto URB_ERROR;
	}

	/* Check transfer endpoint validity */
	if ((dde_urb->endpoint <= (int)HCD_LAST_EP) &&
		(dde_urb->endpoint >= (int)HCD_DEFAULT_EP))
		urb->endpoint = (hcd_reg1)dde_urb->endpoint;
	else {
		USB_MSG("URB endpoint error");
		goto URB_ERROR;
	}

	/* Check transfer interval validity */
	if ((dde_urb->interval <= (int)HCD_HIGHEST_INTERVAL) &&
		(dde_urb->interval >= (int)HCD_LOWEST_INTERVAL))
		urb->interval = (hcd_reg1)dde_urb->interval;
	else {
		USB_MSG("URB interval error");
		goto URB_ERROR;
	}

	/* TODO: Alignment of setup packet. Can DDE client guarantee that? */
	/* Transfer data assignment */
	urb->inout_data = (hcd_reg1 *)dde_urb->data;
	urb->in_setup = (hcd_ctrlrequest *)dde_urb->setup_packet;

	/* TODO: Sane size check? */
	urb->in_size = (hcd_reg4)dde_urb->size;

	/* Buffer validity check */
	if ((NULL == urb->inout_data) && (NULL == urb->in_setup)) {
		USB_MSG("URB buffer error");
		goto URB_ERROR;
	}

	/* Remember device and check for NULL */
	if (NULL == (urb->target_device = (hcd_device_state *)dde_urb->dev)) {
		USB_MSG("URB device pointer error");
		goto URB_ERROR;
	}

	/* Decoding completed */
	return;

	URB_ERROR:
	urb->inout_status = EXIT_FAILURE;
}


/*===========================================================================*
 *    hcd_encode_urb                                                         *
 *===========================================================================*/
static void
hcd_encode_urb(hcd_urb * urb, struct ddekit_usb_urb * dde_urb)
{
	DEBUG_DUMP;

	/* Data buffers are the same, no need to copy */
	/* Rewrite output for DDEKit part */
	dde_urb->actual_length = urb->out_size;
	dde_urb->status = urb->inout_status;
}


/*===========================================================================*
 *    hcd_new_urb                                                            *
 *===========================================================================*/
static hcd_urb *
hcd_new_urb(void)
{
	DEBUG_DUMP;
	return malloc(sizeof(hcd_urb));
}


/*===========================================================================*
 *    hcd_free_urb                                                           *
 *===========================================================================*/
static void
hcd_free_urb(hcd_urb * urb)
{
	DEBUG_DUMP;
	free(urb);
}


/*===========================================================================*
 *    hcd_decode_info                                                        *
 *===========================================================================*/
static void
hcd_decode_info(long type, long invalue, hcd_event * event, hcd_reg1 * outvalue)
{
	DEBUG_DUMP;

	USB_ASSERT((invalue >= 0) && (invalue <= 0xFF),
		"Illegal USB info value received");

	switch ((ddekit_msg_type_t)type) {
		case DDEKIT_HUB_PORT_LS_CONN:
			*event = HCD_EVENT_PORT_LS_CONNECTED;
			break;
		case DDEKIT_HUB_PORT_FS_CONN:
			*event = HCD_EVENT_PORT_FS_CONNECTED;
			break;
		case DDEKIT_HUB_PORT_HS_CONN:
			*event = HCD_EVENT_PORT_HS_CONNECTED;
			break;
		case DDEKIT_HUB_PORT_DISCONN:
			*event = HCD_EVENT_PORT_DISCONNECTED;
			break;
		default:
			*event = HCD_EVENT_INVALID;
			break;
	}

	*outvalue = (hcd_reg1)invalue;
}
