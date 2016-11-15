/* $NetBSD: ifpci.c,v 1.32 2014/03/29 19:28:25 christos Exp $	*/
/*
 *   Copyright (c) 1999 Gary Jennejohn. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *   3. Neither the name of the author nor the names of any co-contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *   4. Altered versions must be plainly marked as such, and must not be
 *      misrepresented as being the original software and/or documentation.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 *   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *   ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 *   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *   SUCH DAMAGE.
 *
 *---------------------------------------------------------------------------
 *   a lot of code was borrowed from i4b_bchan.c and i4b_hscx.c
 *---------------------------------------------------------------------------
 *
 *	Fritz!Card PCI driver
 *	------------------------------------------------
 *
 *	$Id: ifpci.c,v 1.32 2014/03/29 19:28:25 christos Exp $
 *
 *      last edit-date: [Fri Jan  5 11:38:58 2001]
 *
 *---------------------------------------------------------------------------*/

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ifpci.c,v 1.32 2014/03/29 19:28:25 christos Exp $");


#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/mbuf.h>

#include <sys/bus.h>
#include <sys/device.h>

#include <sys/socket.h>
#include <net/if.h>

#include <sys/callout.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <netisdn/i4b_debug.h>
#include <netisdn/i4b_ioctl.h>

#include <netisdn/i4b_global.h>
#include <netisdn/i4b_l2.h>
#include <netisdn/i4b_l1l2.h>
#include <netisdn/i4b_trace.h>
#include <netisdn/i4b_mbuf.h>

#include <dev/ic/isic_l1.h>
#include <dev/ic/isac.h>
#include <dev/ic/hscx.h>

#include <dev/pci/isic_pci.h>

/* PCI config map to use (only one in this driver) */
#define FRITZPCI_PORT0_IO_MAPOFF	PCI_MAPREG_START+4
#define FRITZPCI_PORT0_MEM_MAPOFF	PCI_MAPREG_START

static isdn_link_t *avma1pp_ret_linktab(void *token, int channel);
static void avma1pp_set_link(void *token, int channel, const struct isdn_l4_driver_functions *l4_driver, void *l4_driver_softc);

void n_connect_request(struct call_desc *cd);
void n_connect_response(struct call_desc *cd, int response, int cause);
void n_disconnect_request(struct call_desc *cd, int cause);
void n_alert_request(struct call_desc *cd);
void n_mgmt_command(struct isdn_l3_driver *drv, int cmd, void *parm);

extern const struct isdn_layer1_isdnif_driver isic_std_driver;

const struct isdn_l3_driver_functions
ifpci_l3_driver = {
	avma1pp_ret_linktab,
	avma1pp_set_link,
	n_connect_request,
	n_connect_response,
	n_disconnect_request,
	n_alert_request,
	NULL,
	NULL,
	n_mgmt_command
};

struct ifpci_softc {
	struct isic_softc sc_isic;	/* parent class */

	/* PCI-specific goo */
	void *sc_ih;				/* interrupt handler */
	bus_addr_t sc_base;
	bus_size_t sc_size;
	pci_chipset_tag_t sc_pc;
};

/* prototypes */
static void avma1pp_disable(struct isic_softc *);
static int isic_hscx_fifo(l1_bchan_state_t *chan, struct isic_softc *sc);

static int avma1pp_intr(void*);
static void avma1pp_read_fifo(struct isic_softc *sc, int what, void *buf, size_t size);
static void avma1pp_write_fifo(struct isic_softc *sc, int what, const void *buf, size_t size);
static void avma1pp_write_reg(struct isic_softc *sc, int what, bus_size_t offs, u_int8_t data);
static u_int8_t avma1pp_read_reg(struct isic_softc *sc, int what, bus_size_t offs);
static void hscx_write_fifo(int chan, const void *buf, size_t len, struct isic_softc *sc);
static void hscx_read_fifo(int chan, void *buf, size_t len, struct isic_softc *sc);
static void hscx_write_reg(int chan, u_int off, u_int val, struct isic_softc *sc);
static u_char hscx_read_reg(int chan, u_int off, struct isic_softc *sc);
static u_int hscx_read_reg_int(int chan, u_int off, struct isic_softc *sc);
static void avma1pp_bchannel_stat(isdn_layer1token, int h_chan, bchan_statistics_t *bsp);
static void avma1pp_map_int(struct ifpci_softc *sc, struct pci_attach_args *pa);
static void avma1pp_bchannel_setup(isdn_layer1token, int h_chan, int bprot, int activate);
static void avma1pp_init_linktab(struct isic_softc *);
static int ifpci_match(device_t parent, cfdata_t match, void *aux);
static void ifpci_attach(device_t parent, device_t self, void *aux);
static int ifpci_detach(device_t self, int flags);
static int ifpci_activate(device_t self, enum devact act);

CFATTACH_DECL_NEW(ifpci, sizeof(struct ifpci_softc),
    ifpci_match, ifpci_attach, ifpci_detach, ifpci_activate);

/*---------------------------------------------------------------------------*
 *	AVM PCI Fritz!Card special registers
 *---------------------------------------------------------------------------*/

/*
 *	register offsets from i/o base
 */
#define STAT0_OFFSET            0x02
#define STAT1_OFFSET            0x03
#define ADDR_REG_OFFSET         0x04
/*#define MODREG_OFFSET		0x06
#define VERREG_OFFSET           0x07*/

/* these 2 are used to select an ISAC register set */
#define ISAC_LO_REG_OFFSET	0x04
#define ISAC_HI_REG_OFFSET	0x06

/* offset higher than this goes to the HI register set */
#define MAX_LO_REG_OFFSET	0x2f

/* mask for the offset */
#define ISAC_REGSET_MASK	0x0f

/* the offset from the base to the ISAC registers */
#define ISAC_REG_OFFSET		0x10

/* the offset from the base to the ISAC FIFO */
#define ISAC_FIFO		0x02

/* not really the HSCX, but sort of */
#define HSCX_FIFO		0x00
#define HSCX_STAT		0x04

/*
 *	AVM PCI Status Latch 0 read only bits
 */
