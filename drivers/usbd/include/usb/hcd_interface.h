/*
 * Interface for HCD
 *
 * This file holds prototypes that must be implemented by HCD
 * and event call that should be called when interrupt occurred
 */

#ifndef _HCD_INTERFACE_H_
#define _HCD_INTERFACE_H_

#include <usb/hcd_common.h>


/*===========================================================================*
 *    HCD event handling types                                               *
 *===========================================================================*/
/* Possible HCD events */
typedef enum {

	HCD_EVENT_CONNECTED,
	HCD_EVENT_DISCONNECTED,
	HCD_EVENT_ENDPOINT
}
hcd_event;

/* Possible HCD sub-events */
typedef enum {

	HCD_SUBEVENT_NONE,
	HCD_SUBEVENT_EP0,
}
hcd_subevent;


/*===========================================================================*
 *    HCD additional defines                                                 *
 *===========================================================================*/
#define HCD_READ_ERR -1


/*===========================================================================*
 *    HCD driver structure to be filled
 *===========================================================================*/
struct hcd_driver_state {
	/* Standard USB controller procedures */
	void		(*setup_device)		(void *, hcd_reg1, hcd_reg1);
	void		(*reset_device)		(void *);
	void		(*setup_stage)		(void *, hcd_ctrlrequest *);
	void		(*in_data_stage)	(void *);
	void		(*out_data_stage)	(void *);
	void		(*in_status_stage)	(void *);
	void		(*out_status_stage)	(void *);
	int		(*read_data)		(void *, hcd_reg1 *, int);
	int		(*check_error)		(void *);

	/* Controller's private data (like mapped registers) */
	void *		private_data;

	/* Current state to be handled by driver */
	hcd_event	event;
	hcd_subevent	subevent;
};


/*===========================================================================*
 *    HCD event handling routine                                             *
 *===========================================================================*/
/* Handle asynchronous event
 * This must be called in case of specific HCD interrupts listed above */
void hcd_handle_event(hcd_driver_state *);


#endif /* !_HCD_INTERFACE_H_ */
