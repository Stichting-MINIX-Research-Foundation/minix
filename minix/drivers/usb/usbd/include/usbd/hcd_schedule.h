/*
 * HCD URB scheduler interface
 */

#ifndef _HCD_SCHEDULE_H_
#define _HCD_SCHEDULE_H_

#include <usbd/hcd_common.h>

/* Makes external (device driver) URB schedule enabled */
int hcd_schedule_external_urb(hcd_urb *);

/* Makes internal (HCD) URB schedule enabled */
int hcd_schedule_internal_urb(hcd_urb *);

#endif /* !_HCD_SCHEDULE_H_ */