#define ASL_IRQ_ISAC            0x01    /* ISAC  interrupt, active low */
#define ASL_IRQ_HSCX            0x02    /* HSX   interrupt, active low */
#define ASL_IRQ_TIMER           0x04    /* Timer interrupt, active low */
#define ASL_IRQ_BCHAN           ASL_IRQ_HSCX
/* actually active LOW */
#define ASL_IRQ_Pending         (ASL_IRQ_ISAC | ASL_IRQ_HSCX | ASL_IRQ_TIMER)

/*
 *	AVM Status Latch 0 write only bits
 */
#define ASL_RESET_ALL           0x01  /* reset siemens IC's, active 1 */
#define ASL_TIMERDISABLE        0x02  /* active high */
#define ASL_TIMERRESET          0x04  /* active high */
#define ASL_ENABLE_INT          0x08  /* active high */
#define ASL_TESTBIT	        0x10  /* active high */

/*
 *	AVM Status Latch 1 write only bits
 */
#define ASL1_INTSEL              0x0f  /* active high */
#define ASL1_ENABLE_IOM          0x80  /* active high */

/*
 * "HSCX" mode bits
 */
#define  HSCX_MODE_ITF_FLG	0x01
#define  HSCX_MODE_TRANS	0x02
#define  HSCX_MODE_CCR_7	0x04
#define  HSCX_MODE_CCR_16	0x08
#define  HSCX_MODE_TESTLOOP	0x80

/*
 * "HSCX" status bits
 */
#define  HSCX_STAT_RME		0x01
#define  HSCX_STAT_RDO		0x10
#define  HSCX_STAT_CRCVFRRAB	0x0E
#define  HSCX_STAT_CRCVFR	0x06
#define  HSCX_STAT_RML_MASK	0x3f00

/*
 * "HSCX" interrupt bits
 */
#define  HSCX_INT_XPR		0x80
#define  HSCX_INT_XDU		0x40
#define  HSCX_INT_RPR		0x20
#define  HSCX_INT_MASK		0xE0

/*
 * "HSCX" command bits
 */
#define  HSCX_CMD_XRS		0x80
#define  HSCX_CMD_XME		0x01
#define  HSCX_CMD_RRS		0x20
#define  HSCX_CMD_XML_MASK	0x3f00

/*
 * Commands and parameters are sent to the "HSCX" as a long, but the
 * fields are handled as bytes.
 *
 * The long contains:
 *	(prot << 16)|(txl << 8)|cmd
 *
 * where:
 *	prot = protocol to use
 *	txl = transmit length
 *	cmd = the command to be executed
 *
 * The fields are defined as u_char in struct isic_softc.
 *
 * Macro to coalesce the byte fields into a u_int
 */
#define AVMA1PPSETCMDLONG(f) (f) = ((sc->avma1pp_cmd) | (sc->avma1pp_txl << 8) \
 					| (sc->avma1pp_prot << 16))

/*
 * to prevent deactivating the "HSCX" when both channels are active we
 * define an HSCX_ACTIVE flag which is or'd into the channel's state
 * flag in avma1pp_bchannel_setup upon active and cleared upon deactivation.
 * It is set high to allow room for new flags.
 */
#define HSCX_AVMA1PP_ACTIVE	0x1000

static int
ifpci_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_AVM &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_AVM_FRITZ_CARD)
		return 1;
	return 0;
}

static void
ifpci_attach(device_t parent, device_t self, void *aux)
{
	struct ifpci_softc *psc = device_private(self);
	struct pci_attach_args *pa = aux;
	struct isic_softc *sc = &psc->sc_isic;
	struct isdn_l3_driver *drv;
	u_int v;

	sc->sc_dev = self;

	/* announce */
	printf(": Fritz!PCI card\n");

	/* initialize sc */
	callout_init(&sc->sc_T3_callout, 0);
	callout_init(&sc->sc_T4_callout, 0);

	/* setup io mappings */
	sc->sc_cardtyp = CARD_TYPEP_AVMA1PCI;
	sc->sc_num_mappings = 1;
	MALLOC_MAPS(sc);
	sc->sc_maps[0].size = 0;
	if (pci_mapreg_map(pa, FRITZPCI_PORT0_MEM_MAPOFF, PCI_MAPREG_TYPE_MEM, 0,
	    &sc->sc_maps[0].t, &sc->sc_maps[0].h, &psc->sc_base, &psc->sc_size) != 0
	   && pci_mapreg_map(pa, FRITZPCI_PORT0_IO_MAPOFF, PCI_MAPREG_TYPE_IO, 0,
	    &sc->sc_maps[0].t, &sc->sc_maps[0].h, &psc->sc_base, &psc->sc_size) != 0) {
		aprint_error_dev(self, "can't map card\n");
		return;
	}

	/* setup access routines */

	sc->clearirq = NULL;
	sc->readreg = avma1pp_read_reg;
	sc->writereg = avma1pp_write_reg;

	sc->readfifo = avma1pp_read_fifo;
	sc->writefifo = avma1pp_write_fifo;


	/* setup card type */

	sc->sc_cardtyp = CARD_TYPEP_AVMA1PCI;

	/* setup IOM bus type */

	sc->sc_bustyp = BUS_TYPE_IOM2;

	/* this is no IPAC based card */
	sc->sc_ipac = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;

	/* setup interrupt mapping */
	avma1pp_map_int(psc, pa);

	/* init the card */
	/* the Linux driver does this to clear any pending ISAC interrupts */
	/* see if it helps any - XXXX */
	v = 0;
	v = ISAC_READ(I_STAR);
	v = ISAC_READ(I_MODE);
	v = ISAC_READ(I_ADF2);
	v = ISAC_READ(I_ISTA);
	if (v & ISAC_ISTA_EXI)
	{
		 v = ISAC_READ(I_EXIR);
	}
	v = ISAC_READ(I_CIRR);
	ISAC_WRITE(I_MASK, 0xff);
	/* the Linux driver does this to clear any pending HSCX interrupts */
	v = hscx_read_reg_int(0, HSCX_STAT, sc);
	v = hscx_read_reg_int(1, HSCX_STAT, sc);

	bus_space_write_1(sc->sc_maps[0].t, sc->sc_maps[0].h, STAT0_OFFSET, ASL_RESET_ALL|ASL_TIMERDISABLE);
	DELAY(SEC_DELAY/100); /* 10 ms */
	bus_space_write_1(sc->sc_maps[0].t, sc->sc_maps[0].h, STAT0_OFFSET, ASL_TIMERRESET|ASL_ENABLE_INT|ASL_TIMERDISABLE);
	DELAY(SEC_DELAY/100); /* 10 ms */

	/* setup i4b infrastructure (have to roll our own here) */

	/* sc->sc_isac_version = ((ISAC_READ(I_RBCH)) >> 5) & 0x03; */
	 printf("%s: ISAC %s (IOM-%c)\n", device_xname(self),
  		"2085 Version A1/A2 or 2086/2186 Version 1.1",
		 sc->sc_bustyp == BUS_TYPE_IOM1 ? '1' : '2');

	/* init the ISAC */
	isic_isac_init(sc);

	/* init the "HSCX" */
	avma1pp_bchannel_setup(sc, HSCX_CH_A, BPROT_NONE, 0);

	avma1pp_bchannel_setup(sc, HSCX_CH_B, BPROT_NONE, 0);

	/* can't use the normal B-Channel stuff */
	avma1pp_init_linktab(sc);

	/* set trace level */

	sc->sc_trace = TRACE_OFF;

	sc->sc_state = ISAC_IDLE;

	sc->sc_ibuf = NULL;
	sc->sc_ib = NULL;
	sc->sc_ilen = 0;

	sc->sc_obuf = NULL;
	sc->sc_op = NULL;
	sc->sc_ol = 0;
	sc->sc_freeflag = 0;

	sc->sc_obuf2 = NULL;
	sc->sc_freeflag2 = 0;

	/* init higher protocol layers */
	drv = isdn_attach_isdnif(device_xname(self),
	    "AVM Fritz!PCI", &sc->sc_l2, &ifpci_l3_driver, NBCH_BRI);
	sc->sc_l3token = drv;
	sc->sc_l2.driver = &isic_std_driver;
	sc->sc_l2.l1_token = sc;
	sc->sc_l2.drv = drv;
	isdn_layer2_status_ind(&sc->sc_l2, drv, STI_ATTACH, 1);
	isdn_isdnif_ready(drv->isdnif);
}

