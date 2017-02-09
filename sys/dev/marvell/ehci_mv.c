/*	$NetBSD: ehci_mv.c,v 1.5 2014/03/15 13:33:48 kiyohara Exp $	*/
/*
 * Copyright (c) 2008 KIYOHARA Takashi
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ehci_mv.c,v 1.5 2014/03/15 13:33:48 kiyohara Exp $");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/systm.h>

#include <dev/marvell/marvellreg.h>
#include <dev/marvell/marvellvar.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>

#include <dev/usb/ehcireg.h>
#include <dev/usb/ehcivar.h>

#include "locators.h"

#ifdef EHCI_DEBUG
#define DPRINTF(x)	if (ehcidebug) printf x
extern int ehcidebug;
#else
#define DPRINTF(x)
#endif


#define MARVELL_USB_SIZE		0x1000

#define MARVELL_USB_NWINDOW		4

#define MARVELL_USB_ID			0x000
#define MARVELL_USB_HWGENERAL		0x004
#define MARVELL_USB_HWHOST		0x008
#define MARVELL_USB_HWDEVICE		0x00c
#define MARVELL_USB_HWTXBUF		0x010
#define MARVELL_USB_HWRXBUF		0x014
#define MARVELL_USB_HWTTTXBUF		0x018
#define MARVELL_USB_HWTTRXBUF		0x01c

/* ehci generic registers */
#define MARVELL_USB_EHCI_BASE		0x100
#define MARVELL_USB_EHCI_SIZE		0x100

/* ehci vendor extension registers */
#define MARVELL_USB_EHCI_PS_PSPD	0x0c000000	/* Port speed */
#define MARVELL_USB_EHCI_PS_PSPD_FS	0x00000000	/*  Full speed */
#define MARVELL_USB_EHCI_PS_PSPD_LS	0x04000000	/*  Low speed */
#define MARVELL_USB_EHCI_PS_PSPD_HS	0x08000000	/*  High speed */

#define MARVELL_USB_EHCI_USBMODE	0x68
#define MARVELL_USB_EHCI_MODE_STRMDIS	0x00000008 /* RW straming disable */
#define MARVELL_USB_EHCI_MODE_BE	0x00000004 /* RW B/L endianness select*/
#define MARVELL_USB_EHCI_MODE_HDMASK	0x00000003 /* RW host/device Mask */
#define MARVELL_USB_EHCI_MODE_HOST	0x00000003 /* RW mode host */
#define MARVELL_USB_EHCI_MODE_DEVICE	0x00000002 /* RW mode device */

