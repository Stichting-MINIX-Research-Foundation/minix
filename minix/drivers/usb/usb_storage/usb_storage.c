/*
 * Minix3 USB mass storage driver implementation
 * using DDEkit, and libblockdriver
 */

#include <sys/cdefs.h>			/* __CTASSERT() */
#include <sys/ioc_disk.h>		/* cases for mass_storage_ioctl */
#ifdef USB_STORAGE_SIGNAL
#include <sys/signal.h>			/* signal handling */
#endif

#include <ddekit/minix/msg_queue.h>
#include <ddekit/thread.h>
#include <ddekit/usb.h>

#include <minix/blockdriver.h>
#include <minix/com.h>			/* for msg_queue ranges */
#include <minix/drvlib.h>		/* DEV_PER_DRIVE, partition */
#include <minix/ipc.h>			/* message */
#include <minix/safecopies.h>		/* GRANT_VALID */
#include <minix/sef.h>
#include <minix/sysutil.h>		/* panic */
#include <minix/usb.h>			/* structures like usb_ctrlrequest */
#include <minix/usb_ch9.h>		/* descriptor structures */

#include <assert.h>
#include <limits.h>			/* ULONG_MAX */
#include <time.h>			/* nanosleep */

#include "common.h"
#include "bulk.h"
#include "usb_storage.h"
#include "urb_helper.h"
#include "scsi.h"


/*---------------------------*
 *    declared functions     *
 *---------------------------*/
/* TODO: these are missing from DDE header files */
extern void ddekit_minix_wait_exit(void);
extern void ddekit_shutdown(void);

/* SCSI URB related prototypes */
static int mass_storage_send_scsi_cbw_out(int, scsi_transfer *);
static int mass_storage_send_scsi_data_in(void *, unsigned int);
static int mass_storage_send_scsi_data_out(void *, unsigned int);
static int mass_storage_send_scsi_csw_in(void);

#ifdef MASS_RESET_RECOVERY
/* Bulk only URB related prototypes */
static int mass_storage_reset_recovery(void);
static int mass_storage_send_bulk_reset(void);
static int mass_storage_send_clear_feature(int, int);
#endif

/* SEF related functions */
static int mass_storage_sef_hdlr(int, sef_init_info_t *);
static void mass_storage_signal_handler(int);

/* DDEKit IPC related */
static void ddekit_usb_task(void *);

/* Mass storage related prototypes */
static void mass_storage_task(void *);
static int mass_storage_test(void);
static int mass_storage_check_error(void);
static int mass_storage_try_first_open(void);
static int mass_storage_transfer_restrictions(u64_t, unsigned long);
static ssize_t mass_storage_write(unsigned long, endpoint_t, iovec_t *,
				unsigned int, unsigned long);
static ssize_t mass_storage_read(unsigned long, endpoint_t, iovec_t *,
				unsigned int, unsigned long);

/* Minix's libblockdriver callbacks */
static int mass_storage_open(devminor_t, int);
static int mass_storage_close(devminor_t);
static ssize_t mass_storage_transfer(devminor_t, int, u64_t, endpoint_t,
					iovec_t *, unsigned int, int);
static int mass_storage_ioctl(devminor_t, unsigned long, endpoint_t,
				cp_grant_id_t, endpoint_t);
static void mass_storage_cleanup(void);
static struct device * mass_storage_part(devminor_t);
static void mass_storage_geometry(devminor_t, struct part_geom *);

/* DDEKit's USB driver callbacks */
static void usb_driver_completion(void *);
static void usb_driver_connect(struct ddekit_usb_dev *, unsigned int);
static void usb_driver_disconnect(struct ddekit_usb_dev *);

/* Simplified enumeration method for endpoint resolution */
static int mass_storage_get_endpoints(urb_ep_config *, urb_ep_config *);
static int mass_storage_parse_endpoint(usb_descriptor_t *, urb_ep_config *,
					urb_ep_config *);
static int mass_storage_parse_descriptors(char *, unsigned int, urb_ep_config *,
					urb_ep_config *);


/*---------------------------*
 *    defined variables      *
 *---------------------------*/
#define MASS_PACKED __attribute__((__packed__))

/* Mass Storage callback structure */
static struct blockdriver mass_storage = {
	.bdr_type	= BLOCKDRIVER_TYPE_DISK,
	.bdr_open	= mass_storage_open,
	.bdr_close	= mass_storage_close,
	.bdr_transfer	= mass_storage_transfer,
	.bdr_ioctl	= mass_storage_ioctl,
	.bdr_cleanup	= mass_storage_cleanup,
	.bdr_part	= mass_storage_part,
	.bdr_geometry	= mass_storage_geometry,
	.bdr_intr	= NULL,
	.bdr_alarm	= NULL,
	.bdr_other	= NULL,
	.bdr_device	= NULL
};

/* USB callback structure */
static struct ddekit_usb_driver mass_storage_driver = {
	.completion	= usb_driver_completion,
	.connect	= usb_driver_connect,
	.disconnect	= usb_driver_disconnect
};

/* Instance of global driver information */
mass_storage_state driver_state;

/* Tags used to pair CBW and CSW for bulk communication
 * With this we can check if SCSI reply was meant for SCSI request */
static unsigned int current_cbw_tag = 0;	/* What shall be send next */
static unsigned int last_cbw_tag = 0;		/* What was sent recently */

/* Semaphore used to block mass storage thread to
 * allow DDE dispatcher operation */
static ddekit_sem_t * mass_storage_sem = NULL;

/* Mass storage (using libblockdriver) thread */
ddekit_thread_t * mass_storage_thread = NULL;

/* DDEKit USB message handling thread */
ddekit_thread_t * ddekit_usb_thread = NULL;

/* Static URB buffer size (must be multiple of SECTOR_SIZE) */
#define BUFFER_SIZE (64*SECTOR_SIZE)

/* Large buffer for URB read/write operations */
static unsigned char buffer[BUFFER_SIZE];

/* Length of local buffer where descriptors are temporarily stored */
#define MAX_DESCRIPTORS_LEN 128

/* Maximum 'Test Unit Ready' command retries */
#define MAX_TEST_RETRIES 3

/* 'Test Unit Ready' failure delay time (in nanoseconds) */
#define NEXT_TEST_DELAY 50000000 /* 50ms */


/*---------------------------*
 *    defined functions      *
 *---------------------------*/
/*===========================================================================*
 *    main                                                                   *
 *===========================================================================*/