static int
ifpci_detach(device_t self, int flags)
{
	struct ifpci_softc *psc = device_private(self);

	bus_space_unmap(psc->sc_isic.sc_maps[0].t, psc->sc_isic.sc_maps[0].h, psc->sc_size);
	bus_space_free(psc->sc_isic.sc_maps[0].t, psc->sc_isic.sc_maps[0].h, psc->sc_size);
	pci_intr_disestablish(psc->sc_pc, psc->sc_ih);

	return (0);
}

int
ifpci_activate(device_t self, enum devact act)
{
	struct ifpci_softc *psc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		psc->sc_isic.sc_intr_valid = ISIC_INTR_DYING;
		isdn_layer2_status_ind(&psc->sc_isic.sc_l2, psc->sc_isic.sc_l3token, STI_ATTACH, 0);
		isdn_detach_isdnif(psc->sc_isic.sc_l3token);
		psc->sc_isic.sc_l3token = NULL;
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

/*---------------------------------------------------------------------------*
 *	AVM read fifo routines
 *---------------------------------------------------------------------------*/

static void
avma1pp_read_fifo(struct isic_softc *sc, int what, void *buf, size_t size)
{
	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(sc->sc_maps[0].t, sc->sc_maps[0].h,  ADDR_REG_OFFSET, ISAC_FIFO);
			bus_space_read_multi_1(sc->sc_maps[0].t, sc->sc_maps[0].h,  ISAC_REG_OFFSET, buf, size);
			break;
		case ISIC_WHAT_HSCXA:
			hscx_read_fifo(0, buf, size, sc);
			break;
		case ISIC_WHAT_HSCXB:
			hscx_read_fifo(1, buf, size, sc);
			break;
	}
}

static void
hscx_read_fifo(int chan, void *buf, size_t len, struct isic_softc *sc)
{
	u_int32_t *ip;
	size_t cnt;

	bus_space_write_4(sc->sc_maps[0].t, sc->sc_maps[0].h, ADDR_REG_OFFSET, chan);
	ip = (u_int32_t *)buf;
	cnt = 0;
	/* what if len isn't a multiple of sizeof(int) and buf is */
	/* too small ???? */
	while (cnt < len)
	{
		*ip++ = bus_space_read_4(sc->sc_maps[0].t, sc->sc_maps[0].h, ISAC_REG_OFFSET);
		cnt += 4;
	}
}

/*---------------------------------------------------------------------------*
 *	AVM write fifo routines
 *---------------------------------------------------------------------------*/

static void
avma1pp_write_fifo(struct isic_softc *sc, int what, const void *buf, size_t size)
{
	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(sc->sc_maps[0].t, sc->sc_maps[0].h,  ADDR_REG_OFFSET, ISAC_FIFO);
			bus_space_write_multi_1(sc->sc_maps[0].t, sc->sc_maps[0].h,  ISAC_REG_OFFSET, buf, size);
			break;
		case ISIC_WHAT_HSCXA:
			hscx_write_fifo(0, buf, size, sc);
			break;
		case ISIC_WHAT_HSCXB:
			hscx_write_fifo(1, buf, size, sc);
			break;
	}
}

static void
hscx_write_fifo(int chan, const void *buf, size_t len, struct isic_softc *sc)
{
	const u_int32_t *ip;
	size_t cnt;
	l1_bchan_state_t *Bchan = &sc->sc_chan[chan];

	sc->avma1pp_cmd &= ~HSCX_CMD_XME;
	sc->avma1pp_txl = 0;
	if (Bchan->out_mbuf_cur == NULL)
	{
	  if (Bchan->bprot != BPROT_NONE)
		 sc->avma1pp_cmd |= HSCX_CMD_XME;
	}
	if (len != sc->sc_bfifolen)
		sc->avma1pp_txl = len;

	cnt = 0; /* borrow cnt */
	AVMA1PPSETCMDLONG(cnt);
	hscx_write_reg(chan, HSCX_STAT, cnt, sc);

	ip = (const u_int32_t *)buf;
	cnt = 0;
	while (cnt < len)
	{
		bus_space_write_4(sc->sc_maps[0].t, sc->sc_maps[0].h, ISAC_REG_OFFSET, *ip);
		ip++;
		cnt += 4;
	}
}

