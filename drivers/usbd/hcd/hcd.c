/*
 * Implementation of generic HCD
 */

#include <time.h>				/* nanosleep */
#include <string.h>				/* memcpy */

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

/* For internal use by more general methods */
static int hcd_setup_packet(hcd_device_state *, hcd_ctrlrequest *);


/*===========================================================================*
 *    Local definitions                                                      *
 *===========================================================================*/
/* TODO: Only one device at a time */
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
		driver->event, this_device->state);

	/* Set what was received for device thread to use */
	this_device->driver = driver;

	/* Handle event and forward control to device thread when required */
	switch (driver->event) {
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

	/* No URB's yet */
	this_device->urb = NULL;

	/* Start handling URB's */
	for(;;) {
		/* Block and wait for something like 'submit URB' */
		hcd_device_wait(this_device);

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
		hcd_device_wait(this_device);
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
	d->reset_device(d->private_data);

	/* Set parameters for further communication */
	d->setup_device(d->private_data, HCD_DEFAULT_EP, HCD_DEFAULT_ADDR);

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
	d->setup_device(d->private_data, HCD_DEFAULT_EP, HCD_ATTACHED_ADDR);

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

	/* Output VID/PID when debugging */
	USB_DBG("idVendor: %02X%02X", this_device->device_desc.idVendor[1],
					this_device->device_desc.idVendor[0]);
	USB_DBG("idProduct: %02X%02X", this_device->device_desc.idProduct[1],
					this_device->device_desc.idProduct[0]);

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

	{
		/* Sleep 5ms for proper addressing */
		struct timespec nanotm = {0, HCD_NANOSLEEP_MSEC(5)};
		nanosleep(&nanotm, NULL);
	}

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
	DEBUG_DUMP;

	USB_ASSERT(NULL != this_device->urb, "NULL URB received");

	/* TODO: URB handling will be here */

	/* TODO: call completion */
	/* hcd_completion_cb */

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
	hcd_device_wait(this_device);

	/* Check response */
	if (EXIT_SUCCESS != d->check_error(d->private_data))
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
				hcd_device_wait(this_device);

				/* Check response */
				if (EXIT_SUCCESS != d->check_error(
							d->private_data))
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
		hcd_device_wait(this_device);

		/* Check response */
		if (EXIT_SUCCESS != d->check_error(d->private_data))
			return EXIT_FAILURE;

	} else {

		/* Try getting status confirmation */
		d->in_status_stage(d->private_data);

		/* Wait for response */
		hcd_device_wait(this_device);

		/* Check response */
		if (EXIT_SUCCESS != d->check_error(d->private_data))
			return EXIT_FAILURE;

		/* Read zero data from response to clear registers */
		if (0 != d->read_data(d->private_data, NULL, 0))
			return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
