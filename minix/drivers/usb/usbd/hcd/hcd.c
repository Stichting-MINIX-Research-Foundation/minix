/*
 * Implementation of generic HCD
 */

#include <string.h>				/* memcpy */

#include <minix/drivers.h>			/* errno with sign */

#include <usbd/hcd_common.h>
#include <usbd/hcd_ddekit.h>
#include <usbd/hcd_interface.h>
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

/* Typical USD device communication procedures */
static int hcd_enumerate(hcd_device_state *);
static int hcd_get_device_descriptor(hcd_device_state *);
static int hcd_set_address(hcd_device_state *, hcd_reg1);
static int hcd_get_descriptor_tree(hcd_device_state *);
static int hcd_set_configuration(hcd_device_state *, hcd_reg1);
static void hcd_handle_urb(hcd_device_state *, hcd_urb *);
static int hcd_control_urb(hcd_device_state *, hcd_urb *);
static int hcd_non_control_urb(hcd_device_state *, hcd_urb *);

/* For internal use by more general methods */
static int hcd_setup_packet(hcd_device_state *, hcd_ctrlrequest *, hcd_reg1);
static int hcd_finish_setup(hcd_device_state *, void *, hcd_reg4);
static int hcd_data_transfer(hcd_device_state *, hcd_datarequest *);


/*===========================================================================*
 *    Local definitions                                                      *
 *===========================================================================*/
/* Array of available devices */
static hcd_device_state hcd_device[HCD_NUM_MAX_DEVICES];

/* TODO: This was added for compatibility with DDELinux drivers that
 * allow receiving less data than expected in URB, without error */
#define HCD_ANY_LENGTH 0xFFFFFFFFu


/*===========================================================================*
 *    hcd_handle_event                                                       *
 *===========================================================================*/