/*---------------------------------------------------------------------------*
 *	AVM write register routines
 *---------------------------------------------------------------------------*/

static void
avma1pp_write_reg(struct isic_softc *sc, int what, bus_size_t offs, u_int8_t data)
{
	u_char reg_bank;
	switch (what) {
		case ISIC_WHAT_ISAC:
			reg_bank = (offs > MAX_LO_REG_OFFSET) ? ISAC_HI_REG_OFFSET:ISAC_LO_REG_OFFSET;
			/* set the register bank */
			bus_space_write_1(sc->sc_maps[0].t, sc->sc_maps[0].h, ADDR_REG_OFFSET, reg_bank);
			bus_space_write_1(sc->sc_maps[0].t, sc->sc_maps[0].h, ISAC_REG_OFFSET + (offs & ISAC_REGSET_MASK), data);
			break;
		case ISIC_WHAT_HSCXA:
			hscx_write_reg(0, offs, data, sc);
			break;
		case ISIC_WHAT_HSCXB:
			hscx_write_reg(1, offs, data, sc);
			break;
	}
}

static void
hscx_write_reg(int chan, u_int off, u_int val, struct isic_softc *sc)
{
	/* HACK */
	if (off == H_MASK)
		return;
	/* point at the correct channel */
	bus_space_write_4(sc->sc_maps[0].t, sc->sc_maps[0].h, ADDR_REG_OFFSET, chan);
	bus_space_write_4(sc->sc_maps[0].t, sc->sc_maps[0].h, ISAC_REG_OFFSET + off, val);
}

/*---------------------------------------------------------------------------*
 *	AVM read register routines
 *---------------------------------------------------------------------------*/

static u_int8_t
avma1pp_read_reg(struct isic_softc *sc, int what, bus_size_t offs)
{
	u_char reg_bank;
	switch (what) {
		case ISIC_WHAT_ISAC:
			reg_bank = (offs > MAX_LO_REG_OFFSET) ? ISAC_HI_REG_OFFSET:ISAC_LO_REG_OFFSET;
			/* set the register bank */
			bus_space_write_1(sc->sc_maps[0].t, sc->sc_maps[0].h, ADDR_REG_OFFSET, reg_bank);
			return(bus_space_read_1(sc->sc_maps[0].t, sc->sc_maps[0].h, ISAC_REG_OFFSET +
				(offs & ISAC_REGSET_MASK)));
		case ISIC_WHAT_HSCXA:
			return hscx_read_reg(0, offs, sc);
		case ISIC_WHAT_HSCXB:
			return hscx_read_reg(1, offs, sc);
	}
	return 0;
}

static u_char
hscx_read_reg(int chan, u_int off, struct isic_softc *sc)
{
	return(hscx_read_reg_int(chan, off, sc) & 0xff);
}

/*
 * need to be able to return an int because the RBCH is in the 2nd
 * byte.
 */
static u_int
hscx_read_reg_int(int chan, u_int off, struct isic_softc *sc)
{
	/* HACK */
	if (off == H_ISTA)
		return(0);
	/* point at the correct channel */
	bus_space_write_4(sc->sc_maps[0].t, sc->sc_maps[0].h, ADDR_REG_OFFSET, chan);
	return(bus_space_read_4(sc->sc_maps[0].t, sc->sc_maps[0].h, ISAC_REG_OFFSET + off));
}

/*
 * this is the real interrupt routine
 */
