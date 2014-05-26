/*
 * Externally visible interface for possible USB controllers
 */

#ifndef _HCD_PLATFORMS_H_
#define _HCD_PLATFORMS_H_


/*===========================================================================*
 *    MUSB                                                                   *
 *===========================================================================*/
/* ----- AM335X ----- */
int musb_am335x_init(void);
void musb_am335x_deinit(void);
/* ----- AM/DM37X ----- */
int musb_dm37x_init(void);
void musb_dm37x_deinit(void);


#endif /* !_HCD_PLATFORMS_H_ */
