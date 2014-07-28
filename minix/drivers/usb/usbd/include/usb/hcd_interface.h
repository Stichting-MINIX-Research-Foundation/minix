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
 *    HCD additional defines                                                 *
 *===========================================================================*/
/* Can be returned by 'read_data' to indicate error */
#define HCD_READ_ERR -1


/*===========================================================================*
 *    HCD driver structure to be filled
 *===========================================================================*/
struct hcd_driver_state {
	/* Standard USB controller procedures */
	void	(*setup_device)		(void *, hcd_reg1, hcd_reg1);
	int	(*reset_device)		(void *, hcd_speed *);
	void	(*setup_stage)		(void *, hcd_ctrlrequest *);
	void	(*rx_stage)		(void *, hcd_datarequest *);
	void	(*tx_stage)		(void *, hcd_datarequest *);
	void	(*in_data_stage)	(void *);
	void	(*out_data_stage)	(void *);
	void	(*in_status_stage)	(void *);
	void	(*out_status_stage)	(void *);
	int	(*read_data)		(void *, hcd_reg1 *, hcd_reg1);
	int	(*check_error)		(void *, hcd_transfer, hcd_reg1,
					hcd_direction);

	/* Controller's private data (like mapped registers) */
	void *		private_data;

	/* Current state to be handled by driver */
	hcd_event	current_event;
	hcd_reg1	current_endpoint;
	hcd_event	expected_event;
	hcd_reg1	expected_endpoint;
};


/*===========================================================================*
 *    HCD event handling routine                                             *
 *===========================================================================*/
/* Handle asynchronous event
 * This must be called in case of specific HCD interrupts listed above */
void hcd_handle_event(hcd_driver_state *);


#endif /* !_HCD_INTERFACE_H_ */