static void
avma1pp_hscx_intr(int h_chan, u_int stat, struct isic_softc *sc)
{
	register l1_bchan_state_t *chan = &sc->sc_chan[h_chan];
	int activity = -1;
	u_int param = 0;

	NDBGL1(L1_H_IRQ, "%#x", stat);

	if((stat & HSCX_INT_XDU) && (chan->bprot != BPROT_NONE))/* xmit data underrun */
	{
		chan->stat_XDU++;
		NDBGL1(L1_H_XFRERR, "xmit data underrun");
		/* abort the transmission */
		sc->avma1pp_txl = 0;
		sc->avma1pp_cmd |= HSCX_CMD_XRS;
		AVMA1PPSETCMDLONG(param);
		hscx_write_reg(h_chan, HSCX_STAT, param, sc);
		sc->avma1pp_cmd &= ~HSCX_CMD_XRS;
		AVMA1PPSETCMDLONG(param);
		hscx_write_reg(h_chan, HSCX_STAT, param, sc);

		if (chan->out_mbuf_head != NULL)  /* don't continue to transmit this buffer */
		{
			i4b_Bfreembuf(chan->out_mbuf_head);
			chan->out_mbuf_cur = chan->out_mbuf_head = NULL;
		}
	}

	/*
	 * The following is based on examination of the Linux driver.
	 *
	 * The logic here is different than with a "real" HSCX; all kinds
	 * of information (interrupt/status bits) are in stat.
	 *		HSCX_INT_RPR indicates a receive interrupt
	 *			HSCX_STAT_RDO indicates an overrun condition, abort -
	 *			otherwise read the bytes ((stat & HSCX_STZT_RML_MASK) >> 8)
	 *			HSCX_STAT_RME indicates end-of-frame and apparently any
	 *			CRC/framing errors are only reported in this state.
	 *				if ((stat & HSCX_STAT_CRCVFRRAB) != HSCX_STAT_CRCVFR)
	 *					CRC/framing error
	 */

	if(stat & HSCX_INT_RPR)
	{
		register int fifo_data_len;
		int error = 0;
		/* always have to read the FIFO, so use a scratch buffer */
		u_char scrbuf[HSCX_FIFO_LEN];

		if(stat & HSCX_STAT_RDO)
		{
			chan->stat_RDO++;
			NDBGL1(L1_H_XFRERR, "receive data overflow");
			error++;
		}

		/*
		 * check whether we're receiving data for an inactive B-channel
		 * and discard it. This appears to happen for telephony when
		 * both B-channels are active and one is deactivated. Since
		 * it is not really possible to deactivate the channel in that
		 * case (the ASIC seems to deactivate _both_ channels), the
		 * "deactivated" channel keeps receiving data which can lead
		 * to exhaustion of mbufs and a kernel panic.
		 *
		 * This is a hack, but it's the only solution I can think of
		 * without having the documentation for the ASIC.
		 * GJ - 28 Nov 1999
		 */
		 if (chan->state == HSCX_IDLE)
		 {
			NDBGL1(L1_H_XFRERR, "toss data from %d", h_chan);
			error++;
		 }

		fifo_data_len = ((stat & HSCX_STAT_RML_MASK) >> 8);

		if(fifo_data_len == 0)
			fifo_data_len = sc->sc_bfifolen;

		/* ALWAYS read data from HSCX fifo */

		HSCX_RDFIFO(h_chan, scrbuf, fifo_data_len);
		chan->rxcount += fifo_data_len;

		/* all error conditions checked, now decide and take action */

		if(error == 0)
		{
			if(chan->in_mbuf == NULL)
			{
				if((chan->in_mbuf = i4b_Bgetmbuf(BCH_MAX_DATALEN)) == NULL)
					panic("L1 avma1pp_hscx_intr: RME, cannot allocate mbuf!");
				chan->in_cbptr = chan->in_mbuf->m_data;
				chan->in_len = 0;
			}

			if((chan->in_len + fifo_data_len) <= BCH_MAX_DATALEN)
			{
			   	/* OK to copy the data */
				memcpy(chan->in_cbptr, scrbuf, fifo_data_len);
				chan->in_cbptr += fifo_data_len;
				chan->in_len += fifo_data_len;

				/* setup mbuf data length */

				chan->in_mbuf->m_len = chan->in_len;
				chan->in_mbuf->m_pkthdr.len = chan->in_len;

				if(sc->sc_trace & TRACE_B_RX)
				{
					struct i4b_trace_hdr hdr;
					hdr.type = (h_chan == HSCX_CH_A ? TRC_CH_B1 : TRC_CH_B2);
					hdr.dir = FROM_NT;
					hdr.count = ++sc->sc_trace_bcount;
					isdn_layer2_trace_ind(&sc->sc_l2, sc->sc_l3token, &hdr, chan->in_mbuf->m_len, chan->in_mbuf->m_data);
				}

				if (stat & HSCX_STAT_RME)
				{
				  if((stat & HSCX_STAT_CRCVFRRAB) == HSCX_STAT_CRCVFR)
				  {
					 (*chan->l4_driver->bch_rx_data_ready)(chan->l4_driver_softc);
					 activity = ACT_RX;

					 /* mark buffer ptr as unused */

					 chan->in_mbuf = NULL;
					 chan->in_cbptr = NULL;
					 chan->in_len = 0;
				  }
				  else
				  {
						chan->stat_CRC++;
						NDBGL1(L1_H_XFRERR, "CRC/RAB");
					  if (chan->in_mbuf != NULL)
					  {
						  i4b_Bfreembuf(chan->in_mbuf);
						  chan->in_mbuf = NULL;
						  chan->in_cbptr = NULL;
						  chan->in_len = 0;
					  }
				  }
				}
			} /* END enough space in mbuf */
			else
			{
				 if(chan->bprot == BPROT_NONE)
				 {
					  /* setup mbuf data length */

					  chan->in_mbuf->m_len = chan->in_len;
					  chan->in_mbuf->m_pkthdr.len = chan->in_len;

					  if(sc->sc_trace & TRACE_B_RX)
					  {
							struct i4b_trace_hdr hdr;
							hdr.type = (h_chan == HSCX_CH_A ? TRC_CH_B1 : TRC_CH_B2);
							hdr.dir = FROM_NT;
							hdr.count = ++sc->sc_trace_bcount;
							isdn_layer2_trace_ind(&sc->sc_l2, sc->sc_l3token, &hdr, chan->in_mbuf->m_len, chan->in_mbuf->m_data);
						}

					  if(!(isdn_bchan_silence(chan->in_mbuf->m_data, chan->in_mbuf->m_len)))
						 activity = ACT_RX;

					  /* move rx'd data to rx queue */

					  if (!(IF_QFULL(&chan->rx_queue)))
					  {
					  	IF_ENQUEUE(&chan->rx_queue, chan->in_mbuf);
					  }
					  else
				       	  {
						i4b_Bfreembuf(chan->in_mbuf);
				          }

					  /* signal upper layer that data are available */
					  (*chan->l4_driver->bch_rx_data_ready)(chan->l4_driver_softc);

					  /* alloc new buffer */

					  if((chan->in_mbuf = i4b_Bgetmbuf(BCH_MAX_DATALEN)) == NULL)
						 panic("L1 avma1pp_hscx_intr: RPF, cannot allocate new mbuf!");

					  /* setup new data ptr */

					  chan->in_cbptr = chan->in_mbuf->m_data;

					  /* OK to copy the data */
					  memcpy(chan->in_cbptr, scrbuf, fifo_data_len);

					  chan->in_cbptr += fifo_data_len;
					  chan->in_len = fifo_data_len;

					  chan->rxcount += fifo_data_len;
					}
				 else
					{
					  NDBGL1(L1_H_XFRERR, "RAWHDLC rx buffer overflow in RPF, in_len=%d", chan->in_len);
					  chan->in_cbptr = chan->in_mbuf->m_data;
					  chan->in_len = 0;
					}
			  }
		} /* if(error == 0) */
		else
		{
		  	/* land here for RDO */
			if (chan->in_mbuf != NULL)
			{
				i4b_Bfreembuf(chan->in_mbuf);
				chan->in_mbuf = NULL;
				chan->in_cbptr = NULL;
				chan->in_len = 0;
			}
			sc->avma1pp_txl = 0;
			sc->avma1pp_cmd |= HSCX_CMD_RRS;
			AVMA1PPSETCMDLONG(param);
			hscx_write_reg(h_chan, HSCX_STAT, param, sc);
			sc->avma1pp_cmd &= ~HSCX_CMD_RRS;
			AVMA1PPSETCMDLONG(param);
			hscx_write_reg(h_chan, HSCX_STAT, param, sc);
		}
	}


	/* transmit fifo empty, new data can be written to fifo */

	if(stat & HSCX_INT_XPR)
	{
		/*
		 * for a description what is going on here, please have
		 * a look at isic_bchannel_start() in i4b_bchan.c !
		 */

		NDBGL1(L1_H_IRQ, "%s: chan %d - XPR, Tx Fifo Empty!", device_xname(sc->sc_dev), h_chan);

		if(chan->out_mbuf_cur == NULL) 	/* last frame is transmitted */
		{
			IF_DEQUEUE(&chan->tx_queue, chan->out_mbuf_head);

			if(chan->out_mbuf_head == NULL)
			{
				chan->state &= ~HSCX_TX_ACTIVE;
				(*chan->l4_driver->bch_tx_queue_empty)(chan->l4_driver_softc);
			}
			else
			{
				chan->state |= HSCX_TX_ACTIVE;
				chan->out_mbuf_cur = chan->out_mbuf_head;
				chan->out_mbuf_cur_ptr = chan->out_mbuf_cur->m_data;
				chan->out_mbuf_cur_len = chan->out_mbuf_cur->m_len;

				if(sc->sc_trace & TRACE_B_TX)
				{
					struct i4b_trace_hdr hdr;
					hdr.type = (h_chan == HSCX_CH_A ? TRC_CH_B1 : TRC_CH_B2);
					hdr.dir = FROM_TE;
					hdr.count = ++sc->sc_trace_bcount;
					isdn_layer2_trace_ind(&sc->sc_l2, sc->sc_l3token, &hdr, chan->out_mbuf_cur->m_len, chan->out_mbuf_cur->m_data);
				}

				if(chan->bprot == BPROT_NONE)
				{
					if(!(isdn_bchan_silence(chan->out_mbuf_cur->m_data, chan->out_mbuf_cur->m_len)))
						activity = ACT_TX;
				}
				else
				{
					activity = ACT_TX;
				}
			}
		}

		isic_hscx_fifo(chan, sc);
	}

	/* call timeout handling routine */

	if(activity == ACT_RX || activity == ACT_TX)
		(*chan->l4_driver->bch_activity)(chan->l4_driver_softc, activity);
}

