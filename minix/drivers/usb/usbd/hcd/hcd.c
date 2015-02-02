/*
 * Implementation of generic HCD
 */

#include <string.h>				/* memcpy */

#include <minix/drivers.h>			/* errno with sign */

#include <usbd/hcd_common.h>
#include <usbd/hcd_ddekit.h>
#include <usbd/hcd_interface.h>
#include <usbd/hcd_schedule.h>
#include <usbd/usbd_common.h>


/*===========================================================================*
 *    Local declarations                                                     *
 *===========================================================================*/
/* Thread to handle device logic */
static void hcd_device_thread(void *);

/* Procedure that locks device thread forever in case of error/completion */
static void hcd_device_finish(hcd_device_state *, const char *);

/* Procedure that finds device, waiting for given EP interrupt */
static hcd_device_state * hcd_get_child_for_ep(hcd_device_state *, hcd_reg1);

/* For HCD level, hub handling */
static void hcd_add_child(hcd_device_state *, hcd_reg1, hcd_speed);
static void hcd_delete_child(hcd_device_state *, hcd_reg1);
static void hcd_disconnect_tree(hcd_device_state *);
static void hcd_dump_tree(hcd_device_state *, hcd_reg1);

/* Typical USD device communication procedures */
static int hcd_enumerate(hcd_device_state *);
static int hcd_get_device_descriptor(hcd_device_state *);
static int hcd_set_address(hcd_device_state *);
static int hcd_get_descriptor_tree(hcd_device_state *);
static int hcd_set_configuration(hcd_device_state *, hcd_reg1);
static void hcd_handle_urb(hcd_device_state *);
static void hcd_complete_urb(hcd_device_state *);
static int hcd_control_urb(hcd_device_state *, hcd_urb *);
static int hcd_non_control_urb(hcd_device_state *, hcd_urb *);

/* For internal use by more general methods */
static int hcd_setup_packet(hcd_device_state *, hcd_ctrlrequest *, hcd_reg1);
static int hcd_finish_setup(hcd_device_state *, void *);
static int hcd_data_transfer(hcd_device_state *, hcd_datarequest *);

/* TODO: This is not meant to be explicitly visible outside DDEKit library
 * but there is no other way to set thread priority for now */
extern void _ddekit_thread_set_myprio(int);


/*===========================================================================*
 *    Local definitions                                                      *
 *===========================================================================*/
/* TODO: This was added for compatibility with DDELinux drivers that
 * allow receiving less data than expected in URB, without error */
#define HCD_ANY_LENGTH 0xFFFFFFFFu

/* This doesn't seem to be specified in standard but abnormal values
 * are unlikely so check for this was added below */
#define HCD_SANE_DESCRIPTOR_LENGTH 2048


/*===========================================================================*
 *    hcd_handle_event                                                       *
 *===========================================================================*/
void
hcd_handle_event(hcd_device_state * device, hcd_event event, hcd_reg1 val)
{
	DEBUG_DUMP;

	/* Invalid device may be supplied */
	if (EXIT_SUCCESS != hcd_check_device(device)) {
		USB_MSG("No device available for event: 0x%02X, value: 0x%02X",
			event, val);
		return;
	}

#ifdef HCD_DUMP_DEVICE_TREE
	/* This can be unlocked to dump current USB device tree on event */
	{
		/* Go to the base of USB device tree and
		 * print the current state of it */
		hcd_device_state * base;

		base = device;

		while (NULL != base->parent)
			base = base->parent;

		USB_MSG("Current state of USB device tree:");
		hcd_dump_tree(base, 0);
	}
#endif

	/* Handle event and forward control to device thread when required */
	switch (event) {
		case HCD_EVENT_CONNECTED:
			USB_ASSERT((HCD_STATE_DISCONNECTED == device->state),
				"Device not marked as 'disconnected' "
				"for 'connection' event");

			/* Try creating new thread for device */
			if (hcd_connect_device(device, hcd_device_thread))
				USB_MSG("Device creation failed, nothing more "
					"will happen until disconnected");

			break;

		case HCD_EVENT_DISCONNECTED:
			USB_ASSERT((HCD_STATE_DISCONNECTED != device->state),
				"Device is marked as 'disconnected' "
				"for 'disconnection' event");

			/* Make this device and all attached children
			 * disconnect recursively */
			hcd_disconnect_tree(device);

			break;

		case HCD_EVENT_PORT_LS_CONNECTED:
			USB_ASSERT((HCD_STATE_DISCONNECTED != device->state),
				"Device is marked as 'disconnected' "
				"for 'hub port LS attach' event");

			USB_MSG("Low speed device connected at "
				"hub 0x%p, port %u", device, val);

			hcd_add_child(device, val, HCD_SPEED_LOW);
			break;

		case HCD_EVENT_PORT_FS_CONNECTED:
			USB_ASSERT((HCD_STATE_DISCONNECTED != device->state),
				"Device is marked as 'disconnected' "
				"for 'hub port FS attach' event");

			USB_MSG("Full speed device connected at "
				"hub 0x%p, port %u", device, val);

			hcd_add_child(device, val, HCD_SPEED_FULL);
			break;

		case HCD_EVENT_PORT_HS_CONNECTED:
			USB_ASSERT((HCD_STATE_DISCONNECTED != device->state),
				"Device is marked as 'disconnected' "
				"for 'hub port HS attach' event");

			USB_MSG("High speed device connected at "
				"hub 0x%p, port %u", device, val);

			hcd_add_child(device, val, HCD_SPEED_HIGH);
			break;

		case HCD_EVENT_PORT_DISCONNECTED:
			USB_ASSERT((HCD_STATE_DISCONNECTED != device->state),
				"Device is marked as 'disconnected' "
				"for 'hub port detach' event");

			hcd_delete_child(device, val);

			USB_MSG("Device disconnected from "
				"hub 0x%p, port %u", device, val);

			break;

		case HCD_EVENT_ENDPOINT:
			USB_ASSERT((HCD_STATE_DISCONNECTED != device->state),
				"Parent device is marked as 'disconnected' "
				"for 'endpoint' event");

			/* Alters 'device' when endpoint is allocated to
			 * child rather than parent (hub), which allows
			 * proper thread to continue */
			device = hcd_get_child_for_ep(device, val);

			/* Check if anything at all, waits for such endpoint */
			if (device)
				/* Allow device thread, waiting for endpoint
				 * event, to continue with its logic */
				hcd_device_continue(device, event, val);
			else
				USB_MSG("No device waits for endpoint %u", val);

			break;

		case HCD_EVENT_URB:
			USB_ASSERT((HCD_STATE_DISCONNECTED != device->state),
				"Device is marked as 'disconnected' "
				"for 'URB' event");

			/* Allow device thread to continue with it's logic */
			hcd_device_continue(device, event, val);

			break;

		default:
			USB_ASSERT(0, "Illegal HCD event");
	}
}


