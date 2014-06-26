/*
 * Interface for USBD
 *
 * This file holds prototypes that must be implemented by platform
 * specific USBD
 *
 * Must be implemented once per USBD but may be used for multiple
 * controllers at a time when platform has more than one HCD
 */

#ifndef _USBD_INTERFACE_H_
#define _USBD_INTERFACE_H_

/*===========================================================================*
 *    Prototypes to be implemented                                           *
 *===========================================================================*/
/* Must set up HCDs in general and interrupts to
 * be handled by DDEkit in particular */
int usbd_init_hcd(void);

/* Should clean whatever usbd_init_hcd used */
void usbd_deinit_hcd(void);

#endif /* !_USBD_INTERFACE_H_ */
