/*	$NetBSD: esp_sbus.c,v 1.53 2014/10/18 08:33:28 snj Exp $	*/

/*-
 * Copyright (c) 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum; Jason R. Thorpe of the Numerical Aerospace
 * Simulation Facility, NASA Ames Research Center; Paul Kranenburg.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: esp_sbus.c,v 1.53 2014/10/18 08:33:28 snj Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/malloc.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>
#include <dev/scsipi/scsi_message.h>

#include <sys/bus.h>
#include <sys/intr.h>
#include <machine/autoconf.h>

#include <dev/ic/lsi64854reg.h>
#include <dev/ic/lsi64854var.h>

#include <dev/ic/ncr53c9xreg.h>
#include <dev/ic/ncr53c9xvar.h>

#include <dev/sbus/sbusvar.h>

#include "opt_ddb.h"

/* #define ESP_SBUS_DEBUG */

struct esp_softc {
	struct ncr53c9x_softc sc_ncr53c9x;	/* glue to MI code */

	bus_space_tag_t	sc_bustag;
	bus_dma_tag_t	sc_dmatag;

	bus_space_handle_t sc_reg;		/* the registers */
	struct lsi64854_softc *sc_dma;		/* pointer to my dma */

	int	sc_pri;				/* SBUS priority */
};

int	espmatch_sbus(device_t, cfdata_t, void *);
void	espattach_sbus(device_t, device_t, void *);
void	espattach_dma(device_t, device_t, void *);

static void	espattach(struct esp_softc *, struct ncr53c9x_glue *);

CFATTACH_DECL_NEW(esp_sbus, sizeof(struct esp_softc),
    espmatch_sbus, espattach_sbus, NULL, NULL);

CFATTACH_DECL_NEW(esp_dma, sizeof(struct esp_softc),
    espmatch_sbus, espattach_dma, NULL, NULL);

/*
 * Functions and the switch for the MI code.
 */
static uint8_t	esp_read_reg(struct ncr53c9x_softc *, int);
static void	esp_write_reg(struct ncr53c9x_softc *, int, uint8_t);
static uint8_t	esp_rdreg1(struct ncr53c9x_softc *, int);
static void	esp_wrreg1(struct ncr53c9x_softc *, int, uint8_t);
static int	esp_dma_isintr(struct ncr53c9x_softc *);
static void	esp_dma_reset(struct ncr53c9x_softc *);
static int	esp_dma_intr(struct ncr53c9x_softc *);
static int	esp_dma_setup(struct ncr53c9x_softc *, uint8_t **,
				    size_t *, int, size_t *);
static void	esp_dma_go(struct ncr53c9x_softc *);
static void	esp_dma_stop(struct ncr53c9x_softc *);
static int	esp_dma_isactive(struct ncr53c9x_softc *);

#ifdef DDB
static void	esp_init_ddb_cmds(void);
#endif