/*===========================================================================*
 *    hcd_update_port                                                        *
 *===========================================================================*/
void
hcd_update_port(hcd_driver_state * driver, hcd_event event)
{
	DEBUG_DUMP;

	switch (event) {
		case HCD_EVENT_CONNECTED:
			/* Check if already assigned */
			USB_ASSERT(NULL == driver->port_device,
				"Device was already connected before "
				"receiving 'connection' event");

			/* Assign new blank device */
			driver->port_device = hcd_new_device();

			/* Associate this device with driver */
			driver->port_device->driver = driver;
			break;

		case HCD_EVENT_DISCONNECTED:
			/* Check if already released */
			USB_ASSERT(NULL != driver->port_device,
				"Device was already disconnected before "
				"receiving 'disconnection' event");

			/* Release device */
			hcd_delete_device(driver->port_device);

			/* Clear port device pointer */
			driver->port_device = NULL;
			break;

		default:
			USB_ASSERT(0, "Illegal port update event");
	}
}


/*===========================================================================*
 *    hcd_device_thread                                                      *
 *===========================================================================*/
static void
hcd_device_thread(void * thread_args)
{
	hcd_device_state * this_device;

	DEBUG_DUMP;

	/* Set device thread priority higher so it
	 * won't change context unless explicitly locked */
	_ddekit_thread_set_myprio(2);

	/* Retrieve structures from generic data */
	this_device = (hcd_device_state *)thread_args;

	/* Enumeration sequence */
	if (EXIT_SUCCESS != hcd_enumerate(this_device))
		hcd_device_finish(this_device, "USB device enumeration failed");

	/* Tell everyone that device was connected */
	hcd_connect_cb(this_device);

	/* Fully configured */
	this_device->state = HCD_STATE_CONNECTED;

	USB_DBG("Waiting for URBs");

	/* Start handling URB's */
	for(;;) {
		/* Block and wait for something like 'submit URB' */
		hcd_device_wait(this_device, HCD_EVENT_URB, HCD_UNUSED_VAL);
		hcd_handle_urb(this_device);
	}

	/* Finish device handling to avoid leaving thread */
	hcd_device_finish(this_device, "USB device handling completed");
}


/*===========================================================================*
 *    hcd_device_finish                                                      *
 *===========================================================================*/
static void
hcd_device_finish(hcd_device_state * this_device, const char * finish_msg)
{
	DEBUG_DUMP;

	USB_MSG("USB device handling finished with message: '%s'", finish_msg);

	/* Lock forever */
	for (;;) {
		hcd_device_wait(this_device, HCD_EVENT_URB, HCD_UNUSED_VAL);
		USB_MSG("Failed attempt to continue finished thread");
	}
}


/*===========================================================================*
 *    hcd_get_child_for_ep                                                   *
 *===========================================================================*/
