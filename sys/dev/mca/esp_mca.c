/*	$NetBSD: esp_mca.c,v 1.21 2009/11/23 02:13:47 rmind Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jaromir Dolecek <jdolecek@NetBSD.org>.
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
 * Driver for NCR 53c90, MCA version, with 86c01 DMA controller chip.
 *
 * Some of the information used to write this driver was taken
 * from Tymm Twillman <tymm@computer.org>'s Linux MCA NC53c90 driver,
 * in drivers/scsi/mca_53c9x.c
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: esp_mca.c,v 1.21 2009/11/23 02:13:47 rmind Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/queue.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>
#include <dev/scsipi/scsi_message.h>

#include <sys/bus.h>
#include <sys/cpu.h>

#include <dev/ic/ncr53c9xreg.h>
#include <dev/ic/ncr53c9xvar.h>

#include <dev/mca/espvar.h>
#include <dev/mca/espreg.h>

#include <dev/mca/mcavar.h>
#include <dev/mca/mcareg.h>
#include <dev/mca/mcadevs.h>

#if 0
#if defined(DEBUG) && !defined(NCR53C9X_DEBUG)
#define NCR53C9X_DEBUG
#endif
#endif

#ifdef NCR53C9X_DEBUG
static int esp_mca_debug = 0;
#define DPRINTF(x) if (esp_mca_debug) printf x;
#else
#define DPRINTF(x)
#endif

#define ESP_MCA_IOSIZE  0x20
#define ESP_REG_OFFSET	0x10

static int	esp_mca_match(device_t, cfdata_t, void *);
static void	esp_mca_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(esp_mca, sizeof(struct esp_softc),
    esp_mca_match, esp_mca_attach, NULL, NULL);

/*
 * Functions and the switch for the MI code.
 */
static uint8_t	esp_read_reg(struct ncr53c9x_softc *, int);
static void	esp_write_reg(struct ncr53c9x_softc *, int, uint8_t);
static int	esp_dma_isintr(struct ncr53c9x_softc *);
static void	esp_dma_reset(struct ncr53c9x_softc *);
static int	esp_dma_intr(struct ncr53c9x_softc *);
static int	esp_dma_setup(struct ncr53c9x_softc *, uint8_t **,
	    size_t *, int, size_t *);
static void	esp_dma_go(struct ncr53c9x_softc *);
static void	esp_dma_stop(struct ncr53c9x_softc *);
static int	esp_dma_isactive(struct ncr53c9x_softc *);

static struct ncr53c9x_glue esp_glue = {
	esp_read_reg,
	esp_write_reg,
	esp_dma_isintr,
	esp_dma_reset,
	esp_dma_intr,
	esp_dma_setup,
	esp_dma_go,
	esp_dma_stop,
	esp_dma_isactive,
	NULL,			/* gl_clear_latched_intr */
};

static int
esp_mca_match(device_t parent, cfdata_t cf, void *aux)
{
	struct mca_attach_args *ma = aux;

	switch (ma->ma_id) {
	case MCA_PRODUCT_NCR53C90:
		return 1;
	}

	return 0;
}

