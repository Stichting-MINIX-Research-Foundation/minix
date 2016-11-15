/* $NetBSD: sec.c,v 1.16 2014/01/21 19:50:16 christos Exp $ */

/*-
 * Copyright (c) 2000, 2001, 2006 Ben Harris
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * sec.c -- driver for Acorn SCSI expansion cards (AKA30, AKA31, AKA32)
 *
 * These cards are documented in:
 * Acorn Archimedes 500 series / Acorn R200 series Technical Reference Manual
 * Published by Acorn Computers Limited
 * ISBN 1 85250 086 7
 * Part number 0486,052
 * Issue 1, November 1990
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sec.c,v 1.16 2014/01/21 19:50:16 christos Exp $");

#include <sys/param.h>

#include <sys/buf.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/reboot.h>	/* For bootverbose */
#include <sys/syslog.h>
#include <sys/systm.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

#include <sys/bus.h>

#include <dev/ic/wd33c93reg.h>
#include <dev/ic/wd33c93var.h>
#include <dev/ic/nec71071reg.h>

#include <dev/podulebus/podulebus.h>
#include <dev/podulebus/podules.h>
#include <dev/podulebus/powerromreg.h>
#include <dev/podulebus/secreg.h>

#include "opt_ddb.h"

struct sec_softc {
	struct	wd33c93_softc sc_sbic;
	bus_space_tag_t		sc_pod_t;
	bus_space_handle_t	sc_pod_h;
	bus_space_tag_t		sc_mod_t;
	bus_space_handle_t	sc_mod_h;
	void			*sc_ih;
	struct		evcnt	sc_intrcnt;
	uint8_t			sc_mpr;

	/* Details of the current DMA transfer */
	bool			sc_dmaactive;
	void *			sc_dmaaddr;
	int			sc_dmaoff;
	size_t			sc_dmalen;
	bool			sc_dmain;
	/* Details of the current block within the above transfer */
	size_t			sc_dmablk;
};

#define SEC_DMABLK	16384
#define SEC_NBLKS	3
#define SEC_DMAMODE	MODE_TMODE_DMD

/* autoconfiguration glue */
static int sec_match(device_t, cfdata_t, void *);
static void sec_attach(device_t, device_t, void *);

/* shutdown hook */
static bool sec_shutdown(device_t, int);

/* callbacks from MI WD33C93 driver */
static int sec_dmasetup(struct wd33c93_softc *, void **, size_t *, int,
    size_t *);
static int sec_dmago(struct wd33c93_softc *);
static void sec_dmastop(struct wd33c93_softc *);
static void sec_reset(struct wd33c93_softc *);

static int sec_intr(void *);
static int sec_dmatc(struct sec_softc *sc);

void sec_dumpdma(void *arg);

CFATTACH_DECL_NEW(sec, sizeof(struct sec_softc),
    sec_match, sec_attach, NULL, NULL);

static inline void
sec_setpage(struct sec_softc *sc, int page)
{

	sc->sc_mpr = (sc->sc_mpr & ~SEC_MPR_PAGE) | page;
	bus_space_write_1(sc->sc_pod_t, sc->sc_pod_h, SEC_MPR, sc->sc_mpr);
}

static inline void
sec_cli(struct sec_softc *sc)
{

	bus_space_write_1(sc->sc_pod_t, sc->sc_pod_h, SEC_CLRINT, 0);
}

static inline void
dmac_write(struct sec_softc *sc, int reg, uint8_t val)
{

	bus_space_write_1(sc->sc_mod_t, sc->sc_mod_h,
	    SEC_DMAC + DMAC(reg), val);
}

static inline uint8_t
dmac_read(struct sec_softc *sc, int reg)
{

	return bus_space_read_1(sc->sc_mod_t, sc->sc_mod_h,
	    SEC_DMAC + DMAC(reg));
}

