/*
 * Minix3 USB hub driver implementation
 */

#include <string.h>			/* memset */
#include <stdint.h>
#include <time.h>			/* nanosleep */

#include <ddekit/thread.h>
#include <minix/sef.h>
#include <minix/sysutil.h>		/* panic */
#include <minix/usb.h>			/* usb_ctrlrequest TODO: remove me */

#include "common.h"
#include "urb_helper.h"


/*---------------------------*
 *    declared functions     *
 *---------------------------*/
/* TODO: these are missing from DDE header files */
extern void ddekit_minix_wait_exit(void);
extern void ddekit_shutdown(void);

/* SEF related functions */
static int hub_sef_hdlr(int, sef_init_info_t *);
static void hub_signal_handler(int);

/* DDEKit IPC related */
static void ddekit_usb_task(void *);

/* DDEKit's USB driver callbacks */
static void usb_driver_completion(void *);
static void usb_driver_connect(struct ddekit_usb_dev *, unsigned int);
static void usb_driver_disconnect(struct ddekit_usb_dev *);

/* Hub driver main task */
static void hub_task(void *);


/*---------------------------*
 *    class specific stuff   *
 *---------------------------*/
#define HUB_PACKED __attribute__((__packed__))

/* How often to check for changes */
#define USB_HUB_POLLING_INTERVAL 1000

/* Max number of hub ports */
#define USB_HUB_PORT_LIMIT 8

/* Limits number of communication retries (when needed) */
#define USB_HUB_MAX_TRIES 3

/* How long to wait between retries, in case of reset error (in nanoseconds) */
#define USB_HUB_RESET_DELAY 200000000 /* 200ms */

/* Hub descriptor type */
#define USB_HUB_DESCRIPTOR_TYPE 0x29

/* Hub descriptor structure */
typedef struct HUB_PACKED hub_descriptor {

	uint8_t		bDescLength;
	uint8_t		bDescriptorType;
	uint8_t		bNbrPorts;
	uint16_t	wHubCharacteristics;
	uint8_t		bPwrOn2PwrGood;
	uint8_t		bHubContrCurrent;
	/* Remaining variable length fields are ignored for now */
}
hub_descriptor;

/* Hub port status structure, as defined in USB 2.0 document */
typedef struct HUB_PACKED hub_port_status {

	uint32_t	PORT_CONNECTION : 1;
	uint32_t	PORT_ENABLE : 1;
	uint32_t	PORT_SUSPEND : 1;
	uint32_t	PORT_OVER_CURRENT : 1;
	uint32_t	PORT_RESET : 1;
	uint32_t	RESERVED1 : 3;

	uint32_t	PORT_POWER : 1;
	uint32_t	PORT_LOW_SPEED : 1;
	uint32_t	PORT_HIGH_SPEED : 1;
	uint32_t	PORT_TEST : 1;
	uint32_t	PORT_INDICATOR : 1;
	uint32_t	RESERVED2 : 3;

	uint32_t	C_PORT_CONNECTION : 1;
	uint32_t	C_PORT_ENABLE : 1;
	uint32_t	C_PORT_SUSPEND : 1;
	uint32_t	C_PORT_OVER_CURRENT : 1;
	uint32_t	C_PORT_RESET : 1;
	uint32_t	RESERVED3 : 11;
}
hub_port_status;

/* Hub Class Feature Selectors */
typedef enum {

	C_HUB_LOCAL_POWER	= 0 ,
	C_HUB_OVER_CURRENT	= 1 ,
	PORT_CONNECTION		= 0 ,
	PORT_ENABLE		= 1 ,
	PORT_SUSPEND		= 2 ,
	PORT_OVER_CURRENT	= 3 ,
	PORT_RESET		= 4 ,
	PORT_POWER		= 8 ,
	PORT_LOW_SPEED		= 9 ,
	C_PORT_CONNECTION	= 16,
	C_PORT_ENABLE		= 17,
	C_PORT_SUSPEND		= 18,
	C_PORT_OVER_CURRENT	= 19,
	C_PORT_RESET		= 20,
	PORT_TEST		= 21,
	PORT_INDICATOR		= 22
}
class_feature;