void
hcd_handle_event(hcd_device_state * device, hcd_event event, hcd_reg1 val)
{
	DEBUG_DUMP;

	/* No device may be supplied */
	if (NULL == device) {
		USB_MSG("No device available for event: 0x%02X, value: 0x%02X",
			event, val);
		return;
	}

	/* Handle event and forward control to device thread when required */
	switch (event) {
		case HCD_EVENT_CONNECTED:
			USB_ASSERT((HCD_STATE_DISCONNECTED == device->state),
				"Device not marked as 'disconnected' "
				"for 'connection' event");

			/* Mark as 'plugged in' first */
			device->state = HCD_STATE_CONNECTION_PENDING;

			/* Try creating new thread for device */
			if (hcd_connect_device(device, hcd_device_thread))
				USB_MSG("Device creation failed, nothing more "
					"will happen until disconnected");

			break;

		case HCD_EVENT_DISCONNECTED:
			USB_ASSERT((HCD_STATE_DISCONNECTED != device->state),
				"Device is marked as 'disconnected' "
				"for 'disconnection' event");

			/* If connect callback was used before, call
			 * it's equivalent to signal disconnection */
			if (HCD_STATE_CONNECTED == device->state)
				hcd_disconnect_cb(device);

			hcd_disconnect_device(device);
			device->state = HCD_STATE_DISCONNECTED;

			/* Finally, zero everything to allow
			 * further connections with this object */
			memset(device, 0x00, sizeof(*device));

			break;

		case HCD_EVENT_PORT_CONNECTED:
			/* TODO: Actual port handling */
			USB_MSG("Device connected to hub's port");
			break;

		case HCD_EVENT_PORT_DISCONNECTED:
			/* TODO: Actual port handling */
			USB_MSG("Device disconnected from hub's port");
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
	hcd_device_state * d;
	int devnum;

	const int maxdevs = (sizeof(hcd_device) / sizeof(hcd_device[0]));

	DEBUG_DUMP;

	switch (event) {
		case HCD_EVENT_CONNECTED:
			/* Check if already assigned */
			USB_ASSERT(NULL == driver->port_device,
				"Device was already connected before "
				"receiving 'connection' event");

			/* For short */
			d = hcd_device;

			/* Find first unused device */
			for (devnum = 0; devnum < maxdevs; devnum++) {
				if (HCD_STATE_DISCONNECTED == d[devnum].state) {
					/* Assign free device */
					driver->port_device = &(d[devnum]);
					/* Associate this device with driver */
					driver->port_device->driver = driver;
					/* Device found, exit */
					return;
				}
			}

			/* Nothing found */
			USB_MSG("Device limit reached, no more "
				"free device structures to use");

			break;

		case HCD_EVENT_DISCONNECTED:
			/* Check if already released */
			USB_ASSERT(NULL != driver->port_device,
				"Device was already disconnected before "
				"receiving 'disconnection' event");

			/* Clear port device */
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
		hcd_handle_urb(this_device, this_device->urb);
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
	DEBUG_DUMP;

	/* TODO: When device has multiple children (hub),
	 * this will allow routing interrupt login downstream */
	((void)ep);

	return device;
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

	/* First let driver reset device */
	if (EXIT_SUCCESS != d->reset_device(d->private_data,
					&(this_device->speed))) {
		USB_MSG("Failed to reset device");
		return EXIT_FAILURE;
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

	/* TODO: Dynamic device addressing should be added here, when more
	 * than one device can be handled at a time */

	/* Set address */
	if (EXIT_SUCCESS != hcd_set_address(this_device, HCD_ATTACHED_ADDR)) {
		USB_MSG("Failed to set device address");
		return EXIT_FAILURE;
	}

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

	DEBUG_DUMP;

	/* TODO: magic numbers, no header for these */

	/* Format setup packet */
	setup.bRequestType	= 0x80;			/* IN */
	setup.bRequest		= 0x06;			/* Get descriptor */
	setup.wValue		= 0x0100;		/* Device */
	setup.wIndex		= 0x0000;
	setup.wLength		= sizeof(this_device->device_desc);

	/* Handle formatted setup packet */
	if (EXIT_SUCCESS != hcd_setup_packet(this_device, &setup,
						HCD_DEFAULT_EP)) {
		USB_MSG("Handling setup packet failed");
		return EXIT_FAILURE;
	}

	/* Put what was read in device descriptor */
	if (EXIT_SUCCESS != hcd_finish_setup(this_device,
					&(this_device->device_desc),
					sizeof(this_device->device_desc)))
		return EXIT_FAILURE;

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

	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    hcd_set_address                                                        *
 *===========================================================================*/
static int
hcd_set_address(hcd_device_state * this_device, hcd_reg1 address)
{
	hcd_ctrlrequest setup;

	DEBUG_DUMP;

	/* Check for legal USB device address (must be non-zero as well) */
	USB_ASSERT((address > HCD_DEFAULT_ADDR) && (address <= HCD_LAST_ADDR),
		"Illegal device address supplied");

	/* TODO: magic numbers, no header for these */
	setup.bRequestType	= 0x00;			/* OUT */
	setup.bRequest		= 0x05;			/* Set address */
	setup.wValue		= address;
	setup.wIndex		= 0x0000;
	setup.wLength		= 0x0000;

	/* Handle formatted setup packet */
	if (EXIT_SUCCESS != hcd_setup_packet(this_device, &setup,
						HCD_DEFAULT_EP)) {
		USB_MSG("Handling setup packet failed");
		return EXIT_FAILURE;
	}

	/* Sleep 5msec to allow addressing */
	hcd_os_nanosleep(HCD_NANOSLEEP_MSEC(5));

	/* Remember what was assigned in hardware */
	this_device->address = address;

	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    hcd_get_descriptor_tree                                                *
 *===========================================================================*/
static int
hcd_get_descriptor_tree(hcd_device_state * this_device)
{
	hcd_config_descriptor config_descriptor;
	hcd_ctrlrequest setup;
	hcd_reg4 total_length;
	hcd_reg4 buffer_length;
	int completed;

	DEBUG_DUMP;

	/* First, ask only for configuration itself to get length info */
	buffer_length = sizeof(config_descriptor);
	completed = 0;

	do {
		/* TODO: Default configuration is hard-coded
		 * but others are rarely used anyway */
		/* TODO: magic numbers, no header for these */
		setup.bRequestType	= 0x80;		/* IN */
		setup.bRequest		= 0x06;		/* Get descriptor */
		setup.wValue		= 0x0200 | HCD_DEFAULT_CONFIG;
		setup.wIndex		= 0x0000;
		setup.wLength		= buffer_length;

		/* Handle formatted setup packet */
		if (EXIT_SUCCESS != hcd_setup_packet(this_device, &setup,
							HCD_DEFAULT_EP)) {
			USB_MSG("Handling setup packet failed");
			return EXIT_FAILURE;
		}

		/* If we only asked for configuration itself
		 * then ask again for other descriptors */
		if (sizeof(config_descriptor) == buffer_length) {

			/* Put what was already read in configuration
			 * descriptor for analysis */
			if (EXIT_SUCCESS != hcd_finish_setup(this_device,
							&config_descriptor,
							buffer_length))
				return EXIT_FAILURE;

			/* Continue only if there is more data */
			total_length = UGETW(config_descriptor.wTotalLength);

			if (total_length < sizeof(config_descriptor)) {
				/* This should never happen for a fine device */
				USB_MSG("Illegal wTotalLength value");
				return EXIT_FAILURE;
			} else if (sizeof(config_descriptor) == total_length) {
				/* Nothing more was in descriptor anyway */
				completed = 1;
			} else {
				/* Read whatever is needed */
				buffer_length = total_length;
			}

		} else {
			/* All data for given configuration was read */
			completed = 1;
		}
	}
	while (!completed);

	/* Validate... */
	if (EXIT_SUCCESS != hcd_finish_setup(this_device, NULL, total_length))
		return EXIT_FAILURE;

	/* ... and create tree based on received buffer */
	if (EXIT_SUCCESS != hcd_buffer_to_tree(this_device->control_data,
						this_device->control_len,
						&(this_device->config_tree))) {
		/* This should never happen for a fine device */
		USB_MSG("Illegal descriptor values");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    hcd_set_configuration                                                  *
 *===========================================================================*/
static int
hcd_set_configuration(hcd_device_state * this_device, hcd_reg1 configuration)
{
	hcd_ctrlrequest setup;

	DEBUG_DUMP;

	/* TODO: magic numbers, no header for these */
	setup.bRequestType	= 0x00;		/* OUT */
	setup.bRequest		= 0x09;		/* Set configuration */
	setup.wValue		= configuration;
	setup.wIndex		= 0x0000;
	setup.wLength		= 0x0000;

	/* Handle formatted setup packet */
	if (EXIT_SUCCESS != hcd_setup_packet(this_device, &setup,
						HCD_DEFAULT_EP)) {
		USB_MSG("Handling setup packet failed");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    hcd_handle_urb                                                         *
 *===========================================================================*/
static void
hcd_handle_urb(hcd_device_state * this_device, hcd_urb * urb)
{
	int transfer_status;

	DEBUG_DUMP;

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

	/* Signal scheduler that URB was handled */
	urb->handled();
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
	if (EXIT_SUCCESS != hcd_finish_setup(this_device,
					urb->inout_data,
					HCD_ANY_LENGTH))
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
	request.interval = urb->interval;

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
	USB_ASSERT(this_device->address <= HCD_LAST_ADDR,
		"Invalid device address");

	/* Initially... */
	d = this_device->driver;
	current_byte = this_device->control_data;/* Start reading into this */
	this_device->control_len = 0;		/* Nothing read yet */

	/* Set parameters for further communication */
	d->setup_device(d->private_data, ep, this_device->address);

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
hcd_finish_setup(hcd_device_state * this_device, void * output,
		hcd_reg4 expected)
{
	DEBUG_DUMP;

	/* Validate setup transfer output length */
	if (this_device->control_len < 0) {
		USB_MSG("Negative control transfer output length");
		return EXIT_FAILURE;
	}

	/* In case it is required... */
	if (HCD_ANY_LENGTH != expected) {
		/* ...check for expected length */
		if ((hcd_reg4)this_device->control_len != expected) {
			USB_MSG("Control transfer output length mismatch:"
				"len %d, expected %u", this_device->control_len,
				expected);
			return EXIT_FAILURE;
		}

		/* Valid but there is no need to copy anything */
		if (0u == expected)
			return EXIT_SUCCESS;
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
	USB_ASSERT((this_device->address <= HCD_LAST_ADDR) &&
		(this_device->address > HCD_DEFAULT_ADDR),
		"Invalid device address");

	/* Initially... */
	d = this_device->driver;

	/* Set parameters for further communication */
	d->setup_device(d->private_data, request->endpoint,
			this_device->address);

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