static int
sec_match(device_t parent, cfdata_t cf, void *aux)
{
	struct podulebus_attach_args *pa = aux;

	/* Standard ROM, skipping the MCS card that used the same ID. */
	if (pa->pa_product == PODULE_ACORN_SCSI &&
	    strncmp(pa->pa_descr, "MCS", 3) != 0)
		return 1;

	/* PowerROM */
        if (pa->pa_product == PODULE_ALSYSTEMS_SCSI &&
            podulebus_initloader(pa) == 0 &&
            podloader_callloader(pa, 0, 0) == PRID_ACORN_SCSI1)
                return 1;

	return 0;
}

static void
sec_attach(device_t parent, device_t self, void *aux)
{
	struct podulebus_attach_args *pa = aux;
	struct sec_softc *sc = device_private(self);
	int i;

	sc->sc_sbic.sc_dev = self;
	/* Set up bus spaces */
	sc->sc_pod_t = pa->pa_fast_t;
	bus_space_map(pa->pa_fast_t, pa->pa_fast_base, 0x1000, 0,
	    &sc->sc_pod_h);
	sc->sc_mod_t = pa->pa_mod_t;
	bus_space_map(pa->pa_mod_t, pa->pa_mod_base, 0x1000, 0,
	    &sc->sc_mod_h);

	sc->sc_sbic.sc_regt = sc->sc_mod_t;
	bus_space_subregion(sc->sc_mod_t, sc->sc_mod_h, SEC_SBIC + 0, 1,
	    &sc->sc_sbic.sc_asr_regh);
	bus_space_subregion(sc->sc_mod_t, sc->sc_mod_h, SEC_SBIC + 1, 1,
	    &sc->sc_sbic.sc_data_regh);

	sc->sc_sbic.sc_id = 7;
	sc->sc_sbic.sc_clkfreq = SEC_CLKFREQ;
	sc->sc_sbic.sc_dmamode = SBIC_CTL_BURST_DMA;

	sc->sc_sbic.sc_adapter.adapt_request = wd33c93_scsi_request;
	sc->sc_sbic.sc_adapter.adapt_minphys = minphys;

	sc->sc_sbic.sc_dmasetup = sec_dmasetup;
	sc->sc_sbic.sc_dmago = sec_dmago;
	sc->sc_sbic.sc_dmastop = sec_dmastop;
	sc->sc_sbic.sc_reset = sec_reset;

	sc->sc_mpr = 0;
	bus_space_write_1(sc->sc_pod_t, sc->sc_pod_h, SEC_MPR, sc->sc_mpr);

	for (i = 0; i < SEC_NPAGES; i++) {
		sec_setpage(sc, i);
		bus_space_set_region_2(sc->sc_mod_t, sc->sc_mod_h,
				       SEC_SRAM, 0, SEC_PAGESIZE / 2);
	}

	wd33c93_attach(&sc->sc_sbic);

	evcnt_attach_dynamic(&sc->sc_intrcnt, EVCNT_TYPE_INTR, NULL,
	    device_xname(self), "intr");
	sc->sc_ih = podulebus_irq_establish(pa->pa_ih, IPL_BIO, sec_intr,
	    sc, &sc->sc_intrcnt);
	sec_cli(sc);
	sc->sc_mpr |= SEC_MPR_IE;
	bus_space_write_1(sc->sc_pod_t, sc->sc_pod_h, SEC_MPR, sc->sc_mpr);
	pmf_device_register1(sc->sc_sbic.sc_dev, NULL, NULL, sec_shutdown);
}

/*
 * Before reboot, reset the page register to 0 so that RISC OS can see
 * the podule ROM.
 */
static bool
sec_shutdown(device_t dev, int howto)
{
	struct sec_softc *sc = device_private(dev);

	sec_setpage(sc, 0);
	return true;
}