static struct ncr53c9x_glue esp_sbus_glue = {
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

static struct ncr53c9x_glue esp_sbus_glue1 = {
	esp_rdreg1,
	esp_wrreg1,
	esp_dma_isintr,
	esp_dma_reset,
	esp_dma_intr,
	esp_dma_setup,
	esp_dma_go,
	esp_dma_stop,
	esp_dma_isactive,
	NULL,			/* gl_clear_latched_intr */
};

int
espmatch_sbus(device_t parent, cfdata_t cf, void *aux)
{
	int rv;
	struct sbus_attach_args *sa = aux;

	if (strcmp("SUNW,fas", sa->sa_name) == 0)
	        return 1;

	rv = (strcmp(cf->cf_name, sa->sa_name) == 0 ||
	    strcmp("ptscII", sa->sa_name) == 0);
	return rv;
}

void
espattach_sbus(device_t parent, device_t self, void *aux)
{
	struct esp_softc *esc = device_private(self);
	struct ncr53c9x_softc *sc = &esc->sc_ncr53c9x;
	struct sbus_softc *sbsc = device_private(parent);
	struct sbus_attach_args *sa = aux;
	struct lsi64854_softc *lsc;
	device_t dma_dev;
	int burst, sbusburst;

	sc->sc_dev = self;

#ifdef DDB
	esp_init_ddb_cmds();
#endif

	esc->sc_bustag = sa->sa_bustag;
	esc->sc_dmatag = sa->sa_dmatag;

	sc->sc_id = prom_getpropint(sa->sa_node, "initiator-id", 7);
	sc->sc_freq = prom_getpropint(sa->sa_node, "clock-frequency", -1);
	if (sc->sc_freq < 0)
		sc->sc_freq = sbsc->sc_clockfreq;

#ifdef ESP_SBUS_DEBUG
	aprint_normal("\n");
	aprint_normal_dev(self, "%s: sc_id %d, freq %d\n",
	    __func__, sc->sc_id, sc->sc_freq);
	aprint_normal("%s", device_xname(self));
#endif

	if (strcmp("SUNW,fas", sa->sa_name) == 0) {

		/*
		 * fas has 2 register spaces: dma(lsi64854) and
		 *                            SCSI core (ncr53c9x)
		 */
		if (sa->sa_nreg != 2) {
			aprint_error(": %d register spaces\n", sa->sa_nreg);
			return;
		}

		/*
		 * allocate space for dma, in SUNW,fas there are no separate
		 * dma device
		 */
		lsc = malloc(sizeof(struct lsi64854_softc), M_DEVBUF, M_NOWAIT);

		if (lsc == NULL) {
			aprint_error(": out of memory (lsi64854_softc)\n");
			return;
		}
		lsc->sc_dev = malloc(sizeof(struct device), M_DEVBUF,
		    M_NOWAIT | M_ZERO);
		if (lsc->sc_dev == NULL) {
			aprint_error(": out of memory (device_t)\n");
			free(lsc, M_DEVBUF);
			return;
		}
		esc->sc_dma = lsc;

		lsc->sc_bustag = sa->sa_bustag;
		lsc->sc_dmatag = sa->sa_dmatag;

		strlcpy(lsc->sc_dev->dv_xname, device_xname(sc->sc_dev),
		    sizeof(lsc->sc_dev->dv_xname));

		/* Map dma registers */
		if (sa->sa_npromvaddrs) {
			sbus_promaddr_to_handle(sa->sa_bustag,
			    sa->sa_promvaddrs[0], &lsc->sc_regs);
		} else {
			if (sbus_bus_map(sa->sa_bustag,
			    sa->sa_reg[0].oa_space,
			    sa->sa_reg[0].oa_base,
			    sa->sa_reg[0].oa_size,
			    0, &lsc->sc_regs) != 0) {
				aprint_error(": cannot map dma registers\n");
				return;
			}
		}

		/*
		 * XXX is this common(from bpp.c), the same in dma_sbus...etc.
		 *
		 * Get transfer burst size from PROM and plug it into the
		 * controller registers. This is needed on the Sun4m; do
		 * others need it too?
		 */
		sbusburst = sbsc->sc_burst;
		if (sbusburst == 0)
			sbusburst = SBUS_BURST_32 - 1; /* 1->16 */

		burst = prom_getpropint(sa->sa_node, "burst-sizes", -1);

#if ESP_SBUS_DEBUG
		aprint_normal("%s: burst 0x%x, sbus 0x%x\n",
		    __func__, burst, sbusburst);
		aprint_normal("%s", device_xname(self));
#endif

		if (burst == -1)
			/* take SBus burst sizes */
			burst = sbusburst;

		/* Clamp at parent's burst sizes */
		burst &= sbusburst;
		lsc->sc_burst = (burst & SBUS_BURST_32) ? 32 :
		    (burst & SBUS_BURST_16) ? 16 : 0;

		lsc->sc_channel = L64854_CHANNEL_SCSI;
		lsc->sc_client = sc;

		lsi64854_attach(lsc);

		/*
		 * map SCSI core registers
		 */
		if (sa->sa_npromvaddrs > 1) {
			sbus_promaddr_to_handle(sa->sa_bustag,
			    sa->sa_promvaddrs[1], &esc->sc_reg);
		} else {
			if (sbus_bus_map(sa->sa_bustag,
			    sa->sa_reg[1].oa_space,
			    sa->sa_reg[1].oa_base,
			    sa->sa_reg[1].oa_size,
			    0, &esc->sc_reg) != 0) {
				aprint_error(": cannot map "
				    "scsi core registers\n");
				return;
			}
		}

		if (sa->sa_nintr == 0) {
			aprint_error(": no interrupt property\n");
			return;
		}

		esc->sc_pri = sa->sa_pri;

		espattach(esc, &esp_sbus_glue);

		return;
	}

	/*
	 * Find the DMA by poking around the dma device structures
	 *
	 * What happens here is that if the dma driver has not been
	 * configured, then this returns a NULL pointer. Then when the
	 * dma actually gets configured, it does the opposing test, and
	 * if the sc->sc_esp field in its softc is NULL, then tries to
	 * find the matching esp driver.
	 */
	dma_dev = device_find_by_driver_unit("dma", device_unit(self));
	if (dma_dev == NULL) {
		aprint_error(": no corresponding DMA device\n");
		return;
	}
	esc->sc_dma = device_private(dma_dev);
	esc->sc_dma->sc_client = sc;

	/*
	 * The `ESC' DMA chip must be reset before we can access
	 * the esp registers.
	 */
	if (esc->sc_dma->sc_rev == DMAREV_ESC)
		DMA_RESET(esc->sc_dma);

	/*
	 * Map my registers in, if they aren't already in virtual
	 * address space.
	 */
	if (sa->sa_npromvaddrs) {
		sbus_promaddr_to_handle(sa->sa_bustag,
		    sa->sa_promvaddrs[0], &esc->sc_reg);
	} else {
		if (sbus_bus_map(sa->sa_bustag,
		    sa->sa_slot, sa->sa_offset, sa->sa_size,
		    0, &esc->sc_reg) != 0) {
			aprint_error(": cannot map registers\n");
			return;
		}
	}

	if (sa->sa_nintr == 0) {
		/*
		 * No interrupt properties: we quit; this might
		 * happen on e.g. a Sparc X terminal.
		 */
		aprint_error(": no interrupt property\n");
		return;
	}

	esc->sc_pri = sa->sa_pri;

	if (strcmp("ptscII", sa->sa_name) == 0) {
		espattach(esc, &esp_sbus_glue1);
	} else {
		espattach(esc, &esp_sbus_glue);
	}
}

void
espattach_dma(device_t parent, device_t self, void *aux)
{
	struct esp_softc *esc = device_private(self);
	struct ncr53c9x_softc *sc = &esc->sc_ncr53c9x;
	struct sbus_attach_args *sa = aux;

	if (strcmp("ptscII", sa->sa_name) == 0) {
		return;
	}

	sc->sc_dev = self;

	esc->sc_bustag = sa->sa_bustag;
	esc->sc_dmatag = sa->sa_dmatag;

	sc->sc_id = prom_getpropint(sa->sa_node, "initiator-id", 7);
	sc->sc_freq = prom_getpropint(sa->sa_node, "clock-frequency", -1);

	esc->sc_dma = device_private(parent);
	esc->sc_dma->sc_client = sc;

	/*
	 * Map my registers in, if they aren't already in virtual
	 * address space.
	 */
	if (sa->sa_npromvaddrs) {
		sbus_promaddr_to_handle(sa->sa_bustag,
		    sa->sa_promvaddrs[0], &esc->sc_reg);
	} else {
		if (sbus_bus_map(sa->sa_bustag,
		    sa->sa_slot, sa->sa_offset, sa->sa_size,
		    0, &esc->sc_reg) != 0) {
			aprint_error(": cannot map registers\n");
			return;
		}
	}

	if (sa->sa_nintr == 0) {
		/*
		 * No interrupt properties: we quit; this might
		 * happen on e.g. a Sparc X terminal.
		 */
		aprint_error(": no interrupt property\n");
		return;
	}

	esc->sc_pri = sa->sa_pri;

	espattach(esc, &esp_sbus_glue);
}


/*
 * Attach this instance, and then all the sub-devices
 */
void
espattach(struct esp_softc *esc, struct ncr53c9x_glue *gluep)
{
	struct ncr53c9x_softc *sc = &esc->sc_ncr53c9x;
	unsigned int uid = 0;

	/*
	 * Set up glue for MI code early; we use some of it here.
	 */
	sc->sc_glue = gluep;

	/* gimme MHz */
	sc->sc_freq /= 1000000;

	/*
	 * XXX More of this should be in ncr53c9x_attach(), but
	 * XXX should we really poke around the chip that much in
	 * XXX the MI code?  Think about this more...
	 */

	/*
	 * It is necessary to try to load the 2nd config register here,
	 * to find out what rev the esp chip is, else the ncr53c9x_reset
	 * will not set up the defaults correctly.
	 */
	sc->sc_cfg1 = sc->sc_id | NCRCFG1_PARENB;
	sc->sc_cfg2 = NCRCFG2_SCSI2 | NCRCFG2_RPE;
	sc->sc_cfg3 = NCRCFG3_CDB;
	NCR_WRITE_REG(sc, NCR_CFG2, sc->sc_cfg2);

	if ((NCR_READ_REG(sc, NCR_CFG2) & ~NCRCFG2_RSVD) !=
	    (NCRCFG2_SCSI2 | NCRCFG2_RPE)) {
		sc->sc_rev = NCR_VARIANT_ESP100;
	} else {
		sc->sc_cfg2 = NCRCFG2_SCSI2;
		NCR_WRITE_REG(sc, NCR_CFG2, sc->sc_cfg2);
		sc->sc_cfg3 = 0;
		NCR_WRITE_REG(sc, NCR_CFG3, sc->sc_cfg3);
		sc->sc_cfg3 = (NCRCFG3_CDB | NCRCFG3_FCLK);
		NCR_WRITE_REG(sc, NCR_CFG3, sc->sc_cfg3);
		if (NCR_READ_REG(sc, NCR_CFG3) !=
		    (NCRCFG3_CDB | NCRCFG3_FCLK)) {
			sc->sc_rev = NCR_VARIANT_ESP100A;
		} else {
			/* NCRCFG2_FE enables > 64K transfers */
			sc->sc_cfg2 |= NCRCFG2_FE;
			sc->sc_cfg3 = 0;
			NCR_WRITE_REG(sc, NCR_CFG3, sc->sc_cfg3);
			sc->sc_rev = NCR_VARIANT_ESP200;

			/*
			 * XXX spec says it's valid after power up or
			 * chip reset
			 */
			uid = NCR_READ_REG(sc, NCR_UID);
			if (((uid & 0xf8) >> 3) == 0x0a) /* XXX */
				sc->sc_rev = NCR_VARIANT_FAS366;
		}
	}

#ifdef ESP_SBUS_DEBUG
	aprint_debug("%s: revision %d, uid 0x%x\n", __func__, sc->sc_rev, uid);
	aprint_normal("%s", device_xname(sc->sc_dev));
#endif

	/*
	 * XXX minsync and maxxfer _should_ be set up in MI code,
	 * XXX but it appears to have some dependency on what sort
	 * XXX of DMA we're hooked up to, etc.
	 */

	/*
	 * This is the value used to start sync negotiations
	 * Note that the NCR register "SYNCTP" is programmed
	 * in "clocks per byte", and has a minimum value of 4.
	 * The SCSI period used in negotiation is one-fourth
	 * of the time (in nanoseconds) needed to transfer one byte.
	 * Since the chip's clock is given in MHz, we have the following
	 * formula: 4 * period = (1000 / freq) * 4
	 */
	sc->sc_minsync = 1000 / sc->sc_freq;

	/*
	 * Alas, we must now modify the value a bit, because it's
	 * only valid when can switch on FASTCLK and FASTSCSI bits
	 * in config register 3...
	 */
	switch (sc->sc_rev) {
	case NCR_VARIANT_ESP100:
		sc->sc_maxxfer = 64 * 1024;
		sc->sc_minsync = 0;	/* No synch on old chip? */
		break;

	case NCR_VARIANT_ESP100A:
		sc->sc_maxxfer = 64 * 1024;
		/* Min clocks/byte is 5 */
		sc->sc_minsync = ncr53c9x_cpb2stp(sc, 5);
		break;

	case NCR_VARIANT_ESP200:
	case NCR_VARIANT_FAS366:
		sc->sc_maxxfer = 16 * 1024 * 1024;
		/* XXX - do actually set FAST* bits */
		break;
	}

	/* Establish interrupt channel */
	bus_intr_establish(esc->sc_bustag, esc->sc_pri, IPL_BIO,
	    ncr53c9x_intr, sc);

	/* register interrupt stats */
	evcnt_attach_dynamic(&sc->sc_intrcnt, EVCNT_TYPE_INTR, NULL,
	    device_xname(sc->sc_dev), "intr");

	/* Turn on target selection using the `dma' method */
	if (sc->sc_rev != NCR_VARIANT_FAS366)
		sc->sc_features |= NCR_F_DMASELECT;

	/* Do the common parts of attachment. */
	sc->sc_adapter.adapt_minphys = minphys;
	sc->sc_adapter.adapt_request = ncr53c9x_scsipi_request;
	ncr53c9x_attach(sc);
}

/*
 * Glue functions.
 */

#ifdef ESP_SBUS_DEBUG
int esp_sbus_debug = 0;

static struct {
	char *r_name;
	int   r_flag;
} esp__read_regnames [] = {
	{ "TCL", 0},			/* 0/00 */
	{ "TCM", 0},			/* 1/04 */
	{ "FIFO", 0},			/* 2/08 */
	{ "CMD", 0},			/* 3/0c */
	{ "STAT", 0},			/* 4/10 */
	{ "INTR", 0},			/* 5/14 */
	{ "STEP", 0},			/* 6/18 */
	{ "FFLAGS", 1},			/* 7/1c */
	{ "CFG1", 1},			/* 8/20 */
	{ "STAT2", 0},			/* 9/24 */
	{ "CFG4", 1},			/* a/28 */
	{ "CFG2", 1},			/* b/2c */
	{ "CFG3", 1},			/* c/30 */
	{ "-none", 1},			/* d/34 */
	{ "TCH", 1},			/* e/38 */
	{ "TCX", 1},			/* f/3c */
};

static struct {
	char *r_name;
	int   r_flag;
} esp__write_regnames[] = {
	{ "TCL", 1},			/* 0/00 */
	{ "TCM", 1},			/* 1/04 */
	{ "FIFO", 0},			/* 2/08 */
	{ "CMD", 0},			/* 3/0c */
	{ "SELID", 1},			/* 4/10 */
	{ "TIMEOUT", 1},		/* 5/14 */
	{ "SYNCTP", 1},			/* 6/18 */
	{ "SYNCOFF", 1},		/* 7/1c */
	{ "CFG1", 1},			/* 8/20 */
	{ "CCF", 1},			/* 9/24 */
	{ "TEST", 1},			/* a/28 */
	{ "CFG2", 1},			/* b/2c */
	{ "CFG3", 1},			/* c/30 */
	{ "-none", 1},			/* d/34 */
	{ "TCH", 1},			/* e/38 */
	{ "TCX", 1},			/* f/3c */
};
#endif

uint8_t
esp_read_reg(struct ncr53c9x_softc *sc, int reg)
{
	struct esp_softc *esc = (struct esp_softc *)sc;
	uint8_t v;

	v = bus_space_read_1(esc->sc_bustag, esc->sc_reg, reg * 4);
#ifdef ESP_SBUS_DEBUG
	if (esp_sbus_debug && (reg < 0x10) && esp__read_regnames[reg].r_flag)
		printf("RD:%x <%s> %x\n", reg * 4,
		    ((unsigned int)reg < 0x10) ?
		    esp__read_regnames[reg].r_name : "<***>", v);
#endif
	return v;
}

void
esp_write_reg(struct ncr53c9x_softc *sc, int reg, uint8_t v)
{
	struct esp_softc *esc = (struct esp_softc *)sc;

#ifdef ESP_SBUS_DEBUG
	if (esp_sbus_debug && (reg < 0x10) && esp__write_regnames[reg].r_flag)
		printf("WR:%x <%s> %x\n", reg * 4,
		    ((unsigned int)reg < 0x10) ?
		    esp__write_regnames[reg].r_name : "<***>", v);
#endif
	bus_space_write_1(esc->sc_bustag, esc->sc_reg, reg * 4, v);
}

uint8_t
esp_rdreg1(struct ncr53c9x_softc *sc, int reg)
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	return bus_space_read_1(esc->sc_bustag, esc->sc_reg, reg);
}

