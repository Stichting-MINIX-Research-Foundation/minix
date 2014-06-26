/*
 * Whatever must be known to DDEkit callers
 */

#ifndef _HCD_DDEKIT_H_
#define _HCD_DDEKIT_H_

#include <usbd/hcd_common.h>

/*===========================================================================*
 *    External declarations                                                  *
 *===========================================================================*/
void hcd_connect_cb(hcd_device_state *);
void hcd_disconnect_cb(hcd_device_state *);
void hcd_completion_cb(hcd_urb *);


#endif /* !_HCD_DDEKIT_H_ */