/* Hub Class Request Codes */
typedef enum {

	GET_STATUS		= 0 ,
	CLEAR_FEATURE		= 1 ,
	RESERVED1		= 2 ,
	SET_FEATURE		= 3 ,
	RESERVED2		= 4 ,
	RESERVED3		= 5 ,
	GET_DESCRIPTOR		= 6 ,
	SET_DESCRIPTOR		= 7 ,
	CLEAR_TT_BUFFER		= 8 ,
	RESET_TT		= 9 ,
	GET_TT_STATE		= 10,
	STOP_TT			= 11
}
class_code;

/* Hub port connection state */
typedef enum {

	HUB_PORT_DISCONN = 0,
	HUB_PORT_CONN = 1,
	HUB_PORT_ERROR = 2
}
port_conn;

/* Hub port connection changes */
typedef enum {

	HUB_CHANGE_NONE = 0,		/* Nothing changed since last poll */
	HUB_CHANGE_CONN = 1,		/* Device was just connected */
	HUB_CHANGE_DISCONN= 2,		/* Device was just disconnected */
	HUB_CHANGE_STATUS_ERR = 3,	/* Port status mismatch */
	HUB_CHANGE_COM_ERR = 4		/* Something wrong happened to driver */
}
port_change;

/* Hub get class specific descriptor call */
static int hub_get_descriptor(hub_descriptor *);

/* Hub Set/ClearPortFeature call */
static int hub_port_feature(int, class_code, class_feature);

/* Hub GetPortStatus call */
static int hub_get_port_status(int, hub_port_status *);

/* Handle port status change */
static port_change hub_handle_change(int, hub_port_status *);

/* Handle port connection */
static int hub_handle_connection(int, hub_port_status *);

/* Handle port disconnection */
static int hub_handle_disconnection(int);


/*---------------------------*
 *    defined variables      *
 *---------------------------*/
/* USB hub driver state */
typedef struct hub_state {

	hub_descriptor descriptor;		/* Class specific descriptor */
	struct ddekit_usb_dev * dev;		/* DDEKit device */
	int num_ports;				/* Number of hub ports */
	port_conn conn[USB_HUB_PORT_LIMIT];	/* Map of connected ports */
}
hub_state;

/* Current hub driver state */
static hub_state driver_state;

/* USB callback structure */
static struct ddekit_usb_driver usb_driver = {
	.completion	= usb_driver_completion,
	.connect	= usb_driver_connect,
	.disconnect	= usb_driver_disconnect
};

/* Semaphore used to block hub thread to
 * allow DDE dispatcher operation */
static ddekit_sem_t * hub_sem = NULL;

/* USB hub thread */
ddekit_thread_t * hub_thread = NULL;

/* DDEKit USB message handling thread */
ddekit_thread_t * ddekit_usb_thread = NULL;


/*---------------------------*
 *    defined functions      *
 *---------------------------*/
/*===========================================================================*
 *    main                                                                   *
 *===========================================================================*/