static void
sec_copyin(struct sec_softc *sc, void *dest, int src, size_t size)
{
	uint16_t tmp, *wptr;
	int cnt, extra_byte;

	KASSERT(src >= 0);
	KASSERT(src + size <= SEC_MEMSIZE);
	if (src % 2 != 0) {
		/*
		 * There's a stray byte at the start.  Read the word
		 * containing it.
		 */
		sec_setpage(sc, src / SEC_PAGESIZE);
		tmp = bus_space_read_2(sc->sc_mod_t, sc->sc_mod_h,
		    SEC_SRAM + (src % SEC_PAGESIZE / 2));
		*(uint8_t *)dest = tmp >> 8;
		dest = ((uint8_t *)dest) + 1;
		src++; size--;
	}
	KASSERT(src % 2 == 0);
	KASSERT(ALIGNED_POINTER(dest, uint16_t));
	wptr = dest;
	extra_byte = size % 2;
	size -= extra_byte;
	while (size > 0) {
		cnt = SEC_PAGESIZE - src % SEC_PAGESIZE;
		if (cnt > size)
			cnt = size;
		sec_setpage(sc, src / SEC_PAGESIZE);
		/* bus ops are in words */
		bus_space_read_region_2(sc->sc_mod_t, sc->sc_mod_h,
		    SEC_SRAM + src % SEC_PAGESIZE / 2, wptr, cnt / 2);
		src += cnt;
		wptr += cnt / 2;
		size -= cnt;
	}
	if (extra_byte) {
		sec_setpage(sc, src / SEC_PAGESIZE);
		*(u_int8_t *)wptr =
		    bus_space_read_2(sc->sc_mod_t, sc->sc_mod_h,
		    SEC_SRAM + src % SEC_PAGESIZE / 2) & 0xff;
	}
}	

static void
sec_copyout(struct sec_softc *sc, const void *src, int dest, size_t size)
{
	int cnt, extra_byte;
	const uint16_t *wptr;
	uint16_t tmp;

	KASSERT(dest >= 0);
	KASSERT(dest + size <= SEC_MEMSIZE);
	if (dest % 2 != 0) {
		/*
		 * There's a stray byte at the start.  Read the word
		 * containing it.
		 */
		sec_setpage(sc, dest / SEC_PAGESIZE);
		tmp = bus_space_read_2(sc->sc_mod_t, sc->sc_mod_h,
		    SEC_SRAM + (dest % SEC_PAGESIZE / 2));
		tmp &= 0xff;
		tmp |= *(uint8_t const *)src << 8;
		bus_space_write_2(sc->sc_mod_t, sc->sc_mod_h,
		    SEC_SRAM + (dest % SEC_PAGESIZE / 2), tmp);
		src = ((uint8_t const *)src) + 1;
		dest++; size--;
	}
	KASSERT(dest % 2 == 0);
	KASSERT(ALIGNED_POINTER(src, uint16_t));
	wptr = src;
	extra_byte = size % 2;
	size -= extra_byte;
	while (size > 0) {
		cnt = SEC_PAGESIZE - dest % SEC_PAGESIZE;
		if (cnt > size)
			cnt = size;
		sec_setpage(sc, dest / SEC_PAGESIZE);
		/* bus ops are in words */
		bus_space_write_region_2(sc->sc_mod_t, sc->sc_mod_h,
		    dest % SEC_PAGESIZE / 2, wptr, cnt / 2);
		wptr += cnt / 2;
		dest += cnt;
		size -= cnt;
	}
	if (extra_byte) {
		/*
		 * There's a stray byte at the end.  Read the word
		 * containing it.
		 */
		sec_setpage(sc, dest / SEC_PAGESIZE);
		tmp = bus_space_read_2(sc->sc_mod_t, sc->sc_mod_h,
		    SEC_SRAM + (dest % SEC_PAGESIZE / 2));
		tmp &= 0xff00;
		tmp |= *(uint8_t const *)wptr;
		bus_space_write_2(sc->sc_mod_t, sc->sc_mod_h,
		    SEC_SRAM + (dest % SEC_PAGESIZE / 2), tmp);
	}
}

static void
sec_dmablk(struct sec_softc *sc, int blk)
{
	int off;
	size_t len;

	KASSERT(blk >= 0);
	KASSERT(blk * SEC_DMABLK < sc->sc_dmalen);
	off = (blk % SEC_NBLKS) * SEC_DMABLK + sc->sc_dmaoff;
	len = MIN(SEC_DMABLK, sc->sc_dmalen - (blk * SEC_DMABLK));
	dmac_write(sc, NEC71071_ADDRLO, off & 0xff);
	dmac_write(sc, NEC71071_ADDRMID, off >> 8);
	dmac_write(sc, NEC71071_ADDRHI, 0);
	/*
	 * "Note: The number of DMA transfer cycles is actually the
	 * value of the current count register + 1.  Therefore, when
	 * programming the count register, specify the number of DMA
	 * transfers minus one." -- uPD71071 datasheet
	 */
	dmac_write(sc, NEC71071_COUNTLO, (len - 1) & 0xff);
	dmac_write(sc, NEC71071_COUNTHI, (len - 1) >> 8);
}

