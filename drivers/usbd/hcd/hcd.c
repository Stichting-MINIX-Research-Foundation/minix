/*
 * Implementation of generic HCD
 */

#include <string.h>				/* memcpy */

#include <minix/drivers.h>			/* errno with sign */
#include <minix/usb.h>				/* USB_TRANSFER_CTL...  */

#include <usb/hcd_common.h>
#include <usb/hcd_ddekit.h>
#include <usb/hcd_interface.h>
#include <usb/usb_common.h>


/*===========================================================================*
 *    Local declarations                                                     *
 *===========================================================================*/
/* Thread to handle device logic */
static void hcd_device_thread(void *);

/* Procedure that locks device thread forever in case of error/completion */
static void hcd_device_finish(hcd_device_state *, const char *);

/* Typical USD device communication procedures */
static int hcd_enumerate(hcd_device_state *);
static int hcd_get_device_descriptor(hcd_device_state *);
static int hcd_set_address(hcd_device_state *, int);
static int hcd_get_descriptor_tree(hcd_device_state *);
static int hcd_set_configuration(hcd_device_state *, int);
static int hcd_handle_urb(hcd_device_state *);
static int hcd_control_urb(hcd_device_state *);
static int hcd_non_control_urb(hcd_device_state *, int);

/* For internal use by more general methods */
static int hcd_setup_packet(hcd_device_state *, hcd_ctrlrequest *);
static int hcd_data_transfer(hcd_device_state *, hcd_datarequest *);


/*===========================================================================*
 *    Local definitions                                                      *
 *===========================================================================*/
/* TODO: Only one device at a time
 * If ever HUB functionality is added, one must remember that disconnecting
 * HUB, means disconnecting every device attached to it, so data structure may
 * have to be altered to allow that */
static hcd_device_state hcd_device[1];


/*===========================================================================*
 *    hcd_handle_event                                                       *
 *===========================================================================*/
