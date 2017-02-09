/*	$NetBSD: esp_podule.c,v 1.1 2013/07/11 13:44:50 kiyohara Exp $	*/
/*
 * Copyright (c) 2013 KIYOHARA Takashi
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
__KERNEL_RCSID(0, "$NetBSD: esp_podule.c,v 1.1 2013/07/11 13:44:50 kiyohara Exp $");

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/device.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

#include <dev/ic/ncr53c9xreg.h>
#include <dev/ic/ncr53c9xvar.h>

#include <dev/podulebus/podulebus.h>
#include <dev/podulebus/podules.h>

struct esp_podule_softc {
	struct ncr53c9x_softc sc_ncr53c9x;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;

	struct ncr53c9x_glue sc_esp_glue;

	int sc_active;			/* Pseudo-DMA state vars */
	int sc_tc;
	int sc_datain;
	size_t sc_dmasize;
	size_t sc_dmatrans;
	uint8_t **sc_dmaaddr;
	size_t *sc_dmalen;
};

static int esp_podule_match(device_t, cfdata_t, void *);
static void esp_podule_attach(device_t, device_t, void *);

static int esp_podule_dma_isintr(struct ncr53c9x_softc *);
static void esp_podule_dma_reset(struct ncr53c9x_softc *);
static int esp_podule_dma_intr(struct ncr53c9x_softc *);
static int esp_podule_dma_setup(struct ncr53c9x_softc *, uint8_t **, size_t *,
				int, size_t *);
static void esp_podule_dma_go(struct ncr53c9x_softc *);
static void esp_podule_dma_stop(struct ncr53c9x_softc *);
static int esp_podule_dma_isactive(struct ncr53c9x_softc *);

static uint8_t castle_read_reg(struct ncr53c9x_softc *, int);
static void castle_write_reg(struct ncr53c9x_softc *, int, uint8_t);

CFATTACH_DECL_NEW(esp_podule, sizeof(struct esp_podule_softc),
    esp_podule_match, esp_podule_attach, NULL, NULL);

static struct ncr53c9x_glue esp_podule_glue = {
	NULL,
	NULL,

	esp_podule_dma_isintr,
	esp_podule_dma_reset,
	esp_podule_dma_intr,
	esp_podule_dma_setup,
	esp_podule_dma_go,
	esp_podule_dma_stop,
	esp_podule_dma_isactive,
	NULL
};

