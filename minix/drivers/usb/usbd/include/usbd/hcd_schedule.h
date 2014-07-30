/*
 * HCD URB scheduler interface
 */

#ifndef _HCD_SCHEDULE_H_
#define _HCD_SCHEDULE_H_

#include <usbd/hcd_common.h>

/* Makes URB schedule enabled */
int hcd_schedule_urb(hcd_urb *);

/* Makes URB schedule disabled */
void hcd_unschedule_urb(hcd_urb *);

#endif /* !_HCD_SCHEDULE_H_ */