void
esp_wrreg1(struct ncr53c9x_softc *sc, int reg, uint8_t v)
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	bus_space_write_1(esc->sc_bustag, esc->sc_reg, reg, v);
}

int
esp_dma_isintr(struct ncr53c9x_softc *sc)
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	return DMA_ISINTR(esc->sc_dma);
}

void
esp_dma_reset(struct ncr53c9x_softc *sc)
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	DMA_RESET(esc->sc_dma);
}

int
esp_dma_intr(struct ncr53c9x_softc *sc)
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	return DMA_INTR(esc->sc_dma);
}

int
esp_dma_setup(struct ncr53c9x_softc *sc, uint8_t **addr, size_t *len,
    int datain, size_t *dmasize)
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	return DMA_SETUP(esc->sc_dma, addr, len, datain, dmasize);
}

void
esp_dma_go(struct ncr53c9x_softc *sc)
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	DMA_GO(esc->sc_dma);
}

void
esp_dma_stop(struct ncr53c9x_softc *sc)
{
	struct esp_softc *esc = (struct esp_softc *)sc;
	uint32_t csr;

	csr = L64854_GCSR(esc->sc_dma);
	csr &= ~D_EN_DMA;
	L64854_SCSR(esc->sc_dma, csr);
}

int
esp_dma_isactive(struct ncr53c9x_softc *sc)
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	return DMA_ISACTIVE(esc->sc_dma);
}

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_output.h>
#include <ddb/db_command.h>

