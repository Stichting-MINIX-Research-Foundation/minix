/*	$NetBSD: dpt_isa.c,v 1.22 2012/10/27 17:18:24 chs Exp $	*/

/*
 * Copyright (c) 1999, 2000, 2001 Andrew Doran <ad@NetBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * ISA front-end for DPT EATA SCSI driver.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: dpt_isa.c,v 1.22 2012/10/27 17:18:24 chs Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/queue.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#include <dev/isa/isadmareg.h>
#include <dev/isa/isadmavar.h>

#include <dev/ic/dptreg.h>
#include <dev/ic/dptvar.h>

#include <dev/i2o/dptivar.h>

#define	DPT_ISA_IOSIZE		16
#define DPT_ISA_MAXCCBS		16

static void	dpt_isa_attach(device_t, device_t, void *);
static int	dpt_isa_match(device_t, cfdata_t, void *);
static int	dpt_isa_probe(struct isa_attach_args *, int);
static int	dpt_isa_wait(bus_space_handle_t, bus_space_tag_t, u_int8_t,
			     u_int8_t);

CFATTACH_DECL_NEW(dpt_isa, sizeof(struct dpt_softc),
    dpt_isa_match, dpt_isa_attach, NULL, NULL);

/* Try 'less intrusive' addresses first */
static const int	dpt_isa_iobases[] = { 0x230, 0x330, 0x1f0, 0x170, 0 };

/*
 * Wait for the HBA status register to reach a specific state.
 */
static int
dpt_isa_wait(bus_space_handle_t ioh, bus_space_tag_t iot, u_int8_t mask,
	     u_int8_t state)
{
	int ms;

	for (ms = 2000 * 10; ms; ms--) {
		if ((bus_space_read_1(iot, ioh, HA_STATUS) & mask) == state)
			return (0);
		DELAY(100);
	}

	return (-1);
}

/*
 * Match a supported board.
 */
static int
dpt_isa_match(device_t parent, cfdata_t match, void *aux)
{
	struct isa_attach_args *ia = aux;
	int i;

	if (ia->ia_nio < 1)
		return (0);
	if (ia->ia_nirq < 1)
		return (0);
	if (ia->ia_ndrq < 1)
		return (0);

	if (ISA_DIRECT_CONFIG(ia))
		return (0);

	if (ia->ia_io[0].ir_addr != ISA_UNKNOWN_PORT)
		return (dpt_isa_probe(ia, ia->ia_io[0].ir_addr));

	for (i = 0; dpt_isa_iobases[i] != 0; i++) {
		if (dpt_isa_probe(ia, dpt_isa_iobases[i])) {
			ia->ia_io[0].ir_addr = dpt_isa_iobases[i];
			return (1);
		}
	}

	return (0);
}

/*
 * Probe for a supported board.
 */