int
main(int argc, char * argv[])
{
	MASS_DEBUG_MSG("Starting...");

	/* Store arguments for future parsing */
	env_setargs(argc, argv);

	/* Clear current state */
	memset(&driver_state, 0, sizeof(driver_state));

	/* Initialize SEF related callbacks */
	sef_setcb_init_fresh(mass_storage_sef_hdlr);
	sef_setcb_init_lu(mass_storage_sef_hdlr);
	sef_setcb_init_restart(mass_storage_sef_hdlr);
	sef_setcb_signal_handler(mass_storage_signal_handler);

	/* Initialize DDEkit (involves sef_startup()) */
	ddekit_init();
	MASS_DEBUG_MSG("DDEkit ready...");

	/* Semaphore initialization */
	mass_storage_sem = ddekit_sem_init(0);
	if (NULL == mass_storage_sem)
		panic("Initializing mass_storage_sem failed!");

	/* Starting mass storage thread */
	mass_storage_thread = ddekit_thread_create(mass_storage_task, NULL,
						"mass_storage_task");
	if (NULL == mass_storage_thread)
		panic("Initializing mass_storage_thread failed!");

	MASS_DEBUG_MSG("Storage task (libblockdriver) ready...");

	/* Run USB IPC task to collect messages */
	ddekit_usb_thread = ddekit_thread_create(ddekit_usb_task, NULL,
						"ddekit_task" );
	if (NULL == ddekit_usb_thread)
		panic("Initializing ddekit_usb_thread failed!");

	MASS_DEBUG_MSG("USB IPC task ready...");

	/* Block and wait until exit signal is received */
	ddekit_minix_wait_exit();
	MASS_DEBUG_MSG("Exiting...");

	/* Release objects that were explicitly allocated above */
	ddekit_thread_terminate(ddekit_usb_thread);
	ddekit_thread_terminate(mass_storage_thread);
	ddekit_sem_deinit(mass_storage_sem);

	/* TODO: no ddekit_deinit for proper cleanup? */

	MASS_DEBUG_MSG("Cleanup completed...");

	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    mass_storage_send_scsi_cbw_out                                         *
 *===========================================================================*/
static int
mass_storage_send_scsi_cbw_out(int scsi_cmd, scsi_transfer * info)
{
	/* URB to be send */
	struct ddekit_usb_urb urb;

	/* CBW data buffer */
	mass_storage_cbw cbw;

	MASS_DEBUG_DUMP;

	/* Reset URB and assign given values */
	init_urb(&urb, driver_state.cur_periph->dev,
		&(driver_state.cur_periph->ep_out));

	/* Reset CBW and assign default values */
	init_cbw(&cbw, last_cbw_tag = current_cbw_tag++);

	/* Fill CBW with SCSI command */
	if (create_scsi_cmd(&cbw, scsi_cmd, info))
		return EXIT_FAILURE;

	/* Attach CBW to URB */
	attach_urb_data(&urb, URB_BUF_TYPE_DATA, &cbw, sizeof(cbw));

	/* Send and wait for response */
	if (blocking_urb_submit(&urb, mass_storage_sem, URB_SUBMIT_CHECK_LEN))
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    mass_storage_send_scsi_data_in                                         *
 *===========================================================================*/
static int
mass_storage_send_scsi_data_in(void * buf, unsigned int in_len)
{
	/* URB to be send */
	struct ddekit_usb_urb urb;

	MASS_DEBUG_DUMP;

	/* Reset URB and assign given values */
	init_urb(&urb, driver_state.cur_periph->dev,
		&(driver_state.cur_periph->ep_in));

	/* Attach buffer to URB */
	attach_urb_data(&urb, URB_BUF_TYPE_DATA, buf, in_len);

	/* Send and wait for response */
	if (blocking_urb_submit(&urb, mass_storage_sem, URB_SUBMIT_CHECK_LEN))
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    mass_storage_send_scsi_data_out                                        *
 *===========================================================================*/
static int
mass_storage_send_scsi_data_out(void * buf, unsigned int out_len)
{
	/* URB to be send */
	struct ddekit_usb_urb urb;

	MASS_DEBUG_DUMP;

	/* Reset URB and assign given values */
	init_urb(&urb, driver_state.cur_periph->dev,
		&(driver_state.cur_periph->ep_out));

	/* Attach buffer to URB */
	attach_urb_data(&urb, URB_BUF_TYPE_DATA, buf, out_len);

	/* Send and wait for response */
	if (blocking_urb_submit(&urb, mass_storage_sem, URB_SUBMIT_CHECK_LEN))
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    mass_storage_send_scsi_csw_in                                          *
 *===========================================================================*/
static int
mass_storage_send_scsi_csw_in(void)
{
	/* URB to be send */
	struct ddekit_usb_urb urb;

	/* CBW data buffer */
	mass_storage_csw csw;

	MASS_DEBUG_DUMP;

	/* Reset URB and assign given values */
	init_urb(&urb, driver_state.cur_periph->dev,
		&(driver_state.cur_periph->ep_in));

	/* Clear CSW for receiving */
	init_csw(&csw);

	/* Attach CSW to URB */
	attach_urb_data(&urb, URB_BUF_TYPE_DATA, &csw, sizeof(csw));

	/* Send and wait for response */
	if (blocking_urb_submit(&urb, mass_storage_sem, URB_SUBMIT_CHECK_LEN))
		return EXIT_FAILURE;

	/* Check for proper reply */
	if (check_csw(&csw, last_cbw_tag))
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}


#ifdef MASS_RESET_RECOVERY
/*===========================================================================*
 *    mass_storage_reset_recovery                                            *
 *===========================================================================*/
static int
mass_storage_reset_recovery(void)
{
	MASS_DEBUG_DUMP;

	/* Bulk-Only Mass Storage Reset */
	if (mass_storage_send_bulk_reset()) {
		MASS_MSG("Bulk-only mass storage reset failed");
		return EIO;
	}

	/* Clear Feature HALT to the Bulk-In endpoint */
	if (URB_INVALID_EP != driver_state.cur_periph->ep_in.ep_num)
		if (mass_storage_send_clear_feature(
					driver_state.cur_periph->ep_in.ep_num,
					DDEKIT_USB_IN)) {
			MASS_MSG("Resetting IN EP failed");
			return EIO;
		}

	/* Clear Feature HALT to the Bulk-Out endpoint */
	if (URB_INVALID_EP != driver_state.cur_periph->ep_out.ep_num)
		if (mass_storage_send_clear_feature(
					driver_state.cur_periph->ep_out.ep_num,
					DDEKIT_USB_OUT)) {
			MASS_MSG("Resetting OUT EP failed");
			return EIO;
		}

	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    mass_storage_send_bulk_reset                                           *
 *===========================================================================*/
static int
mass_storage_send_bulk_reset(void)
{
	/* URB to be send */
	struct ddekit_usb_urb urb;

	/* Setup buffer to be send */
	struct usb_ctrlrequest bulk_setup;

	/* Control EP configuration */
	urb_ep_config ep_conf;

	MASS_DEBUG_DUMP;

	/* Initialize EP configuration */
	ep_conf.ep_num = 0;
	ep_conf.direction = DDEKIT_USB_OUT;
	ep_conf.type = DDEKIT_USB_TRANSFER_CTL;
	ep_conf.max_packet_size = 0;
	ep_conf.interval = 0;

	/* Reset URB and assign given values */
	init_urb(&urb, driver_state.cur_periph->dev, &ep_conf);

	/* Clear setup data */
	memset(&bulk_setup, 0, sizeof(bulk_setup));

	/* For explanation of these values see usbmassbulk_10.pdf */
	/* 3.1 Bulk-Only Mass Storage Reset */
	bulk_setup.bRequestType = 0x21; /* Class, Interface, host to device */
	bulk_setup.bRequest = 0xff;
	bulk_setup.wValue = 0x00;
	bulk_setup.wIndex = 0x00; /* TODO: hard-coded interface 0 */
	bulk_setup.wLength = 0x00;

	/* Attach request to URB */
	attach_urb_data(&urb, URB_BUF_TYPE_SETUP,
			&bulk_setup, sizeof(bulk_setup));

	/* Send and wait for response */
	if (blocking_urb_submit(&urb, mass_storage_sem, URB_SUBMIT_CHECK_LEN))
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    mass_storage_send_clear_feature                                        *
 *===========================================================================*/
static int
mass_storage_send_clear_feature(int ep_num, int direction)
{
	/* URB to be send */
	struct ddekit_usb_urb urb;

	/* Setup buffer to be send */
	struct usb_ctrlrequest bulk_setup;

	/* Control EP configuration */
	urb_ep_config ep_conf;

	MASS_DEBUG_DUMP;

	assert((ep_num >= 0) && (ep_num < 16));
	assert((DDEKIT_USB_OUT == direction) || (DDEKIT_USB_IN == direction));

	/* Initialize EP configuration */
	ep_conf.ep_num = 0;
	ep_conf.direction = DDEKIT_USB_OUT;
	ep_conf.type = DDEKIT_USB_TRANSFER_CTL;
	ep_conf.max_packet_size = 0;
	ep_conf.interval = 0;

	/* Reset URB and assign given values */
	init_urb(&urb, driver_state.cur_periph->dev, &ep_conf);

	/* Clear setup data */
	memset(&bulk_setup, 0, sizeof(bulk_setup));

	/* For explanation of these values see usbmassbulk_10.pdf */
	/* 3.1 Bulk-Only Mass Storage Reset */
	bulk_setup.bRequestType = 0x02; /* Standard, Endpoint, host to device */
	bulk_setup.bRequest = 0x01; /* CLEAR_FEATURE */
	bulk_setup.wValue = 0x00; /* Endpoint */
	bulk_setup.wIndex = ep_num; /* Endpoint number... */
	if (DDEKIT_USB_IN == direction)
		bulk_setup.wIndex |= UE_DIR_IN; /* ...and direction bit */
	bulk_setup.wLength = 0x00;

	/* Attach request to URB */
	attach_urb_data(&urb, URB_BUF_TYPE_SETUP,
			&bulk_setup, sizeof(bulk_setup));

	/* Send and wait for response */
	if (blocking_urb_submit(&urb, mass_storage_sem, URB_SUBMIT_CHECK_LEN))
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
#endif


/*===========================================================================*
 *    mass_storage_sef_hdlr                                                  *
 *===========================================================================*/
static int
mass_storage_sef_hdlr(int type, sef_init_info_t * UNUSED(info))
{
	int env_res;

	MASS_DEBUG_DUMP;

	/* Parse given environment */
	env_res = env_parse("instance", "d", 0,
			&(driver_state.instance),0, 255);

	/* Get instance number */
	if (EP_UNSET == env_res) {
		MASS_DEBUG_MSG("Instance number was not supplied");
		driver_state.instance = 0;
	} else {
		/* Only SET and UNSET are allowed */
		if (EP_SET != env_res)
			return EXIT_FAILURE;
	}

	switch (type) {
		case SEF_INIT_FRESH:
			/* Announce we are up! */
			blockdriver_announce(type);
			return EXIT_SUCCESS;
		case SEF_INIT_LU:
		case SEF_INIT_RESTART:
			MASS_MSG("Only 'fresh' SEF initialization supported\n");
			break;
		default:
			MASS_MSG("illegal SEF type\n");
			break;
	}

	return EXIT_FAILURE;
}


/*===========================================================================*
 *    mass_storage_signal_handler                                            *
 *===========================================================================*/
static void
mass_storage_signal_handler(int this_signal)
{
	MASS_DEBUG_DUMP;

#ifdef USB_STORAGE_SIGNAL
	/* Only check for termination signal, ignore anything else. */
	if (this_signal != SIGTERM)
		return;
#else
	MASS_MSG("Handling signal 0x%X", this_signal);
#endif

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
	MASS_DEBUG_DUMP;

	/* TODO: This call was meant to return 'int' but loops forever instead,
	 * so no return value is checked */
	ddekit_usb_init(&mass_storage_driver, NULL, NULL);
}


/*===========================================================================*
 *    mass_storage_task                                                      *
 *===========================================================================*/
static void
mass_storage_task(void * UNUSED(unused))
{
	message m;
	int ipc_status;
	struct ddekit_minix_msg_q * mq;

	MASS_DEBUG_DUMP;

	mq = ddekit_minix_create_msg_q(BDEV_RQ_BASE, BDEV_RQ_BASE + 0xff);

	for (;;) {
		ddekit_minix_rcv(mq, &m, &ipc_status);
		blockdriver_process(&mass_storage, &m, ipc_status);
	}
}


/*===========================================================================*
 *    mass_storage_test                                                      *
 *===========================================================================*/
static int
mass_storage_test(void)
{
	int repeat;
	int error;

	struct timespec test_wait;

	MASS_DEBUG_DUMP;

	/* Delay between consecutive test commands, in case of their failure */
	test_wait.tv_nsec = NEXT_TEST_DELAY;
	test_wait.tv_sec = 0;

	for (repeat = 0; repeat < MAX_TEST_RETRIES; repeat++) {

		/* SCSI TEST UNIT READY OUT stage */
		if (mass_storage_send_scsi_cbw_out(SCSI_TEST_UNIT_READY, NULL))
			return EIO;

		/* TODO: Only CSW failure should normally contribute to retry */

		/* SCSI TEST UNIT READY IN stage */
		if (EXIT_SUCCESS == mass_storage_send_scsi_csw_in())
			return EXIT_SUCCESS;

		/* Check for errors */
		if (EXIT_SUCCESS != (error = mass_storage_check_error())) {
			MASS_MSG("SCSI sense error checking failed");
			return error;
		}

		/* Ignore potential signal interruption (no return value check),
		 * since it causes driver termination anyway */
		if (nanosleep(&test_wait, NULL))
			MASS_MSG("Calling nanosleep() failed");
	}

	return EIO;
}


/*===========================================================================*
 *    mass_storage_check_error                                               *
 *===========================================================================*/
static int
mass_storage_check_error(void)
{
	/* SCSI sense structure for local use */
	typedef struct MASS_PACKED scsi_sense {

		uint8_t code : 7;
		uint8_t valid : 1;
		uint8_t obsolete : 8;
		uint8_t sense : 4;
		uint8_t reserved : 1;
		uint8_t ili : 1;
		uint8_t eom : 1;
		uint8_t filemark : 1;
		uint32_t information : 32;
		uint8_t additional_len : 8;
		uint32_t command_specific : 32;
		uint8_t additional_code : 8;
		uint8_t additional_qual : 8;
		uint8_t unit_code : 8;
		uint8_t key_specific1 : 7;
		uint8_t sksv : 1;
		uint16_t key_specific2 : 16;
	}
	scsi_sense;

	/* Sense variable to hold received data */
	scsi_sense sense;

	MASS_DEBUG_DUMP;

	/* Check if bit-fields are packed correctly */
	__CTASSERT(sizeof(sense) == SCSI_REQUEST_SENSE_DATA_LEN);

	/* SCSI REQUEST SENSE OUT stage */
	if (mass_storage_send_scsi_cbw_out(SCSI_REQUEST_SENSE, NULL))
		return EIO;

	/* SCSI REQUEST SENSE first IN stage */
	if (mass_storage_send_scsi_data_in(&sense, sizeof(sense)))
		return EIO;

	/* SCSI REQUEST SENSE second IN stage */
	if (mass_storage_send_scsi_csw_in())
		return EIO;

	/* When any sense code is present something may have failed */
	if (sense.sense) {
#ifdef MASS_DEBUG
		MASS_MSG("SCSI sense:                ");
		MASS_MSG("code             : %8X", sense.code            );
		MASS_MSG("valid            : %8X", sense.valid           );
		MASS_MSG("obsolete         : %8X", sense.obsolete        );
		MASS_MSG("sense            : %8X", sense.sense           );
		MASS_MSG("reserved         : %8X", sense.reserved        );
		MASS_MSG("ili              : %8X", sense.ili             );
		MASS_MSG("eom              : %8X", sense.eom             );
		MASS_MSG("filemark         : %8X", sense.filemark        );
		MASS_MSG("information      : %8X", sense.information     );
		MASS_MSG("additional_len   : %8X", sense.additional_len  );
		MASS_MSG("command_specific : %8X", sense.command_specific);
		MASS_MSG("additional_code  : %8X", sense.additional_code );
		MASS_MSG("additional_qual  : %8X", sense.additional_qual );
		MASS_MSG("unit_code        : %8X", sense.unit_code       );
		MASS_MSG("key_specific1    : %8X", sense.key_specific1   );
		MASS_MSG("sksv             : %8X", sense.sksv            );
		MASS_MSG("key_specific2    : %8X", sense.key_specific2   );
#else
		MASS_MSG("SCSI sense: 0x%02X 0x%02X 0x%02X", sense.sense,
			sense.additional_code, sense.additional_qual);
#endif
	}

	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    mass_storage_try_first_open                                            *
 *===========================================================================*/
static int
mass_storage_try_first_open()
{
	unsigned int llba;
	unsigned int blen;
	unsigned char inquiry[SCSI_INQUIRY_DATA_LEN];
	unsigned char capacity[SCSI_READ_CAPACITY_DATA_LEN];

	MASS_DEBUG_DUMP;

	assert(NULL != driver_state.cur_drive);

	llba = 0; /* Last logical block address */
	blen = 0; /* Block length (usually 512B) */

	/* Run TEST UNIT READY before other SCSI command
	 * Some devices refuse to work without this */
	if (mass_storage_test())
		return EIO;

	/* SCSI INQUIRY OUT stage */
	if (mass_storage_send_scsi_cbw_out(SCSI_INQUIRY, NULL))
		return EIO;

	/* SCSI INQUIRY first IN stage */
	if (mass_storage_send_scsi_data_in(inquiry, sizeof(inquiry)))
		return EIO;

	/* SCSI INQUIRY second IN stage */
	if (mass_storage_send_scsi_csw_in())
		return EIO;

	/* Check for proper reply */
	if (check_inquiry_reply(inquiry))
		return EIO;

	/* Run TEST UNIT READY before other SCSI command
	 * Some devices refuse to work without this */
	if (mass_storage_test())
		return EIO;

	/* SCSI READ CAPACITY OUT stage */
	if (mass_storage_send_scsi_cbw_out(SCSI_READ_CAPACITY, NULL))
		return EIO;

	/* SCSI READ CAPACITY first IN stage */
	if (mass_storage_send_scsi_data_in(capacity, sizeof(capacity)))
		return EIO;

	/* SCSI READ CAPACITY second IN stage */
	if (mass_storage_send_scsi_csw_in())
		return EIO;

	/* Check for proper reply */
	if (check_read_capacity_reply(capacity, &llba, &blen))
		return EIO;

	/* For now only Minix's default SECTOR_SIZE is supported */
	if (SECTOR_SIZE != blen)
		panic("Invalid block size used by USB device!");

	/* Get information about capacity from reply */
	driver_state.cur_drive->disk.dv_base = 0;
	driver_state.cur_drive->disk.dv_size = llba * blen;

	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    mass_storage_transfer_restrictions                                     *
 *===========================================================================*/
static int
mass_storage_transfer_restrictions(u64_t pos, unsigned long bytes)
{
	MASS_DEBUG_DUMP;

	assert(NULL != driver_state.cur_device);

	/* Zero-length request must not be issued */
	if (0 == bytes) {
		MASS_MSG("Transfer request length equals 0");
		return EINVAL;
	}

	/* Starting position must be aligned to sector */
	if (0 != (pos % SECTOR_SIZE)) {
		MASS_MSG("Transfer position not divisible by %u", SECTOR_SIZE);
		return EINVAL;
	}

	/* Length must be integer multiple of sector sizes */
	if (0 != (bytes % SECTOR_SIZE)) {
		MASS_MSG("Data length not divisible by %u", SECTOR_SIZE);
		return EINVAL;
	}

	/* Guard against ridiculous 64B overflow */
	if ((pos + bytes) <= pos) {
		MASS_MSG("Request outside available address space");
		return EINVAL;
	}

	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    mass_storage_write                                                     *
 *===========================================================================*/
static ssize_t
mass_storage_write(unsigned long sector_number,
		endpoint_t endpt,
		iovec_t * iov,
		unsigned int iov_count,
		unsigned long bytes_left)
{
	/*
	 * This function writes whatever was put in 'iov' array
	 * (iov[0] : iov[iov_count]), into continuous region of mass storage,
	 * starting from sector 'sector_number'. Total amount of 'iov'
	 * data should be greater or equal to initial value of 'bytes_left'.
	 *
	 * Endpoint value 'endpt', determines if vectors 'iov' contain memory
	 * addresses for copying or grant IDs.
	 */

	iov_state cur_iov;		/* Current state of vector copying */
	unsigned long bytes_to_write;	/* To be written in this iteration */
	ssize_t bytes_already_written;	/* Total amount written (retval) */

	MASS_DEBUG_DUMP;

	/* Initialize locals */
	cur_iov.remaining_bytes = 0;	/* No IO vector initially */
	cur_iov.iov_idx = 0;		/* Starting from first vector */
	bytes_already_written = 0;	/* Nothing copied yet */

	/* Mass storage operations are sector based */
	assert(0 == (sizeof(buffer) % SECTOR_SIZE));
	assert(0 == (bytes_left % SECTOR_SIZE));

	while (bytes_left > 0) {

		/* Fill write buffer with data from IO Vectors */
		{
			unsigned long buf_offset;
			unsigned long copy_len;

			/* Start copying to the beginning of the buffer */
			buf_offset = 0;

			/* Copy as long as not copied vectors exist or
			 * buffer is not fully filled */
			for (;;) {

				/* If entire previous vector
				 * was used get new one */
				if (0 == cur_iov.remaining_bytes) {
					/* Check if there are
					 * vectors to copied */
					if (cur_iov.iov_idx < iov_count) {

						cur_iov.base_addr =
						  iov[cur_iov.iov_idx].iov_addr;
						cur_iov.remaining_bytes =
						  iov[cur_iov.iov_idx].iov_size;
						cur_iov.offset = 0;
						cur_iov.iov_idx++;

					} else {
						/* All vectors copied */
						break;
					}
				}

				/* Copy as much as it is possible from vector
				 * and at most the amount that can be
				 * put in buffer */
				copy_len = MIN(sizeof(buffer) - buf_offset,
						cur_iov.remaining_bytes);

				/* This distinction is required as transfer can
				 * be used from within this process and meaning
				 * of IO vector'a address is different than
				 * grant ID */
				if (endpt == SELF) {
					memcpy(&buffer[buf_offset],
						(void*)(cur_iov.base_addr +
						cur_iov.offset), copy_len);
				} else {
					ssize_t copy_res;
					if ((copy_res = sys_safecopyfrom(endpt,
						cur_iov.base_addr,
						cur_iov.offset,
						(vir_bytes)
						(&buffer[buf_offset]),
						copy_len))) {
						MASS_MSG("sys_safecopyfrom "
							"failed");
						return copy_res;
					}
				}

				/* Alter current state of copying */
				buf_offset += copy_len;
				cur_iov.offset += copy_len;
				cur_iov.remaining_bytes -= copy_len;

				/* Buffer was filled */
				if (sizeof(buffer) == buf_offset)
					break;
			}

			/* Determine how many bytes from copied buffer we wish
			 * to write, buf_offset represents total amount of
			 * bytes copied above */
			if (bytes_left >= buf_offset) {
				bytes_to_write = buf_offset;
				bytes_left -= buf_offset;
			} else {
				bytes_to_write = bytes_left;
				bytes_left = 0;
			}
		}

		/* Send URB and alter sector number */
		if (bytes_to_write > 0) {

			scsi_transfer info;

			info.length = bytes_to_write;
			info.lba = sector_number;

			/* SCSI WRITE first OUT stage */
			if (mass_storage_send_scsi_cbw_out(SCSI_WRITE, &info))
				return EIO;

			/* SCSI WRITE second OUT stage */
			if (mass_storage_send_scsi_data_out(buffer,
							bytes_to_write))
				return EIO;

			/* SCSI WRITE IN stage */
			if (mass_storage_send_scsi_csw_in())
				return EIO;

			/* Writing completed so shift starting
			 * sector for next iteration */
			sector_number += bytes_to_write / SECTOR_SIZE;

			/* Update amount of data already copied */
			bytes_already_written += bytes_to_write;
		}
	}

	return bytes_already_written;
}


/*===========================================================================*
 *    mass_storage_read                                                      *
 *===========================================================================*/
static ssize_t
mass_storage_read(unsigned long sector_number,
		endpoint_t endpt,
		iovec_t * iov,
		unsigned int iov_count,
		unsigned long bytes_left)
{
	/*
	 * This function reads 'bytes_left' bytes of mass storage data into
	 * 'iov' array (iov[0] : iov[iov_count]) starting from sector
	 * 'sector_number'. Total amount of 'iov' data should be greater or
	 * equal to initial value of 'bytes_left'.
	 *
	 * Endpoint value 'endpt', determines if vectors 'iov' contain memory
	 * addresses for copying or grant IDs.
	 */

	iov_state cur_iov;		/* Current state of vector copying */
	unsigned long bytes_to_read;	/* To be read in this iteration */
	ssize_t bytes_already_read;	/* Total amount read (retval) */

	MASS_DEBUG_DUMP;

	/* Initialize locals */
	cur_iov.remaining_bytes = 0;	/* No IO vector initially */
	cur_iov.iov_idx = 0;		/* Starting from first vector */
	bytes_already_read = 0;		/* Nothing copied yet */

	/* Mass storage operations are sector based */
	assert(0 == (sizeof(buffer) % SECTOR_SIZE));
	assert(0 == (bytes_left % SECTOR_SIZE));

	while (bytes_left > 0) {

		/* Decide read length and alter remaining bytes */
		{
			/* Number of bytes to be read in next URB */
			if (bytes_left > sizeof(buffer)) {
				bytes_to_read = sizeof(buffer);
			} else {
				bytes_to_read = bytes_left;
			}

			bytes_left -= bytes_to_read;
		}

		/* Send URB and alter sector number */
		{
			scsi_transfer info;

			info.length = bytes_to_read;
			info.lba = sector_number;

			/* SCSI READ OUT stage */
			if (mass_storage_send_scsi_cbw_out(SCSI_READ, &info))
				return EIO;

			/* SCSI READ first IN stage */
			if (mass_storage_send_scsi_data_in(buffer,
							bytes_to_read))
				return EIO;

			/* SCSI READ second IN stage */
			if (mass_storage_send_scsi_csw_in())
				return EIO;

			/* Reading completed so shift starting
			 * sector for next iteration */
			sector_number += bytes_to_read / SECTOR_SIZE;
		}

		/* Fill IO Vectors with data from buffer */
		{
			unsigned long buf_offset;
			unsigned long copy_len;

			/* Start copying from the beginning of the buffer */
			buf_offset = 0;

			/* Copy as long as there are unfilled vectors
			 * or data in buffer remains */
			for (;;) {

				/* If previous vector was filled get new one */
				if (0 == cur_iov.remaining_bytes) {
					/* Check if there are vectors
					 * to be filled */
					if (cur_iov.iov_idx < iov_count) {

						cur_iov.base_addr =
						  iov[cur_iov.iov_idx].iov_addr;
						cur_iov.remaining_bytes =
						  iov[cur_iov.iov_idx].iov_size;
						cur_iov.offset = 0;
						cur_iov.iov_idx++;

					} else {
						/* Total length of vectors
						 * should be greater or equal
						 * to initial value of
						 * bytes_left. Being here means
						 * that everything should
						 * have been copied already */
						assert(0 == bytes_left);
						break;
					}
				}

				/* Copy as much as it is possible from buffer
				 * and at most the amount that can be
				 * put in vector */
				copy_len = MIN(bytes_to_read - buf_offset,
						cur_iov.remaining_bytes);

				/* This distinction is required as transfer can
				 * be used from within this process and meaning
				 * of IO vector'a address is different than
				 * grant ID */
				if (endpt == SELF) {
					memcpy((void*)(cur_iov.base_addr +
							cur_iov.offset),
							&buffer[buf_offset],
							copy_len);
				} else {
					ssize_t copy_res;
					if ((copy_res = sys_safecopyto(endpt,
						cur_iov.base_addr,
						cur_iov.offset,
						(vir_bytes)
						(&buffer[buf_offset]),
						copy_len))) {
						MASS_MSG("sys_safecopyto "
							"failed");
						return copy_res;
					}
				}

				/* Alter current state of copying */
				buf_offset += copy_len;
				cur_iov.offset += copy_len;
				cur_iov.remaining_bytes -= copy_len;

				/* Everything was copied */
				if (bytes_to_read == buf_offset)
					break;
			}

			/* Update amount of data already copied */
			bytes_already_read += buf_offset;
		}
	}

	return bytes_already_read;
}


/*===========================================================================*
 *    mass_storage_open                                                      *
 *===========================================================================*/
static int
mass_storage_open(devminor_t minor, int UNUSED(access))
{
	mass_storage_drive * d;
	int r;

	MASS_DEBUG_DUMP;

	/* Decode minor into drive device information */
	if (NULL == (mass_storage_part(minor)))
		return ENXIO;

	/* Copy evaluated current drive for simplified dereference */
	d = driver_state.cur_drive;

#ifdef MASS_RESET_RECOVERY
	/* In case of previous CBW mismatch */
	if (mass_storage_reset_recovery()) {
		MASS_MSG("Resetting mass storage device failed");
		return EIO;
	}
#endif

	/* In case of missing endpoint information, do simple
	 * enumeration and hold it for future use */
	if ((URB_INVALID_EP == driver_state.cur_periph->ep_in.ep_num) ||
		(URB_INVALID_EP == driver_state.cur_periph->ep_out.ep_num)) {

		if (mass_storage_get_endpoints(&driver_state.cur_periph->ep_in,
					&driver_state.cur_periph->ep_out))
			return EIO;
	}

	/* If drive hasn't been opened yet, try to open it */
	if (d->open_ct == 0) {
		if ((r = mass_storage_try_first_open())) {
			MASS_MSG("Opening mass storage device"
				" for the first time failed");

			/* Do one more test before failing, to output
			 * sense errors in case they weren't dumped already */
			if (mass_storage_test())
				MASS_MSG("Final TEST UNIT READY failed");

			return r;
		}

		/* Clear remembered device state for current
		 * drive before calling partition */
		memset(&d->part[0],	0, sizeof(d->part));
		memset(&d->subpart[0],	0, sizeof(d->subpart));

		/* Try parsing partition table (for entire drive) */
		/* Warning!! This call uses mass_storage_part with own minors
		 * and alters global driver_state.cur_device! */
		partition(&mass_storage, (d->drive_idx * DEV_PER_DRIVE),
			P_PRIMARY, 0);

		/* Decode minor into UPDATED drive device information */
		if (NULL == (mass_storage_part(minor)))
			return ENXIO;

		/* Decoded size must be positive or else
		 * we assume device (partition) is unavailable */
		if (0 == driver_state.cur_device->dv_size)
			return ENXIO;
	}

	/* Run TEST UNIT READY before further commands
	 * Some devices refuse to work without this */
	if (mass_storage_test())
		return EIO;

	/* Opening completed */
	d->open_ct++;

	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    mass_storage_close                                                     *
 *===========================================================================*/
static int mass_storage_close(devminor_t minor)
{
	MASS_DEBUG_DUMP;

	/* Decode minor into drive device information */
	if (NULL == (mass_storage_part(minor)))
		return ENXIO;

	/* If drive hasn't been opened yet */
	if (driver_state.cur_drive->open_ct == 0) {
		MASS_MSG("Device was not opened yet");
		return ERESTART;
	}

	/* Act accordingly */
	driver_state.cur_drive->open_ct--;

	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    mass_storage_transfer                                                  *
 *===========================================================================*/
static ssize_t
mass_storage_transfer(devminor_t minor,		/* device minor number */
		int do_write,			/* 1 write, 0 read */
		u64_t pos,			/* position of starting point */
		endpoint_t endpt,		/* endpoint */
		iovec_t * iov,			/* vector */
		unsigned int iov_count,		/* how many vectors */
		int UNUSED(flags))		/* transfer flags */
{
	u64_t temp_sector_number;
	unsigned long bytes;
	unsigned long sector_number;
	unsigned int cur_iov_idx;
	int r;

	MASS_DEBUG_DUMP;

	/* Decode minor into drive device information */
	if (NULL == (mass_storage_part(minor)))
		return ENXIO;

	bytes = 0;

	/* How much data is going to be transferred? */
	for (cur_iov_idx = 0; cur_iov_idx < iov_count; ++cur_iov_idx) {

		/* Check if grant ID was supplied
		 * instead of address and if it is valid */
		if (endpt != SELF)
			if (!GRANT_VALID((cp_grant_id_t)
				(iov[cur_iov_idx].iov_addr)))
				return EINVAL;

		/* All supplied vectors must have positive length */
		if ((signed long)(iov[cur_iov_idx].iov_size) <= 0) {
			MASS_MSG("Transfer request length is not positive");
			return EINVAL;
		}

		/* Requirements were met, more bytes can be transferred */
		bytes += iov[cur_iov_idx].iov_size;

		/* Request size must never overflow */
		if ((signed long)bytes <= 0) {
			MASS_MSG("Transfer request length overflowed");
			return EINVAL;
		}
	}

	/* Check if reading beyond device/partition */
	if (pos >= driver_state.cur_device->dv_size) {
		MASS_MSG("Request out of bounds for given device");
		return 0; /* No error and no bytes read */
	}

	/* Check if arguments agree with accepted restriction
	 * for parameters of transfer */
	if ((r = mass_storage_transfer_restrictions(pos, bytes)))
		return r;

	/* If 'hard' requirements above were met, do additional
	 * limiting to device/partition boundary */
	if ((pos + bytes) > driver_state.cur_device->dv_size)
		bytes = (driver_state.cur_device->dv_size - pos) &
			~SECTOR_MASK;

	/* We have to obtain sector number of given position
	 * and limit it to what URB can handle */
	temp_sector_number = (driver_state.cur_device->dv_base + pos) /
				SECTOR_SIZE;
	assert(temp_sector_number < ULONG_MAX); /* LBA is limited to 32B */
	sector_number = (unsigned long)temp_sector_number;

	if (do_write)
		return mass_storage_write(sector_number, endpt, iov,
					iov_count, bytes);
	else
		return mass_storage_read(sector_number, endpt, iov,
					iov_count, bytes);
}


/*===========================================================================*
 *    mass_storage_ioctl                                                     *
 *===========================================================================*/
static int
mass_storage_ioctl(devminor_t minor, unsigned long request, endpoint_t endpt,
			cp_grant_id_t grant, endpoint_t UNUSED(user_endpt))
{
	MASS_DEBUG_DUMP;

	/* Decode minor into drive device information */
	if (NULL == (mass_storage_part(minor)))
		return ENXIO;

	switch (request) {
		case DIOCOPENCT:
			if (sys_safecopyto(endpt, grant, 0,
				(vir_bytes) &(driver_state.cur_drive->open_ct),
				sizeof(driver_state.cur_drive->open_ct)))
				panic("sys_safecopyto failed!");

			return EXIT_SUCCESS;

		default:
			MASS_MSG("Unimplemented IOCTL request: 0x%X",
				(int)request);
			break;
	}

	return ENOTTY;
}


/*===========================================================================*
 *    mass_storage_cleanup                                                   *
 *===========================================================================*/
static void mass_storage_cleanup(void)
{
	MASS_DEBUG_DUMP;
	return;
}


/*===========================================================================*
 *    mass_storage_part                                                      *
 *===========================================================================*/
static struct device *
mass_storage_part(devminor_t minor)
{
	unsigned long sel_drive;
	unsigned long sel_device;

	MASS_DEBUG_DUMP;

	/* Override every time before further decision */
	driver_state.cur_drive = NULL;
	driver_state.cur_device = NULL;
	driver_state.cur_periph = NULL;

	/* Decode 'minor' code to find which device file was used */
	if (minor < MINOR_d0p0s0) {

		/* No sub-partitions used */
		sel_drive = minor / DEV_PER_DRIVE;
		sel_device = minor % DEV_PER_DRIVE;

		/* Only valid minors */
		if (sel_drive < MAX_DRIVES) {
			/* Associate minor (device/partition)
			 * with peripheral number */
			/* TODO:PERIPH
			 * Proper peripheral selection based
			 * on minor should be here: */
			driver_state.cur_periph = &driver_state.periph[0];

			/* Select drive entry for opening count etc. */
			driver_state.cur_drive =
				&(driver_state.cur_periph->drives[sel_drive]);

			/* Select device entry for given device file */
			/* Device 0 means entire drive.
			 * Devices 1,2,3,4 mean partitions 0,1,2,3 */
			if (0 == sel_device)
				driver_state.cur_device =
					&(driver_state.cur_drive->disk);
			else
				driver_state.cur_device =
					&(driver_state.cur_drive->part
								[sel_device-1]);
		}

	} else {

		/* Shift values accordingly */
		minor -= MINOR_d0p0s0;

		/* Sub-partitions used */
		sel_drive = minor / SUBPART_PER_DISK;
		sel_device = minor % SUBPART_PER_DISK;

		/* Only valid minors */
		if (sel_drive < MAX_DRIVES) {
			/* Leave in case of ridiculously high number */
			if (minor < SUBPART_PER_DISK) {
				/* Associate minor (device/partition)
				 * with peripheral number */
				/* TODO:PERIPH
				 * Proper peripheral selection based
				 * on minor should be here: */
				driver_state.cur_periph =
						&driver_state.periph[0];

				/* Select drive entry for opening count etc. */
				driver_state.cur_drive =
					&(driver_state.cur_periph->drives
								[sel_drive]);
				/* Select device entry for given
				 * sub-partition device file */
				driver_state.cur_device =
					&(driver_state.cur_drive->subpart
								[sel_device]);
			}
		}
	}

	/* Check for success */
	if (NULL == driver_state.cur_device) {
		MASS_MSG("Device for minor: %u not found", minor);
	} else {
		/* Assign index as well */
		driver_state.cur_drive->drive_idx = sel_drive;
	}

	return driver_state.cur_device;
}


/*===========================================================================*
 *    mass_storage_geometry                                                  *
 *===========================================================================*/
/* This command is optional for most mass storage devices
 * It should rather be used with USB floppy disk reader */
#ifdef MASS_USE_GEOMETRY
static void
mass_storage_geometry(devminor_t minor, struct part_geom * part)
{
	char flexible_disk_page[SCSI_MODE_SENSE_FLEX_DATA_LEN];

	MASS_DEBUG_DUMP;

	/* Decode minor into drive device information */
	if (NULL == (mass_storage_part(minor)))
		return;

	/* SCSI MODE SENSE OUT stage */
	if (mass_storage_send_scsi_cbw_out(SCSI_MODE_SENSE, NULL))
		return;

	/* SCSI MODE SENSE first IN stage */
	if (mass_storage_send_scsi_data_in(flexible_disk_page,
					sizeof(flexible_disk_page)))
		return;

	/* SCSI MODE SENSE second IN stage */
	if (mass_storage_send_scsi_csw_in())
		return;

	/* Get geometry from reply */
	if (check_mode_sense_reply(flexible_disk_page, &(part->cylinders),
				&(part->heads), &(part->sectors)))
		return;
#else
static void
mass_storage_geometry(devminor_t UNUSED(minor), struct part_geom * part)
{
	MASS_DEBUG_DUMP;

	part->cylinders = part->size / SECTOR_SIZE;
	part->heads = 64;
	part->sectors = 32;
#endif
}


/*===========================================================================*
 *    usb_driver_completion                                                  *
 *===========================================================================*/
static void
usb_driver_completion(void * UNUSED(priv))
{
	/* Last request was completed so allow continuing
	 * execution from place where semaphore was downed */
	ddekit_sem_up(mass_storage_sem);
}


/*===========================================================================*
 *    usb_driver_connect                                                     *
 *===========================================================================*/
static void
usb_driver_connect(struct ddekit_usb_dev * dev,
		unsigned int interfaces)
{
	MASS_DEBUG_DUMP;

	/* TODO:PERIPH
	 * Some sort of more complex peripheral assignment should be here */
	driver_state.cur_periph = &driver_state.periph[0];

	if (NULL != driver_state.cur_periph->dev)
		panic("Only one peripheral can be connected!");

	/* Hold host information for future use */
	driver_state.cur_periph->dev = dev;
	driver_state.cur_periph->interfaces = interfaces;
	driver_state.cur_periph->ep_in.ep_num = URB_INVALID_EP;
	driver_state.cur_periph->ep_out.ep_num = URB_INVALID_EP;
}


/*===========================================================================*
 *    usb_driver_disconnect                                                  *
 *===========================================================================*/
static void
usb_driver_disconnect(struct ddekit_usb_dev * UNUSED(dev))
{
	MASS_DEBUG_DUMP;

	/* TODO:PERIPH
	 * Some sort of peripheral discard should be here */
	driver_state.cur_periph = &driver_state.periph[0];

	assert(NULL != driver_state.cur_periph->dev);

	/* Clear */
	driver_state.cur_periph->dev = NULL;
	driver_state.cur_periph->interfaces = 0;
	driver_state.cur_periph->ep_in.ep_num = URB_INVALID_EP;
	driver_state.cur_periph->ep_out.ep_num = URB_INVALID_EP;
}


/*===========================================================================*
 *    mass_storage_get_endpoints                                             *
 *===========================================================================*/
static int
mass_storage_get_endpoints(urb_ep_config * ep_in, urb_ep_config * ep_out)
{
	/* URB to be send */
	struct ddekit_usb_urb urb;

	/* Setup buffer to be attached */
	struct usb_ctrlrequest setup_buf;

	/* Control EP configuration */
	urb_ep_config ep_conf;

	/* Descriptors' buffer */
	unsigned char descriptors[MAX_DESCRIPTORS_LEN];

	MASS_DEBUG_DUMP;

	/* Initialize EP configuration */
	ep_conf.ep_num = 0;
	ep_conf.direction = DDEKIT_USB_IN;
	ep_conf.type = DDEKIT_USB_TRANSFER_CTL;
	ep_conf.max_packet_size = 0;
	ep_conf.interval = 0;

	/* Reset URB and assign given values */
	init_urb(&urb, driver_state.cur_periph->dev, &ep_conf);

	/* Clear setup data */
	memset(&setup_buf, 0, sizeof(setup_buf));

	/* Standard get endpoint request */
	setup_buf.bRequestType = 0x80; /* Device to host */
	setup_buf.bRequest = UR_GET_DESCRIPTOR;
	setup_buf.wValue = UDESC_CONFIG << 8; /* TODO: configuration 0 */
	setup_buf.wIndex = 0x00;
	setup_buf.wLength = MAX_DESCRIPTORS_LEN;

	/* Attach buffers to URB */
	attach_urb_data(&urb, URB_BUF_TYPE_SETUP,
			&setup_buf, sizeof(setup_buf));
	attach_urb_data(&urb, URB_BUF_TYPE_DATA,
			descriptors, sizeof(descriptors));

	/* Send and wait for response */
	if (blocking_urb_submit(&urb, mass_storage_sem,
				URB_SUBMIT_ALLOW_MISMATCH))
		return EXIT_FAILURE;

	/* Check if buffer was supposed to hold more data */
	if (urb.size < urb.actual_length) {
		MASS_MSG("Too much descriptor data received");
		return EXIT_FAILURE;
	}

	return mass_storage_parse_descriptors(urb.data, urb.actual_length,
						ep_in, ep_out);
}


/*===========================================================================*
 *    mass_storage_parse_endpoint                                            *
 *===========================================================================*/
static int
mass_storage_parse_endpoint(usb_descriptor_t * cur_desc,
			urb_ep_config * ep_in, urb_ep_config * ep_out)
{
	usb_endpoint_descriptor_t * ep_desc;

	urb_ep_config * this_ep;

	MASS_DEBUG_DUMP;

	ep_desc = (usb_endpoint_descriptor_t *)cur_desc;

	/* Only bulk with no other attributes are important */
	if (UE_BULK == ep_desc->bmAttributes) {

		/* Check for direction */
		if (UE_DIR_IN == UE_GET_DIR(ep_desc->bEndpointAddress)) {

			this_ep = ep_in;
			this_ep->direction = DDEKIT_USB_IN;

		} else {

			this_ep = ep_out;
			this_ep->direction = DDEKIT_USB_OUT;
		}

		/* Check if it was set before */
		if (URB_INVALID_EP != this_ep->ep_num) {
			MASS_MSG("BULK EP already set");
			return EXIT_FAILURE;
		}

		/* Assign rest */
		this_ep->ep_num = UE_GET_ADDR(ep_desc->bEndpointAddress);
		this_ep->type = DDEKIT_USB_TRANSFER_BLK;
		this_ep->max_packet_size = UGETW(ep_desc->wMaxPacketSize);
		this_ep->interval = ep_desc->bInterval;
	}

	/* EP type other than bulk, is also correct,
	 * just no parsing is performed */
	return EXIT_SUCCESS;
}

/*===========================================================================*
 *    mass_storage_parse_descriptors                                         *
 *===========================================================================*/
static int
mass_storage_parse_descriptors(char * desc_buf, unsigned int buf_len,
				urb_ep_config * ep_in, urb_ep_config * ep_out)
{
	/* Currently parsed, descriptors */
	usb_descriptor_t * cur_desc;
	usb_interface_descriptor_t * ifc_desc;

	/* Byte counter for descriptor parsing */
	unsigned int cur_byte;

	/* Non zero if recently parsed interface is valid for this device */
	int valid_interface;

	MASS_DEBUG_DUMP;

	/* Parse descriptors to get endpoints */
	ep_in->ep_num = URB_INVALID_EP;
	ep_out->ep_num = URB_INVALID_EP;
	valid_interface = 0;
	cur_byte = 0;

	while (cur_byte < buf_len) {

		/* Map descriptor to buffer */
		/* Structure is packed so alignment should not matter */
		cur_desc = (usb_descriptor_t *)&(desc_buf[cur_byte]);

		/* Check this so we won't be reading
		 * memory outside the buffer */
		if ((cur_desc->bLength > 3) &&
			(cur_byte + cur_desc->bLength <= buf_len)) {

			/* Parse based on descriptor type */
			switch (cur_desc->bDescriptorType) {

				case UDESC_CONFIG: {
					if (USB_CONFIG_DESCRIPTOR_SIZE !=
						cur_desc->bLength) {
						MASS_MSG("Wrong configuration"
							" descriptor length");
						return EXIT_FAILURE;
					}
					break;
				}

				case UDESC_STRING:
					break;

				case UDESC_INTERFACE: {
					ifc_desc =
					 (usb_interface_descriptor_t *)cur_desc;

					if (USB_INTERFACE_DESCRIPTOR_SIZE !=
						cur_desc->bLength) {
						MASS_MSG("Wrong interface"
							" descriptor length");
						return EXIT_FAILURE;
					}

					/* Check if following data is meant
					 * for our interfaces */
					if ((1 << ifc_desc->bInterfaceNumber) &
					    driver_state.cur_periph->interfaces)
						valid_interface = 1;
					else
						valid_interface = 0;

					break;
				}

				case UDESC_ENDPOINT: {
					if (USB_ENDPOINT_DESCRIPTOR_SIZE !=
						cur_desc->bLength) {
						MASS_MSG("Wrong endpoint"
							" descriptor length");
						return EXIT_FAILURE;
					}

					/* Previous interface was,
					 * what we were looking for */
					if (valid_interface) {
						if (EXIT_SUCCESS !=
						    mass_storage_parse_endpoint(
						    cur_desc, ep_in, ep_out))
							return EXIT_FAILURE;

					}

					break;
				}

				default: {
					MASS_MSG("Wrong descriptor type");
					return EXIT_FAILURE;
				}
			}

		} else {
			MASS_MSG("Invalid descriptor length");
			return EXIT_FAILURE;
		}

		/* Get next descriptor */
		cur_byte += cur_desc->bLength;
	}

	/* Total length should match sum of all descriptors' lengths... */
	if (cur_byte > buf_len)
		return EXIT_FAILURE;

	/* ...and descriptors should be valid */
	if ((URB_INVALID_EP == ep_in->ep_num) ||
		(URB_INVALID_EP == ep_out->ep_num)) {
		MASS_MSG("Valid bulk endpoints not found");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