#define PODULE_DEVICE(m, p, read, write) \
	{ MANUFACTURER_ ## m, PODULE_ ## m ## _ ## p, read, write }
static struct {
	int manufacturer;
	int product;
	uint8_t (*read_reg)(struct ncr53c9x_softc *, int);
	void (*write_reg)(struct ncr53c9x_softc *, int, uint8_t);
} devices[] = {
	PODULE_DEVICE(CASTLE, SCSI16, castle_read_reg, castle_write_reg),
};


static int
esp_podule_match(device_t parent, cfdata_t cf, void *aux)
{
	struct podulebus_attach_args *pa = aux;
	int i;

	for (i = 0; i < __arraycount(devices); i++)
		if (pa->pa_manufacturer == devices[i].manufacturer &&
		    pa->pa_product == devices[i].product)
			return 1;

	return 0;
}

static void
esp_podule_attach(device_t parent, device_t self, void *aux)
{
	struct esp_podule_softc *esc = device_private(self);
	struct ncr53c9x_softc *sc = &esc->sc_ncr53c9x;
	struct podulebus_attach_args *pa = aux;
	int i;

	for (i = 0; i < __arraycount(devices); i++)
		if (pa->pa_manufacturer == devices[i].manufacturer &&
		    pa->pa_product == devices[i].product)
			break;
	if (i == __arraycount(devices)) {
		aprint_error(": lost manufacturer 0x%04x, product 0x%04x\n",
		    pa->pa_manufacturer, pa->pa_product);
		return;
	}

	sc->sc_dev = self;
	esc->sc_esp_glue = esp_podule_glue;
	esc->sc_esp_glue.gl_read_reg = devices[i].read_reg;
	esc->sc_esp_glue.gl_write_reg = devices[i].write_reg;
	sc->sc_glue = &esc->sc_esp_glue;

	esc->sc_iot = pa->pa_slow_t;
	bus_space_map(esc->sc_iot, pa->pa_slow_base, 0x4000, 0, &esc->sc_ioh);
	sc->sc_freq = 2;	/* XXXX ??? */

	/* Get current ID */
	sc->sc_id = NCR_READ_REG(sc, NCR_CFG1) & NCRCFG1_BUSID;

	sc->sc_cfg1 = sc->sc_id;
	sc->sc_cfg2 = NCRCFG2_SCSI2 | NCRCFG2_DREQ | NCRCFG2_FE;
	sc->sc_rev = NCR_VARIANT_NCR53C96;	/* XXXX ??? */

	sc->sc_minsync = 1000 / sc->sc_freq;

	sc->sc_maxxfer = 64 * 1024;

	evcnt_attach_dynamic(&sc->sc_intrcnt, EVCNT_TYPE_INTR, NULL,
	    device_xname(self), "intr");
	podulebus_irq_establish(pa->pa_ih, IPL_BIO, ncr53c9x_intr, sc,
	    &sc->sc_intrcnt);

	sc->sc_adapter.adapt_minphys = minphys;
	sc->sc_adapter.adapt_request = ncr53c9x_scsipi_request;
	ncr53c9x_attach(sc);
}


static int
esp_podule_dma_isintr(struct ncr53c9x_softc *sc)
{

	return NCR_READ_REG(sc, NCR_STAT) & NCRSTAT_INT;
}

static void
esp_podule_dma_reset(struct ncr53c9x_softc *sc)
{
	struct esp_podule_softc *esc = (struct esp_podule_softc *)sc;

	esc->sc_active = 0;
	esc->sc_tc = 0;
}

static int
esp_podule_dma_intr(struct ncr53c9x_softc *sc)
{
	struct esp_podule_softc *esc = (struct esp_podule_softc *)sc;
	uint8_t *p;
	u_int espphase, espstat, espintr;
	int cnt;

	if (esc->sc_active == 0) {
		printf("%s: dma_intr--inactive DMA\n",
		    device_xname(sc->sc_dev));
		return -1;
	}

	if ((sc->sc_espintr & NCRINTR_BS) == 0) {
		esc->sc_active = 0;
		return 0;
	}

	cnt = *esc->sc_dmalen;
	if (*esc->sc_dmalen == 0) {
		printf("%s: data interrupt, but no count left\n",
		    device_xname(sc->sc_dev));
	}

	p = *esc->sc_dmaaddr;
	espphase = sc->sc_phase;
	espstat = (u_int)sc->sc_espstat;
	espintr = (u_int)sc->sc_espintr;
	do {
		if (esc->sc_datain) {
			*p++ = NCR_READ_REG(sc, NCR_FIFO);
			cnt--;
			if (espphase == DATA_IN_PHASE)
				NCR_WRITE_REG(sc, NCR_CMD, NCRCMD_TRANS);
			else
				esc->sc_active = 0;
		} else {
			if ((espphase == DATA_OUT_PHASE) ||
			    (espphase == MESSAGE_OUT_PHASE)) {
				NCR_WRITE_REG(sc, NCR_FIFO, *p++);
				cnt--;
				NCR_WRITE_REG(sc, NCR_CMD, NCRCMD_TRANS);
			} else
				esc->sc_active = 0;
		}

		if (esc->sc_active) {
			while ((NCR_READ_REG(sc, NCR_STAT) & NCRSTAT_INT) == 0)
				;
			espstat = NCR_READ_REG(sc, NCR_STAT);
			espintr = NCR_READ_REG(sc, NCR_INTR);
			espphase = (espintr & NCRINTR_DIS) ?
			    /* Disconnected */ BUSFREE_PHASE :
			    espstat & PHASE_MASK;
		}
	} while (esc->sc_active && espintr);
	sc->sc_phase = espphase;
	sc->sc_espstat = (uint8_t)espstat;
	sc->sc_espintr = (uint8_t)espintr;
	*esc->sc_dmaaddr = p;
	*esc->sc_dmalen = cnt;

	if (*esc->sc_dmalen == 0)
		esc->sc_tc = NCRSTAT_TC;
	sc->sc_espstat |= esc->sc_tc;

	return 0;
}

static int
esp_podule_dma_setup(struct ncr53c9x_softc *sc, uint8_t **addr, size_t *len,
		     int datain, size_t *dmasize)
{
	struct esp_podule_softc *esc = (struct esp_podule_softc *)sc;

	esc->sc_dmaaddr = addr;
	esc->sc_dmalen = len;
	esc->sc_datain = datain;
	esc->sc_dmasize = *dmasize;
	esc->sc_tc = 0;

	return 0;
}

static void
esp_podule_dma_go(struct ncr53c9x_softc *sc)
{
	struct esp_podule_softc *esc = (struct esp_podule_softc *)sc;

	if (esc->sc_datain == 0) {
		NCR_WRITE_REG(sc, NCR_FIFO, **esc->sc_dmaaddr);
		(*esc->sc_dmalen)--;
		(*esc->sc_dmaaddr)++;
	}

	esc->sc_active = 1;
}

static void
esp_podule_dma_stop(struct ncr53c9x_softc *sc)
{
	/* Nothing */
}

static int
esp_podule_dma_isactive(struct ncr53c9x_softc *sc)
{
	struct esp_podule_softc *esc = (struct esp_podule_softc *)sc;

	return esc->sc_active;
}

/*
 * Functions for access to Castle SCSI registers.
 */
#define CASTLE_ESP_OFFSET	0xf00
#define CASTLE_ESP_READ(sc, r) \
    bus_space_read_1((sc)->sc_iot, (sc)->sc_ioh, CASTLE_ESP_OFFSET + ((r) << 2))
#define CASTLE_ESP_WRITE(sc, r, v) \
    bus_space_write_1((sc)->sc_iot, (sc)->sc_ioh, \
					CASTLE_ESP_OFFSET + ((r) << 2), (v))

static uint8_t
castle_read_reg(struct ncr53c9x_softc *sc, int reg)
{
	struct esp_podule_softc *esc = (struct esp_podule_softc *)sc;
	uint8_t val;

	val = CASTLE_ESP_READ(esc, reg);
	return val;
}

static void
castle_write_reg(struct ncr53c9x_softc *sc, int reg, uint8_t val)
{
	struct esp_podule_softc *esc = (struct esp_podule_softc *)sc;

	if (reg == NCR_CMD && val == (NCRCMD_TRANS | NCRCMD_DMA))
		val &= ~NCRCMD_DMA;			/* DMA not support */
	CASTLE_ESP_WRITE(esc, reg, val);
}