static void
sec_copyoutblk(struct sec_softc *sc, int blk)
{
	int off;
	size_t len;

	KASSERT(blk >= 0);
	KASSERT(blk * SEC_DMABLK < sc->sc_dmalen);
	KASSERT(!sc->sc_dmain);
	off = (blk % SEC_NBLKS) * SEC_DMABLK + sc->sc_dmaoff;
	len = MIN(SEC_DMABLK, sc->sc_dmalen - (blk * SEC_DMABLK));
	sec_copyout(sc, (char*)sc->sc_dmaaddr + (blk * SEC_DMABLK), off, len);
}

static void
sec_copyinblk(struct sec_softc *sc, int blk)
{
	int off;
	size_t len;

	KASSERT(blk >= 0);
	KASSERT(blk * SEC_DMABLK < sc->sc_dmalen);
	KASSERT(sc->sc_dmain);
	off = (blk % SEC_NBLKS) * SEC_DMABLK + sc->sc_dmaoff;
	len = MIN(SEC_DMABLK, sc->sc_dmalen - (blk * SEC_DMABLK));
	sec_copyin(sc, (char*)sc->sc_dmaaddr + (blk * SEC_DMABLK), off, len);
}

static int
sec_dmasetup(struct wd33c93_softc *sc_sbic, void **addr, size_t *len,
    int datain, size_t *dmasize)
{
	struct sec_softc *sc = (struct sec_softc *)sc_sbic;
	uint8_t mode;

	sc->sc_dmaaddr = *addr;
	sc->sc_dmaoff = ALIGNED_POINTER(*addr, uint16_t) ? 0 : 1;
	sc->sc_dmalen = *len;
	sc->sc_dmain = datain;
	sc->sc_dmablk = 0;
	mode = SEC_DMAMODE | (datain ? MODE_TDIR_IOTM : MODE_TDIR_MTIO);
	/* Program first block into DMAC and queue up second. */
	dmac_write(sc, NEC71071_CHANNEL, 0);
	if (!sc->sc_dmain)
		sec_copyoutblk(sc, 0);
	sec_dmablk(sc, 0);
	/* Mode control register */
	dmac_write(sc, NEC71071_MODE, mode);
	return sc->sc_dmalen;
}

static int
sec_dmago(struct wd33c93_softc *sc_sbic)
{
	struct sec_softc *sc = (struct sec_softc *)sc_sbic;

	dmac_write(sc, NEC71071_MASK, 0xe);
	sc->sc_dmaactive = true;
	if (!sc->sc_dmain && sc->sc_dmalen > SEC_DMABLK)
		sec_copyoutblk(sc, 1);
	return sc->sc_dmalen;
}

static void
sec_dmastop(struct wd33c93_softc *sc_sbic)
{
	struct sec_softc *sc = (struct sec_softc *)sc_sbic;

	dmac_write(sc, NEC71071_MASK, 0xf);
	if (sc->sc_dmaactive && sc->sc_dmain)
		sec_copyinblk(sc, sc->sc_dmablk);
	sc->sc_dmaactive = false;
}

/*
 * Reset the SCSI bus, and incidentally the SBIC and DMAC.
 */
static void
sec_reset(struct wd33c93_softc *sc_sbic)
{
	struct sec_softc *sc = (struct sec_softc *)sc_sbic;
	uint8_t asr, csr;

	bus_space_write_1(sc->sc_pod_t, sc->sc_pod_h, SEC_MPR, 
	    sc->sc_mpr | SEC_MPR_UR);
	DELAY(7);
	bus_space_write_1(sc->sc_pod_t, sc->sc_pod_h, SEC_MPR, sc->sc_mpr);
	/* Wait for and clear the reset-complete interrupt */
	do
		GET_SBIC_asr(sc_sbic, asr);
	while (!(asr & SBIC_ASR_INT));
	GET_SBIC_csr(sc_sbic, csr);
	__USE(csr);
	dmac_write(sc, NEC71071_DCTRL1, DCTRL1_CMP | DCTRL1_RQL);
	dmac_write(sc, NEC71071_DCTRL2, 0);
	sec_cli(sc);
}