void db_esp(db_expr_t, bool, db_expr_t, const char*);

const struct db_command db_esp_command_table[] = {
	{ DDB_ADD_CMD("esp",	db_esp,	0, 
	  "display status of all esp SCSI controllers and their devices",
	  NULL, NULL) },
	{ DDB_ADD_CMD(NULL,	NULL,	0, NULL, NULL, NULL) }
};

static void
esp_init_ddb_cmds(void)
{
	static int db_cmds_initialized = 0;

	if (db_cmds_initialized)
		return;
	db_cmds_initialized = 1;
	(void)db_register_tbl(DDB_MACH_CMD, db_esp_command_table);
}

void
db_esp(db_expr_t addr, bool have_addr, db_expr_t count, const char *modif)
{
	device_t dv;
	struct ncr53c9x_softc *sc;
	struct ncr53c9x_ecb *ecb;
	struct ncr53c9x_linfo *li;
	int u, t, i;

	for (u = 0; u < 10; u++) {
		dv = device_find_by_driver_unit("esp", u);
		if (dv == NULL)
			continue;
		sc = device_private(dv);

		db_printf("%s: nexus %p phase %x prev %x"
		    " dp %p dleft %lx ify %x\n", device_xname(dv),
		    sc->sc_nexus, sc->sc_phase, sc->sc_prevphase,
		      sc->sc_dp, sc->sc_dleft, sc->sc_msgify);
		db_printf("\tmsgout %x msgpriq %x msgin %x:%x:%x:%x:%x\n",
		     sc->sc_msgout, sc->sc_msgpriq, sc->sc_imess[0],
		     sc->sc_imess[1], sc->sc_imess[2], sc->sc_imess[3],
		     sc->sc_imess[0]);
		db_printf("ready: ");
		for (ecb = TAILQ_FIRST(&sc->ready_list); ecb != NULL;
		    ecb = TAILQ_NEXT(ecb, chain)) {
			db_printf("ecb %p ", ecb);
			if (ecb == TAILQ_NEXT(ecb, chain)) {
				db_printf("\nWARNING: tailq loop on ecb %p",
				    ecb);
				break;
			}
		}
		db_printf("\n");

		for (t = 0; t < sc->sc_ntarg; t++) {
			LIST_FOREACH(li, &sc->sc_tinfo[t].luns, link) {
				db_printf("t%d lun %d untagged %p"
				    " busy %d used %x\n",
				    t, (int)li->lun, li->untagged, li->busy,
				    li->used);
				for (i = 0; i < 256; i++)
					ecb = li->queued[i];
					if (ecb != NULL) {
						db_printf("ecb %p tag %x\n",
						    ecb, i);
					}
			}
		}
	}
}
#endif
