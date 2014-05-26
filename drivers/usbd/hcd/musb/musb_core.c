/*
 * Implementation of low level MUSB core logic (variant independent)
 */

#include <string.h>				/* memcpy */
#include <time.h>				/* nanosleep */

#include <usb/hcd_common.h>
#include <usb/hcd_interface.h>
#include <usb/usb_common.h>

#include "musb_core.h"
#include "musb_regs.h"


/*===========================================================================*
 *    Local defines                                                          *
 *===========================================================================*/
#define HCD_COPYBUF_BYTES 64 /* Stack allocated, must be multiple of 4 */
#define HCD_COPYBUF_WORDS (HCD_COPYBUF_BYTES/4)


/*===========================================================================*
 *    Local prototypes                                                       *
 *===========================================================================*/
static void musb_set_state(musb_core_config *);
static int musb_check_rxpktrdy(void *);
static void musb_in_stage_cleanup(void *);
static void musb_clear_rxpktrdy(void *);
static void musb_clear_statuspkt(void *);
static int musb_get_count(void *);
static void musb_read_fifo(void *, hcd_reg1 *, int, int);


/*===========================================================================*
 *                                                                           *
 *    MUSB core implementation                                               *
 *                                                                           *
 *===========================================================================*/

/*===========================================================================*
 *    musb_set_state                                                         *
 *===========================================================================*/
static void
musb_set_state(musb_core_config * cfg)
{
	void * r;
	hcd_reg1 idx;

	DEBUG_DUMP;

	r = cfg->regs;

	/* Set EP and address to be used in next MUSB command */

	/* Set EP by selecting INDEX */
	idx = HCD_RD1(r, MUSB_REG_INDEX);
	HCD_CLR(idx, 0x0F);
	HCD_SET(idx, cfg->ep & 0x0F);
	HCD_WR1(r, MUSB_REG_INDEX, idx);

	/* Use device with address 'cfg->addr' */
	HCD_WR2(r, MUSB_REG_RXFUNCADDR, cfg->addr);
	HCD_WR2(r, MUSB_REG_TXFUNCADDR, cfg->addr);
}


/*===========================================================================*
 *    musb_check_rxpktrdy                                                    *
 *===========================================================================*/
static int
musb_check_rxpktrdy(void * cfg)
{
	void * r;
	hcd_reg2 host_csr0;

	DEBUG_DUMP;

	r = ((musb_core_config *)cfg)->regs;

	/* Set EP and device address to be used in this command */
	musb_set_state((musb_core_config *)cfg);

	/* Get control status register for EP 0 */
	host_csr0 = HCD_RD2(r, MUSB_REG_HOST_CSR0);

	/* Check for RXPKTRDY */
	if (host_csr0 & MUSB_VAL_HOST_CSR0_RXPKTRDY)
		return EXIT_SUCCESS;

	return EXIT_FAILURE;
}


/*===========================================================================*
 *    musb_in_stage_cleanup                                                  *
 *===========================================================================*/
static void
musb_in_stage_cleanup(void * cfg)
{
	DEBUG_DUMP;

	musb_clear_rxpktrdy(cfg);
	musb_clear_statuspkt(cfg);
}


/*===========================================================================*
 *    musb_clear_rxpktrdy                                                    *
 *===========================================================================*/
static void
musb_clear_rxpktrdy(void * cfg)
{
	void * r;
	hcd_reg2 host_csr0;

	DEBUG_DUMP;

	r = ((musb_core_config *)cfg)->regs;

	/* Set EP and device address to be used in this command */
	musb_set_state((musb_core_config *)cfg);

	/* Get control status register for EP 0 */
	host_csr0 = HCD_RD2(r, MUSB_REG_HOST_CSR0);

	/* Clear RXPKTRDY to signal receive completion */
	HCD_CLR(host_csr0, MUSB_VAL_HOST_CSR0_RXPKTRDY);
	HCD_WR2(r, MUSB_REG_HOST_CSR0, host_csr0);
}


/*===========================================================================*
 *    musb_clear_statuspkt                                                   *
 *===========================================================================*/
static void
musb_clear_statuspkt(void * cfg)
{
	void * r;
	hcd_reg2 host_csr0;

	DEBUG_DUMP;

	r = ((musb_core_config *)cfg)->regs;

	/* Set EP and device address to be used in this command */
	musb_set_state((musb_core_config *)cfg);

	/* Get control status register for EP 0 */
	host_csr0 = HCD_RD2(r, MUSB_REG_HOST_CSR0);

	/* Clear STATUSPKT to signal status packet completion */
	HCD_CLR(host_csr0, MUSB_VAL_HOST_CSR0_STATUSPKT);
	HCD_WR2(r, MUSB_REG_HOST_CSR0, host_csr0);
}