static int
dpt_isa_probe(struct isa_attach_args *ia, int iobase)
{
	struct eata_cfg ec;
	bus_space_handle_t ioh;
	bus_space_tag_t iot;
	int i, j, stat, irq, drq;
	u_int16_t *p;

	iot = ia->ia_iot;

	if (bus_space_map(iot, iobase, DPT_ISA_IOSIZE, 0, &ioh) != 0)
		return(0);

	/*
	 * Assumuing the DPT BIOS reset the board, we shouldn't need to
	 * re-do it here.  The tests below should weed out non-EATA devices
	 * before we start poking any registers.
	 */
	for (i = 1000; i; i--) {
		if ((bus_space_read_1(iot, ioh, HA_STATUS) & HA_ST_READY) != 0)
			break;
		DELAY(2000);
	}

	if (i == 0)
		goto bad;

	while((((stat = bus_space_read_1(iot, ioh, HA_STATUS))
	    != (HA_ST_READY|HA_ST_SEEK_COMPLETE))
	    && (stat != (HA_ST_READY|HA_ST_SEEK_COMPLETE|HA_ST_ERROR))
	    && (stat != (HA_ST_READY|HA_ST_SEEK_COMPLETE|HA_ST_ERROR|HA_ST_DRQ)))
	    || (dpt_isa_wait(ioh, iot, HA_ST_BUSY, 0)))
		/* RAID drives still spinning up? */
		if (bus_space_read_1(iot, ioh, HA_ERROR) != 'D' ||
		    bus_space_read_1(iot, ioh, HA_ERROR + 1) != 'P' ||
		    bus_space_read_1(iot, ioh, HA_ERROR + 2) != 'T')
			goto bad;

	/*
	 * At this point we can be confident that we are dealing with a DPT
	 * HBA.  Issue the read-config command and wait for the data to
	 * appear.  XXX We shouldn't be doing this with PIO, but it makes it
	 * a lot easier as no DMA setup is required.
	 */
	bus_space_write_1(iot, ioh, HA_COMMAND, CP_PIO_GETCFG);
	memset(&ec, 0, sizeof(ec));
	i = ((uintptr_t)&((struct eata_cfg *)0)->ec_cfglen +
	    sizeof(ec.ec_cfglen)) >> 1;
	p = (u_int16_t *)&ec;

	if (dpt_isa_wait(ioh, iot, 0xFF, HA_ST_DATA_RDY))
		goto bad;

	/* Begin reading */
 	while (i--)
		*p++ = bus_space_read_stream_2(iot, ioh, HA_DATA);

	if ((i = ec.ec_cfglen) > (sizeof(struct eata_cfg)
	    - (uintptr_t)(&(((struct eata_cfg *)0L)->ec_cfglen))
	    - sizeof(ec.ec_cfglen)))
		i = sizeof(struct eata_cfg)
		  - (uintptr_t)(&(((struct eata_cfg *)0L)->ec_cfglen))
		  - sizeof(ec.ec_cfglen);

	j = i + (uintptr_t)(&(((struct eata_cfg *)0L)->ec_cfglen)) +
	    sizeof(ec.ec_cfglen);
	i >>= 1;

	while (i--)
		*p++ = bus_space_read_stream_2(iot, ioh, HA_DATA);

	/* Flush until we have read 512 bytes. */
	i = (512 - j + 1) >> 1;
	while (i--)
 		bus_space_read_stream_2(iot, ioh, HA_DATA);

	/* Puke if we don't like the returned configuration data. */
	if ((bus_space_read_1(iot, ioh,  HA_STATUS) & HA_ST_ERROR) != 0 ||
	    memcmp(ec.ec_eatasig, "EATA", 4) != 0 ||
	    (ec.ec_feat0 & (EC_F0_HBA_VALID | EC_F0_DMA_SUPPORTED)) !=
	    (EC_F0_HBA_VALID | EC_F0_DMA_SUPPORTED))
	    	goto bad;

	/*
	 * Which DMA channel to use: if it was hardwired in the kernel
	 * configuration, use that value.  If the HBA told us, use that
	 * value.  Otherwise, puke.
	 */
	if ((drq = ia->ia_drq[0].ir_drq) == ISA_UNKNOWN_DRQ) {
		int dmanum = ((ec.ec_feat1 & EC_F1_DMA_NUM_MASK) >>
		    EC_F1_DMA_NUM_SHIFT);

		if ((ec.ec_feat0 & EC_F0_DMA_NUM_VALID) == 0 || dmanum > 3)
			goto bad;
		drq = "\0\7\6\5"[dmanum];
	}

	/*
	 * Which IRQ to use: if it was hardwired in the kernel configuration,
	 * use that value.  Otherwise, use what the HBA told us.
	 */
	if ((irq = ia->ia_irq[0].ir_irq) == ISA_UNKNOWN_IRQ)
		irq = ((ec.ec_feat1 & EC_F1_IRQ_NUM_MASK) >>
		    EC_F1_IRQ_NUM_SHIFT);

	ia->ia_nio = 1;
	ia->ia_io[0].ir_size = DPT_ISA_IOSIZE;

	ia->ia_nirq = 1;
	ia->ia_irq[0].ir_irq = irq;

	ia->ia_ndrq = 1;
	ia->ia_drq[0].ir_drq = drq;

	ia->ia_niomem = 0;

	bus_space_unmap(iot, ioh, DPT_ISA_IOSIZE);
	return (1);
 bad:
	bus_space_unmap(iot, ioh, DPT_ISA_IOSIZE);
	return (0);
}

/*
 * Attach a matched board.
 */
static void
dpt_isa_attach(device_t parent, device_t self, void *aux)
{
	struct isa_attach_args *ia;
	isa_chipset_tag_t ic;
	bus_space_handle_t ioh;
	bus_space_tag_t iot;
	struct dpt_softc *sc;
	struct eata_cfg *ec;
	int error;

	ia = aux;
	sc = device_private(self);
	sc->sc_dev = self;
	iot = ia->ia_iot;
	ic = ia->ia_ic;

	printf(": ");

	if ((error = bus_space_map(iot, ia->ia_io[0].ir_addr, DPT_ISA_IOSIZE,
	     0, &ioh)) != 0) {
		printf("can't map i/o space, error = %d\n", error);
		return;
	}

	sc->sc_iot = iot;
	sc->sc_ioh = ioh;
	sc->sc_dmat = ia->ia_dmat;

	if ((error = isa_dmacascade(ic, ia->ia_drq[0].ir_drq)) != 0) {
		printf("unable to cascade DRQ, error = %d\n", error);
		return;
	}

	/* Establish the interrupt. */
	sc->sc_ih = isa_intr_establish(ic, ia->ia_irq[0].ir_irq, IST_EDGE,
	    IPL_BIO, dpt_intr, sc);
	if (sc->sc_ih == NULL) {
		printf("can't establish interrupt\n");
		return;
	}

	if (dpt_readcfg(sc)) {
		printf("readcfg failed - see dpt(4)\n");
		return;
	}

	/*
	 * Now attach to the bus-independent code.  XXX We need to force
	 * parameters that aren't filled in by some ISA boards.  In
	 * particular, due to the limited amount of memory we have to play
	 * with for DMA, clamp the number of CCBs to 16.
	 */
	ec = &sc->sc_ec;

	if (be16toh(*(int16_t *)ec->ec_queuedepth) > DPT_ISA_MAXCCBS)
		*(int16_t *)ec->ec_queuedepth = htobe16(DPT_ISA_MAXCCBS);
	if (ec->ec_maxlun == 0)
		ec->ec_maxlun = 7;
	if ((ec->ec_feat3 & EC_F3_MAX_TARGET_MASK) >> EC_F3_MAX_TARGET_SHIFT
	    == 0)
		ec->ec_feat3 = (ec->ec_feat3 & ~EC_F3_MAX_TARGET_MASK) |
		    (7 << EC_F3_MAX_TARGET_SHIFT);

	sc->sc_bustype = SI_ISA_BUS;
	sc->sc_isaport = ia->ia_io[0].ir_addr;
	sc->sc_isairq = ia->ia_irq[0].ir_irq;
	sc->sc_isadrq = ia->ia_drq[0].ir_drq;

	dpt_init(sc, NULL);
}
