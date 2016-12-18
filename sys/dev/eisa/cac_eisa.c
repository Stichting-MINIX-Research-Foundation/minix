/*	$NetBSD: cac_eisa.c,v 1.23 2014/03/29 19:28:24 christos Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 2000 Jonathan Lemon
 * Copyright (c) 1999 by Matthew N. Dodd <winter@jurai.net>
 * All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * EISA front-end for cac(4) driver.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: cac_eisa.c,v 1.23 2014/03/29 19:28:24 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/eisa/eisavar.h>
#include <dev/eisa/eisadevs.h>

#include <dev/ic/cacreg.h>
#include <dev/ic/cacvar.h>

#define CAC_EISA_SLOT_OFFSET		0x0c88
#define CAC_EISA_IOSIZE			0x0017
#define CAC_EISA_IOCONF			0x38

static void	cac_eisa_attach(device_t, device_t, void *);
static int	cac_eisa_match(device_t, cfdata_t, void *);

static struct	cac_ccb *cac_eisa_l0_completed(struct cac_softc *);
static int	cac_eisa_l0_fifo_full(struct cac_softc *);
static void	cac_eisa_l0_intr_enable(struct cac_softc *, int);
static int	cac_eisa_l0_intr_pending(struct cac_softc *);
static void	cac_eisa_l0_submit(struct cac_softc *, struct cac_ccb *);

CFATTACH_DECL_NEW(cac_eisa, sizeof(struct cac_softc),
    cac_eisa_match, cac_eisa_attach, NULL, NULL);

static const struct cac_linkage cac_eisa_l0 = {
	cac_eisa_l0_completed,
	cac_eisa_l0_fifo_full,
	cac_eisa_l0_intr_enable,
	cac_eisa_l0_intr_pending,
	cac_eisa_l0_submit
};

static struct cac_eisa_type {
	const char	*ct_prodstr;
	const char	*ct_typestr;
	const struct	cac_linkage *ct_linkage;
} cac_eisa_type[] = {
	{ "CPQ4001",	"IDA",		&cac_eisa_l0 },
	{ "CPQ4002",	"IDA-2",	&cac_eisa_l0 },
	{ "CPQ4010",	"IEAS",		&cac_eisa_l0 },
	{ "CPQ4020",	"SMART",	&cac_eisa_l0 },
	{ "CPQ4030",	"SMART-2/E",	&cac_l0 },
};

static int
cac_eisa_match(device_t parent, cfdata_t match,
    void *aux)
{
	struct eisa_attach_args *ea;
	int i;

	ea = aux;

	for (i = 0; i < sizeof(cac_eisa_type) / sizeof(cac_eisa_type[0]); i++)
		if (strcmp(ea->ea_idstring, cac_eisa_type[i].ct_prodstr) == 0)
			return (1);

	return (0);
}

static void
cac_eisa_attach(device_t parent, device_t self, void *aux)
{
	struct eisa_attach_args *ea;
	bus_space_handle_t ioh;
	eisa_chipset_tag_t ec;
	eisa_intr_handle_t ih;
	struct cac_softc *sc;
	bus_space_tag_t iot;
	const char *intrstr;
	int irq, i;
	char intrbuf[EISA_INTRSTR_LEN];

	ea = aux;
	sc = device_private(self);
	iot = ea->ea_iot;
	ec = ea->ea_ec;

	if (bus_space_map(iot, EISA_SLOT_ADDR(ea->ea_slot) +
	    CAC_EISA_SLOT_OFFSET, CAC_EISA_IOSIZE, 0, &ioh)) {
		printf("can't map i/o space\n");
		return;
	}

	sc->sc_dev = self;
	sc->sc_iot = iot;
	sc->sc_ioh = ioh;
	sc->sc_dmat = ea->ea_dmat;

	/*
	 * Map and establish the interrupt.
	 */
	switch (bus_space_read_1(iot, ioh, CAC_EISA_IOCONF) & 0xf0) {
	case 0x20:
		irq = 10;
		break;
	case 0x10:
		irq = 11;
		break;
	case 0x40:
		irq = 14;
		break;
	case 0x80:
		irq = 15;
		break;
	default:
		printf("controller on invalid IRQ\n");
		return;
	}

	if (eisa_intr_map(ec, irq, &ih)) {
		printf("can't map interrupt (%d)\n", irq);
		return;
	}

	intrstr = eisa_intr_string(ec, ih, intrbuf, sizeof(intrbuf));
	if ((sc->sc_ih = eisa_intr_establish(ec, ih, IST_LEVEL, IPL_BIO,
	    cac_intr, sc)) == NULL) {
		printf("can't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}

	/*
	 * Print board type and attach to the bus-independent code.
	 */
	for (i = 0; i < sizeof(cac_eisa_type) / sizeof(cac_eisa_type[0]); i++)
		if (strcmp(ea->ea_idstring, cac_eisa_type[i].ct_prodstr) == 0)
			break;

	printf(": Compaq %s\n", cac_eisa_type[i].ct_typestr);
	memcpy(&sc->sc_cl, cac_eisa_type[i].ct_linkage, sizeof(sc->sc_cl));
	cac_init(sc, intrstr, 0);
}

/*
 * Linkage specific to EISA boards.
 */

static int
cac_eisa_l0_fifo_full(struct cac_softc *sc)
{

	return ((cac_inb(sc, CAC_EISAREG_SYSTEM_DOORBELL) &
	    CAC_EISA_CHANNEL_CLEAR) == 0);
}

static void
cac_eisa_l0_submit(struct cac_softc *sc, struct cac_ccb *ccb)
{
	u_int16_t size;

	/*
	 * On these boards, `ccb_hdr.size' is actually for control flags.
	 * Set it to zero and pass the value by means of an I/O port.
	 */
	size = le16toh(ccb->ccb_hdr.size) << 2;
	ccb->ccb_hdr.size = 0;

	bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap,
	    (char *)ccb - (char *)sc->sc_ccbs,
	    sizeof(struct cac_ccb), BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);

	cac_outb(sc, CAC_EISAREG_SYSTEM_DOORBELL, CAC_EISA_CHANNEL_CLEAR);
	cac_outl(sc, CAC_EISAREG_LIST_ADDR, ccb->ccb_paddr);
	cac_outw(sc, CAC_EISAREG_LIST_LEN, size);
	cac_outb(sc, CAC_EISAREG_LOCAL_DOORBELL, CAC_EISA_CHANNEL_BUSY);
}

