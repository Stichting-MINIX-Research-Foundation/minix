/*
 * Implementation of low level MUSB core logic (variant independent)
 */

#include <string.h>				/* memcpy */

#include <usbd/hcd_common.h>
#include <usbd/hcd_interface.h>
#include <usbd/usbd_common.h>

#include "musb_core.h"
#include "musb_regs.h"


/*===========================================================================*
 *    Local prototypes                                                       *
 *===========================================================================*/
static void musb_set_state(musb_core_config *);
static int musb_check_rxpktrdy(void *, hcd_reg1);
static void musb_in_stage_cleanup(void *, hcd_reg1);
static void musb_clear_rxpktrdy(void *, hcd_reg1);
static void musb_clear_statuspkt(void *);
static int musb_get_count(void *);
static void musb_read_fifo(void *, void *, int, hcd_reg1);
static void musb_write_fifo(void *, void *, int, hcd_reg1);


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

	USB_ASSERT(cfg->ep <= HCD_LAST_EP, "Invalid EP supplied");
	USB_ASSERT(cfg->addr <= HCD_LAST_ADDR, "Invalid address supplied");

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
musb_check_rxpktrdy(void * cfg, hcd_reg1 ep_num)
{
	void * r;

	DEBUG_DUMP;

	r = ((musb_core_config *)cfg)->regs;

	/* Set EP and device address to be used in this command */
	musb_set_state((musb_core_config *)cfg);

	/* Check for RXPKTRDY */
	if (HCD_DEFAULT_EP == ep_num) {
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
musb_in_stage_cleanup(void * cfg, hcd_reg1 ep_num)
{
	DEBUG_DUMP;

	musb_clear_rxpktrdy(cfg, ep_num);

	/* For control EP 0 also clear STATUSPKT */
	if (HCD_DEFAULT_EP == ep_num)
		musb_clear_statuspkt(cfg);
}


/*===========================================================================*
 *    musb_clear_rxpktrdy                                                    *
 *===========================================================================*/
static void
musb_clear_rxpktrdy(void * cfg, hcd_reg1 ep_num)
{
	void * r;
	hcd_reg2 host_csr;

	DEBUG_DUMP;

	r = ((musb_core_config *)cfg)->regs;

	/* Set EP and device address to be used in this command */
	musb_set_state((musb_core_config *)cfg);

	/* Check for RXPKTRDY */
	if (HCD_DEFAULT_EP == ep_num) {
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
musb_read_fifo(void * cfg, void * output, int size, hcd_reg1 fifo_num)
{
	void * r;

	hcd_reg1 * output_b;
	hcd_reg4 * output_w;

	hcd_addr fifo_addr;

	DEBUG_DUMP;

	USB_ASSERT(fifo_num <= HCD_LAST_EP, "Invalid FIFO number");

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
	if (0 == ((hcd_addr)output_w % sizeof(hcd_addr))) {

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
 *    musb_write_fifo                                                        *
 *===========================================================================*/
static void
musb_write_fifo(void * cfg, void * input, int size, hcd_reg1 fifo_num)
{
	void * r;

	hcd_reg1 * input_b;
	hcd_reg4 * input_w;

	hcd_addr fifo_addr;

	DEBUG_DUMP;

	USB_ASSERT(fifo_num <= HCD_LAST_EP, "Invalid FIFO number");

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
	if (0 == ((hcd_addr)input_w % sizeof(hcd_addr))) {

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

	/* Disable all interrupts */
	HCD_WR1(r, MUSB_REG_INTRUSBE, MUSB_VAL_INTRUSBE_NONE);

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
musb_setup_device(void * cfg, hcd_reg1 ep, hcd_reg1 addr,
		hcd_datatog * tx_tog, hcd_datatog * rx_tog)
{
	DEBUG_DUMP;

	/* Assign  */
	((musb_core_config *)cfg)->ep = ep;
	((musb_core_config *)cfg)->addr = addr;
	((musb_core_config *)cfg)->datatog_tx = tx_tog;
	((musb_core_config *)cfg)->datatog_rx = rx_tog;
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
	musb_setup_device(core, HCD_DEFAULT_EP, HCD_DEFAULT_ADDR, NULL, NULL);

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
		host_type0 = HCD_RD1(r, MUSB_REG_HOST_TYPE0);
		HCD_CLR(host_type0, MUSB_VAL_HOST_TYPE0_MASK);
		HCD_SET(host_type0, MUSB_VAL_HOST_TYPE0_FULL_SPEED);
		HCD_WR1(r, MUSB_REG_HOST_TYPE0, host_type0);

		*speed = HCD_SPEED_FULL;

		USB_DBG("High speed USB disabled");
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
 *    musb_rx_stage                                                          *
 *===========================================================================*/
void
musb_rx_stage(void * cfg, hcd_datarequest * request)
{
	musb_core_config * core;
	hcd_reg2 host_rxcsr;
	hcd_reg1 host_rxtype;
	void * r;

	DEBUG_DUMP;

	core = (musb_core_config *)cfg;
	r = core->regs;

	USB_ASSERT(request->max_packet_size <= HCD_MAX_MAXPACKETSIZE,
			"Invalid wMaxPacketSize");
	USB_ASSERT((core->ep <= HCD_LAST_EP) && (core->ep > HCD_DEFAULT_EP),
			"Invalid bulk EP supplied");

	/* Set EP and device address to be used in this command */
	musb_set_state(core);

	/* Evaluate RXTYPE */
	host_rxtype = core->ep;

	switch (request->type) {
		case HCD_TRANSFER_BULK:
			host_rxtype |= MUSB_VAL_HOST_XXTYPE_BULK;
			break;
		case HCD_TRANSFER_INTERRUPT:
			host_rxtype |= MUSB_VAL_HOST_XXTYPE_INTERRUPT;
			break;
		default:
			USB_ASSERT(0, "Unsupported transfer type");
	}

	if (HCD_SPEED_HIGH == request->speed)
		host_rxtype |= MUSB_VAL_HOST_XXTYPE_HIGH_SPEED;
	else
		host_rxtype |= MUSB_VAL_HOST_XXTYPE_FULL_SPEED;

	/* Rewrite HOST_RXTYPE */
	HCD_WR1(r, MUSB_REG_HOST_RXTYPE, host_rxtype);

	/* Rewrite RXMAXP */
	HCD_WR2(r, MUSB_REG_RXMAXP, request->max_packet_size);

	/* Set HOST_RXINTERVAL (which means interval or NAK limit) */
	HCD_WR1(r, MUSB_REG_HOST_RXINTERVAL, request->interval);

#if 0
	{
		/* Not required by all MUSB implementations, but
		 * left here just in case */
		hcd_reg2 intrrxe;

		/* Enable this interrupt */
		intrrxe = HCD_RD2(r, MUSB_REG_INTRRXE);
		HCD_SET(intrrxe, HCD_BIT(core->ep));
		HCD_WR2(r, MUSB_REG_INTRRXE, intrrxe);
	}
#endif

	/* TODO: One reusable FIFO, no double buffering */
	/* TODO: With this, only one device/EP can work at a time. Should this
	 * be changed, hcd_device_wait/hcd_device_continue calls must be fixed
	 * accordingly to allow handling multiple EP interrupts */
	/* Assign FIFO */
	HCD_WR2(r, MUSB_REG_RXFIFOADDR, MUSB_VAL_XXFIFOADDR_EP0_END);
	HCD_WR1(r, MUSB_REG_RXFIFOSZ, MUSB_VAL_XXFIFOSZ_4096);

	/* Make controller reconfigure */
	host_rxcsr = HCD_RD2(r, MUSB_REG_HOST_RXCSR);
	HCD_SET(host_rxcsr, MUSB_VAL_HOST_RXCSR_DATATOGWREN); /* Enable first */
	HCD_SET(host_rxcsr, MUSB_VAL_HOST_RXCSR_FLUSHFIFO);
	HCD_WR2(r, MUSB_REG_HOST_RXCSR, host_rxcsr);

	/* Set data toggle and start receiving */
	host_rxcsr = HCD_RD2(r, MUSB_REG_HOST_RXCSR);
	if (HCD_DATATOG_DATA0 == *(core->datatog_rx))
		HCD_CLR(host_rxcsr, MUSB_VAL_HOST_RXCSR_DATATOG);
	else
		HCD_SET(host_rxcsr, MUSB_VAL_HOST_RXCSR_DATATOG);
	HCD_SET(host_rxcsr, MUSB_VAL_HOST_RXCSR_REQPKT);
	HCD_WR2(r, MUSB_REG_HOST_RXCSR, host_rxcsr);
}


/*===========================================================================*
 *    musb_tx_stage                                                          *
 *===========================================================================*/
void
musb_tx_stage(void * cfg, hcd_datarequest * request)
{
	musb_core_config * core;
	hcd_reg2 host_txcsr;
	hcd_reg1 host_txtype;
	void * r;

	DEBUG_DUMP;

	core = (musb_core_config *)cfg;
	r = core->regs;

	USB_ASSERT(request->max_packet_size <= HCD_MAX_MAXPACKETSIZE,
			"Invalid wMaxPacketSize");
	USB_ASSERT((core->ep <= HCD_LAST_EP) && (core->ep > HCD_DEFAULT_EP),
			"Invalid bulk EP supplied");

	/* Set EP and device address to be used in this command */
	musb_set_state(core);

	/* Evaluate TXTYPE */
	host_txtype = core->ep;

	switch (request->type) {
		case HCD_TRANSFER_BULK:
			host_txtype |= MUSB_VAL_HOST_XXTYPE_BULK;
			break;
		case HCD_TRANSFER_INTERRUPT:
			host_txtype |= MUSB_VAL_HOST_XXTYPE_INTERRUPT;
			break;
		default:
			USB_ASSERT(0, "Unsupported transfer type");
	}

	if (HCD_SPEED_HIGH == request->speed)
		host_txtype |= MUSB_VAL_HOST_XXTYPE_HIGH_SPEED;
	else
		host_txtype |= MUSB_VAL_HOST_XXTYPE_FULL_SPEED;

	/* Rewrite HOST_TXTYPE */
	HCD_WR1(r, MUSB_REG_HOST_TXTYPE, host_txtype);

	/* Rewrite TXMAXP */
	HCD_WR2(r, MUSB_REG_TXMAXP, request->max_packet_size);

	/* Set HOST_TXINTERVAL (which means interval or NAK limit) */
	HCD_WR1(r, MUSB_REG_HOST_TXINTERVAL, request->interval);

#if 0
	{
		/* Not required by all MUSB implementations, but
		 * left here just in case */
		hcd_reg2 intrtxe;

		/* Enable this interrupt */
		intrtxe = HCD_RD2(r, MUSB_REG_INTRTXE);
		HCD_SET(intrtxe, HCD_BIT(core->ep));
		HCD_WR2(r, MUSB_REG_INTRTXE, intrtxe);
	}
#endif

	/* TODO: One reusable FIFO, no double buffering */
	/* TODO: With this, only one device/EP can work at a time. Should this
	 * be changed, hcd_device_wait/hcd_device_continue calls must be fixed
	 * accordingly to allow handling multiple EP interrupts */
	/* Assign FIFO */
	HCD_WR2(r, MUSB_REG_TXFIFOADDR, MUSB_VAL_XXFIFOADDR_EP0_END);
	HCD_WR1(r, MUSB_REG_TXFIFOSZ, MUSB_VAL_XXFIFOSZ_4096);

	/* Make controller reconfigure */
	host_txcsr = HCD_RD2(r, MUSB_REG_HOST_TXCSR);
	HCD_CLR(host_txcsr, MUSB_VAL_HOST_TXCSR_DMAMODE);
	HCD_CLR(host_txcsr, MUSB_VAL_HOST_TXCSR_FRCDATATOG);
	HCD_CLR(host_txcsr, MUSB_VAL_HOST_TXCSR_DMAEN);
	HCD_SET(host_txcsr, MUSB_VAL_HOST_TXCSR_MODE);
	HCD_CLR(host_txcsr, MUSB_VAL_HOST_TXCSR_ISO);
	HCD_CLR(host_txcsr, MUSB_VAL_HOST_TXCSR_AUTOSET);
	HCD_SET(host_txcsr, MUSB_VAL_HOST_TXCSR_DATATOGWREN); /* Enable first */
	/* TODO: May have no effect */
	HCD_SET(host_txcsr, MUSB_VAL_HOST_TXCSR_FLUSHFIFO);
	HCD_WR2(r, MUSB_REG_HOST_TXCSR, host_txcsr);

	/* Put data in FIFO */
	musb_write_fifo(cfg, request->data, request->data_left, core->ep);

	/* Set data toggle and start transmitting */
	host_txcsr = HCD_RD2(r, MUSB_REG_HOST_TXCSR);
	if (HCD_DATATOG_DATA0 == *(core->datatog_tx))
		HCD_CLR(host_txcsr, MUSB_VAL_HOST_TXCSR_DATATOG);
	else
		HCD_SET(host_txcsr, MUSB_VAL_HOST_TXCSR_DATATOG);
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

	/* TODO: Not needed for enumeration but may be needed later, if
	 * additional control transfers are implemented */
	USB_ASSERT(0, "Setup packet's 'DATA OUT' stage not implemented");
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
musb_read_data(void * cfg, hcd_reg1 * buffer, hcd_reg1 ep_num)
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
musb_check_error(void * cfg, hcd_transfer xfer, hcd_reg1 ep, hcd_direction dir)
{
	/* Possible error handling schemes for MUSB */
	typedef enum {

		MUSB_INVALID_ERROR_CASE,
		MUSB_EP0_ERROR_CASE,
		MUSB_OUT_ERROR_CASE,
		MUSB_IN_ERROR_CASE,
	}
	musb_error_case;

	musb_core_config * core;
	void * r;
	hcd_reg2 host_csr;
	musb_error_case error_case;

	DEBUG_DUMP;

	/* TODO: ISO transfer */
	USB_ASSERT(HCD_TRANSFER_ISOCHRONOUS != xfer,
		"ISO transfer not supported");

	core = (musb_core_config *)cfg;
	r = core->regs;

	/* Set EP and device address to be used in this command */
	musb_set_state((musb_core_config *)cfg);

	/* Resolve which error needs to be checked
	 * so we can use proper MUSB registers */
	error_case = MUSB_INVALID_ERROR_CASE;

	if (HCD_DEFAULT_EP == ep) {
		if (HCD_TRANSFER_CONTROL == xfer)
			/* EP0, control, any direction */
			error_case = MUSB_EP0_ERROR_CASE;
	} else {
		if (HCD_TRANSFER_CONTROL != xfer) {
			if (HCD_DIRECTION_OUT == dir)
				/* EP>0, non-control, out */
				error_case = MUSB_OUT_ERROR_CASE;
			else if (HCD_DIRECTION_IN == dir)
				/* EP>0, non-control, in */
				error_case = MUSB_IN_ERROR_CASE;
		}
	}

	/* In MUSB, EP0 has it's own registers for error handling and it also
	 * seems to be the only way to handle errors for control transfers */
	if (MUSB_EP0_ERROR_CASE == error_case) {
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

	if (MUSB_OUT_ERROR_CASE == error_case) {
		/* Get TX status register */
		host_csr = HCD_RD2(r, MUSB_REG_HOST_TXCSR);

		/* Check for completion */
		if (!(host_csr & MUSB_VAL_HOST_TXCSR_TXPKTRDY)) {
			/* ACK received update data toggle */
			*(core->datatog_tx) ^= HCD_DATATOG_DATA1;
			return EXIT_SUCCESS;
		}

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
			/* Flush FIFO before clearing NAKTIMEOUT
			 * to abort transfer */
			HCD_SET(host_csr, MUSB_VAL_HOST_TXCSR_FLUSHFIFO);
			HCD_WR2(r, MUSB_REG_HOST_TXCSR, host_csr);
			host_csr = HCD_RD2(r, MUSB_REG_HOST_TXCSR);
			HCD_CLR(host_csr, MUSB_VAL_HOST_TXCSR_NAK_TIMEOUT);
			HCD_WR2(r, MUSB_REG_HOST_TXCSR, host_csr);
			return EXIT_FAILURE;
		}

		USB_ASSERT(0, "Invalid state of HOST_TXCSR");
	}

	if (MUSB_IN_ERROR_CASE == error_case) {
		/* Get RX status register */
		host_csr = HCD_RD2(r, MUSB_REG_HOST_RXCSR);

		/* Check for completion */
		if (host_csr & MUSB_VAL_HOST_RXCSR_RXPKTRDY) {
			/* ACK received update data toggle */
			*(core->datatog_rx) ^= HCD_DATATOG_DATA1;
			return EXIT_SUCCESS;
		}

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
			/* Clear REQPKT before NAKTIMEOUT to abort transfer */
			HCD_CLR(host_csr, MUSB_VAL_HOST_RXCSR_REQPKT);
			HCD_WR2(r, MUSB_REG_HOST_RXCSR, host_csr);
			host_csr = HCD_RD2(r, MUSB_REG_HOST_RXCSR);
			HCD_CLR(host_csr, MUSB_VAL_HOST_RXCSR_NAKTIMEOUT);
			HCD_WR2(r, MUSB_REG_HOST_RXCSR, host_csr);
			return EXIT_FAILURE;
		}

		USB_ASSERT(0, "Invalid state of HOST_RXCSR");
	}

	USB_MSG("Invalid USB transfer error check: 0x%X, 0x%X, 0x%X",
		(int)xfer, (int)ep, (int)dir);
	return EXIT_FAILURE;
}