void
hcd_handle_event(hcd_driver_state * driver)
{
	hcd_device_state * this_device;

	DEBUG_DUMP;

	/* TODO: Finding which hcd_device is in use should be performed here */
	this_device = &(hcd_device[0]);

	/* Sometimes interrupts occur in a weird order (EP after disconnect)
	 * This helps finding ordering errors in DEBUG */
	USB_DBG("Event: 0x%02X, state: 0x%02X",
		driver->current_event, this_device->state);

	/* Set what was received for device thread to use */
	this_device->driver = driver;

	/* Handle event and forward control to device thread when required */
	switch (driver->current_event) {
		case HCD_EVENT_CONNECTED:
			if (HCD_STATE_DISCONNECTED == this_device->state) {
				if (EXIT_SUCCESS != hcd_connect_device(
							this_device,
							hcd_device_thread))
					USB_MSG("Device creation failed");
			} else
				USB_MSG("Device not marked as 'disconnected' "
					"for 'connection' event");

			break;

		case HCD_EVENT_DISCONNECTED:
			if (HCD_STATE_DISCONNECTED != this_device->state) {
				/* If connect callback was used before, call
				 * it's equivalent to signal disconnection */
				if (HCD_STATE_CONNECTED == this_device->state)
					hcd_disconnect_cb(this_device);
				hcd_disconnect_device(this_device);
				this_device->state = HCD_STATE_DISCONNECTED;
			} else
				USB_MSG("Device is marked as 'disconnected' "
					"for 'disconnection' event");

			break;

		case HCD_EVENT_ENDPOINT:
		case HCD_EVENT_URB:
			/* Allow device thread to continue with it's logic */
			if (HCD_STATE_DISCONNECTED != this_device->state)
				hcd_device_continue(this_device);
			else
				USB_MSG("Device is marked as 'disconnected' "
					"for 'EP' event");

			break;

		default:
			USB_ASSERT(0, "Illegal HCD event");
			break;
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

	/* Plugged in */
	this_device->state = HCD_STATE_CONNECTION_PENDING;

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
		/* No URB's yet */
		this_device->urb = NULL;

		/* Block and wait for something like 'submit URB' */
		hcd_device_wait(this_device, HCD_EVENT_URB, HCD_NO_ENDPOINT);

		if (EXIT_SUCCESS != hcd_handle_urb(this_device))
			hcd_device_finish(this_device, "URB handling failed");
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
		hcd_device_wait(this_device, HCD_EVENT_URB, HCD_NO_ENDPOINT);
		USB_MSG("Failed attempt to continue finished thread");
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

	/* First let driver reset device */
	if (EXIT_SUCCESS != d->reset_device(d->private_data,
					&(this_device->speed))) {
		USB_MSG("Failed to reset device");
		return EXIT_FAILURE;
	}

	/* Get device descriptor */
	if (EXIT_SUCCESS != hcd_get_device_descriptor(this_device)) {
		USB_MSG("Failed to get device descriptor");
		return EXIT_FAILURE;
	}

	/* TODO: dynamic device address when more devices are available */

	/* Set address */
	if (EXIT_SUCCESS != hcd_set_address(this_device, HCD_ATTACHED_ADDR)) {
		USB_MSG("Failed to set device address");
		return EXIT_FAILURE;
	}

	/* Set parameters for further communication */
	d->setup_device(d->private_data, HCD_DEFAULT_EP, this_device->address);

	/* Get other descriptors */
	if (EXIT_SUCCESS != hcd_get_descriptor_tree(this_device)) {
		USB_MSG("Failed to get configuration descriptor tree");
		return EXIT_FAILURE;
	}

	/* TODO: always first configuration */
	/* Set configuration */
	if (EXIT_SUCCESS != hcd_set_configuration(this_device, 0x01)) {
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
	if (EXIT_SUCCESS != hcd_setup_packet(this_device, &setup)) {
		USB_MSG("Handling setup packet failed");
		return EXIT_FAILURE;
	}

	/* Put what was read in device descriptor */
	memcpy(&(this_device->device_desc), this_device->buffer,
		sizeof(this_device->device_desc));

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
hcd_set_address(hcd_device_state * this_device, int address)
{
	hcd_ctrlrequest setup;

	DEBUG_DUMP;

	USB_ASSERT((address > 0) && (address < 128), "Illegal address");

	/* TODO: magic numbers, no header for these */
	setup.bRequestType	= 0x00;			/* OUT */
	setup.bRequest		= 0x05;			/* Set address */
	setup.wValue		= address;
	setup.wIndex		= 0x0000;
	setup.wLength		= 0x0000;

	/* Handle formatted setup packet */
	if (EXIT_SUCCESS != hcd_setup_packet(this_device, &setup)) {
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
	int completed;
	int total_length;
	int buffer_length;

	DEBUG_DUMP;

	/* First, ask only for configuration itself to get length info */
	buffer_length = sizeof(config_descriptor);
	completed = 0;

	do {
		/* TODO: configuration 0 is hard-coded
		 * but others are rarely used anyway */
		/* TODO: magic numbers, no header for these */
		setup.bRequestType	= 0x80;		/* IN */
		setup.bRequest		= 0x06;		/* Get descriptor */
		setup.wValue		= 0x0200;	/* Configuration 0 */
		setup.wIndex		= 0x0000;
		setup.wLength		= buffer_length;

		/* Handle formatted setup packet */
		if (EXIT_SUCCESS != hcd_setup_packet(this_device, &setup)) {
			USB_MSG("Handling setup packet failed");
			return EXIT_FAILURE;
		}

		/* If we only asked for configuration itself
		 * then ask again for other descriptors */
		if (sizeof(config_descriptor) == buffer_length) {

			/* Put what was read in configuration descriptor */
			memcpy(&config_descriptor, this_device->buffer,
				sizeof(config_descriptor));

			/* Continue only if there is more data */
			total_length = config_descriptor.wTotalLength[0] +
				(config_descriptor.wTotalLength[1] << 8);

			if (total_length < (int)sizeof(config_descriptor)) {
				/* This should never happen for a fine device */
				USB_MSG("Illegal wTotalLength value");
				return EXIT_FAILURE;
			}
			else if (sizeof(config_descriptor) == total_length) {
				/* Nothing more was in descriptor anyway */
				completed = 1;
			}
			else {
				/* Read whatever is needed */
				buffer_length = total_length;
			}

		} else {
			/* All data for given configuration was read */
			completed = 1;
		}
	}
	while (!completed);

	/* Create tree based on received buffer */
	if (EXIT_SUCCESS != hcd_buffer_to_tree(this_device->buffer,
						this_device->data_len,
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
hcd_set_configuration(hcd_device_state * this_device, int configuration)
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
	if (EXIT_SUCCESS != hcd_setup_packet(this_device, &setup)) {
		USB_MSG("Handling setup packet failed");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    hcd_handle_urb                                                         *
 *===========================================================================*/
static int
hcd_handle_urb(hcd_device_state * this_device)
{
	hcd_urb * urb;
	int transfer_status;

	DEBUG_DUMP;

	transfer_status = EXIT_FAILURE;
	urb = this_device->urb;

	USB_ASSERT(NULL != urb, "NULL URB given");
	/* TODO: One device only */
	USB_ASSERT((void *)this_device != (void *)urb->dev,
		"Unknown device for URB");

	switch (urb->type) {

		case USB_TRANSFER_CTL:
			transfer_status = hcd_control_urb(this_device);
			break;

		case USB_TRANSFER_BLK:
		case USB_TRANSFER_INT:
			transfer_status = hcd_non_control_urb(this_device,
								urb->type);
			break;

		case USB_TRANSFER_ISO:
			/* TODO: ISO transfer */
			USB_MSG("ISO transfer not supported");
			break;

		default:
			USB_MSG("Invalid transfer type 0x%X", urb->type);
			break;
	}

	if (EXIT_SUCCESS != transfer_status)
		USB_MSG("USB transfer failed");

	/* Call completion regardless of status */
	hcd_completion_cb(urb->priv);

	/* TODO: Only critical failures should ever yield EXIT_FAILURE, so
	 * return is not bound to transfer_status for now, to let device
	 * driver act accordingly */
	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    hcd_control_urb                                                        *
 *===========================================================================*/
static int
hcd_control_urb(hcd_device_state * this_device)
{
	hcd_urb * urb;
	hcd_ctrlrequest setup;

	DEBUG_DUMP;

	urb = this_device->urb;

	/* Assume bad values unless something different occurs later */
	urb->status = EINVAL;

	/* Must have setup packet */
	if (NULL == urb->setup_packet) {
		USB_MSG("No setup packet in URB, for control transfer");
		return EXIT_FAILURE;
	}

	/* TODO: Only EP0 can have control transfer */
	if (0 != urb->endpoint) {
		USB_MSG("Control transfer for non zero EP");
		return EXIT_FAILURE;
	}

	/* Hold setup packet and analyze it */
	memcpy(&setup, urb->setup_packet, sizeof(setup));

	/* TODO: broken constants for urb->direction (USB_OUT...) */
	if (((setup.bRequestType >> 7) & 0x01) != urb->direction) {
		USB_MSG("URB Direction mismatch");
		return EXIT_FAILURE;
	}

	/* Send setup packet */
	if (EXIT_SUCCESS != hcd_setup_packet(this_device, &setup)) {
		USB_MSG("Sending URB setup packet, failed");
		urb->status = EPIPE;
		return EXIT_FAILURE;
	}

	urb->status = EXIT_SUCCESS;
	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    hcd_non_control_urb                                                    *
 *===========================================================================*/
static int
hcd_non_control_urb(hcd_device_state * this_device, int type)
{
	hcd_endpoint * e;
	hcd_datarequest request;
	hcd_urb * urb;

	DEBUG_DUMP;

	urb = this_device->urb;

	/* Assume bad values unless something different occurs later */
	urb->status = EINVAL;

	if (NULL == urb->data) {
		USB_MSG("No data packet in URB");
		return EXIT_FAILURE;
	}

	if ((UE_GET_ADDR(urb->endpoint) >= 16) ||
		(UE_GET_ADDR(urb->endpoint) <= 0)) {
		USB_MSG("Illegal EP number");
		return EXIT_FAILURE;
	}

	/* TODO: broken USB_IN... constants */
	if ((1 != urb->direction) && (0 != urb->direction)) {
		USB_MSG("Illegal EP direction");
		return EXIT_FAILURE;
	}

	/* TODO: usb.h constants to type mapping */
	switch (type) {
		case USB_TRANSFER_BLK:
			request.type = HCD_TRANSFER_BULK;
			break;
		case USB_TRANSFER_INT:
			request.type = HCD_TRANSFER_INTERRUPT;
			break;
		default:
			/* TODO: ISO transfer */
			USB_MSG("Invalid transfer type");
			return EXIT_FAILURE;
	}

	/* TODO: Any additional checks? (sane size?) */

	/* Assign to data request structure */
	request.endpoint = urb->endpoint;
	request.direction = urb->direction;
	request.size = (int)urb->size;
	request.data = urb->data;
	request.interval = urb->interval;

	/* Check if EP number is valid */
	e = hcd_tree_find_ep(&(this_device->config_tree), request.endpoint);

	if (NULL == e) {
		USB_MSG("Invalid EP value");
		return EXIT_FAILURE;
	}

	/* TODO: broken constants for urb->direction (USB_OUT...) */
	/* Check if remembered direction matches */
	if (((e->descriptor.bEndpointAddress >> 7) & 0x01) != urb->direction) {
		USB_MSG("EP direction mismatch");
		return EXIT_FAILURE;
	}

	/* Check if remembered type matches */
	if (UE_GET_XFERTYPE(e->descriptor.bmAttributes) != (int)request.type) {
		USB_MSG("EP type mismatch");
		return EXIT_FAILURE;
	}

	/* Assign to let know how much data can be transfered at a time */
	request.max_packet_size = UGETW(e->descriptor.wMaxPacketSize);

	/* Let know how to configure EP for speed */
	request.speed = this_device->speed;

	/* Start sending data */
	if (EXIT_SUCCESS != hcd_data_transfer(this_device, &request)) {
		USB_MSG("URB non-control transfer, failed");
		urb->status = EPIPE;
		return EXIT_FAILURE;
	}

	/* Transfer successfully completed */
	urb->status = EXIT_SUCCESS;
	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    hcd_setup_packet                                                       *
 *===========================================================================*/
static int
hcd_setup_packet(hcd_device_state * this_device, hcd_ctrlrequest * setup)
{
	hcd_driver_state * d;
	hcd_reg1 * current_byte;
	int expected_len;
	int received_len;

	DEBUG_DUMP;

	/* Initially... */
	d = this_device->driver;
	expected_len = (int)setup->wLength;
	current_byte = this_device->buffer;

	/* Send setup packet */
	d->setup_stage(d->private_data, setup);

	/* Wait for response */
	hcd_device_wait(this_device, HCD_EVENT_ENDPOINT, HCD_ENDPOINT_0);

	/* Check response */
	if (EXIT_SUCCESS != d->check_error(d->private_data,
					HCD_TRANSFER_CONTROL,
					HCD_DIRECTION_UNUSED))
		return EXIT_FAILURE;

	/* For data packets... */
	if (expected_len > 0) {

		/* TODO: magic number */
		/* ...IN data packets */
		if (setup->bRequestType & 0x80) {

			/* What was received until now */
			this_device->data_len = 0;

			do {
				/* Try getting data */
				d->in_data_stage(d->private_data);

				/* Wait for response */
				hcd_device_wait(this_device,
						HCD_EVENT_ENDPOINT,
						HCD_ENDPOINT_0);

				/* Check response */
				if (EXIT_SUCCESS != d->check_error(
							d->private_data,
							HCD_TRANSFER_CONTROL,
							HCD_DIRECTION_UNUSED))
					return EXIT_FAILURE;

				/* Read data received as response */
				received_len = d->read_data(d->private_data,
							current_byte, 0);

				/* Data reading should always yield positive
				 * results for proper setup packet */
				if (received_len > 0) {
					/* Try next packet */
					this_device->data_len += received_len;
					current_byte += received_len;
				} else
					return EXIT_FAILURE;

			} while (expected_len > this_device->data_len);

			/* Should be exactly what we requested, no more */
			if (this_device->data_len != expected_len) {
				USB_MSG("Received more data than expected");
				return EXIT_FAILURE;
			}

		} else {
			/* TODO: unimplemented */
			USB_MSG("Illegal non-zero length OUT setup packet");
			return EXIT_FAILURE;
		}
	}

	/* Status stages */
	if (setup->bRequestType & 0x80) {

		/* Try confirming data receive */
		d->out_status_stage(d->private_data);

		/* Wait for response */
		hcd_device_wait(this_device, HCD_EVENT_ENDPOINT,
				HCD_ENDPOINT_0);

		/* Check response */
		if (EXIT_SUCCESS != d->check_error(d->private_data,
						HCD_TRANSFER_CONTROL,
						HCD_DIRECTION_UNUSED))
			return EXIT_FAILURE;

	} else {

		/* Try getting status confirmation */
		d->in_status_stage(d->private_data);

		/* Wait for response */
		hcd_device_wait(this_device, HCD_EVENT_ENDPOINT,
				HCD_ENDPOINT_0);

		/* Check response */
		if (EXIT_SUCCESS != d->check_error(d->private_data,
						HCD_TRANSFER_CONTROL,
						HCD_DIRECTION_UNUSED))
			return EXIT_FAILURE;

		/* Read zero data from response to clear registers */
		if (0 != d->read_data(d->private_data, NULL, 0))
			return EXIT_FAILURE;
	}

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

	/* Initially... */
	d = this_device->driver;

	/* Set parameters for further communication */
	d->setup_device(d->private_data,
			request->endpoint,
			this_device->address);

	/* TODO: broken USB_IN... constants */
	if (1 == request->direction) {

		do {
			/* Start actual data transfer */
			d->rx_stage(d->private_data, request);

			/* Wait for response */
			hcd_device_wait(this_device, HCD_EVENT_ENDPOINT,
					request->endpoint);

			/* Check response */
			if (EXIT_SUCCESS != d->check_error(d->private_data,
							request->type,
							HCD_DIRECTION_IN))
				return EXIT_FAILURE;

			/* Read data received as response */
			transfer_len = d->read_data(d->private_data,
						(hcd_reg1 *)request->data,
						request->endpoint);

			request->size -= transfer_len;
			request->data += transfer_len;

			/* Total length shall not become negative */
			if (request->size < 0) {
				USB_MSG("Invalid amount of data received");
				return EXIT_FAILURE;
			}

#ifdef DEBUG
			/* TODO: REMOVEME (dumping of data transfer) */
			{
				int i;
				USB_MSG("RECEIVED: %d", transfer_len);
				for (i = 0; i < transfer_len; i++)
					USB_MSG("0x%02X: %c",
					(request->data-transfer_len)[i],
					(request->data-transfer_len)[i]);
			}
#endif

		} while (0 != request->size);

	} else if (0 == request->direction) {

		do {
			temp_req = *request;

			/* Decide transfer size */
			if (temp_req.size > (int)temp_req.max_packet_size) {
				temp_req.size = temp_req.max_packet_size;
			}

			request->data += temp_req.size;
			request->size -= temp_req.size;

			/* Total length shall not become negative */
			USB_ASSERT(request->size >= 0,
				"Invalid amount of transfer data calculated");

			/* Start actual data transfer */
			d->tx_stage(d->private_data, &temp_req);

			/* Wait for response */
			hcd_device_wait(this_device, HCD_EVENT_ENDPOINT,
					request->endpoint);

			/* Check response */
			if (EXIT_SUCCESS != d->check_error(d->private_data,
							request->type,
							HCD_DIRECTION_OUT))
				return EXIT_FAILURE;

		} while (0 != request->size);

	} else
		USB_ASSERT(0, "Invalid transfer direction");

	return EXIT_SUCCESS;
}