static struct cac_ccb *
cac_eisa_l0_completed(struct cac_softc *sc)
{
	struct cac_ccb *ccb;
	u_int32_t off;
	u_int8_t status;

	if ((cac_inb(sc, CAC_EISAREG_SYSTEM_DOORBELL) &
	    CAC_EISA_CHANNEL_BUSY) == 0)
		return (NULL);

	cac_outb(sc, CAC_EISAREG_SYSTEM_DOORBELL, CAC_EISA_CHANNEL_BUSY);
	off = cac_inl(sc, CAC_EISAREG_COMPLETE_ADDR);
	status = cac_inb(sc, CAC_EISAREG_LIST_STATUS);
	cac_outb(sc, CAC_EISAREG_LOCAL_DOORBELL, CAC_EISA_CHANNEL_CLEAR);

	if (off == 0)
		return (NULL);

	off = (off & ~3) - sc->sc_ccbs_paddr;
	ccb = (struct cac_ccb *)((char *)sc->sc_ccbs + off);

	bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap, off, sizeof(struct cac_ccb),
	    BUS_DMASYNC_POSTWRITE | BUS_DMASYNC_POSTREAD);

	ccb->ccb_req.error = status;

	if ((off & 3) != 0 && ccb->ccb_req.error == 0)
		ccb->ccb_req.error = CAC_RET_CMD_REJECTED;

	return (ccb);
}

static int
cac_eisa_l0_intr_pending(struct cac_softc *sc)
{

	return (cac_inb(sc, CAC_EISAREG_SYSTEM_DOORBELL) &
	    CAC_EISA_CHANNEL_BUSY);
}

static void
cac_eisa_l0_intr_enable(struct cac_softc *sc, int state)
{

	if (state) {
		cac_outb(sc, CAC_EISAREG_SYSTEM_DOORBELL,
		    ~CAC_EISA_CHANNEL_CLEAR);
		cac_outb(sc, CAC_EISAREG_LOCAL_DOORBELL,
		    CAC_EISA_CHANNEL_BUSY);
		cac_outb(sc, CAC_EISAREG_INTR_MASK, CAC_INTR_ENABLE);
		cac_outb(sc, CAC_EISAREG_SYSTEM_MASK, CAC_INTR_ENABLE);
	} else
		cac_outb(sc, CAC_EISAREG_SYSTEM_MASK, CAC_INTR_DISABLE);
}
