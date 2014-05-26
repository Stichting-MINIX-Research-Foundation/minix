/*
 * Implementation of DDEkit related calls/data
 */

#include <string.h>				/* memset */

#include <ddekit/usb.h>

#include <usb/hcd_ddekit.h>
#include <usb/hcd_interface.h>
#include <usb/usb_common.h>


/*===========================================================================*
 *    Local declarations                                                     *
 *===========================================================================*/
/*
 * In this file "struct ddekit_usb_dev" equals "hcd_device_state"
 * */
struct ddekit_usb_device_id;
struct ddekit_usb_urb;
struct ddekit_usb_dev;

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


/* TODO: This was in header file but is not used anywhere */
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
	hcd_device_state * dev;
	hcd_driver_state * drv;

	DEBUG_DUMP;

	urb = (hcd_urb *)d_urb;
	dev = (hcd_device_state *)(urb->dev);
	drv = (hcd_driver_state *)(dev->driver);

	dev->urb = urb;
	drv->current_event = HCD_EVENT_URB;

	/* TODO: URB's must be queued somewhere */
	hcd_handle_event(drv);

	return EXIT_SUCCESS;
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
	return EXIT_SUCCESS;
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
void hcd_connect_cb(hcd_device_state * dev)
{
	unsigned int if_bitmask;

	DEBUG_DUMP;

	/* TODO: magic numbers like in ddekit/devman */
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
void hcd_disconnect_cb(hcd_device_state * dev)
{
	DEBUG_DUMP;

	disconnect_cb((struct ddekit_usb_dev *)dev);
}


/*===========================================================================*
 *    hcd_completion_cb                                                      *
 *===========================================================================*/
void hcd_completion_cb(void * priv)
{
	DEBUG_DUMP;

	completion_cb(priv);
}