#define MARVELL_USB_DCIVERSION		0x120
#define MARVELL_USB_DCCPARAMS		0x124
#define MARVELL_USB_TTCTRL		0x15c
#define MARVELL_USB_BURSTSIZE		0x160
#define MARVELL_USB_TXFILLTUNING	0x164
#define MARVELL_USB_TXTTFILLTUNING	0x168
#define MARVELL_USB_OTGSC		0x1a4
#define MARVELL_USB_USBMODE		0x1a8
#define MARVELL_USB_USBMODE_MASK		(3 << 0)
#define MARVELL_USB_USBMODE_HOST		(3 << 0)
#define MARVELL_USB_USBMODE_DEVICE		(2 << 0)
#define MARVELL_USB_USBMODE_STREAMDISABLE	(1 << 4)
#define MARVELL_USB_ENPDTSETUPSTAT	0x1ac
#define MARVELL_USB_ENDPTPRIME		0x1b0
#define MARVELL_USB_ENDPTFLUSH		0x1b4
#define MARVELL_USB_ENDPTSTATS		0x1b8
#define MARVELL_USB_ENDPTCOMPLETE	0x1bc
#define MARVELL_USB_ENDPTCTRL0		0x1c0
#define MARVELL_USB_ENDPTCTRL1		0x1c1
#define MARVELL_USB_ENDPTCTRL2		0x1c2
#define MARVELL_USB_ENDPTCTRL3		0x1c3
/* Bridge Control And Status Registers */
#define MARVELL_USB_BCR			0x300 /* Control */
/* Bridge Interrupt and Error Registers */
#define MARVELL_USB_BICR		0x310 /* Interrupt Cause */
#define MARVELL_USB_BIMR		0x314 /* Interrupt Mask */
#define MARVELL_USB_BIR_ADDRDECERR		(1 << 0)
#define MARVELL_USB_BEAR		0x31c /* Error Address */
/* Bridge Address Decoding Registers */
#define MARVELL_USB_WCR(n)		(0x320 + (n) * 0x10) /* WinN Control */
#define MARVELL_USB_WCR_WINEN			(1 << 0)
#define MARVELL_USB_WCR_TARGET(t)		(((t) & 0xf) << 4)
#define MARVELL_USB_WCR_ATTR(a)			(((a) & 0xff) << 8)
#define MARVELL_USB_WCR_SIZE(s)			(((s) - 1) & 0xffff0000)
#define MARVELL_USB_WBR(n)		(0x324 + (n) * 0x10) /* WinN Base */
#define MARVELL_USB_WBR_BASE(b)			((b) & 0xffff0000)
/* IPG Metal Fix Register ??? */
#define MARVELL_USB_IPGR		0x360
/* USB 2.0 PHY Register Map */
#define MARVELL_USB_PCR			0x400 /* Power Control */
#define MARVELL_USB_PCR_PU			(1 << 0)        /* Power Up */
#define MARVELL_USB_PCR_PUPLL			(1 << 1)        /*Power Up PLL*/
#define MARVELL_USB_PCR_SUSPENDM		(1 << 2)
#define MARVELL_USB_PCR_VBUSPWRFAULT		(1 << 3)
#define MARVELL_USB_PCR_PWRCTLWAKEUP		(1 << 4)
#define MARVELL_USB_PCR_PUREF			(1 << 5)
#define MARVELL_USB_PCR_BGVSEL_MASK		(3 << 6)
#define MARVELL_USB_PCR_BGVSEL_CONNECT_ANAGRP	(1 << 6)
#define MARVELL_USB_PCR_REGARCDPDMMODE		(1 << 8)
#define MARVELL_USB_PCR_REGDPPULLDOWN		(1 << 9)
#define MARVELL_USB_PCR_REGDMPULLDOWN		(1 << 10)
#define MARVELL_USB_PCR_UTMISESSION		(1 << 23)
#define MARVELL_USB_PCR_UTMIVBUSVALID		(1 << 24)
#define MARVELL_USB_PCR_UTMIAVALID		(1 << 25)
#define MARVELL_USB_PCR_UTMIBVALID		(1 << 26)
#define MARVELL_USB_PCR_TXBITSTUFF		(1 << 27)
/* USB PHY Tx Control Register */
#define MARVELL_USB_PTCR		0x420
/* USB PHY Rx Control Register */
#define MARVELL_USB_PRCR		0x430
/* USB PHY IVREFF Control Register */
#define MARVELL_USB_PIVREFFCR		0x440
/* USB PHY Test Group Control Register */
#define MARVELL_USB_PTGCR		0x450


struct mvusb_softc {
	ehci_softc_t sc;

	int sc_model;
	int sc_rev;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
};

static int mvusb_match(device_t, cfdata_t, void *);
static void mvusb_attach(device_t, device_t, void *);

static void mvusb_init(struct mvusb_softc *, enum marvell_tags *);
static void mvusb_wininit(struct mvusb_softc *, enum marvell_tags *);

static void mvusb_vendor_init(struct ehci_softc *);
static int mvusb_vendor_port_status(struct ehci_softc *, uint32_t, int);

CFATTACH_DECL2_NEW(mvusb_gt, sizeof(struct mvusb_softc),
    mvusb_match, mvusb_attach, NULL, ehci_activate, NULL, ehci_childdet);
CFATTACH_DECL2_NEW(mvusb_mbus, sizeof(struct mvusb_softc),
    mvusb_match, mvusb_attach, NULL, ehci_activate, NULL, ehci_childdet);


/* ARGSUSED */
static int
mvusb_match(device_t parent, cfdata_t match, void *aux)
{
	struct marvell_attach_args *mva = aux;

	if (strcmp(mva->mva_name, match->cf_name) != 0)
		return 0;
	if (mva->mva_offset == MVA_OFFSET_DEFAULT ||
	    mva->mva_irq == MVA_IRQ_DEFAULT)
		return 0;

	mva->mva_size = MARVELL_USB_SIZE;
	return 1;
}