/*
 * this is the main routine which checks each channel and then calls
 * the real interrupt routine as appropriate
 */
static void
avma1pp_hscx_int_handler(struct isic_softc *sc)
{
	u_int stat;

	/* has to be a u_int because the byte count is in the 2nd byte */
	stat = hscx_read_reg_int(0, HSCX_STAT, sc);
	if (stat & HSCX_INT_MASK)
	  avma1pp_hscx_intr(0, stat, sc);
	stat = hscx_read_reg_int(1, HSCX_STAT, sc);
	if (stat & HSCX_INT_MASK)
	  avma1pp_hscx_intr(1, stat, sc);
}

static void
avma1pp_disable(struct isic_softc *sc)
{
	bus_space_write_1(sc->sc_maps[0].t, sc->sc_maps[0].h, STAT0_OFFSET, ASL_RESET_ALL|ASL_TIMERDISABLE);
}

static int
avma1pp_intr(void *parm)
{
	struct isic_softc *sc = parm;
	int ret = 0;
#define OURS	ret = 1
	u_char stat;

	if (sc->sc_intr_valid != ISIC_INTR_VALID)
		return 0;

	stat = bus_space_read_1(sc->sc_maps[0].t, sc->sc_maps[0].h, STAT0_OFFSET);
	NDBGL1(L1_H_IRQ, "stat %x", stat);
	/* was there an interrupt from this card ? */
	if ((stat & ASL_IRQ_Pending) == ASL_IRQ_Pending)
		return 0; /* no */
	/* interrupts are low active */
	if (!(stat & ASL_IRQ_TIMER))
	  NDBGL1(L1_H_IRQ, "timer interrupt ???");
	if (!(stat & ASL_IRQ_HSCX))
	{
	  NDBGL1(L1_H_IRQ, "HSCX");
		avma1pp_hscx_int_handler(sc);
		OURS;
	}
	if (!(stat & ASL_IRQ_ISAC))
	{
	  NDBGL1(L1_H_IRQ, "ISAC");
		for (;;) {
			/* get isac irq status */
			u_int8_t isac_irq_stat = ISAC_READ(I_ISTA);
			if (!isac_irq_stat)
				break;
			isic_isac_irq(sc, isac_irq_stat);
		}
		OURS;
	}
	return ret;
}

static void
avma1pp_map_int(struct ifpci_softc *psc, struct pci_attach_args *pa)
{
	struct isic_softc *sc = &psc->sc_isic;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char *intrstr;
	char intrbuf[PCI_INTRSTR_LEN];

	/* Map and establish the interrupt. */
	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(sc->sc_dev, "couldn't map interrupt\n");
		avma1pp_disable(sc);
		return;
	}
	psc->sc_pc = pc;
	intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));
	psc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, avma1pp_intr, sc);
	if (psc->sc_ih == NULL) {
		aprint_error_dev(sc->sc_dev, "couldn't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		avma1pp_disable(sc);
		return;
	}
	aprint_normal_dev(sc->sc_dev, "interrupting at %s\n", intrstr);
}

