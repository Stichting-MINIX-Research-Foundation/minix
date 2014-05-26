/*
 * Entry point for USBD service, that handles USB HCDs
 */

#include <ddekit/ddekit.h>		/* ddekit_init */
#include <ddekit/thread.h>		/* DDEKit threading */

#include <minix/devman.h>		/* Initializing 'devman' */
#include <minix/sef.h>			/* SEF handling */

#include <usb/usb_common.h>
#include <usb/usbd_interface.h>


/*===========================================================================*
 *    Local declarations                                                     *
 *===========================================================================*/
static int usbd_sef_handler(int, sef_init_info_t *);
static int usbd_start(void);
static void usbd_init(void);
static void usbd_server_thread(void *);

/* TODO: No headers for these... */
extern void ddekit_minix_wait_exit(void); /* dde.c */
extern void ddekit_usb_server_init(void); /* usb_server.c */


/*===========================================================================*
 *    main                                                                   *
 *===========================================================================*/
int
main(int UNUSED(argc), char * UNUSED(argv[]))
{
	int ret_val;

	USB_MSG("Starting USBD");

	/* Basic SEF,DDE,... initialization */
	usbd_init();

	/* Assume failure unless usbd_start exits gracefully */
	ret_val = EXIT_FAILURE;

	/* USB host controllers initialization */
	if (EXIT_SUCCESS == usbd_init_hcd()) {

		/* Try initializing 'devman' */
		if (EXIT_SUCCESS == devman_init()) {

			/* Run USB driver (actually DDEKit threads)
			 * until this call returns: */
			ret_val = usbd_start();

		} else
			USB_MSG("Initializing devman, failed");

		/* Clean whatever was initialized */
		usbd_deinit_hcd();

	} else
		USB_MSG("Initializing HCDs, failed");

	return ret_val;
}


/*===========================================================================*
 *    usbd_sef_handler                                                       *
 *===========================================================================*/
static int
usbd_sef_handler(int type, sef_init_info_t * UNUSED(info))
{
	DEBUG_DUMP;

	switch (type) {
		case SEF_INIT_FRESH:
			USB_MSG("Initializing");
			return EXIT_SUCCESS;

		case SEF_INIT_LU:
			USB_MSG("Updating, not implemented");
			return EXIT_FAILURE;

		case SEF_INIT_RESTART:
			USB_MSG("Restarting, not implemented");
			return EXIT_FAILURE;

		default:
			USB_MSG("illegal SEF type");
			return EXIT_FAILURE;
	}
}


/*===========================================================================*
 *    usbd_start                                                             *
 *===========================================================================*/
static int
usbd_start(void)
{
	DEBUG_DUMP;

	/* Driver's "main loop" is within DDEKit server thread */
	if (NULL != ddekit_thread_create(usbd_server_thread, NULL, "USBD")) {
		/* This will lock current thread until DDEKit terminates */
		ddekit_minix_wait_exit();
		return EXIT_SUCCESS;
	} else
		return EXIT_FAILURE;
}


/*===========================================================================*
 *    usbd_init                                                              *
 *===========================================================================*/
static void
usbd_init(void)
{
	DEBUG_DUMP;

	/* Set one handler for all messages */
	sef_setcb_init_fresh(usbd_sef_handler);
	sef_setcb_init_lu(usbd_sef_handler);
	sef_setcb_init_restart(usbd_sef_handler);

	/* Initialize DDEkit (involves sef_startup()) */
	ddekit_init();
}


/*===========================================================================*
 *    usbd_server_thread                                                     *
 *===========================================================================*/
static void
usbd_server_thread(void * UNUSED(unused))
{
	DEBUG_DUMP;

	ddekit_usb_server_init();
}