static void
esp_mca_attach(device_t parent, device_t self, void *aux)
{
	struct esp_softc *esc = device_private(self);
	struct ncr53c9x_softc *sc = &esc->sc_ncr53c9x;
	struct mca_attach_args *ma = aux;
	uint16_t iobase;
	int scsi_id, irq, drq, error;
	bus_space_handle_t ioh;
	int pos2, pos3, pos5;

	static const uint16_t ncrmca_iobase[] = {
		0, 0x240, 0x340, 0x400, 0x420, 0x3240, 0x8240, 0xa240
	};

	sc->sc_dev = self;

	/*
	 * NCR SCSI Adapter (ADF 7f4f)
	 *
	 * POS register 2: (adf pos0)
	 *
	 * 7 6 5 4 3 2 1 0
	 *     \_/ \___/ \__ enable: 0=adapter disabled, 1=adapter enabled
	 *      |      \____ I/O base (32B): 001=0x240 010=0x340 011=0x400
	 *      |              100=0x420 101=0x3240 110=0x8240 111=0xa240
	 *       \__________ IRQ: 00=3 01=5 10=7 11=9
	 *
	 * POS register 3: (adf pos1)
	 *
	 * 7 6 5 4 3 2 1 0
	 * 1 1 1 | \_____/
	 *       |       \__ DMA level
	 *        \_________ Fairness: 1=enabled 0=disabled
	 *
	 * POS register 5: (adf pos3)
	 *
	 * 7 6 5 4 3 2 1 0
	 * 1   |     \___/
	 *     |         \__ Static Ram: 0xC8000-0xC87FF + XX*0x4000
	 *      \___________ Host Adapter ID: 1=7 0=6
	 */

	pos2 = mca_conf_read(ma->ma_mc, ma->ma_slot, 2);
	pos3 = mca_conf_read(ma->ma_mc, ma->ma_slot, 3);
	pos5 = mca_conf_read(ma->ma_mc, ma->ma_slot, 5);

	iobase = ncrmca_iobase[(pos2 & 0x0e) >> 1];
	irq = 3 + 2 * ((pos2 & 0x30) >> 4);
	drq = (pos3 & 0x0f);
	scsi_id = 6 + ((pos5 & 0x20) ? 1 : 0);

	aprint_normal(" slot %d irq %d drq %d: NCR SCSI Adapter\n",
	    ma->ma_slot + 1, irq, drq);

	/* Map the 86C01 registers */
	if (bus_space_map(ma->ma_iot, iobase, ESP_MCA_IOSIZE, 0, &ioh)) {
		aprint_error_dev(sc->sc_dev, "can't map i/o space\n");
		return;
	}

	esc->sc_iot = ma->ma_iot;
	esc->sc_ioh = ioh;

	/* Submap the 'esp' registers */
	if (bus_space_subregion(ma->ma_iot, ioh, ESP_REG_OFFSET,
	    ESP_MCA_IOSIZE-ESP_REG_OFFSET, &esc->sc_esp_ioh)) {
		aprint_error_dev(sc->sc_dev, "can't subregion i/o space\n");
		return;
	}

	/* Setup DMA map */
	esc->sc_dmat = ma->ma_dmat;
	if ((error = mca_dmamap_create(esc->sc_dmat, MAXPHYS,
            BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW | MCABUS_DMA_IOPORT,
	    &esc->sc_xfer, drq)) != 0){
                aprint_error_dev(sc->sc_dev,
		    "couldn't create DMA map - error %d\n", error);
                return;
        }

	/* MI code glue */
	sc->sc_id = scsi_id;
	sc->sc_freq = 25;		/* MHz */

	sc->sc_glue = &esp_glue;

	sc->sc_cfg1 = sc->sc_id | NCRCFG1_PARENB; //| NCRCFG1_SLOW;
	/* No point setting sc_cfg[2345], they won't be used */

	sc->sc_rev = NCR_VARIANT_NCR53C90_86C01;
	sc->sc_minsync = 0;

	/* max 64KB DMA */
	sc->sc_maxxfer = 64 * 1024;

	/* Establish interrupt */
	esc->sc_ih = mca_intr_establish(ma->ma_mc, irq, IPL_BIO, ncr53c9x_intr,
	    esc);
	if (esc->sc_ih == NULL) {
		aprint_error_dev(sc->sc_dev, "couldn't establish interrupt\n");
		return;
	}

	/*
	 * Massage the 86C01 chip - setup MCA DMA controller for DMA via
	 * the 86C01 register, and enable 86C01 interrupts.
	 */
	mca_dma_set_ioport(drq, iobase + N86C01_PIO);

	bus_space_write_1(esc->sc_iot, esc->sc_ioh, N86C01_MODE_ENABLE,
	    bus_space_read_1(esc->sc_iot, esc->sc_ioh, N86C01_MODE_ENABLE) |
	    N86C01_INTR_ENABLE);

	/*
	 * Now try to attach all the sub-devices
	 */
	sc->sc_adapter.adapt_minphys = minphys;
	sc->sc_adapter.adapt_request = ncr53c9x_scsipi_request;

	/* Do the common parts of attachment. */
	printf("%s", device_xname(self));
	ncr53c9x_attach(sc);
}

/*
 * Glue functions.
 */

static uint8_t
esp_read_reg(struct ncr53c9x_softc *sc, int reg)
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	return bus_space_read_1(esc->sc_iot, esc->sc_esp_ioh, reg);
}

static void
esp_write_reg(struct ncr53c9x_softc *sc, int reg, uint8_t val)
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	bus_space_write_1(esc->sc_iot, esc->sc_esp_ioh, reg, val);
}

static int
esp_dma_isintr(struct ncr53c9x_softc *sc)
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	DPRINTF(("[esp_dma_isintr] "));
	return bus_space_read_1(esc->sc_iot, esc->sc_ioh, N86C01_STATUS) &
	    N86C01_IRQ_PEND;
}