/*===========================================================================*
 *    musb_get_count                                                         *
 *===========================================================================*/
static int
musb_get_count(void * cfg)
{
	void * r;

	DEBUG_DUMP;

	r = ((musb_core_config *)cfg)->regs;

	/* Set EP and device address to be used in this command */
	musb_set_state((musb_core_config *)cfg);

	/* Reserved part returns zero so no need to generalize
	 * this return for MUSB_REG_RXCOUNT */
	return (int)(HCD_RD2(r, MUSB_REG_COUNT0));
}


/*===========================================================================*
 *    musb_read_fifo                                                         *
 *===========================================================================*/
static void
musb_read_fifo(void * cfg, hcd_reg1 * output, int size, int fifo_num)
{
	void * r;
	hcd_reg4 * word;
	hcd_reg4 copy_buf[HCD_COPYBUF_WORDS];
	hcd_addr fifo_addr;
	int limit;
	int idx;

	DEBUG_DUMP;

	USB_ASSERT((fifo_num >= 0) && (fifo_num <= 4), "Wrong FIFO number");

	r = ((musb_core_config *)cfg)->regs;
	fifo_addr = MUSB_REG_FIFO0 + (fifo_num * MUSB_REG_FIFO_LEN);

	/* Set EP and device address to be used in this command */
	musb_set_state((musb_core_config *)cfg);

	/* Read full words from MUSB FIFO */
	while (size > 0) {
		/* Largest amount of bytes that can be copied at a time */
		limit = (size < HCD_COPYBUF_BYTES) ? size : HCD_COPYBUF_BYTES;

		/* Start copying into that */
		word = copy_buf;

		/* Read words from FIFO into copy_buf */
		for (idx = 0; idx < limit; idx += sizeof(*word))
			*word++ = HCD_RD4(r, fifo_addr);

		/* Copy and shift */
		memcpy(output, copy_buf, limit);
		output += limit;
		size -= limit;
	}
}


/*===========================================================================*
 *    musb_core_start                                                        *
 *===========================================================================*/
void
musb_core_start(void * cfg)
{
	void * r;
	hcd_reg1 devctl;

	DEBUG_DUMP;

	r = ((musb_core_config *)cfg)->regs;

	/* Enable all interrupts valid for host */
	HCD_WR1(r, MUSB_REG_INTRUSBE,
		MUSB_VAL_INTRUSBE_SUSPEND			|
		MUSB_VAL_INTRUSBE_RESUME			|
		MUSB_VAL_INTRUSBE_RESET_BABBLE			|
		/* MUSB_VAL_INTRUSBE_SOF			| */
		MUSB_VAL_INTRUSBE_CONN				|
		MUSB_VAL_INTRUSBE_DISCON			|
		MUSB_VAL_INTRUSBE_SESSREQ			|
		MUSB_VAL_INTRUSBE_VBUSERR);

	/* Start session */
	devctl = HCD_RD1(r, MUSB_REG_DEVCTL);
	HCD_SET(devctl, MUSB_VAL_DEVCTL_SESSION);
	HCD_WR1(r, MUSB_REG_DEVCTL, devctl);
}


/*===========================================================================*
 *    musb_core_stop                                                         *
 *===========================================================================*/
void
musb_core_stop(void * cfg)
{
	void * r;
	hcd_reg1 devctl;

	DEBUG_DUMP;

	r = ((musb_core_config *)cfg)->regs;

	/* TODO: add hardware interrupt disable */

	/* Stop session */
	devctl = HCD_RD1(r, MUSB_REG_DEVCTL);
	HCD_CLR(devctl, MUSB_VAL_DEVCTL_SESSION);
	HCD_WR1(r, MUSB_REG_DEVCTL, devctl);
}


/*===========================================================================*
 *    musb_ep0_config                                                        *
 *===========================================================================*/
