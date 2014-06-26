/*
 * USBD URB scheduler interface
 */

#ifndef _USBD_SCHEDULE_H_
#define _USBD_SCHEDULE_H_

/* Should be used to create/destroy URB scheduler in base code */
int usbd_init_scheduler(void);
void usbd_deinit_scheduler(void);

#endif /* !_USBD_SCHEDULE_H_ */
