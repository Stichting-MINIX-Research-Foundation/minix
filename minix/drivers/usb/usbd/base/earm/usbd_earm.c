/*
 * EARM USBD setup
 */

#include <minix/board.h>
#include <minix/syslib.h>

#include <usbd/hcd_platforms.h>
#include <usbd/usbd_common.h>
#include <usbd/usbd_interface.h>


/*===========================================================================*
 *    usbd_init_hcd                                                          *
 *===========================================================================*/
int
usbd_init_hcd(void)
{
	/* More specific platform type than just EARM */
	static struct machine platform;

	DEBUG_DUMP;

	if (sys_getmachine(&platform)) {
		USB_MSG("Getting machine type, failed");
		return EXIT_FAILURE;
	}

	if (BOARD_IS_BB(platform.board_id)) {
		USB_MSG("Using AM335x driver");
		return musb_am335x_init();
	} else {
		USB_MSG("Only AM335x driver available");
		return EXIT_FAILURE;
	}
}


/*===========================================================================*
 *    usbd_deinit_hcd                                                        *
 *===========================================================================*/
void
usbd_deinit_hcd(void)
{
	/* More specific platform type than just EARM */
	static struct machine platform;

	DEBUG_DUMP;

	if (sys_getmachine(&platform)) {
		USB_MSG("Getting machine type, failed");
		return;
	}

	if (BOARD_IS_BB(platform.board_id))
		musb_am335x_deinit();
	else
		USB_MSG("Only AM335x driver available");
}