/* ARGSUSED */
static void
mvusb_attach(device_t parent, device_t self, void *aux)
{
	struct mvusb_softc *sc = device_private(self);
	struct marvell_attach_args *mva = aux;
	usbd_status r;

	aprint_normal(": Marvell USB 2.0 Interface\n");
	aprint_naive("\n");

	sc->sc.sc_dev = self;
	sc->sc.sc_bus.hci_private = sc;

	sc->sc_model = mva->mva_model;
	sc->sc_rev = mva->mva_revision;
	sc->sc_iot = mva->mva_iot;

	/* Map I/O registers for marvell usb */
	if (bus_space_subregion(mva->mva_iot, mva->mva_ioh, mva->mva_offset,
	    mva->mva_size, &sc->sc_ioh)) {
		aprint_error_dev(self, "can't map registers\n");
		return;
	}
	mvusb_init(sc, mva->mva_tags);

	/* Map I/O registers for ehci */
	sc->sc.sc_size = MARVELL_USB_EHCI_SIZE;
	if (bus_space_subregion(sc->sc_iot, sc->sc_ioh, MARVELL_USB_EHCI_BASE,
	    sc->sc.sc_size, &sc->sc.ioh)) {
		aprint_error_dev(self, "can't subregion registers\n");
		return;
	}
	sc->sc.iot = sc->sc_iot;
	sc->sc.sc_bus.dmatag = mva->mva_dmat;

	/* Disable interrupts, so we don't get any spurious ones. */
	sc->sc.sc_offs = EREAD1(&sc->sc, EHCI_CAPLENGTH);
	DPRINTF(("%s: offs=%d\n", device_xname(self), sc->sc.sc_offs));
	EOWRITE2(&sc->sc, EHCI_USBINTR, 0);

	marvell_intr_establish(mva->mva_irq, IPL_USB, ehci_intr, sc);

	sc->sc.sc_bus.usbrev = USBREV_2_0;
	/* Figure out vendor for root hub descriptor. */
	sc->sc.sc_id_vendor = 0x0000;				/* XXXXX */
	strcpy(sc->sc.sc_vendor, "Marvell");

	sc->sc.sc_vendor_init = mvusb_vendor_init;
	sc->sc.sc_vendor_port_status = mvusb_vendor_port_status;

	r = ehci_init(&sc->sc);
	if (r != USBD_NORMAL_COMPLETION) {
		aprint_error_dev(self, "init failed, error=%d\n", r);
		return;
	}

	/* Attach usb device. */
	sc->sc.sc_child = config_found(self, &sc->sc.sc_bus, usbctlprint);
}

static void
mvusb_init(struct mvusb_softc *sc, enum marvell_tags *tags)
{
	uint32_t reg;
	int opr_offs;

	/* Clear Interrupt Cause and Mask registers */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MARVELL_USB_BICR, 0);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MARVELL_USB_BIMR, 0);

	opr_offs = bus_space_read_1(sc->sc_iot, sc->sc_ioh,
	    MARVELL_USB_EHCI_BASE + EHCI_CAPLENGTH);

	/* Reset controller */
	reg = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
	    MARVELL_USB_EHCI_BASE + opr_offs + EHCI_USBCMD);
	reg |= EHCI_CMD_HCRESET;
	bus_space_write_4(sc->sc_iot, sc->sc_ioh,
	    MARVELL_USB_EHCI_BASE + opr_offs + EHCI_USBCMD, reg);
	while (bus_space_read_4(sc->sc_iot, sc->sc_ioh,
	    MARVELL_USB_EHCI_BASE + opr_offs + EHCI_USBCMD) & EHCI_CMD_HCRESET);

	if (!((sc->sc_model == MARVELL_ORION_1_88F5181 &&
					(sc->sc_rev <= 3 || sc->sc_rev == 8)) ||
	    (sc->sc_model == MARVELL_ORION_1_88F5182 && sc->sc_rev <= 1) ||
	    (sc->sc_model == MARVELL_ORION_2_88F5281 && sc->sc_rev <= 1))) {
		reg =
		    bus_space_read_4(sc->sc_iot, sc->sc_ioh, MARVELL_USB_IPGR);
		/*
		 * Change bits[14:8] - IPG for non Start of Frame Packets
		 * from 0x9(default) to 0xc
		 */
		reg &= ~(0x7f << 8);
		reg |= (0x0c << 8);
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, MARVELL_USB_IPGR,
		    reg);
	}
	if (!(sc->sc_model == MARVELL_ARMADAXP_MV78460)) {
		reg = bus_space_read_4(sc->sc_iot, sc->sc_ioh, MARVELL_USB_PCR);
		reg &= ~MARVELL_USB_PCR_BGVSEL_MASK;
		reg |= MARVELL_USB_PCR_BGVSEL_CONNECT_ANAGRP;
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, MARVELL_USB_PCR, reg);

		reg = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		    MARVELL_USB_PTCR);
		if (sc->sc_model == MARVELL_ORION_1_88F5181 && sc->sc_rev <= 1)
			/* For OrionI A1/A0 rev: bit[21]=0 (TXDATA_BLOCK_EN=0) */
			reg &= ~(1 << 21);
		else
			reg |= (1 << 21);
		/* bit[13]=1, (REG_EXT_RCAL_EN=1) */
		reg |= (1 << 13);
		/* bits[6:3]=8 (IMP_CAL=8) */
		reg &= ~(0xf << 3);
		reg |= (8 << 3);
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, MARVELL_USB_PTCR,
		    reg);

		reg = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		    MARVELL_USB_PRCR);
		/* bits[8:9] - (DISCON_THRESHOLD ) */
		/*
		 * Orion1-A0/A1/B0=11, Orion2-A0=10,
		 * Orion1-B1 and Orion2-B0 later=00 
		 */
		reg &= ~(3 << 8);
		if (sc->sc_model == MARVELL_ORION_1_88F5181 && sc->sc_rev <= 2)
			reg |= (3 << 8);
		else if (sc->sc_model == MARVELL_ORION_2_88F5281 &&
		    sc->sc_rev == 0)
			reg |= (2 << 8);
		/* bit[21]=0 (CDR_FASTLOCK_EN=0) */
		reg &= ~(1 << 21);
		/* bits[27:26]=0 (EDGE_DET_SEL=0) */
		reg &= ~(3 << 26);
		/* bits[31:30]=3 (RXDATA_BLOCK_LENGHT=3) */
		reg |= (3 << 30);
		/* bits[7:4]=1 (SQ_THRESH=1) */
		reg &= ~(0xf << 4);
		reg |= (1 << 4);
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, MARVELL_USB_PRCR,
		    reg);

		reg = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		    MARVELL_USB_PIVREFFCR);
		/* bits[1:0]=2 (PLLVDD12=2)*/
		reg &= ~(3 << 0);
		reg |= (2 << 0);
		/* bits[5:4]=3 (RXVDD=3) */
		reg &= ~(3 << 4);
		reg |= (3 << 4);
		/* bit[19] (Reserved) */
		reg &= ~(1 << 19);
		bus_space_write_4(sc->sc_iot, sc->sc_ioh,
		    MARVELL_USB_PIVREFFCR, reg);

		reg = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		    MARVELL_USB_PTGCR);
		/* bit[15]=0 (REG_FIFO_SQ_RST=0) */
		reg &= ~(1 << 15);
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, MARVELL_USB_PTGCR,
		    reg);
	}

	mvusb_wininit(sc, tags);
}