static hcd_device_state *
hcd_get_child_for_ep(hcd_device_state * device, hcd_reg1 ep)
{
	hcd_device_state * child_found;
	hcd_device_state * final_found;
	hcd_device_state * child;
	hcd_reg1 child_num;

	DEBUG_DUMP;

	/* Nothing yet */
	final_found = NULL;

	/* Check if any children (and their children) wait for EP event */
	/* Every device in tree is checked every time so errors can be found */
	for (child_num = 0; child_num < HCD_CHILDREN; child_num++) {
		/* Device, to be checked for EP event recursively... */
		child = device->child[child_num];

		/* ...but only if attached */
		if (NULL != child) {
			/* Look deeper first */
			child_found = hcd_get_child_for_ep(child, ep);

			if (NULL != child_found) {
				/* Only one device can wait for EP event */
				USB_ASSERT((NULL == final_found),
					"More than one device waits for EP");
				/* Remember what was found */
				final_found = child_found;
			}
		}
	}

	/* Check this device last */
	if ((HCD_EVENT_ENDPOINT == device->wait_event) &&
	    (ep == device->wait_ep)) {
		/* Only one device can wait for EP event */
		USB_ASSERT((NULL == final_found),
			"More than one device waits for EP");
		/* Remember what was found */
		final_found = device;
	}

	return final_found;
}


/*===========================================================================*
 *    hcd_add_child                                                          *
 *===========================================================================*/
static void
hcd_add_child(hcd_device_state * parent, hcd_reg1 port, hcd_speed speed)
{
	DEBUG_DUMP;

	USB_ASSERT(port < HCD_CHILDREN, "Port number too high");
	USB_ASSERT(NULL == parent->child[port], "Child device already exists");

	/* Basic addition */
	parent->child[port] = hcd_new_device();
	parent->child[port]->parent = parent;

	/* Inherit parent's driver */
	parent->child[port]->driver = parent->driver;

	/* Remember speed, determined by hub driver */
	parent->child[port]->speed = speed;

	/* Try creating new thread for device */
	if (hcd_connect_device(parent->child[port], hcd_device_thread))
		USB_MSG("Device creation failed, nothing more "
			"will happen until disconnected");
}


/*===========================================================================*
 *    hcd_delete_child                                                       *
 *===========================================================================*/
static void
hcd_delete_child(hcd_device_state * parent, hcd_reg1 port)
{
	hcd_device_state * child;

	DEBUG_DUMP;

	USB_ASSERT(port < HCD_CHILDREN, "Port number too high");

	child = parent->child[port]; /* Child to be detached */

	USB_ASSERT(NULL != child, "Child device does not exist");

	/* Make this child device and all its attached children
	 * disconnect recursively */
	hcd_disconnect_tree(child);

	/* Delete to release device itself */
	hcd_delete_device(child);

	/* Mark as released */
	parent->child[port] = NULL;
}


/*===========================================================================*
 *    hcd_disconnect_tree                                                    *
 *===========================================================================*/
static void
hcd_disconnect_tree(hcd_device_state * device)
{
	hcd_reg1 child_num;

	DEBUG_DUMP;

	/* Generate disconnect event for all children */
	for (child_num = 0; child_num < HCD_CHILDREN; child_num++) {
		if (NULL != device->child[child_num])
			hcd_handle_event(device, HCD_EVENT_PORT_DISCONNECTED,
					child_num);
	}

	/* If this device was detached during URB handling, some steps must be
	 * taken to ensure that no process/thread is waiting for completion */
	if (NULL != device->urb) {
		USB_MSG("Unplugged device had unhandled URB");
		/* Tell device driver that device was detached */
		/* TODO: ENODEV selected for that */
		device->urb->inout_status = ENODEV;
		hcd_complete_urb(device);
	}

	/* If connect callback was used before, call
	 * it's equivalent to signal disconnection */
	if (HCD_STATE_CONNECTED == device->state)
		hcd_disconnect_cb(device);

	/* Handle device disconnection (freeing memory etc.) */
	hcd_disconnect_device(device);
}


/*===========================================================================*
 *    hcd_dump_tree                                                          *
 *===========================================================================*/
static void
hcd_dump_tree(hcd_device_state * device, hcd_reg1 level)
{
	hcd_reg1 child_num;

	/* DEBUG_DUMP; */ /* Let's keep tree output cleaner */

	USB_MSG("Device on level %03u: 0x%p", level, device);

	/* Traverse device tree recursively */
	for (child_num = 0; child_num < HCD_CHILDREN; child_num++) {
		if (NULL != device->child[child_num])
			hcd_dump_tree(device->child[child_num], level + 1);
	}
}


/*===========================================================================*
 *    hcd_enumerate                                                          *
 *===========================================================================*/
