/*
 * Entry point for USBD service, that handles USB HCDs
 */

#include <ddekit/ddekit.h>		/* ddekit_init */
#include <ddekit/thread.h>		/* DDEKit threading */

#include <libdde/usb_server.h>		/* DDEKit USB server */

#include <minix/devman.h>		/* Initializing 'devman' */
#include <minix/sef.h>			/* SEF handling */

#include <usbd/usbd_common.h>
#include <usbd/usbd_interface.h>
#include <usbd/usbd_schedule.h>


/*===========================================================================*
 *    Local declarations                                                     *
 *===========================================================================*/
static int usbd_sef_handler(int, sef_init_info_t *);
static void usbd_signal_handler(int);
static int usbd_start(void);
static void usbd_init(void);
static void usbd_server_thread(void *);

/* TODO: No headers for these... */
extern void ddekit_minix_wait_exit(void);	/* dde.c */
extern void ddekit_shutdown(void);		/* dde.c */


/*===========================================================================*
 *    main                                                                   *
 *===========================================================================*/
int
main(int UNUSED(argc), char * UNUSED(argv[]))
{
	int ret_val;

	USB_MSG("Starting USBD");
	USB_MSG("Built: %s %s", __DATE__, __TIME__);

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
	/* No DEBUG_DUMP, threading unavailable yet */

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
 *    usbd_signal_handler                                                    *
 *===========================================================================*/
static void
usbd_signal_handler(int UNUSED(signo))
{
	DEBUG_DUMP;

	USB_MSG("Signal received, exiting USBD...");

	/* Try graceful DDEKit exit */
	ddekit_shutdown();

	/* Unreachable, when ddekit_shutdown works correctly */
	USB_ASSERT(0, "Calling ddekit_shutdown failed!");
}


/*===========================================================================*
 *    usbd_start                                                             *
 *===========================================================================*/
static int
usbd_start(void)
{
	ddekit_thread_t * usbd_th;

	DEBUG_DUMP;

	/* Driver's "main loop" is within DDEKit server thread */
	usbd_th = ddekit_thread_create(usbd_server_thread, NULL, "ddekit_usb");

	/* After spawning, allow server thread to work */
	if (NULL != usbd_th) {

		/* Allow URB scheduling */
		if (usbd_init_scheduler()) {
			USB_MSG("Failed to start URB scheduler");
		} else {
			/* This will lock current thread until DDEKit exits */
			ddekit_minix_wait_exit();
		}

		/* Disallow URB scheduling */
		usbd_deinit_scheduler();

		/* Cleanup */
		ddekit_thread_terminate(usbd_th);

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
	/* No DEBUG_DUMP, threading unavailable yet */

	/* Set one handler for all messages */
	sef_setcb_init_fresh(usbd_sef_handler);
	sef_setcb_init_lu(usbd_sef_handler);
	sef_setcb_init_restart(usbd_sef_handler);

	/* Initialize DDEkit (involves sef_startup()) */
	ddekit_init();

	/* After threading initialization, add signal handler */
	sef_setcb_signal_handler(usbd_signal_handler);
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