static void
esp_dma_reset(struct ncr53c9x_softc *sc)
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	DPRINTF(("[esp_dma_reset] "));

	if (esc->sc_flags & ESP_XFER_LOADED) {
		bus_dmamap_unload(esc->sc_dmat, esc->sc_xfer);
		esc->sc_flags &= ~ESP_XFER_LOADED;
	}

	if (esc->sc_flags & ESP_XFER_ACTIVE) {
		esc->sc_flags &= ~ESP_XFER_ACTIVE;
		mca_disk_unbusy();
	}
}

static int
esp_dma_intr(struct ncr53c9x_softc *sc)
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	DPRINTF(("[esp_dma_intr] "));

	if ((esc->sc_flags & ESP_XFER_ACTIVE) == 0) {
		printf("%s: dma_intr--inactive DMA\n",
		    device_xname(sc->sc_dev));
		return -1;
	}

	if ((sc->sc_espintr & NCRINTR_BS) == 0) {
		esc->sc_flags &= ~ESP_XFER_ACTIVE;
		mca_disk_unbusy();
		return 0;
	}

	sc->sc_espstat |= NCRSTAT_TC;	/* XXX */

	if ((sc->sc_espstat & NCRSTAT_TC) == 0) {
		printf("%s: DMA not complete?\n", device_xname(sc->sc_dev));
		return 1;
	}

	bus_dmamap_sync(esc->sc_dmat, esc->sc_xfer, 0, *esc->sc_xfer_len,
	    (esc->sc_flags & ESP_XFER_READ) ?
	    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);

	bus_dmamap_unload(esc->sc_dmat, esc->sc_xfer);
	esc->sc_flags &= ~ESP_XFER_LOADED;

	*esc->sc_xfer_addr += *esc->sc_xfer_len;
	*esc->sc_xfer_len = 0;

	esc->sc_flags &= ~ESP_XFER_ACTIVE;
	mca_disk_unbusy();

	return 0;
}

/*
 * Setup DMA transfer.
 */
static int
esp_dma_setup(struct ncr53c9x_softc *sc, uint8_t **addr, size_t *len,
    int datain, size_t *dmasize)
{
	struct esp_softc *esc = (struct esp_softc *)sc;
	int error;
	int fl;

	DPRINTF(("[esp_dma_setup] "));

	if (esc->sc_flags & ESP_XFER_LOADED) {
		printf("%s: %s: unloading leaked xfer\n",
		    device_xname(sc->sc_dev), __func__);
		bus_dmamap_unload(esc->sc_dmat, esc->sc_xfer);
		esc->sc_flags &= ~ESP_XFER_LOADED;
	}

	/* Load the buffer for DMA transfer. */
	fl = (datain) ? BUS_DMA_READ : BUS_DMA_WRITE;

	if ((error = bus_dmamap_load(esc->sc_dmat, esc->sc_xfer, *addr,
	    *len, NULL, BUS_DMA_STREAMING|fl))) {
		printf("%s: %s: unable to load DMA buffer - error %d\n",
		    device_xname(sc->sc_dev), __func__, error);
		return error;
	}

	bus_dmamap_sync(esc->sc_dmat, esc->sc_xfer, 0, *len,
	    (datain) ? BUS_DMASYNC_PREREAD : BUS_DMASYNC_PREWRITE);

	esc->sc_flags |= ESP_XFER_LOADED | (datain ? ESP_XFER_READ : 0);
	esc->sc_xfer_addr = addr;
	esc->sc_xfer_len  = len;

	return 0;
}

static void
esp_dma_go(struct ncr53c9x_softc *sc)
{
	struct esp_softc *esc = (struct esp_softc *)sc;
	DPRINTF(("[esp_dma_go] "));

	esc->sc_flags |= ESP_XFER_ACTIVE;
	mca_disk_busy();
}

static void
esp_dma_stop(struct ncr53c9x_softc *sc)
{

	DPRINTF(("[esp_dma_stop] "));

	panic("%s: stop not yet implemented", device_xname(sc->sc_dev));
}

static int
esp_dma_isactive(struct ncr53c9x_softc *sc)
{
	struct esp_softc *esc = (struct esp_softc *)sc;
	DPRINTF(("[esp_dma_isactive] "));

	return esc->sc_flags & ESP_XFER_ACTIVE;
}