void
musb_ep0_config(void * cfg)
{
	void * r;
	hcd_reg1 host_type0;
	hcd_reg2 intrtxe;

	DEBUG_DUMP;

	r = ((musb_core_config *)cfg)->regs;

	/* Set parameters temporarily */
	musb_setup_device((musb_core_config *)cfg,
			HCD_DEFAULT_EP,
			HCD_DEFAULT_ADDR);

	/* Set EP and device address to be used in this command */
	musb_set_state((musb_core_config *)cfg);

	/* Set high speed for EP0 */
	host_type0 = HCD_RD1(r, MUSB_REG_HOST_TYPE0);
	HCD_CLR(host_type0, MUSB_VAL_HOST_TYPE0_MASK);
	HCD_SET(host_type0, MUSB_VAL_HOST_TYPE0_HIGH_SPEED);
	HCD_WR1(r, MUSB_REG_HOST_TYPE0, host_type0);

	/* Enable EP interrupt */
	intrtxe = HCD_RD2(r, MUSB_REG_INTRTXE);
	HCD_SET(intrtxe, MUSB_VAL_INTRTXE_EP0);
	HCD_WR2(r, MUSB_REG_INTRTXE, intrtxe);
}


/*===========================================================================*
 *                                                                           *
 *    HCD interface implementation                                           *
 *                                                                           *
 *===========================================================================*/

/*===========================================================================*
 *    musb_setup_device                                                      *
 *===========================================================================*/
void
musb_setup_device(void * cfg, hcd_reg1 ep, hcd_reg1 addr)
{
	DEBUG_DUMP;

	/* Assign  */
	((musb_core_config *)cfg)->ep = ep;
	((musb_core_config *)cfg)->addr = addr;
}


/*===========================================================================*
 *    musb_reset_device                                                      *
 *===========================================================================*/
void
musb_reset_device(void * cfg)
{
	void * r;
	hcd_reg1 power;

	DEBUG_DUMP;

	r = ((musb_core_config *)cfg)->regs;

	/* Write reset bit and high speed negotiation wait for at least
	 * 20ms for reset, clear reset bit and wait for device */
	power = HCD_RD1(r, MUSB_REG_POWER);
	HCD_SET(power, MUSB_VAL_POWER_RESET | MUSB_VAL_POWER_HSEN);
	HCD_WR1(r, MUSB_REG_POWER, power);

	{
		/* Sleep 25ms */
		struct timespec nanotm = {0, HCD_NANOSLEEP_MSEC(25)};
		nanosleep(&nanotm, NULL);
	}

	power = HCD_RD1(r, MUSB_REG_POWER);
	HCD_CLR(power, MUSB_VAL_POWER_RESET);
	HCD_WR1(r, MUSB_REG_POWER, power);

	{
		/* Sleep 25ms */
		struct timespec nanotm = {0, HCD_NANOSLEEP_MSEC(25)};
		nanosleep(&nanotm, NULL);
	}
}


/*===========================================================================*
 *    musb_setup_stage                                                       *
 *===========================================================================*/
void
musb_setup_stage(void * cfg, hcd_ctrlrequest * setup)
{
	void * r;
	char * setup_byte;
	hcd_reg2 host_csr0;

	DEBUG_DUMP;

	r = ((musb_core_config *)cfg)->regs;
	setup_byte = (char*)setup;

	/* Set EP and device address to be used in this command */
	musb_set_state((musb_core_config *)cfg);

	/* TODO: check for ongoing transmission */

	/* Put USB setup data into corresponding FIFO */
	HCD_WR4(r, MUSB_REG_FIFO0, HCD_8TO32(&setup_byte[0]));
	HCD_WR4(r, MUSB_REG_FIFO0, HCD_8TO32(&setup_byte[sizeof(hcd_reg4)]));

	/* Get control status register for EP 0 */
	host_csr0 = HCD_RD2(r, MUSB_REG_HOST_CSR0);

	/* Send actual packet from FIFO */
	HCD_SET(host_csr0, MUSB_VAL_HOST_CSR0_TXPKTRDY |
			MUSB_VAL_HOST_CSR0_SETUPPKT);

	HCD_WR2(r, MUSB_REG_HOST_CSR0, host_csr0);
}


/*===========================================================================*
 *    musb_in_data_stage                                                     *
 *===========================================================================*/
void
musb_in_data_stage(void * cfg)
{
	void * r;
	hcd_reg2 host_csr0;

	DEBUG_DUMP;

	r = ((musb_core_config *)cfg)->regs;

	/* Set EP and device address to be used in this command */
	musb_set_state((musb_core_config *)cfg);

	/* Get control status register for EP 0 */
	host_csr0 = HCD_RD2(r, MUSB_REG_HOST_CSR0);

	/* Request IN DATA stage */
	HCD_SET(host_csr0, MUSB_VAL_HOST_CSR0_REQPKT);
	HCD_WR2(r, MUSB_REG_HOST_CSR0, host_csr0);
}


/*===========================================================================*
 *    musb_out_data_stage                                                    *
 *===========================================================================*/