static void
avma1pp_hscx_init(struct isic_softc *sc, int h_chan, int activate)
{
	l1_bchan_state_t *chan = &sc->sc_chan[h_chan];
	u_int param = 0;

	NDBGL1(L1_BCHAN, "%s: channel=%d, %s",
		device_xname(sc->sc_dev), h_chan, activate ? "activate" : "deactivate");

	if (activate == 0)
	{
		/* only deactivate if both channels are idle */
		if (sc->sc_chan[HSCX_CH_A].state != HSCX_IDLE ||
			sc->sc_chan[HSCX_CH_B].state != HSCX_IDLE)
		{
			return;
		}
		sc->avma1pp_cmd = HSCX_CMD_XRS|HSCX_CMD_RRS;
		sc->avma1pp_prot = HSCX_MODE_TRANS;
		AVMA1PPSETCMDLONG(param);
		hscx_write_reg(h_chan, HSCX_STAT, param, sc);
		return;
	}
	if(chan->bprot == BPROT_RHDLC)
	{
		  NDBGL1(L1_BCHAN, "BPROT_RHDLC");

		/* HDLC Frames, transparent mode 0 */
		sc->avma1pp_cmd = HSCX_CMD_XRS|HSCX_CMD_RRS;
		sc->avma1pp_prot = HSCX_MODE_ITF_FLG;
		AVMA1PPSETCMDLONG(param);
		hscx_write_reg(h_chan, HSCX_STAT, param, sc);
		sc->avma1pp_cmd = HSCX_CMD_XRS;
		AVMA1PPSETCMDLONG(param);
		hscx_write_reg(h_chan, HSCX_STAT, param, sc);
		sc->avma1pp_cmd = 0;
	}
	else
	{
		  NDBGL1(L1_BCHAN, "BPROT_NONE??");

		/* Raw Telephony, extended transparent mode 1 */
		sc->avma1pp_cmd = HSCX_CMD_XRS|HSCX_CMD_RRS;
		sc->avma1pp_prot = HSCX_MODE_TRANS;
		AVMA1PPSETCMDLONG(param);
		hscx_write_reg(h_chan, HSCX_STAT, param, sc);
		sc->avma1pp_cmd = HSCX_CMD_XRS;
		AVMA1PPSETCMDLONG(param);
		hscx_write_reg(h_chan, HSCX_STAT, param, sc);
		sc->avma1pp_cmd = 0;
	}
}

static void
avma1pp_bchannel_setup(isdn_layer1token t, int h_chan, int bprot, int activate)
{
	struct isic_softc *sc = (struct isic_softc*)t;
	l1_bchan_state_t *chan = &sc->sc_chan[h_chan];

	int s = splnet();

	if(activate == 0)
	{
		/* deactivation */
		chan->state = HSCX_IDLE;
		avma1pp_hscx_init(sc, h_chan, activate);
	}

	NDBGL1(L1_BCHAN, "%s: channel=%d, %s",
		device_xname(sc->sc_dev), h_chan, activate ? "activate" : "deactivate");

	/* general part */

	chan->channel = h_chan;		/* B channel */
	chan->bprot = bprot;		/* B channel protocol */
	chan->state = HSCX_IDLE;	/* B channel state */

	/* receiver part */

	i4b_Bcleanifq(&chan->rx_queue);	/* clean rx queue */

	chan->rx_queue.ifq_maxlen = IFQ_MAXLEN;

	chan->rxcount = 0;		/* reset rx counter */

	i4b_Bfreembuf(chan->in_mbuf);	/* clean rx mbuf */

	chan->in_mbuf = NULL;		/* reset mbuf ptr */
	chan->in_cbptr = NULL;		/* reset mbuf curr ptr */
	chan->in_len = 0;		/* reset mbuf data len */

	/* transmitter part */

	i4b_Bcleanifq(&chan->tx_queue);	/* clean tx queue */

	chan->tx_queue.ifq_maxlen = IFQ_MAXLEN;

	chan->txcount = 0;		/* reset tx counter */

	i4b_Bfreembuf(chan->out_mbuf_head);	/* clean tx mbuf */

	chan->out_mbuf_head = NULL;	/* reset head mbuf ptr */
	chan->out_mbuf_cur = NULL;	/* reset current mbuf ptr */
	chan->out_mbuf_cur_ptr = NULL;	/* reset current mbuf data ptr */
	chan->out_mbuf_cur_len = 0;	/* reset current mbuf data cnt */

	if(activate != 0)
	{
		/* activation */
		avma1pp_hscx_init(sc, h_chan, activate);
		chan->state |= HSCX_AVMA1PP_ACTIVE;
	}

	splx(s);
}

static void
avma1pp_bchannel_start(isdn_layer1token t, int h_chan)
{
	struct isic_softc *sc = (struct isic_softc*)t;
	register l1_bchan_state_t *chan = &sc->sc_chan[h_chan];
	int s;
	int activity = -1;

	s = splnet();				/* enter critical section */
	if(chan->state & HSCX_TX_ACTIVE)	/* already running ? */
	{
		splx(s);
		return;				/* yes, leave */
	}

	/* get next mbuf from queue */

	IF_DEQUEUE(&chan->tx_queue, chan->out_mbuf_head);

	if(chan->out_mbuf_head == NULL)		/* queue empty ? */
	{
		splx(s);			/* leave critical section */
		return;				/* yes, exit */
	}

	/* init current mbuf values */

	chan->out_mbuf_cur = chan->out_mbuf_head;
	chan->out_mbuf_cur_len = chan->out_mbuf_cur->m_len;
	chan->out_mbuf_cur_ptr = chan->out_mbuf_cur->m_data;

	/* activity indicator for timeout handling */

	if(chan->bprot == BPROT_NONE)
	{
		if(!(isdn_bchan_silence(chan->out_mbuf_cur->m_data, chan->out_mbuf_cur->m_len)))
			activity = ACT_TX;
	}
	else
	{
		activity = ACT_TX;
	}

	chan->state |= HSCX_TX_ACTIVE;		/* we start transmitting */

	if(sc->sc_trace & TRACE_B_TX)	/* if trace, send mbuf to trace dev */
	{
		struct i4b_trace_hdr hdr;
		hdr.type = (h_chan == HSCX_CH_A ? TRC_CH_B1 : TRC_CH_B2);
		hdr.dir = FROM_TE;
		hdr.count = ++sc->sc_trace_bcount;
		isdn_layer2_trace_ind(&sc->sc_l2, sc->sc_l3token, &hdr, chan->out_mbuf_cur->m_len, chan->out_mbuf_cur->m_data);
	}

	isic_hscx_fifo(chan, sc);

	/* call timeout handling routine */

	if(activity == ACT_RX || activity == ACT_TX)
		(*chan->l4_driver->bch_activity)(chan->l4_driver_softc, activity);

	splx(s);
}

/*---------------------------------------------------------------------------*
 *	return the address of isic drivers linktab
 *---------------------------------------------------------------------------*/
