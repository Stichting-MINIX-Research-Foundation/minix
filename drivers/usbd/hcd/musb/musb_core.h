/*
 * Interface of low level MUSB core logic (variant independent)
 */

#ifndef _MUSB_CORE_H_
#define _MUSB_CORE_H_

#include <usb/hcd_common.h>


/*===========================================================================*
 *    Types and constants                                                    *
 *===========================================================================*/
/* Holds info on DATA toggle (DATA0/DATA1) initialization,
 * required by bulk transfers */
typedef enum {

	MUSB_DATATOG_UNKNOWN = 0,	/* Default with memset 0 */
	MUSB_DATATOG_INIT
}
musb_datatog;

/* Structure to hold Mentor USB core configuration
 * May be more than one on a single chip
 * Should be initialized by MUSB's variant specific code (like AM335x) */
typedef struct {

	void * regs;	/* Points to beginning of memory mapped registers */
	hcd_reg1 ep;	/* Currently used endpoint */
	hcd_reg1 addr;	/* Currently used address */
	musb_datatog datatog_tx[HCD_TOTAL_EP];
	musb_datatog datatog_rx[HCD_TOTAL_EP];
}
musb_core_config;


/*===========================================================================*
 *    Function prototypes                                                    *
 *===========================================================================*/
/* Only to be used outside generic HCD code */
void musb_core_start(void *);
void musb_core_stop(void *);


/* For HCD interface */
void musb_setup_device(void *, hcd_reg1, hcd_reg1);
int musb_reset_device(void *, hcd_speed *);
void musb_setup_stage(void *, hcd_ctrlrequest *);
void musb_rx_stage(void *, hcd_datarequest *);
void musb_tx_stage(void *, hcd_datarequest *);
void musb_in_data_stage(void *);
void musb_out_data_stage(void *);
void musb_in_status_stage(void *);
void musb_out_status_stage(void *);
int musb_read_data(void *, hcd_reg1 *, hcd_reg1);
int musb_check_error(void *, hcd_transfer, hcd_reg1, hcd_direction);


#endif /* !_MUSB_CORE_H_ */