void
musb_out_data_stage(void * cfg)
{
	DEBUG_DUMP;

	/* Set EP and device address to be used in this command */
	musb_set_state((musb_core_config *)cfg);

	/* TODO: not needed for enumeration but will be needed later */
	((void)cfg);
	USB_MSG("NOT IMPLEMENTED");
}


/*===========================================================================*
 *    musb_in_status_stage                                                   *
 *===========================================================================*/
void
musb_in_status_stage(void * cfg)
{
	void * r;
	hcd_reg2 host_csr0;

	DEBUG_DUMP;

	r = ((musb_core_config *)cfg)->regs;

	/* Set EP and device address to be used in this command */
	musb_set_state((musb_core_config *)cfg);

	/* Get control status register for EP 0 */
	host_csr0 = HCD_RD2(r, MUSB_REG_HOST_CSR0);

	/* Request IN STATUS stage */
	HCD_SET(host_csr0, MUSB_VAL_HOST_CSR0_REQPKT |
		MUSB_VAL_HOST_CSR0_STATUSPKT);

	HCD_WR2(r, MUSB_REG_HOST_CSR0, host_csr0);
}


/*===========================================================================*
 *    musb_out_status_stage                                                  *
 *===========================================================================*/
void
musb_out_status_stage(void * cfg)
{
	void * r;
	hcd_reg2 host_csr0;

	DEBUG_DUMP;

	r = ((musb_core_config *)cfg)->regs;

	/* Set EP and device address to be used in this command */
	musb_set_state((musb_core_config *)cfg);

	/* Get control status register for EP 0 */
	host_csr0 = HCD_RD2(r, MUSB_REG_HOST_CSR0);

	/* Request OUT STATUS stage */
	HCD_SET(host_csr0, MUSB_VAL_HOST_CSR0_TXPKTRDY |
		MUSB_VAL_HOST_CSR0_STATUSPKT);

	HCD_WR2(r, MUSB_REG_HOST_CSR0, host_csr0);
}


/*===========================================================================*
 *    musb_read_data                                                         *
 *===========================================================================*/
int
musb_read_data(void * cfg, hcd_reg1 * buffer, int buffer_num)
{
	int count0;

	DEBUG_DUMP;

	/* Check if anything received at all */
	if (EXIT_SUCCESS != musb_check_rxpktrdy(cfg)) {
		USB_MSG("RXPKTRDY not set when receiving");
		return HCD_READ_ERR;
	}

	/* Number of bytes received at EP0 */
	count0 = musb_get_count(cfg);

	/* Read from given FIFO */
	if ((NULL != buffer) && (count0 > 0))
		musb_read_fifo(cfg, buffer, count0, buffer_num);

	/* Cleanup after reading */
	musb_in_stage_cleanup(cfg);

	return count0;
}


/*===========================================================================*
 *    musb_check_error                                                       *
 *===========================================================================*/
int
musb_check_error(void * cfg)
{
	void * r;
	hcd_reg2 host_csr0;

	DEBUG_DUMP;

	r = ((musb_core_config *)cfg)->regs;

	/* Set EP and device address to be used in this command */
	musb_set_state((musb_core_config *)cfg);

	/* Get control status register for EP 0 */
	host_csr0 = HCD_RD2(r, MUSB_REG_HOST_CSR0);

	/* Check for common errors */
	if (host_csr0 & MUSB_VAL_HOST_CSR0_ERROR) {
		USB_MSG("HOST_CSR0 ERROR: %04X", host_csr0);
		HCD_CLR(host_csr0, MUSB_VAL_HOST_CSR0_ERROR);
		HCD_WR2(r, MUSB_REG_HOST_CSR0, host_csr0);
		return EXIT_FAILURE;
	}

	if (host_csr0 & MUSB_VAL_HOST_CSR0_RXSTALL) {
		USB_MSG("HOST_CSR0 STALL: %04X", host_csr0);
		HCD_CLR(host_csr0, MUSB_VAL_HOST_CSR0_RXSTALL);
		HCD_WR2(r, MUSB_REG_HOST_CSR0, host_csr0);
		return EXIT_FAILURE;
	}

	if (host_csr0 & MUSB_VAL_HOST_CSR0_NAK_TIMEOUT) {
		USB_MSG("HOST_CSR0 NAK_TIMEOUT: %04X", host_csr0);
		HCD_CLR(host_csr0, MUSB_VAL_HOST_CSR0_NAK_TIMEOUT);
		HCD_WR2(r, MUSB_REG_HOST_CSR0, host_csr0);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
