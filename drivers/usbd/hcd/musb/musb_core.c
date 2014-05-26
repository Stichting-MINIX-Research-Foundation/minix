/*
 * Implementation of low level MUSB core logic (variant independent)
 */

#include <string.h>				/* memcpy */

#include <usb/hcd_common.h>
#include <usb/hcd_interface.h>
#include <usb/usb_common.h>

#include "musb_core.h"
#include "musb_regs.h"


/*===========================================================================*
 *    Local prototypes                                                       *
 *===========================================================================*/
static void musb_set_state(musb_core_config *);
static int musb_check_rxpktrdy(void *, int);
static void musb_in_stage_cleanup(void *, int);
static void musb_clear_rxpktrdy(void *, int);
static void musb_clear_statuspkt(void *);
static int musb_get_count(void *);
static void musb_read_fifo(void *, void *, int, int);
static void musb_write_fifo(void *, void *, int, int);


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

	DEBUG_DUMP;

	r = cfg->regs;

	USB_ASSERT(cfg->ep <= 15, "Invalid EP supplied");
	USB_ASSERT(cfg->addr <= 127, "Invalid device address supplied");

	/* Set EP and address to be used in next MUSB command */

	/* Set EP by selecting INDEX */
	HCD_WR1(r, MUSB_REG_INDEX, cfg->ep);

	/* Use device with address 'cfg->addr' */
	HCD_WR1(r, MUSB_REG_FADDR, cfg->addr);
	HCD_WR2(r, MUSB_REG_CONFIG(cfg->ep, MUSB_REG_RXFUNCADDR), cfg->addr);
	HCD_WR2(r, MUSB_REG_CONFIG(cfg->ep, MUSB_REG_TXFUNCADDR), cfg->addr);
}


/*===========================================================================*
 *    musb_check_rxpktrdy                                                    *
 *===========================================================================*/
static int
musb_check_rxpktrdy(void * cfg, int ep_num)
{
	void * r;

	DEBUG_DUMP;

	r = ((musb_core_config *)cfg)->regs;

	/* Set EP and device address to be used in this command */
	musb_set_state((musb_core_config *)cfg);

	/* Check for RXPKTRDY */
	if (0 == ep_num) {
		/* Get control status register for EP 0 */
		if (HCD_RD2(r, MUSB_REG_HOST_CSR0) &
			MUSB_VAL_HOST_CSR0_RXPKTRDY)
			return EXIT_SUCCESS;
	} else {
		/* Get RX status register for any other EP */
		if (HCD_RD2(r, MUSB_REG_HOST_RXCSR) &
			MUSB_VAL_HOST_RXCSR_RXPKTRDY)
			return EXIT_SUCCESS;
	}

	return EXIT_FAILURE;
}


/*===========================================================================*
 *    musb_in_stage_cleanup                                                  *
 *===========================================================================*/
static void
musb_in_stage_cleanup(void * cfg, int ep_num)
{
	DEBUG_DUMP;

	musb_clear_rxpktrdy(cfg, ep_num);

	/* For control EP 0 also clear STATUSPKT */
	if (0 == ep_num)
		musb_clear_statuspkt(cfg);
}


/*===========================================================================*
 *    musb_clear_rxpktrdy                                                    *
 *===========================================================================*/