static isdn_link_t *
avma1pp_ret_linktab(void *token, int channel)
{
	struct l2_softc *l2sc = token;
	struct isic_softc *sc = l2sc->l1_token;

	l1_bchan_state_t *chan = &sc->sc_chan[channel];

	return(&chan->isdn_linktab);
}

/*---------------------------------------------------------------------------*
 *	set the driver linktab in the b channel softc
 *---------------------------------------------------------------------------*/
static void
avma1pp_set_link(void *token, int channel, const struct isdn_l4_driver_functions *l4_driver, void *l4_driver_softc)
{
	struct l2_softc *l2sc = token;
	struct isic_softc *sc = l2sc->l1_token;
	l1_bchan_state_t *chan = &sc->sc_chan[channel];

	chan->l4_driver = l4_driver;
	chan->l4_driver_softc = l4_driver_softc;
}

static const struct isdn_l4_bchannel_functions
avma1pp_l4_bchannel_functions = {
	avma1pp_bchannel_setup,
	avma1pp_bchannel_start,
	avma1pp_bchannel_stat
};

/*---------------------------------------------------------------------------*
 *	initialize our local linktab
 *---------------------------------------------------------------------------*/
static void
avma1pp_init_linktab(struct isic_softc *sc)
{
	l1_bchan_state_t *chan = &sc->sc_chan[HSCX_CH_A];
	isdn_link_t *lt = &chan->isdn_linktab;

	/* local setup */
	lt->l1token = sc;
	lt->channel = HSCX_CH_A;
	lt->bchannel_driver = &avma1pp_l4_bchannel_functions;
	lt->tx_queue = &chan->tx_queue;

	/* used by non-HDLC data transfers, i.e. telephony drivers */
	lt->rx_queue = &chan->rx_queue;

	/* used by HDLC data transfers, i.e. ipr and isp drivers */
	lt->rx_mbuf = &chan->in_mbuf;

	chan = &sc->sc_chan[HSCX_CH_B];
	lt = &chan->isdn_linktab;

	lt->l1token = sc;
	lt->channel = HSCX_CH_B;
	lt->bchannel_driver = &avma1pp_l4_bchannel_functions;
	lt->tx_queue = &chan->tx_queue;

	/* used by non-HDLC data transfers, i.e. telephony drivers */
	lt->rx_queue = &chan->rx_queue;

	/* used by HDLC data transfers, i.e. ipr and isp drivers */
	lt->rx_mbuf = &chan->in_mbuf;
}

/*
 * use this instead of isic_bchannel_stat in i4b_bchan.c because it's static
 */
static void
avma1pp_bchannel_stat(isdn_layer1token t, int h_chan, bchan_statistics_t *bsp)
{
	struct isic_softc *sc = (struct isic_softc*)t;
	l1_bchan_state_t *chan = &sc->sc_chan[h_chan];
	int s;

	s = splnet();

	bsp->outbytes = chan->txcount;
	bsp->inbytes = chan->rxcount;

	chan->txcount = 0;
	chan->rxcount = 0;

	splx(s);
}

/*---------------------------------------------------------------------------*
 *	fill HSCX fifo with data from the current mbuf
 *	Put this here until it can go into i4b_hscx.c
 *---------------------------------------------------------------------------*/
static int
isic_hscx_fifo(l1_bchan_state_t *chan, struct isic_softc *sc)
{
	int len;
	int nextlen;
	int i;
	int cmd;
	/* using a scratch buffer simplifies writing to the FIFO */
	u_char scrbuf[HSCX_FIFO_LEN];

	len = 0;
	cmd = 0;

	/*
	 * fill the HSCX tx fifo with data from the current mbuf. if
	 * current mbuf holds less data than HSCX fifo length, try to
	 * get the next mbuf from (a possible) mbuf chain. if there is
	 * not enough data in a single mbuf or in a chain, then this
	 * is the last mbuf and we tell the HSCX that it has to send
	 * CRC and closing flag
	 */

	while(chan->out_mbuf_cur && len != sc->sc_bfifolen)
	{
		nextlen = min(chan->out_mbuf_cur_len, sc->sc_bfifolen - len);

#ifdef NOTDEF
		printf("i:mh=%p, mc=%p, mcp=%p, mcl=%d l=%d nl=%d # ",
			chan->out_mbuf_head,
			chan->out_mbuf_cur,
			chan->out_mbuf_cur_ptr,
			chan->out_mbuf_cur_len,
			len,
			nextlen);
#endif

		cmd |= HSCX_CMDR_XTF;
		/* collect the data in the scratch buffer */
		for (i = 0; i < nextlen; i++)
			scrbuf[i + len] = chan->out_mbuf_cur_ptr[i];

		len += nextlen;
		chan->txcount += nextlen;

		chan->out_mbuf_cur_ptr += nextlen;
		chan->out_mbuf_cur_len -= nextlen;

		if(chan->out_mbuf_cur_len == 0)
		{
			if((chan->out_mbuf_cur = chan->out_mbuf_cur->m_next) != NULL)
			{
				chan->out_mbuf_cur_ptr = chan->out_mbuf_cur->m_data;
				chan->out_mbuf_cur_len = chan->out_mbuf_cur->m_len;

				if(sc->sc_trace & TRACE_B_TX)
				{
					struct i4b_trace_hdr hdr;
					hdr.type = (chan->channel == HSCX_CH_A ? TRC_CH_B1 : TRC_CH_B2);
					hdr.dir = FROM_TE;
					hdr.count = ++sc->sc_trace_bcount;
					isdn_layer2_trace_ind(&sc->sc_l2, sc->sc_l3token, &hdr, chan->out_mbuf_cur->m_len, chan->out_mbuf_cur->m_data);
				}
			}
			else
			{
				if (chan->bprot != BPROT_NONE)
					cmd |= HSCX_CMDR_XME;
				i4b_Bfreembuf(chan->out_mbuf_head);
				chan->out_mbuf_head = NULL;
			}
		}
	}
	/* write what we have from the scratch buf to the HSCX fifo */
	if (len != 0)
		HSCX_WRFIFO(chan->channel, scrbuf, len);
	return(cmd);
}