static int
sec_intr(void *arg)
{
	struct sec_softc *sc = arg;
	u_int8_t isr;

	isr = bus_space_read_1(sc->sc_pod_t, sc->sc_pod_h, SEC_ISR);
	if (!(isr & SEC_ISR_IRQ))
		return 0;
	if (isr & SEC_ISR_DMAC)
		sec_dmatc(sc);
	if (isr & SEC_ISR_SBIC)
		wd33c93_intr(&sc->sc_sbic);
	return 1;
}

static int
sec_dmatc(struct sec_softc *sc)
{

	sec_cli(sc);
	/* DMAC finished block n-1 and is now working on block n */
	sc->sc_dmablk++;
	if (sc->sc_dmalen > sc->sc_dmablk * SEC_DMABLK) {
		dmac_write(sc, NEC71071_CHANNEL, 0);
		sec_dmablk(sc, sc->sc_dmablk);
		dmac_write(sc, NEC71071_MASK, 0xe);
		if (!sc->sc_dmain &&
		    sc->sc_dmalen > (sc->sc_dmablk + 1) * SEC_DMABLK)
			sec_copyoutblk(sc, sc->sc_dmablk + 1);
	} else {
		/* All blocks fully processed. */
		sc->sc_dmaactive = false;
	}
	if (sc->sc_dmain)
		sec_copyinblk(sc, sc->sc_dmablk - 1);
	return 1;
}

#ifdef DDB
void
sec_dumpdma(void *arg)
{
	struct sec_softc *sc = arg;

	dmac_write(sc, NEC71071_CHANNEL, 0);
	printf("%s: DMA state: cur count %02x%02x cur addr %02x%02x%02x ",
	    device_xname(sc->sc_sbic.sc_dev),
	    dmac_read(sc, NEC71071_COUNTHI), dmac_read(sc, NEC71071_COUNTLO),
	    dmac_read(sc, NEC71071_ADDRHI), dmac_read(sc, NEC71071_ADDRMID),
	    dmac_read(sc, NEC71071_ADDRLO));
	dmac_write(sc, NEC71071_CHANNEL, 0 | CHANNEL_WBASE);
	printf("base count %02x%02x base addr %02x%02x%02x\n",
	    dmac_read(sc, NEC71071_COUNTHI), dmac_read(sc, NEC71071_COUNTLO),
	    dmac_read(sc, NEC71071_ADDRHI), dmac_read(sc, NEC71071_ADDRMID),
	    dmac_read(sc, NEC71071_ADDRLO));
	printf("%s: DMA state: dctrl %1x%02x mode %02x status %02x req %02x "
	    "mask %02x\n",
	    device_xname(sc->sc_sbic.sc_dev), dmac_read(sc, NEC71071_DCTRL2),
	    dmac_read(sc, NEC71071_DCTRL1), dmac_read(sc, NEC71071_MODE),
	    dmac_read(sc, NEC71071_STATUS), dmac_read(sc, NEC71071_REQUEST),
	    dmac_read(sc, NEC71071_MASK));
	printf("%s: soft DMA state: %zd@%p%s%d\n",
	    device_xname(sc->sc_sbic.sc_dev),
	    sc->sc_dmalen, sc->sc_dmaaddr, sc->sc_dmain ? "<-" : "->",
	    sc->sc_dmaoff);
}

void sec_dumpall(void); /* Call from DDB */

extern struct cfdriver sec_cd;

void sec_dumpall(void)
{
	int i;
	struct sec_softc *sc;

	for (i = 0; i < sec_cd.cd_ndevs; ++i) {
		sc = device_lookup_private(&sec_cd, i);
		if (sc != NULL)
			sec_dumpdma(sc);
	}
}
#endif