static void
musb_clear_rxpktrdy(void * cfg, int ep_num)
{
	void * r;
	hcd_reg2 host_csr;

	DEBUG_DUMP;

	r = ((musb_core_config *)cfg)->regs;

	/* Set EP and device address to be used in this command */
	musb_set_state((musb_core_config *)cfg);

	/* Check for RXPKTRDY */
	if (0 == ep_num) {
		/* Get control status register for EP 0 */
		host_csr = HCD_RD2(r, MUSB_REG_HOST_CSR0);

		/* Clear RXPKTRDY to signal receive completion */
		HCD_CLR(host_csr, MUSB_VAL_HOST_CSR0_RXPKTRDY);
		HCD_WR2(r, MUSB_REG_HOST_CSR0, host_csr);
	} else {
		/* Get RX status register for any other EP */
		host_csr = HCD_RD2(r, MUSB_REG_HOST_RXCSR);

		/* Clear RXPKTRDY to signal receive completion */
		HCD_CLR(host_csr, MUSB_VAL_HOST_RXCSR_RXPKTRDY);
		HCD_WR2(r, MUSB_REG_HOST_RXCSR, host_csr);
	}
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
musb_read_fifo(void * cfg, void * output, int size, int fifo_num)
{
	void * r;

	hcd_reg1 * output_b;
	hcd_reg4 * output_w;

	hcd_addr fifo_addr;

	DEBUG_DUMP;

	USB_ASSERT((fifo_num >= 0) && (fifo_num <= 15), "Wrong FIFO number");

	r = ((musb_core_config *)cfg)->regs;
	fifo_addr = MUSB_REG_FIFO0 + (fifo_num * MUSB_REG_FIFO_LEN);

	/* TODO: Apparently, FIFO can only be read by:
	 * 1. initially using words
	 * 2. using bytes for whatever remains
	 * Reading bytes first to achieve alignment of remaining data
	 * will for some reason disable further word based reading
	 * Such reading method, may not be optimal for unaligned data */
	output_w = (hcd_reg4 *)output;

	/* Try and copy aligned words */
	if (0 == ((hcd_addr)output_w % 4)) {

		while (size > (int)(sizeof(*output_w) - 1)) {
			*output_w++ = HCD_RD4(r, fifo_addr);
			size -= sizeof(*output_w);
		}
	}

	output_b = (hcd_reg1 *)output_w;

	/* Then, go with remaining bytes */
	while (size > 0) {
		*output_b++ = HCD_RD1(r, fifo_addr);
		size--;
	}
}


/*===========================================================================*
 *    musb_write_fifo                                                         *
 *===========================================================================*/
static void
musb_write_fifo(void * cfg, void * input, int size, int fifo_num)
{
	void * r;

	hcd_reg1 * input_b;
	hcd_reg4 * input_w;

	hcd_addr fifo_addr;

	DEBUG_DUMP;

	USB_ASSERT((fifo_num >= 0) && (fifo_num <= 15), "Wrong FIFO number");

	r = ((musb_core_config *)cfg)->regs;
	fifo_addr = MUSB_REG_FIFO0 + (fifo_num * MUSB_REG_FIFO_LEN);

	/* TODO: Apparently, FIFO can only be written by:
	 * 1. initially using words
	 * 2. using bytes for whatever remains
	 * Writing bytes first to achieve alignment of remaining data
	 * will for some reason disable further word based writing
	 * Such writing method, may not be optimal for unaligned data */
	input_w = (hcd_reg4 *)input;

	/* Try and copy aligned words */
	if (0 == ((hcd_addr)input_w % 4)) {

		while (size > (int)(sizeof(*input_w) - 1)) {
			HCD_WR4(r, fifo_addr, *input_w++);
			size -= sizeof(*input_w);
		}
	}

	input_b = (hcd_reg1 *)input_w;

	/* Then, go with remaining bytes */
	while (size > 0) {
		HCD_WR1(r, fifo_addr, *input_b++);
		size--;
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
int
musb_reset_device(void * cfg, hcd_speed * speed)
{
	void * r;
	musb_core_config * core;
	hcd_reg1 power;
	hcd_reg1 host_type0;

	DEBUG_DUMP;

	core = (musb_core_config *)cfg;
	r = core->regs;

	/* Set initial parameters */
	musb_setup_device(core, HCD_DEFAULT_EP, HCD_DEFAULT_ADDR);

	/* Set EP and device address to be used in this command */
	musb_set_state(core);

	/* Write reset bit and high speed negotiation wait for at least
	 * 20ms for reset, clear reset bit and wait for device */
	power = HCD_RD1(r, MUSB_REG_POWER);
	HCD_SET(power, MUSB_VAL_POWER_RESET | MUSB_VAL_POWER_HSEN);
	HCD_WR1(r, MUSB_REG_POWER, power);

	/* Sleep 25msec */
	hcd_os_nanosleep(HCD_NANOSLEEP_MSEC(25));

	power = HCD_RD1(r, MUSB_REG_POWER);
	HCD_CLR(power, MUSB_VAL_POWER_RESET);
	HCD_WR1(r, MUSB_REG_POWER, power);

	/* Sleep 25msec */
	hcd_os_nanosleep(HCD_NANOSLEEP_MSEC(25));

	/* High speed check */
	power = HCD_RD1(r, MUSB_REG_POWER);

	if (power & MUSB_VAL_POWER_HSMODE) {
		/* Set high-speed for EP0 */
		host_type0 = HCD_RD1(r, MUSB_REG_HOST_TYPE0);
		HCD_CLR(host_type0, MUSB_VAL_HOST_TYPE0_MASK);
		HCD_SET(host_type0, MUSB_VAL_HOST_TYPE0_HIGH_SPEED);
		HCD_WR1(r, MUSB_REG_HOST_TYPE0, host_type0);

		*speed = HCD_SPEED_HIGH;

		USB_DBG("High speed USB enabled");
	} else {
		/* Only full-speed supported */
		USB_DBG("High speed USB disabled");

		*speed = HCD_SPEED_FULL;
	}

	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    musb_setup_stage                                                       *
 *===========================================================================*/
void
musb_setup_stage(void * cfg, hcd_ctrlrequest * setup)
{
	void * r;
	char * setup_byte;
	musb_core_config * core;
	hcd_reg2 host_csr0;

	DEBUG_DUMP;

	core = (musb_core_config *)cfg;
	r = core->regs;
	setup_byte = (char*)setup;

	USB_ASSERT(0 == core->ep, "Only EP 0 can handle control transfers");

	/* Set EP and device address to be used in this command */
	musb_set_state(core);

	/* Put USB setup data into EP0 FIFO */
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
 *    musb_bulk_in_stage                                                     *
 *===========================================================================*/
void
musb_bulk_in_stage(void * cfg, hcd_bulkrequest * request)
{
	musb_core_config * core;
#if 0
	hcd_reg2 intrrxe;
#endif
	hcd_reg2 host_rxcsr;
	hcd_reg1 host_rxtype;
	void * r;

	DEBUG_DUMP;

	core = (musb_core_config *)cfg;
	r = core->regs;

	USB_ASSERT(request->max_packet_size <= 1024, "Invalid wMaxPacketSize");
	USB_ASSERT((core->ep <= 15) && (core->ep > 0),
		"Invalid bulk EP supplied");

	/* Set EP and device address to be used in this command */
	musb_set_state(core);

	/* Evaluate RXTYPE */
	host_rxtype = MUSB_VAL_HOST_XXTYPE_BULK | core->ep;

	if (HCD_SPEED_HIGH == request->speed)
		host_rxtype |= MUSB_VAL_HOST_XXTYPE_HIGH_SPEED;
	else
		host_rxtype |= MUSB_VAL_HOST_XXTYPE_FULL_SPEED;

	/* Rewrite HOST_RXTYPE */
	HCD_WR1(r, MUSB_REG_HOST_RXTYPE, host_rxtype);

	/* Rewrite RXMAXP */
	HCD_WR2(r, MUSB_REG_RXMAXP, request->max_packet_size);

	/* Rewrite HOST_RXINTERVAL */
	HCD_WR1(r, MUSB_REG_HOST_RXINTERVAL, MUSB_VAL_HOST_XXINTERVAL_DEFAULT);

	/* Not required in some MUSB implementations */
#if 0
	/* Enable this interrupt */
	intrrxe = HCD_RD2(r, MUSB_REG_INTRRXE);
	HCD_SET(intrrxe, HCD_BIT(core->ep));
	HCD_WR2(r, MUSB_REG_INTRRXE, intrrxe);
#endif

	/* TODO: One reusable FIFO, no double buffering */
	/* TODO: With this, only one device can work at a time but it
	 * may be impossible to have MUSB work reasonably with multiple
	 * EP interrupts anyway */
	/* Assign FIFO */
	HCD_WR2(r, MUSB_REG_RXFIFOADDR, MUSB_VAL_XXFIFOADDR_EP0_END);
	HCD_WR1(r, MUSB_REG_RXFIFOSZ, MUSB_VAL_XXFIFOSZ_4096);

	/* TODO: decide which is better (or working at all when we use more
	 * than one transfer for bulk data in single device) */
#if 0
	/* Make controller reconfigure */
	host_rxcsr = HCD_RD2(r, MUSB_REG_HOST_RXCSR);
	HCD_SET(host_rxcsr, MUSB_VAL_HOST_RXCSR_CLRDATATOG);
	HCD_SET(host_rxcsr, MUSB_VAL_HOST_RXCSR_FLUSHFIFO);
	HCD_WR2(r, MUSB_REG_HOST_RXCSR, host_rxcsr);
#else
	/* Reset and flush */
	host_rxcsr = 0;
	HCD_SET(host_rxcsr, MUSB_VAL_HOST_RXCSR_CLRDATATOG);
	HCD_SET(host_rxcsr, MUSB_VAL_HOST_RXCSR_FLUSHFIFO);
	HCD_WR2(r, MUSB_REG_HOST_RXCSR, host_rxcsr);
#endif

	/* Request packet */
	host_rxcsr = HCD_RD2(r, MUSB_REG_HOST_RXCSR);
	HCD_SET(host_rxcsr, MUSB_VAL_HOST_RXCSR_REQPKT);
	HCD_WR2(r, MUSB_REG_HOST_RXCSR, host_rxcsr);
}


/*===========================================================================*
 *    musb_bulk_out_stage                                                    *
 *===========================================================================*/
void
musb_bulk_out_stage(void * cfg, hcd_bulkrequest * request)
{
	musb_core_config * core;
#if 0
	hcd_reg2 intrtxe;
#endif
	hcd_reg2 host_txcsr;
	hcd_reg1 host_txtype;
	void * r;

	DEBUG_DUMP;

	core = (musb_core_config *)cfg;
	r = core->regs;

	USB_ASSERT(request->max_packet_size <= 1024, "Invalid wMaxPacketSize");
	USB_ASSERT((core->ep <= 15) && (core->ep > 0),
		"Invalid bulk EP supplied");

	/* Set EP and device address to be used in this command */
	musb_set_state(core);

	/* Evaluate TXTYPE */
	host_txtype = MUSB_VAL_HOST_XXTYPE_BULK | core->ep;

	if (HCD_SPEED_HIGH == request->speed)
		host_txtype |= MUSB_VAL_HOST_XXTYPE_HIGH_SPEED;
	else
		host_txtype |= MUSB_VAL_HOST_XXTYPE_FULL_SPEED;

	/* Rewrite HOST_TXTYPE */
	HCD_WR1(r, MUSB_REG_HOST_TXTYPE, host_txtype);

	/* Rewrite TXMAXP */
	HCD_WR2(r, MUSB_REG_TXMAXP, request->max_packet_size);

	/* Rewrite HOST_TXINTERVAL */
	HCD_WR1(r, MUSB_REG_HOST_TXINTERVAL, MUSB_VAL_HOST_XXINTERVAL_DEFAULT);

	/* Not required in some MUSB implementations */
#if 0
	/* Enable this interrupt */
	intrtxe = HCD_RD2(r, MUSB_REG_INTRTXE);
	HCD_SET(intrtxe, HCD_BIT(core->ep));
	HCD_WR2(r, MUSB_REG_INTRTXE, intrtxe);
#endif

	/* TODO: One reusable FIFO, no double buffering */
	/* TODO: With this, only one device can work at a time but it
	 * may be impossible to have MUSB work reasonably with multiple
	 * EP interrupts anyway */
	/* Assign FIFO */
	HCD_WR2(r, MUSB_REG_TXFIFOADDR, MUSB_VAL_XXFIFOADDR_EP0_END);
	HCD_WR1(r, MUSB_REG_TXFIFOSZ, MUSB_VAL_XXFIFOSZ_4096);

	/* TODO: decide which is better (or working at all when we use more
	 * than one transfer for bulk data in single device) */
#if 0
	/* Make controller reconfigure */
	host_txcsr = HCD_RD2(r, MUSB_REG_HOST_TXCSR);
	HCD_CLR(host_txcsr, MUSB_VAL_HOST_TXCSR_DMAMODE);
	HCD_CLR(host_txcsr, MUSB_VAL_HOST_TXCSR_FRCDATATOG);
	HCD_CLR(host_txcsr, MUSB_VAL_HOST_TXCSR_DMAEN);
	HCD_SET(host_txcsr, MUSB_VAL_HOST_TXCSR_MODE);
	HCD_CLR(host_txcsr, MUSB_VAL_HOST_TXCSR_ISO);
	HCD_CLR(host_txcsr, MUSB_VAL_HOST_TXCSR_AUTOSET);
	HCD_SET(host_txcsr, MUSB_VAL_HOST_TXCSR_CLRDATATOG);
	HCD_SET(host_txcsr, MUSB_VAL_HOST_TXCSR_FLUSHFIFO);
	HCD_WR2(r, MUSB_REG_HOST_TXCSR, host_txcsr);
#else
	/* Reset and flush */
	host_txcsr = 0;
	HCD_SET(host_txcsr, MUSB_VAL_HOST_TXCSR_MODE);
	HCD_SET(host_txcsr, MUSB_VAL_HOST_TXCSR_CLRDATATOG);
	HCD_SET(host_txcsr, MUSB_VAL_HOST_TXCSR_FLUSHFIFO);
	HCD_WR2(r, MUSB_REG_HOST_TXCSR, host_txcsr);
#endif

	/* Put data in FIFO */
	musb_write_fifo(cfg, request->data, request->size, core->ep);

	/* Request packet */
	host_txcsr = HCD_RD2(r, MUSB_REG_HOST_TXCSR);
	HCD_SET(host_txcsr, MUSB_VAL_HOST_TXCSR_TXPKTRDY);
	HCD_WR2(r, MUSB_REG_HOST_TXCSR, host_txcsr);
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

	/* TODO: not needed for enumeration but may be needed later */
	USB_MSG("Setup packet's 'DATA OUT' stage not implemented");
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
musb_read_data(void * cfg, hcd_reg1 * buffer, int ep_num)
{
	int count;

	DEBUG_DUMP;

	/* Check if anything received at all */
	if (EXIT_SUCCESS != musb_check_rxpktrdy(cfg, ep_num)) {
		USB_MSG("RXPKTRDY not set when receiving");
		return HCD_READ_ERR;
	}

	/* Number of bytes received at any EP */
	count = musb_get_count(cfg);

	/* Read from given FIFO */
	if ((NULL != buffer) && (count > 0))
		musb_read_fifo(cfg, buffer, count, ep_num);

	/* Cleanup after reading */
	musb_in_stage_cleanup(cfg, ep_num);

	return count;
}


/*===========================================================================*
 *    musb_check_error                                                       *
 *===========================================================================*/
int
musb_check_error(void * cfg, hcd_transfer transfer, hcd_direction dir)
{
	void * r;
	hcd_reg2 host_csr;

	DEBUG_DUMP;

	r = ((musb_core_config *)cfg)->regs;

	/* Set EP and device address to be used in this command */
	musb_set_state((musb_core_config *)cfg);

	/* TODO: In MUSB only EP0 is allowed to handle control transfers
	 * so there is no EP checking in this function */
	if (HCD_TRANSFER_CONTROL == transfer) {
		/* Get control status register */
		host_csr = HCD_RD2(r, MUSB_REG_HOST_CSR0);

		/* Check for common errors */
		if (host_csr & MUSB_VAL_HOST_CSR0_ERROR) {
			USB_MSG("HOST_CSR0 ERROR: %04X", host_csr);
			HCD_CLR(host_csr, MUSB_VAL_HOST_CSR0_ERROR);
			HCD_WR2(r, MUSB_REG_HOST_CSR0, host_csr);
			return EXIT_FAILURE;
		}

		if (host_csr & MUSB_VAL_HOST_CSR0_RXSTALL) {
			USB_MSG("HOST_CSR0 STALL: %04X", host_csr);
			HCD_CLR(host_csr, MUSB_VAL_HOST_CSR0_RXSTALL);
			HCD_WR2(r, MUSB_REG_HOST_CSR0, host_csr);
			return EXIT_FAILURE;
		}

		if (host_csr & MUSB_VAL_HOST_CSR0_NAK_TIMEOUT) {
			USB_MSG("HOST_CSR0 NAK_TIMEOUT: %04X", host_csr);
			HCD_CLR(host_csr, MUSB_VAL_HOST_CSR0_NAK_TIMEOUT);
			HCD_WR2(r, MUSB_REG_HOST_CSR0, host_csr);
			return EXIT_FAILURE;
		}

		return EXIT_SUCCESS;
	}

	if ((HCD_TRANSFER_BULK == transfer) && (HCD_DIRECTION_OUT == dir)) {
		/* Get RX status register */
		host_csr = HCD_RD2(r, MUSB_REG_HOST_TXCSR);

		/* Check for common errors */
		if (host_csr & MUSB_VAL_HOST_TXCSR_ERROR) {
			USB_MSG("HOST_TXCSR ERROR: %04X", host_csr);
			HCD_CLR(host_csr, MUSB_VAL_HOST_TXCSR_ERROR);
			HCD_WR2(r, MUSB_REG_HOST_TXCSR, host_csr);
			return EXIT_FAILURE;
		}

		if (host_csr & MUSB_VAL_HOST_TXCSR_RXSTALL) {
			USB_MSG("HOST_TXCSR STALL: %04X", host_csr);
			HCD_CLR(host_csr, MUSB_VAL_HOST_TXCSR_RXSTALL);
			HCD_WR2(r, MUSB_REG_HOST_TXCSR, host_csr);
			return EXIT_FAILURE;
		}

		if (host_csr & MUSB_VAL_HOST_TXCSR_NAK_TIMEOUT) {
			USB_MSG("HOST_TXCSR NAK_TIMEOUT: %04X", host_csr);
			HCD_CLR(host_csr, MUSB_VAL_HOST_TXCSR_NAK_TIMEOUT);
			HCD_WR2(r, MUSB_REG_HOST_TXCSR, host_csr);
			return EXIT_FAILURE;
		}

		return EXIT_SUCCESS;
	}

	if ((HCD_TRANSFER_BULK == transfer) && (HCD_DIRECTION_IN == dir)) {
		/* Get RX status register */
		host_csr = HCD_RD2(r, MUSB_REG_HOST_RXCSR);

		/* Check for common errors */
		if (host_csr & MUSB_VAL_HOST_RXCSR_ERROR) {
			USB_MSG("HOST_RXCSR ERROR: %04X", host_csr);
			HCD_CLR(host_csr, MUSB_VAL_HOST_RXCSR_ERROR);
			HCD_WR2(r, MUSB_REG_HOST_RXCSR, host_csr);
			return EXIT_FAILURE;
		}

		if (host_csr & MUSB_VAL_HOST_RXCSR_RXSTALL) {
			USB_MSG("HOST_RXCSR STALL: %04X", host_csr);
			HCD_CLR(host_csr, MUSB_VAL_HOST_RXCSR_RXSTALL);
			HCD_WR2(r, MUSB_REG_HOST_RXCSR, host_csr);
			return EXIT_FAILURE;
		}

		if (host_csr & MUSB_VAL_HOST_RXCSR_NAKTIMEOUT) {
			USB_MSG("HOST_RXCSR NAK_TIMEOUT: %04X", host_csr);
			HCD_CLR(host_csr, MUSB_VAL_HOST_RXCSR_NAKTIMEOUT);
			HCD_WR2(r, MUSB_REG_HOST_RXCSR, host_csr);
			return EXIT_FAILURE;
		}

		return EXIT_SUCCESS;
	}

	USB_MSG("Invalid USB transfer 0x%X:0x%X", (int)transfer, (int)dir);
	return EXIT_FAILURE;
}