int
main(int argc, char * argv[])
{
	HUB_MSG("Starting driver... (built: %s %s)", __DATE__, __TIME__);

	/* Store arguments for future parsing */
	env_setargs(argc, argv);

	/* Clear current state */
	memset(&driver_state, 0, sizeof(driver_state));

	/* Initialize SEF related callbacks */
	sef_setcb_init_fresh(hub_sef_hdlr);
	sef_setcb_init_lu(hub_sef_hdlr);
	sef_setcb_init_restart(hub_sef_hdlr);
	sef_setcb_signal_handler(hub_signal_handler);

	/* Initialize DDEkit (involves sef_startup()) */
	ddekit_init();
	HUB_DEBUG_MSG("DDEkit ready...");

	/* Semaphore initialization */
	hub_sem = ddekit_sem_init(0);
	if (NULL == hub_sem)
		panic("Initializing USB hub semaphore, failed!");

	/* Starting hub thread */
	hub_thread = ddekit_thread_create(hub_task, NULL, "hub_task");
	if (NULL == hub_thread)
		panic("Initializing USB hub thread failed!");

	HUB_DEBUG_MSG("USB HUB task ready...");

	/* Run USB IPC task to collect messages */
	ddekit_usb_thread = ddekit_thread_create(ddekit_usb_task, NULL,
						"ddekit_task" );
	if (NULL == ddekit_usb_thread)
		panic("Initializing ddekit_usb_thread failed!");

	HUB_DEBUG_MSG("USB IPC task ready...");

	/* Block and wait until exit signal is received */
	ddekit_minix_wait_exit();
	HUB_DEBUG_MSG("Exiting...");

	/* Release objects that were explicitly allocated above */
	ddekit_thread_terminate(ddekit_usb_thread);
	ddekit_thread_terminate(hub_thread);
	ddekit_sem_deinit(hub_sem);

	/* TODO: No ddekit_deinit for proper cleanup? */

	HUB_DEBUG_MSG("Cleanup completed...");

	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    hub_sef_hdlr                                                           *
 *===========================================================================*/
static int
hub_sef_hdlr(int type, sef_init_info_t * UNUSED(info))
{
	HUB_DEBUG_DUMP;

	switch (type) {
		case SEF_INIT_FRESH:
			return EXIT_SUCCESS;
		case SEF_INIT_LU:
		case SEF_INIT_RESTART:
			HUB_MSG("Only 'fresh' SEF initialization supported");
			break;
		default:
			HUB_MSG("Illegal SEF type");
			break;
	}

	return EXIT_FAILURE;
}


/*===========================================================================*
 *    hub_signal_handler                                                     *
 *===========================================================================*/
static void
hub_signal_handler(int this_signal)
{
	HUB_DEBUG_DUMP;

	HUB_MSG("Handling signal 0x%X", this_signal);

	/* TODO: Any signal means shutdown for now (it may be OK anyway) */
	/* Try graceful DDEKit exit */
	ddekit_shutdown();

	/* Unreachable, when ddekit_shutdown works correctly */
	panic("Calling ddekit_shutdown failed!");
}


/*===========================================================================*
 *    ddekit_usb_task                                                        *
 *===========================================================================*/
static void
ddekit_usb_task(void * UNUSED(arg))
{
	HUB_DEBUG_DUMP;

	/* TODO: This call was meant to return 'int' but loops forever instead,
	 * so no return value is checked */
	ddekit_usb_init(&usb_driver, NULL, NULL);
}


/*===========================================================================*
 *    usb_driver_completion                                                  *
 *===========================================================================*/
static void
usb_driver_completion(void * UNUSED(priv))
{
	HUB_DEBUG_DUMP;

	/* Last request was completed so allow continuing
	 * execution from place where semaphore was downed */
	ddekit_sem_up(hub_sem);
}


/*===========================================================================*
 *    usb_driver_connect                                                     *
 *===========================================================================*/
static void
usb_driver_connect(struct ddekit_usb_dev * dev, unsigned int interfaces)
{
	HUB_DEBUG_DUMP;

	if (NULL != driver_state.dev)
		panic("HUB device driver can be connected only once!");

	/* Clear current state */
	memset(&driver_state, 0, sizeof(driver_state));

	/* Hold host information for future use */
	driver_state.dev = dev;

	/* Let driver logic work */
	ddekit_sem_up(hub_sem);
}


/*===========================================================================*
 *    usb_driver_disconnect                                                  *
 *===========================================================================*/
static void
usb_driver_disconnect(struct ddekit_usb_dev * UNUSED(dev))
{
	HUB_DEBUG_DUMP;

	if (NULL == driver_state.dev)
		panic("HUB device driver was never connected!");

	/* Discard connected device information */
	driver_state.dev = NULL;
}


/*===========================================================================*
 *    hub_task                                                               *
 *===========================================================================*/
static void
hub_task(void * UNUSED(arg))
{
	hub_port_status port_status;
	hub_state * s;
	hub_descriptor * d;
	int port;

	HUB_DEBUG_DUMP;

	/* For short */
	s = &(driver_state);
	d = &(s->descriptor);

	/* Wait for connection */
	ddekit_sem_down(hub_sem);

	if (hub_get_descriptor(d)) {
		HUB_MSG("Getting hub descriptor failed");
		goto HUB_ERROR;
	}

	/* Output hub descriptor in debug mode */
	HUB_DEBUG_MSG("bDescLength         %4X", d->bDescLength);
	HUB_DEBUG_MSG("bDescriptorType     %4X", d->bDescriptorType);
	HUB_DEBUG_MSG("bNbrPorts           %4X", d->bNbrPorts);
	HUB_DEBUG_MSG("wHubCharacteristics %4X", d->wHubCharacteristics);
	HUB_DEBUG_MSG("bPwrOn2PwrGood      %4X", d->bPwrOn2PwrGood);
	HUB_DEBUG_MSG("bHubContrCurrent    %4X", d->bHubContrCurrent);

	/* Check for sane number of ports... */
	if (d->bNbrPorts > USB_HUB_PORT_LIMIT) {
		HUB_MSG("Too many hub ports declared: %d", d->bNbrPorts);
		goto HUB_ERROR;
	}

	/* ...and reassign */
	s->num_ports = (int)d->bNbrPorts;

	/* Initialize all available ports starting
	 * from 1, as defined by USB 2.0 document */
	for (port = 1; port <= s->num_ports; port++) {
		if (hub_port_feature(port, SET_FEATURE, PORT_POWER)) {
			HUB_MSG("Powering port%d failed", port);
			goto HUB_ERROR;
		}
	}

	/*
	 * Connection polling loop
	 */
	for (;;) {
		for (port = 1; port <= s->num_ports; port++) {

			/* Ignore previously blocked ports */
			if (HUB_PORT_ERROR == s->conn[port]) {
				HUB_DEBUG_MSG("Blocked hub port ignored");
				continue;
			}

			/* Get port status */
			if (hub_get_port_status(port, &port_status)) {
				HUB_MSG("Reading port%d status failed", port);
				goto HUB_ERROR;
			}

			/* Resolve port changes */
			switch (hub_handle_change(port, &port_status)) {

				case HUB_CHANGE_NONE:
					break;

				case HUB_CHANGE_CONN:
					s->conn[port] = HUB_PORT_CONN;
					break;

				case HUB_CHANGE_DISCONN:
					s->conn[port] = HUB_PORT_DISCONN;
					break;

				case HUB_CHANGE_STATUS_ERR:
					/* Turn off port */
					if (hub_port_feature(port,
							CLEAR_FEATURE,
							PORT_POWER)) {
						HUB_MSG("Halting port%d "
							"failed", port);
						goto HUB_ERROR;
					}
					/* Block this port forever */
					s->conn[port] = HUB_PORT_ERROR;

					HUB_MSG("Port%d status ERROR", port);
					HUB_MSG("Port%d will be blocked, until "
						"hub is detached", port);

					break;

				case HUB_CHANGE_COM_ERR:
					/* Serious error, hang */
					HUB_MSG("Handling port%d "
						"change failed", port);
					goto HUB_ERROR;
			}
		}

		ddekit_thread_msleep(USB_HUB_POLLING_INTERVAL);
		HUB_DEBUG_MSG("Polling USB hub for status change");
	}

	return;

	HUB_ERROR:
	for (;;) {
		/* Hang till removed by devmand */
		HUB_MSG("Hub driver error occurred, hanging up");
		ddekit_sem_down(hub_sem);
	}
}


/*===========================================================================*
 *    hub_get_descriptor                                                     *
 *===========================================================================*/
static int
hub_get_descriptor(hub_descriptor * descriptor)
{
	/* URB to be send */
	struct ddekit_usb_urb urb;

	/* Setup buffer to be attached */
	struct usb_ctrlrequest setup_buf;

	/* Control EP configuration */
	urb_ep_config ep_conf;

	HUB_DEBUG_DUMP;

	/* Initialize EP configuration */
	ep_conf.ep_num = 0;
	ep_conf.direction = DDEKIT_USB_IN;
	ep_conf.type = DDEKIT_USB_TRANSFER_CTL;
	ep_conf.max_packet_size = 0;
	ep_conf.interval = 0;

	/* Reset URB and assign given values */
	init_urb(&urb, driver_state.dev, &ep_conf);

	/* Clear setup data */
	memset(&setup_buf, 0, sizeof(setup_buf));

	/* Class get hub descriptor request */
	setup_buf.bRequestType = 0xA0;
	setup_buf.bRequest = 0x06;
	setup_buf.wValue = USB_HUB_DESCRIPTOR_TYPE << 8;
	setup_buf.wIndex = 0x00;
	setup_buf.wLength = sizeof(*descriptor);

	/* Attach buffers to URB */
	attach_urb_data(&urb, URB_BUF_TYPE_SETUP,
			&setup_buf, sizeof(setup_buf));
	attach_urb_data(&urb, URB_BUF_TYPE_DATA,
			descriptor, sizeof(*descriptor));

	/* Send and wait for response */
	if (blocking_urb_submit(&urb, hub_sem, URB_SUBMIT_CHECK_LEN)) {
		HUB_MSG("Submitting HUB URB failed");
		return EXIT_FAILURE;
	} else {
		HUB_DEBUG_MSG("HUB descriptor received");
		return EXIT_SUCCESS;
	}
}


/*===========================================================================*
 *    hub_port_feature                                                       *
 *===========================================================================*/
static int
hub_port_feature(int port_num, class_code code, class_feature feature)
{
	/* URB to be send */
	struct ddekit_usb_urb urb;

	/* Setup buffer to be attached */
	struct usb_ctrlrequest setup_buf;

	/* Control EP configuration */
	urb_ep_config ep_conf;

	HUB_DEBUG_DUMP;

	/* TODO: Add more checks when needed */
	if (!((port_num <= driver_state.num_ports) && (port_num > 0)))
		return EXIT_FAILURE;

	if (!((code == SET_FEATURE) || (code == CLEAR_FEATURE)))
		return EXIT_FAILURE;

	if (!((feature == PORT_RESET) || (feature == PORT_POWER) ||
		(feature == C_PORT_CONNECTION) || (feature == C_PORT_RESET)))
		return EXIT_FAILURE;

	/* Initialize EP configuration */
	ep_conf.ep_num = 0;
	ep_conf.direction = DDEKIT_USB_OUT;
	ep_conf.type = DDEKIT_USB_TRANSFER_CTL;
	ep_conf.max_packet_size = 0;
	ep_conf.interval = 0;

	/* Reset URB and assign given values */
	init_urb(&urb, driver_state.dev, &ep_conf);

	/* Clear setup data */
	memset(&setup_buf, 0, sizeof(setup_buf));

	/* Standard get endpoint request */
	setup_buf.bRequestType = 0x23;
	setup_buf.bRequest = (u8_t)code;
	setup_buf.wValue = (u16_t)feature;
	setup_buf.wIndex = (u16_t)port_num;
	setup_buf.wLength = 0;

	/* Attach buffers to URB */
	attach_urb_data(&urb, URB_BUF_TYPE_SETUP,
			&setup_buf, sizeof(setup_buf));

	/* Send and wait for response */
	if (blocking_urb_submit(&urb, hub_sem, URB_SUBMIT_CHECK_LEN)) {
		HUB_MSG("Submitting HUB URB failed");
		return EXIT_FAILURE;
	} else {
		HUB_DEBUG_MSG("PortFeature operation completed");
		return EXIT_SUCCESS;
	}
}


/*===========================================================================*
 *    hub_get_port_status                                                    *
 *===========================================================================*/
static int
hub_get_port_status(int port_num, hub_port_status * p)
{
	/* URB to be send */
	struct ddekit_usb_urb urb;

	/* Setup buffer to be attached */
	struct usb_ctrlrequest setup_buf;

	/* Control EP configuration */
	urb_ep_config ep_conf;

	HUB_DEBUG_DUMP;

	if (!((port_num <= driver_state.num_ports) && (port_num > 0)))
		return EXIT_FAILURE;

	/* Initialize EP configuration */
	ep_conf.ep_num = 0;
	ep_conf.direction = DDEKIT_USB_IN;
	ep_conf.type = DDEKIT_USB_TRANSFER_CTL;
	ep_conf.max_packet_size = 0;
	ep_conf.interval = 0;

	/* Reset URB and assign given values */
	init_urb(&urb, driver_state.dev, &ep_conf);

	/* Clear setup data */
	memset(&setup_buf, 0, sizeof(setup_buf));

	/* Standard get endpoint request */
	setup_buf.bRequestType = 0xA3;
	setup_buf.bRequest = (u8_t)GET_STATUS;
	setup_buf.wValue = 0x00;
	setup_buf.wIndex = (u16_t)port_num;
	setup_buf.wLength = sizeof(*p);

	/* Attach buffers to URB */
	attach_urb_data(&urb, URB_BUF_TYPE_SETUP,
			&setup_buf, sizeof(setup_buf));
	attach_urb_data(&urb, URB_BUF_TYPE_DATA,
			p, sizeof(*p));

	/* Send and wait for response */
	if (blocking_urb_submit(&urb, hub_sem, URB_SUBMIT_CHECK_LEN)) {
		HUB_MSG("Submitting HUB URB failed");
		return EXIT_FAILURE;
	} else {
		HUB_DEBUG_MSG("Port%d status:      ", port_num);
		HUB_DEBUG_MSG("PORT_CONNECTION   %01X", p->PORT_CONNECTION);
		HUB_DEBUG_MSG("PORT_ENABLE       %01X", p->PORT_ENABLE);
		HUB_DEBUG_MSG("PORT_POWER        %01X", p->PORT_POWER);
		HUB_DEBUG_MSG("C_PORT_CONNECTION %01X", p->C_PORT_CONNECTION);
		HUB_DEBUG_MSG("C_PORT_ENABLE     %01X", p->C_PORT_ENABLE);
		return EXIT_SUCCESS;
	}
}


/*===========================================================================*
 *    hub_handle_change                                                      *
 *===========================================================================*/
static port_change
hub_handle_change(int port_num, hub_port_status * status)
{
	port_conn * c;

	HUB_DEBUG_DUMP;

	/* Possible combinations: */
	/* Change = status->C_PORT_CONNECTION	(hub connection change bit)
	 * Local = driver_state.conn[port_num]	(local connection status)
	 * Remote = status->PORT_CONNECTION	(hub connection status) */
	/*
	Case	Change	Local	Remote	Description
	1.	1	1	1	Polling mismatch (quick disconn-conn)
	2.	1	1	0	Just disconnected
	3.	1	0	1	Just connected
	4.	1	0	0	Polling mismatch (quick conn-disconn)
	5.	0	1	1	Still connected
	6.	0	1	0	Serious ERROR
	7.	0	0	1	Serious ERROR
	8.	0	0	0	Still disconnected
	*/

	/* Reassign for code cleanliness */
	c = driver_state.conn;

	/* Resolve combination */
	if (status->C_PORT_CONNECTION) {

		/* C_PORT_CONNECTION was set, so clear change bit
		 * to allow further polling */
		if (hub_port_feature(port_num, CLEAR_FEATURE,
					C_PORT_CONNECTION)) {
			HUB_MSG("Clearing port%d change bit failed", port_num);
			return HUB_CHANGE_COM_ERR;
		}

		if (HUB_PORT_CONN == c[port_num]) {
			if (status->PORT_CONNECTION) {

				/*
				 * 1
				 */
				/* Make hub disconnect and connect again */
				if (hub_handle_disconnection(port_num) ||
					hub_handle_connection(port_num, status))
					return HUB_CHANGE_STATUS_ERR;
				else
					return HUB_CHANGE_CONN;

			} else {

				/*
				 * 2
				 */
				/* Handle disconnection */
				if (hub_handle_disconnection(port_num))
					return HUB_CHANGE_STATUS_ERR;
				else
					return HUB_CHANGE_DISCONN;

			}
		} else if (HUB_PORT_DISCONN == c[port_num]) {
			if (status->PORT_CONNECTION) {

				/*
				 * 3
				 */
				/* Handle connection */
				if (hub_handle_connection(port_num, status))
					return HUB_CHANGE_STATUS_ERR;
				else
					return HUB_CHANGE_CONN;

			} else {

				/*
				 * 4
				 */
				/* Since we were disconnected before and
				 * are disconnected now, additional handling
				 * may be ignored */
				return HUB_CHANGE_NONE;

			}
		}
	} else {
		if (HUB_PORT_CONN == c[port_num]) {
			if (status->PORT_CONNECTION) {

				/*
				 * 5
				 */
				/* Connected (nothing changed) */
				return HUB_CHANGE_NONE;

			} else {

				/*
				 * 6
				 */
				/* Serious status error */
				return HUB_CHANGE_STATUS_ERR;

			}
		} else if (HUB_PORT_DISCONN == c[port_num]) {
			if (status->PORT_CONNECTION) {

				/*
				 * 7
				 */
				/* Serious status error */
				return HUB_CHANGE_STATUS_ERR;

			} else {

				/*
				 * 8
				 */
				/* Disconnected (nothing changed) */
				return HUB_CHANGE_NONE;

			}
		}
	}

	return HUB_CHANGE_COM_ERR;
}


/*===========================================================================*
 *    hub_handle_connection                                                  *
 *===========================================================================*/
static int
hub_handle_connection(int port_num, hub_port_status * status)
{
	struct timespec wait_time;
	int reset_tries;
	long port_speed;

	HUB_DEBUG_DUMP;

	HUB_MSG("Device connected to port%d", port_num);

	/* This should never happen if power-off works as intended */
	if (status->C_PORT_RESET) {
		HUB_MSG("Unexpected reset state for port%d", port_num);
		return EXIT_FAILURE;
	}

	/* Start reset signaling for this port */
	if (hub_port_feature(port_num, SET_FEATURE, PORT_RESET)) {
		HUB_MSG("Resetting port%d failed", port_num);
		return EXIT_FAILURE;
	}

	reset_tries = 0;
	wait_time.tv_sec = 0;
	wait_time.tv_nsec = USB_HUB_RESET_DELAY;

	/* Wait for reset completion */
	while (!status->C_PORT_RESET) {
		/* To avoid endless loop */
		if (reset_tries >= USB_HUB_MAX_TRIES) {
			HUB_MSG("Port%d reset took too long", port_num);
			return EXIT_FAILURE;
		}

		/* Get port status again */
		if (hub_get_port_status(port_num, status)) {
			HUB_MSG("Reading port%d status failed", port_num);
			return EXIT_FAILURE;
		}

		reset_tries++;

		/* Ignore potential signal interruption (no return value check),
		 * since it causes driver termination anyway */
		if (nanosleep(&wait_time, NULL))
			HUB_MSG("Calling nanosleep() failed");
	}

	/* Reset completed */
	HUB_DEBUG_MSG("Port%d reset complete", port_num);

	/* Dump full status for analysis (high-speed, ...) */
	HUB_DEBUG_MSG("C_PORT_CONNECTION   %1X", status->C_PORT_CONNECTION  );
	HUB_DEBUG_MSG("C_PORT_ENABLE       %1X", status->C_PORT_ENABLE      );
	HUB_DEBUG_MSG("C_PORT_OVER_CURRENT %1X", status->C_PORT_OVER_CURRENT);
	HUB_DEBUG_MSG("C_PORT_RESET        %1X", status->C_PORT_RESET       );
	HUB_DEBUG_MSG("C_PORT_SUSPEND      %1X", status->C_PORT_SUSPEND     );
	HUB_DEBUG_MSG("PORT_CONNECTION     %1X", status->PORT_CONNECTION    );
	HUB_DEBUG_MSG("PORT_ENABLE         %1X", status->PORT_ENABLE        );
	HUB_DEBUG_MSG("PORT_HIGH_SPEED     %1X", status->PORT_HIGH_SPEED    );
	HUB_DEBUG_MSG("PORT_INDICATOR      %1X", status->PORT_INDICATOR     );
	HUB_DEBUG_MSG("PORT_LOW_SPEED      %1X", status->PORT_LOW_SPEED     );
	HUB_DEBUG_MSG("PORT_OVER_CURRENT   %1X", status->PORT_OVER_CURRENT  );
	HUB_DEBUG_MSG("PORT_POWER          %1X", status->PORT_POWER         );
	HUB_DEBUG_MSG("PORT_RESET          %1X", status->PORT_RESET         );
	HUB_DEBUG_MSG("PORT_SUSPEND        %1X", status->PORT_SUSPEND       );
	HUB_DEBUG_MSG("PORT_TEST           %1X", status->PORT_TEST          );

	/* Clear reset change bit for further devices */
	if (hub_port_feature(port_num, CLEAR_FEATURE, C_PORT_RESET)) {
		HUB_MSG("Clearing port%d reset bit failed", port_num);
		return EXIT_FAILURE;
	}

	/* Should never happen */
	if (!status->PORT_CONNECTION || !status->PORT_ENABLE) {
		HUB_MSG("Port%d unexpectedly unavailable", port_num);
		return EXIT_FAILURE;
	}

	/* Determine port speed from status bits */
	if (status->PORT_LOW_SPEED) {
		if (status->PORT_HIGH_SPEED) {
			HUB_MSG("Port%d has invalid speed flags", port_num);
			return EXIT_FAILURE;
		} else
			port_speed = (long)DDEKIT_HUB_PORT_LS_CONN;
	} else {
		if (status->PORT_HIGH_SPEED)
			port_speed = (long)DDEKIT_HUB_PORT_HS_CONN;
		else
			port_speed = (long)DDEKIT_HUB_PORT_FS_CONN;
	}

	/* Signal to HCD that port has device connected at given speed */
	return ddekit_usb_info(driver_state.dev, port_speed, (long)port_num);
}


/*===========================================================================*
 *    hub_handle_disconnection                                               *
 *===========================================================================*/
static int
hub_handle_disconnection(int port_num)
{
	HUB_DEBUG_DUMP;

	HUB_MSG("Device disconnected from port%d", port_num);

	return ddekit_usb_info(driver_state.dev, (long)DDEKIT_HUB_PORT_DISCONN,
				(long)port_num);
}