static void
mvusb_wininit(struct mvusb_softc *sc, enum marvell_tags *tags)
{
	device_t pdev = device_parent(sc->sc.sc_dev);
	uint64_t base;
	uint32_t size;
	int window, target, attr, rv, i;

	for (window = 0, i = 0;
	    tags[i] != MARVELL_TAG_UNDEFINED && window < MARVELL_USB_NWINDOW;
	    i++) {
		rv = marvell_winparams_by_tag(pdev, tags[i],
		    &target, &attr, &base, &size);
		if (rv != 0 || size == 0)
			continue;
		if (base > 0xffffffffULL) {
			aprint_error_dev(sc->sc.sc_dev,
			    "tag %d address 0x%llx not support\n",
			    tags[i], base);
			continue;
		}

		bus_space_write_4(sc->sc_iot, sc->sc_ioh,
		    MARVELL_USB_WCR(window),
		    MARVELL_USB_WCR_WINEN |
		    MARVELL_USB_WCR_TARGET(target) |
		    MARVELL_USB_WCR_ATTR(attr) |
		    MARVELL_USB_WCR_SIZE(size));
		bus_space_write_4(sc->sc_iot, sc->sc_ioh,
		    MARVELL_USB_WBR(window), MARVELL_USB_WBR_BASE(base));
		window++;
	}
	for (; window < MARVELL_USB_NWINDOW; window++)
		bus_space_write_4(sc->sc_iot, sc->sc_ioh,
		    MARVELL_USB_WCR(window), 0);
}

static void
mvusb_vendor_init(struct ehci_softc *sc)
{
	uint32_t mode;

	/* put TDI/ARC silicon into EHCI mode */
	mode = EOREAD4(sc, MARVELL_USB_EHCI_USBMODE);
	mode &= ~MARVELL_USB_EHCI_MODE_HDMASK;	/* Host/Device Mask */
	mode |= MARVELL_USB_EHCI_MODE_HOST;
	mode |= MARVELL_USB_EHCI_MODE_STRMDIS;
	EOWRITE4(sc, MARVELL_USB_EHCI_USBMODE, mode);
}

static int
mvusb_vendor_port_status(struct ehci_softc *sc, uint32_t v, int i)
{

	i &= ~UPS_HIGH_SPEED;
	if (v & EHCI_PS_CS) {
		switch (v & MARVELL_USB_EHCI_PS_PSPD) {
		case MARVELL_USB_EHCI_PS_PSPD_FS:
			break;
		case MARVELL_USB_EHCI_PS_PSPD_LS:
			i |= UPS_LOW_SPEED;
			break;
		case MARVELL_USB_EHCI_PS_PSPD_HS:
		default:
			i |= UPS_HIGH_SPEED;
		}
	}

	return i;
}