static int
hcd_enumerate(hcd_device_state * this_device)
{
	hcd_driver_state * d;

	DEBUG_DUMP;

	d = this_device->driver;

	/* Having a parent device also means being reseted by it
	 * so only reset devices that have no parents */
	if (NULL == this_device->parent) {
		/* First let driver reset device */
		if (EXIT_SUCCESS != d->reset_device(d->private_data,
						&(this_device->speed))) {
			USB_MSG("Failed to reset device");
			return EXIT_FAILURE;
		}
	}

	/* Default MaxPacketSize, based on speed */
	if (HCD_SPEED_HIGH == this_device->speed)
		this_device->max_packet_size = HCD_HS_MAXPACKETSIZE;
	else
		this_device->max_packet_size = HCD_LS_MAXPACKETSIZE;

	/* Get device descriptor */
	if (EXIT_SUCCESS != hcd_get_device_descriptor(this_device)) {
		USB_MSG("Failed to get device descriptor");
		return EXIT_FAILURE;
	}

	/* Remember max packet size from device descriptor */
	this_device->max_packet_size = this_device->device_desc.bMaxPacketSize;

	/* Dump device descriptor in debug mode */
#ifdef DEBUG
	{
		hcd_device_descriptor * d;
		d = &(this_device->device_desc);

		USB_DBG("<<DEVICE>>");
		USB_DBG("bLength %02X",			d->bLength);
		USB_DBG("bDescriptorType %02X",		d->bDescriptorType);
		USB_DBG("bcdUSB %04X",			UGETW(d->bcdUSB));
		USB_DBG("bDeviceClass %02X",		d->bDeviceClass);
		USB_DBG("bDeviceSubClass %02X",		d->bDeviceSubClass);
		USB_DBG("bDeviceProtocol %02X",		d->bDeviceProtocol);
		USB_DBG("bMaxPacketSize %02X",		d->bMaxPacketSize);
		USB_DBG("idVendor %04X",		UGETW(d->idVendor));
		USB_DBG("idProduct %04X",		UGETW(d->idProduct));
		USB_DBG("bcdDevice %04X",		UGETW(d->bcdDevice));
		USB_DBG("iManufacturer %02X",		d->iManufacturer);
		USB_DBG("iProduct %02X",		d->iProduct);
		USB_DBG("iSerialNumber %02X",		d->iSerialNumber);
		USB_DBG("bNumConfigurations %02X",	d->bNumConfigurations);
	}
#endif

	/* Set reserved address */
	if (EXIT_SUCCESS != hcd_set_address(this_device)) {
		USB_MSG("Failed to set device address");
		return EXIT_FAILURE;
	}

	/* Sleep 5msec to allow addressing */
	hcd_os_nanosleep(HCD_NANOSLEEP_MSEC(5));

	/* Remember what was assigned in hardware */
	this_device->current_address = this_device->reserved_address;

	/* Get other descriptors */
	if (EXIT_SUCCESS != hcd_get_descriptor_tree(this_device)) {
		USB_MSG("Failed to get configuration descriptor tree");
		return EXIT_FAILURE;
	}

	/* TODO: Always use first configuration, as there is no support for
	 * multiple configurations in DDEKit/devman and devices rarely have
	 * more than one anyway */
	/* Set configuration */
	if (EXIT_SUCCESS != hcd_set_configuration(this_device,
				HCD_SET_CONFIG_NUM(HCD_DEFAULT_CONFIG))) {
		USB_MSG("Failed to set configuration");
		return EXIT_FAILURE;
	}

	USB_DBG("Enumeration completed");

	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    hcd_get_device_descriptor                                              *
 *===========================================================================*/
static int
hcd_get_device_descriptor(hcd_device_state * this_device)
{
	hcd_ctrlrequest setup;
	hcd_urb urb;

	DEBUG_DUMP;

	/* TODO: magic numbers, no header for these */
	/* Format setup packet */
	setup.bRequestType	= 0x80;			/* IN */
	setup.bRequest		= 0x06;			/* Get descriptor */
	setup.wValue		= 0x0100;		/* Device */
	setup.wIndex		= 0x0000;
	setup.wLength		= sizeof(this_device->device_desc);

	/* Prepare self-URB */
	memset(&urb, 0, sizeof(urb));
	urb.direction = HCD_DIRECTION_IN;
	urb.endpoint = HCD_DEFAULT_EP;
	urb.in_setup = &setup;
	urb.inout_data = (hcd_reg1 *)(&(this_device->device_desc));
	urb.target_device = this_device;
	urb.type = HCD_TRANSFER_CONTROL;

	/* Put it to be scheduled and wait for control to get back */
	hcd_schedule_internal_urb(&urb);
	hcd_device_wait(this_device, HCD_EVENT_URB, HCD_UNUSED_VAL);
	hcd_handle_urb(this_device);

	/* Check if URB submission completed successfully */
	if (urb.inout_status) {
		USB_MSG("URB submission failed");
		return EXIT_FAILURE;
	}

	/* Check if expected size was received */
	if (urb.out_size != setup.wLength) {
		USB_MSG("URB submission returned invalid amount of data");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    hcd_set_address                                                        *
 *===========================================================================*/
static int
hcd_set_address(hcd_device_state * this_device)
{
	hcd_ctrlrequest setup;
	hcd_urb urb;

	DEBUG_DUMP;

	/* Check for legal USB device address (must be non-zero as well) */
	USB_ASSERT((this_device->reserved_address > HCD_DEFAULT_ADDR) &&
		(this_device->reserved_address <= HCD_LAST_ADDR),
		"Illegal device address supplied");

	/* TODO: magic numbers, no header for these */
	setup.bRequestType	= 0x00;			/* OUT */
	setup.bRequest		= 0x05;			/* Set address */
	setup.wValue		= this_device->reserved_address;
	setup.wIndex		= 0x0000;
	setup.wLength		= 0x0000;

	/* Prepare self-URB */
	memset(&urb, 0, sizeof(urb));
	urb.direction = HCD_DIRECTION_OUT;
	urb.endpoint = HCD_DEFAULT_EP;
	urb.in_setup = &setup;
	urb.inout_data = NULL;
	urb.target_device = this_device;
	urb.type = HCD_TRANSFER_CONTROL;

	/* Put it to be scheduled and wait for control to get back */
	hcd_schedule_internal_urb(&urb);
	hcd_device_wait(this_device, HCD_EVENT_URB, HCD_UNUSED_VAL);
	hcd_handle_urb(this_device);

	/* Check if URB submission completed successfully */
	if (urb.inout_status) {
		USB_MSG("URB submission failed");
		return EXIT_FAILURE;
	}

	/* Check if expected size was received */
	if (urb.out_size != setup.wLength) {
		USB_MSG("URB submission returned invalid amount of data");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    hcd_get_descriptor_tree                                                *
 *===========================================================================*/
static int
hcd_get_descriptor_tree(hcd_device_state * this_device)
{
	hcd_config_descriptor temp_config_descriptor;
	hcd_ctrlrequest setup;
	hcd_urb urb;

	/* To receive data */
	hcd_reg4 expected_length;
	hcd_reg1 * expected_buffer;

	int retval;

	DEBUG_DUMP;

	/* Initially */
	retval = EXIT_FAILURE;
	expected_buffer = NULL;

	/* First part gets only configuration to find out total length */
	{
		/* TODO: Default configuration is hard-coded
		 * but others are rarely used anyway */
		/* TODO: magic numbers, no header for these */
		setup.bRequestType	= 0x80;		/* IN */
		setup.bRequest		= 0x06;		/* Get descriptor */
		setup.wValue		= 0x0200 | HCD_DEFAULT_CONFIG;
		setup.wIndex		= 0x0000;
		setup.wLength		= sizeof(temp_config_descriptor);

		/* Prepare self-URB */
		memset(&urb, 0, sizeof(urb));
		urb.direction = HCD_DIRECTION_IN;
		urb.endpoint = HCD_DEFAULT_EP;
		urb.in_setup = &setup;
		urb.inout_data = (hcd_reg1 *)(&temp_config_descriptor);
		urb.target_device = this_device;
		urb.type = HCD_TRANSFER_CONTROL;

		/* Put it to be scheduled and wait for control to get back */
		hcd_schedule_internal_urb(&urb);
		hcd_device_wait(this_device, HCD_EVENT_URB, HCD_UNUSED_VAL);
		hcd_handle_urb(this_device);

		/* Check if URB submission completed successfully */
		if (urb.inout_status) {
			USB_MSG("URB submission failed");
			goto FINISH;
		}

		/* Check if expected size was received */
		if (urb.out_size != setup.wLength) {
			USB_MSG("URB submission returned "
				"invalid amount of data");
			goto FINISH;
		}
	}

	/* Get total expected length */
	expected_length = UGETW(temp_config_descriptor.wTotalLength);

	/* Check for abnormal value */
	if (expected_length > HCD_SANE_DESCRIPTOR_LENGTH) {
		USB_MSG("Total descriptor length declared is too high");
		goto FINISH;
	}

	/* Get descriptor buffer to hold everything expected */
	if (NULL == (expected_buffer = malloc(expected_length))) {
		USB_MSG("Descriptor allocation failed");
		goto FINISH;
	}

	/* Second part gets all available descriptors */
	{
		/* TODO: Default configuration is hard-coded
		 * but others are rarely used anyway */
		/* TODO: magic numbers, no header for these */
		setup.bRequestType	= 0x80;		/* IN */
		setup.bRequest		= 0x06;		/* Get descriptor */
		setup.wValue		= 0x0200 | HCD_DEFAULT_CONFIG;
		setup.wIndex		= 0x0000;
		setup.wLength		= expected_length;

		/* Prepare self-URB */
		memset(&urb, 0, sizeof(urb));
		urb.direction = HCD_DIRECTION_IN;
		urb.endpoint = HCD_DEFAULT_EP;
		urb.in_setup = &setup;
		urb.inout_data = expected_buffer;
		urb.target_device = this_device;
		urb.type = HCD_TRANSFER_CONTROL;

		/* Put it to be scheduled and wait for control to get back */
		hcd_schedule_internal_urb(&urb);
		hcd_device_wait(this_device, HCD_EVENT_URB, HCD_UNUSED_VAL);
		hcd_handle_urb(this_device);

		/* Check if URB submission completed successfully */
		if (urb.inout_status) {
			USB_MSG("URB submission failed");
			goto FINISH;
		}

		/* Check if expected size was received */
		if (urb.out_size != setup.wLength) {
			USB_MSG("URB submission returned "
				"invalid amount of data");
			goto FINISH;
		}
	}

	if (EXIT_SUCCESS != hcd_buffer_to_tree(expected_buffer,
						(int)expected_length,
						&(this_device->config_tree))) {
		USB_MSG("Broken descriptor data");
		goto FINISH;
	}

	/* No errors occurred */
	retval = EXIT_SUCCESS;

	FINISH:

	/* Release allocated buffer */
	if (expected_buffer)
		free(expected_buffer);

	return retval;
}


/*===========================================================================*
 *    hcd_set_configuration                                                  *
 *===========================================================================*/
static int
hcd_set_configuration(hcd_device_state * this_device, hcd_reg1 configuration)
{
	hcd_ctrlrequest setup;
	hcd_urb urb;

	DEBUG_DUMP;

	/* TODO: magic numbers, no header for these */
	setup.bRequestType	= 0x00;		/* OUT */
	setup.bRequest		= 0x09;		/* Set configuration */
	setup.wValue		= configuration;
	setup.wIndex		= 0x0000;
	setup.wLength		= 0x0000;

	/* Prepare self-URB */
	memset(&urb, 0, sizeof(urb));
	urb.direction = HCD_DIRECTION_OUT;
	urb.endpoint = HCD_DEFAULT_EP;
	urb.in_setup = &setup;
	urb.inout_data = NULL;
	urb.target_device = this_device;
	urb.type = HCD_TRANSFER_CONTROL;

	/* Put it to be scheduled and wait for control to get back */
	hcd_schedule_internal_urb(&urb);
	hcd_device_wait(this_device, HCD_EVENT_URB, HCD_UNUSED_VAL);
	hcd_handle_urb(this_device);

	return urb.inout_status;
}


/*===========================================================================*
 *    hcd_handle_urb                                                         *
 *===========================================================================*/
static void
hcd_handle_urb(hcd_device_state * this_device)
{
	hcd_urb * urb;
	int transfer_status;

	DEBUG_DUMP;

	/* Retrieve URB */
	urb = this_device->urb;

	USB_ASSERT(NULL != urb, "No URB supplied");
	USB_ASSERT(this_device == urb->target_device, "Unknown device for URB");

	/* Only if URB parsing was completed... */
	if (EXIT_SUCCESS == urb->inout_status) {

		transfer_status = EXIT_FAILURE;

		/* ...check for URB to handle */
		switch (urb->type) {
			case HCD_TRANSFER_CONTROL:
				transfer_status = hcd_control_urb(
							this_device, urb);
				break;

			case HCD_TRANSFER_BULK:
			case HCD_TRANSFER_INTERRUPT:
				transfer_status = hcd_non_control_urb(
							this_device, urb);
				break;

			default:
				USB_MSG("Unsupported transfer type 0x%02X",
							(int)urb->type);
				break;
		}

		/* In case of error, only dump message */
		if (EXIT_SUCCESS != transfer_status)
			USB_MSG("USB transfer failed");

	} else
		USB_MSG("Invalid URB supplied");

	/* Perform completion routine */
	hcd_complete_urb(this_device);
}


/*===========================================================================*
 *    hcd_complete_urb                                                       *
 *===========================================================================*/
static void
hcd_complete_urb(hcd_device_state * this_device)
{
	DEBUG_DUMP;

	/* Signal scheduler that URB was handled */
	this_device->urb->handled(this_device->urb);

	/* Use this callback in case it is an external URB */
	hcd_completion_cb(this_device->urb);

	/* Make device forget about this URB */
	this_device->urb = NULL;
}


/*===========================================================================*
 *    hcd_control_urb                                                        *
 *===========================================================================*/
static int
hcd_control_urb(hcd_device_state * this_device, hcd_urb * urb)
{
	DEBUG_DUMP;

	/* Assume bad values unless something different occurs later */
	urb->inout_status = EINVAL;

	/* Must have setup packet for control transfer */
	if (NULL == urb->in_setup) {
		USB_MSG("No setup packet in URB, for control transfer");
		return EXIT_FAILURE;
	}

	/* TODO: Only EP0 can have control transfer */
	if (HCD_DEFAULT_EP != urb->endpoint) {
		USB_MSG("Control transfer for non zero EP");
		return EXIT_FAILURE;
	}

	/* Setup and URB directions should match */
	if (((urb->in_setup->bRequestType >> 7) & 0x01) != urb->direction) {
		USB_MSG("URB Direction mismatch");
		return EXIT_FAILURE;
	}

	/* Send setup packet */
	if (EXIT_SUCCESS != hcd_setup_packet(this_device, urb->in_setup,
						urb->endpoint)) {
		USB_MSG("Sending URB setup packet, failed");
		urb->inout_status = EPIPE;
		return EXIT_FAILURE;
	}

	/* Put what was read back into URB */
	if (EXIT_SUCCESS != hcd_finish_setup(this_device, urb->inout_data))
		return EXIT_FAILURE;

	/* Write transfer output info to URB */
	urb->out_size = (hcd_reg4)this_device->control_len;
	urb->inout_status = EXIT_SUCCESS;

	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    hcd_non_control_urb                                                    *
 *===========================================================================*/
static int
hcd_non_control_urb(hcd_device_state * this_device, hcd_urb * urb)
{
	hcd_endpoint * e;
	hcd_datarequest request;

	DEBUG_DUMP;

	/* Assume bad values unless something different occurs later */
	urb->inout_status = EINVAL;

	/* Must have data buffer to send/receive */
	if (NULL == urb->inout_data) {
		USB_MSG("No data packet in URB");
		return EXIT_FAILURE;
	}

	if (HCD_DEFAULT_EP == urb->endpoint) {
		USB_MSG("Non-control transfer for EP0");
		return EXIT_FAILURE;
	}

	/* Check if EP number is valid within remembered descriptor tree */
	e = hcd_tree_find_ep(&(this_device->config_tree), urb->endpoint);

	if (NULL == e) {
		USB_MSG("Invalid EP number for this device");
		return EXIT_FAILURE;
	}

	/* Check if remembered descriptor direction, matches the one in URB */
	if (((e->descriptor.bEndpointAddress >> 7) & 0x01) != urb->direction) {
		USB_MSG("EP direction mismatch");
		return EXIT_FAILURE;
	}

	/* Check if remembered type matches */
	if (UE_GET_XFERTYPE(e->descriptor.bmAttributes) != urb->type) {
		USB_MSG("EP type mismatch");
		return EXIT_FAILURE;
	}

	/* Check if remembered interval matches */
	if ((hcd_reg1)e->descriptor.bInterval != urb->interval) {
		USB_MSG("EP interval mismatch");
		return EXIT_FAILURE;
	}

	/* Assign URB values to data request structure */
	request.type = urb->type;
	request.endpoint = urb->endpoint;
	request.direction = urb->direction;
	request.data_left = (int)urb->in_size;
	request.data = urb->inout_data;
	/* TODO: This was changed to allow software scheduler to work correctly
	 * by switching URBs when they NAK, rather than waiting forever if URB
	 * which requires such waiting, was issued */
#if 0
	request.interval = urb->interval;
#else
	request.interval = HCD_DEFAULT_NAKLIMIT;
#endif

	/* Assign to let know how much data can be transfered at a time */
	request.max_packet_size = UGETW(e->descriptor.wMaxPacketSize);

	/* Let know how to configure EP for speed */
	request.speed = this_device->speed;

	/* Start sending data */
	if (EXIT_SUCCESS != hcd_data_transfer(this_device, &request)) {
		USB_MSG("URB non-control transfer, failed");
		urb->inout_status = EPIPE;
		return EXIT_FAILURE;
	}

	/* Transfer successfully completed update URB */
	USB_ASSERT(request.data_left >= 0,
		"Negative amount of transfer data remains");
	urb->out_size = urb->in_size - (hcd_reg4)request.data_left;
	urb->inout_status = EXIT_SUCCESS;

	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    hcd_setup_packet                                                       *
 *===========================================================================*/
static int
hcd_setup_packet(hcd_device_state * this_device, hcd_ctrlrequest * setup,
		hcd_reg1 ep)
{
	hcd_driver_state * d;
	hcd_reg1 * current_byte;
	int rx_len;

	DEBUG_DUMP;

	/* Should have been set at enumeration or with default values */
	USB_ASSERT(this_device->max_packet_size >= HCD_LS_MAXPACKETSIZE,
		"Illegal MaxPacketSize");
	USB_ASSERT(ep <= HCD_LAST_EP, "Invalid EP number");
	USB_ASSERT(this_device->current_address <= HCD_LAST_ADDR,
		"Invalid device address");

	/* Initially... */
	d = this_device->driver;
	current_byte = this_device->control_data;/* Start reading into this */
	this_device->control_len = 0;		/* Nothing read yet */

	/* Set parameters for further communication */
	d->setup_device(d->private_data, ep, this_device->current_address,
			NULL, NULL);

	/* Send setup packet */
	d->setup_stage(d->private_data, setup);

	/* Wait for response */
	hcd_device_wait(this_device, HCD_EVENT_ENDPOINT, ep);

	/* Check response */
	if (EXIT_SUCCESS != d->check_error(d->private_data,
					HCD_TRANSFER_CONTROL,
					ep,
					HCD_DIRECTION_UNUSED))
		return EXIT_FAILURE;

	/* For data packets... */
	if (setup->wLength > 0) {

		/* TODO: magic number */
		/* ...IN data packets */
		if (setup->bRequestType & 0x80) {

			for(;;) {

				/* Try getting data */
				d->in_data_stage(d->private_data);

				/* Wait for response */
				hcd_device_wait(this_device,
						HCD_EVENT_ENDPOINT, ep);

				/* Check response */
				if (EXIT_SUCCESS != d->check_error(
							d->private_data,
							HCD_TRANSFER_CONTROL,
							ep,
							HCD_DIRECTION_UNUSED))
					return EXIT_FAILURE;

				/* Read data received as response */
				rx_len = d->read_data(d->private_data,
						current_byte, ep);

				/* Increment */
				current_byte += rx_len;
				this_device->control_len += rx_len;

				/* If max sized packet was read (or more)... */
				if (rx_len >= (int)this_device->max_packet_size)
					/* ...try reading next packet even if
					 * zero bytes may be received */
					continue;

				/* If less than max data was read... */
				if (rx_len < (int)this_device->max_packet_size)
					/* ...it must have been
					 * the last packet */
					break;

				/* Unreachable during normal operation */
				USB_MSG("rx_len: %d; max_packet_size: %d",
					rx_len, this_device->max_packet_size);
				USB_ASSERT(0, "Illegal state of data "
					"receive operation");
			}

		} else {
			/* TODO: Unimplemented OUT DATA stage */
			d->out_data_stage(d->private_data);

			return EXIT_FAILURE;
		}
	}

	/* Status stages */
	if (setup->bRequestType & 0x80) {

		/* Try confirming data receive */
		d->out_status_stage(d->private_data);

		/* Wait for response */
		hcd_device_wait(this_device, HCD_EVENT_ENDPOINT, ep);

		/* Check response */
		if (EXIT_SUCCESS != d->check_error(d->private_data,
						HCD_TRANSFER_CONTROL,
						ep,
						HCD_DIRECTION_UNUSED))
			return EXIT_FAILURE;

	} else {

		/* Try getting status confirmation */
		d->in_status_stage(d->private_data);

		/* Wait for response */
		hcd_device_wait(this_device, HCD_EVENT_ENDPOINT, ep);

		/* Check response */
		if (EXIT_SUCCESS != d->check_error(d->private_data,
						HCD_TRANSFER_CONTROL,
						ep,
						HCD_DIRECTION_UNUSED))
			return EXIT_FAILURE;

		/* Read zero data from response to clear registers */
		if (0 != d->read_data(d->private_data, NULL, ep))
			return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    hcd_finish_setup                                                       *
 *===========================================================================*/
static int
hcd_finish_setup(hcd_device_state * this_device, void * output)
{
	DEBUG_DUMP;

	/* Validate setup transfer output length */
	if (this_device->control_len < 0) {
		USB_MSG("Negative control transfer output length");
		return EXIT_FAILURE;
	}

	/* Length is valid but output not supplied */
	if (NULL == output)
		return EXIT_SUCCESS;

	/* Finally, copy when needed */
	memcpy(output, this_device->control_data, this_device->control_len);

	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    hcd_data_transfer                                                      *
 *===========================================================================*/
static int
hcd_data_transfer(hcd_device_state * this_device, hcd_datarequest * request)
{
	hcd_driver_state * d;
	hcd_datarequest temp_req;
	int transfer_len;

	DEBUG_DUMP;

	USB_ASSERT((request->endpoint <= HCD_LAST_EP) &&
		(request->endpoint > HCD_DEFAULT_EP),
		"Invalid EP number");
	USB_ASSERT((this_device->current_address <= HCD_LAST_ADDR) &&
		(this_device->current_address > HCD_DEFAULT_ADDR),
		"Invalid device address");

	/* Initially... */
	d = this_device->driver;

	/* Set parameters for further communication */
	d->setup_device(d->private_data, request->endpoint,
			this_device->current_address,
			&(this_device->ep_tx_tog[request->endpoint]),
			&(this_device->ep_rx_tog[request->endpoint]));

	/* Check transfer direction first */
	if (HCD_DIRECTION_IN == request->direction) {

		do {
			/* Start actual data transfer */
			d->rx_stage(d->private_data, request);

			/* Wait for response */
			hcd_device_wait(this_device, HCD_EVENT_ENDPOINT,
					request->endpoint);

			/* Check response */
			if (EXIT_SUCCESS != d->check_error(d->private_data,
							request->type,
							request->endpoint,
							HCD_DIRECTION_IN))
				return EXIT_FAILURE;

			/* Read data received as response */
			transfer_len = d->read_data(d->private_data,
						request->data,
						request->endpoint);

			request->data_left -= transfer_len;
			request->data += transfer_len;

			/* Total length shall not become negative */
			if (request->data_left < 0) {
				USB_MSG("Invalid amount of data received");
				return EXIT_FAILURE;
			}

		} while (0 != request->data_left);

	} else if (HCD_DIRECTION_OUT == request->direction) {

		do {
			temp_req = *request;

			/* Decide temporary transfer size */
			if (temp_req.data_left > (int)temp_req.max_packet_size)
				temp_req.data_left =
					(int)temp_req.max_packet_size;

			/* Alter actual transfer size */
			request->data += temp_req.data_left;
			request->data_left -= temp_req.data_left;

			/* Total length shall not become negative */
			USB_ASSERT(request->data_left >= 0,
				"Invalid amount of transfer data calculated");

			/* Start actual data transfer */
			d->tx_stage(d->private_data, &temp_req);

			/* Wait for response */
			hcd_device_wait(this_device, HCD_EVENT_ENDPOINT,
					request->endpoint);

			/* Check response */
			if (EXIT_SUCCESS != d->check_error(d->private_data,
							request->type,
							request->endpoint,
							HCD_DIRECTION_OUT))
				return EXIT_FAILURE;

		} while (0 != request->data_left);

	} else
		USB_ASSERT(0, "Invalid transfer direction");

	return EXIT_SUCCESS;
}
