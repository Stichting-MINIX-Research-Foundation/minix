/*	$NetBSD: esp_isa.c,v 1.37 2014/10/18 08:33:28 snj Exp $	*/

/*-
 * Copyright (c) 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum and by Jason R. Thorpe of the Numerical Aerospace
 * Simulation Facility, NASA Ames Research Center.
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
 * Copyright (c) 1994 Peter Galbavy
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Peter Galbavy
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

/*
 * Based on aic6360 by Jarle Greipsland
 *
 * Acknowledgements: Many of the algorithms used in this driver are
 * inspired by the work of Julian Elischer (julian@tfs.com) and
 * Charles Hannum (mycroft@duality.gnu.ai.mit.edu).  Thanks a million!
 */

/*
 * Initial m68k mac support from Allen Briggs <briggs@macbsd.com>
 * (basically consisting of the match, a bit of the attach, and the
 *  "DMA" glue functions).
 */

/*
 * Copyright (c) 1997 Eric S. Hvozda (hvozda@netcom.com)
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Eric S. Hvozda.
 * 4. The name of Eric S. Hvozda may not be used to endorse or promote products
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: esp_isa.c,v 1.37 2014/10/18 08:33:28 snj Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/buf.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsiconf.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/ic/ncr53c9xreg.h>
#include <dev/ic/ncr53c9xvar.h>

#include <dev/isa/esp_isavar.h>

int	esp_isa_match(device_t, cfdata_t, void *);
void	esp_isa_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(esp_isa, sizeof(struct esp_isa_softc),
    esp_isa_match, esp_isa_attach, NULL, NULL);

int esp_isa_debug = 0;	/* ESP_SHOWTRAC | ESP_SHOWREGS | ESP_SHOWMISC */

/*
 * Functions and the switch for the MI code.
 */
uint8_t	esp_isa_read_reg(struct ncr53c9x_softc *, int);
void	esp_isa_write_reg(struct ncr53c9x_softc *, int, uint8_t);
int	esp_isa_dma_isintr(struct ncr53c9x_softc *);
void	esp_isa_dma_reset(struct ncr53c9x_softc *);
int	esp_isa_dma_intr(struct ncr53c9x_softc *);
int	esp_isa_dma_setup(struct ncr53c9x_softc *, uint8_t **,
	    size_t *, int, size_t *);
void	esp_isa_dma_go(struct ncr53c9x_softc *);
void	esp_isa_dma_stop(struct ncr53c9x_softc *);
int	esp_isa_dma_isactive(struct ncr53c9x_softc *);

struct ncr53c9x_glue esp_isa_glue = {
	esp_isa_read_reg,
	esp_isa_write_reg,
	esp_isa_dma_isintr,
	esp_isa_dma_reset,
	esp_isa_dma_intr,
	esp_isa_dma_setup,
	esp_isa_dma_go,
	esp_isa_dma_stop,
	esp_isa_dma_isactive,
	NULL,			/* gl_clear_latched_intr */
};

/*
 * Look for the board
 */
int
esp_isa_find(bus_space_tag_t iot, bus_space_handle_t ioh,
    struct esp_isa_probe_data *epd)
{
	u_int vers;
	u_int p1;
	u_int p2;
	u_int jmp;

	ESP_TRACE(("[esp_isa_find] "));

	/* reset card before we probe? */

	epd->sc_cfg4 = NCRCFG4_ACTNEG;
	epd->sc_cfg5 = NCRCFG5_CRS1 | NCRCFG5_AADDR | NCRCFG5_PTRINC;

	/*
	 * Switch to the PIO regs and look for the bit pattern
	 * we expect...
	 */
	bus_space_write_1(iot, ioh, NCR_CFG5, epd->sc_cfg5);

#define SIG_MASK 0x87
#define REV_MASK 0x70
#define	M1	 0x02
#define	M2	 0x05
#define ISNCR	 0x80
#define ISESP406 0x40

	vers = bus_space_read_1(iot, ioh, NCR_SIGNTR);
	p1 = bus_space_read_1(iot, ioh, NCR_SIGNTR) & SIG_MASK;
	p2 = bus_space_read_1(iot, ioh, NCR_SIGNTR) & SIG_MASK;

	ESP_MISC(("esp_isa_find: 0x%0x 0x%0x 0x%0x\n", vers, p1, p2));

	if (!((p1 == M1 && p2 == M2) || (p1 == M2 && p2 == M1)))
		return 0;

	/* Ok, what is it? */
	epd->sc_isncr = (vers & ISNCR);
	epd->sc_rev = ((vers & REV_MASK) == ISESP406) ?
	    NCR_VARIANT_ESP406 : NCR_VARIANT_FAS408;

	/* What do the jumpers tell us? */
	jmp = bus_space_read_1(iot, ioh, NCR_JMP);

	epd->sc_msize = (jmp & NCRJMP_ROMSZ) ? 0x4000 : 0x8000;
	epd->sc_parity = jmp & NCRJMP_J2;
	epd->sc_sync = jmp & NCRJMP_J4;
	epd->sc_id = (jmp & NCRJMP_J3) ? 7 : 6;
	switch (jmp & (NCRJMP_J0 | NCRJMP_J1)) {
		case NCRJMP_J0 | NCRJMP_J1:
			epd->sc_irq = 11;
			break;
		case NCRJMP_J0:
			epd->sc_irq = 10;
			break;
		case NCRJMP_J1:
			epd->sc_irq = 15;
			break;
		default:
			epd->sc_irq = 12;
			break;
	}

	bus_space_write_1(iot, ioh, NCR_CFG5, epd->sc_cfg5);

	/* Try to set NCRESPCFG3_FCLK, some FAS408's don't support
	 * NCRESPCFG3_FCLK even though it is documented.  A bad
	 * batch of chips perhaps?
	 */
	bus_space_write_1(iot, ioh, NCR_ESPCFG3,
	    bus_space_read_1(iot, ioh, NCR_ESPCFG3) | NCRESPCFG3_FCLK);
	epd->sc_isfast = bus_space_read_1(iot, ioh, NCR_ESPCFG3)
	    & NCRESPCFG3_FCLK;

	return 1;
}

void
esp_isa_init(struct esp_isa_softc *esc, struct esp_isa_probe_data *epd)
{
	struct ncr53c9x_softc *sc = &esc->sc_ncr53c9x;

	ESP_TRACE(("[esp_isa_init] "));

	/*
	 * Set up the glue for MI code early; we use some of it here.
	 */
	sc->sc_glue = &esp_isa_glue;

	sc->sc_rev = epd->sc_rev;
	sc->sc_id = epd->sc_id;

	/* If we could set NCRESPCFG3_FCLK earlier, we can really move */
	sc->sc_cfg3 = NCR_READ_REG(sc, NCR_ESPCFG3);
	if ((epd->sc_rev == NCR_VARIANT_FAS408) && epd->sc_isfast) {
		sc->sc_freq = 40;
		sc->sc_cfg3 |= NCRESPCFG3_FCLK;
	} else
		sc->sc_freq = 24;

	/* Setup the register defaults */
	sc->sc_cfg1 = sc->sc_id;
	if (epd->sc_parity)
		sc->sc_cfg1 |= NCRCFG1_PARENB;
	sc->sc_cfg2 = NCRCFG2_SCSI2;
	sc->sc_cfg3 |= NCRESPCFG3_IDM | NCRESPCFG3_FSCSI;
	sc->sc_cfg4 = epd->sc_cfg4;
	sc->sc_cfg5 = epd->sc_cfg5;

	/*
	 * This is the value used to start sync negotiations
	 * Note that the NCR register "SYNCTP" is programmed
	 * in "clocks per byte", and has a minimum value of 4.
	 * The SCSI period used in negotiation is one-fourth
	 * of the time (in nanoseconds) needed to transfer one byte.
	 * Since the chip's clock is given in MHz, we have the following
	 * formula: 4 * period = (1000 / freq) * 4
	 */
	if (epd->sc_sync) {
#ifdef DIAGNOSTIC
		aprint_normal_dev(sc->sc_dev,
		    "sync requested, but not supported; will do async\n");
#endif
		epd->sc_sync = 0;
	}

	sc->sc_minsync = 0;

	/* Really no limit, but since we want to fit into the TCR... */
	sc->sc_maxxfer = 64 * 1024;
}

/*
 * Check the slots looking for a board we recognise
 * If we find one, note its address (slot) and call
 * the actual probe routine to check it out.
 */
int
esp_isa_match(device_t parent, cfdata_t cf, void *aux)
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	struct esp_isa_probe_data epd;
	int rv;

	if (ia->ia_nio < 1)
		return 0;
	if (ia->ia_nirq < 1)
		return 0;
	if (ia->ia_ndrq < 1)
		return 0;

	if (ISA_DIRECT_CONFIG(ia))
		return 0;

	ESP_TRACE(("[esp_isa_match] "));

	if (ia->ia_io[0].ir_addr == ISA_UNKNOWN_PORT)
		return 0;

	if (bus_space_map(iot, ia->ia_io[0].ir_addr, ESP_ISA_IOSIZE, 0, &ioh))
		return 0;

	rv = esp_isa_find(iot, ioh, &epd);

	bus_space_unmap(iot, ioh, ESP_ISA_IOSIZE);

	if (rv) {
		if (ia->ia_irq[0].ir_irq != ISA_UNKNOWN_IRQ &&
		    ia->ia_irq[0].ir_irq != epd.sc_irq) {
#ifdef DIAGNOSTIC
		printf("%s: configured IRQ (%0d) does not "
		    "match board IRQ (%0d), device not configured\n",
		    __func__, ia->ia_irq[0].ir_irq, epd.sc_irq);
#endif
			return 0;
		}
		ia->ia_irq[0].ir_irq = epd.sc_irq;
		ia->ia_iomem[0].ir_size = 0;
		ia->ia_io[0].ir_size = ESP_ISA_IOSIZE;
	}
	return rv;
}

/*
 * Attach this instance, and then all the sub-devices
 */
void
esp_isa_attach(device_t parent, device_t self, void *aux)
{
	struct esp_isa_softc *esc = device_private(self);
	struct ncr53c9x_softc *sc = &esc->sc_ncr53c9x;
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	struct esp_isa_probe_data epd;
	isa_chipset_tag_t ic = ia->ia_ic;
	int error;

	sc->sc_dev = self;
	aprint_normal("\n");
	ESP_TRACE(("[esp_isa_attach] "));

	if (bus_space_map(iot, ia->ia_io[0].ir_addr, ESP_ISA_IOSIZE, 0, &ioh)) {
		aprint_error_dev(self, "can't map i/o space\n");
		return;
	}

	if (!esp_isa_find(iot, ioh, &epd)) {
		aprint_error_dev(self, "esp_isa_find failed\n");
		return;
	}

	if (ia->ia_drq[0].ir_drq != ISA_UNKNOWN_DRQ) {
		if ((error = isa_dmacascade(ic, ia->ia_drq[0].ir_drq)) != 0) {
			aprint_error_dev(self,
			    "unable to cascade DRQ, error = %d\n", error);
			return;
		}
	}

	esc->sc_ih = isa_intr_establish(ic, ia->ia_irq[0].ir_irq, IST_EDGE,
	    IPL_BIO, ncr53c9x_intr, esc);
	if (esc->sc_ih == NULL) {
		aprint_error_dev(self, "couldn't establish interrupt\n");
		return;
	}

	esc->sc_ioh = ioh;
	esc->sc_iot = iot;
	esp_isa_init(esc, &epd);

	aprint_normal_dev(self, "%ssync,%sparity\n",
	    epd.sc_sync ? " " : " no ", epd.sc_parity ? " " : " no ");
	aprint_normal("%s", device_xname(self));

	/*
	 * Now try to attach all the sub-devices
	 */
	sc->sc_adapter.adapt_minphys = minphys;
	sc->sc_adapter.adapt_request = ncr53c9x_scsipi_request;
	ncr53c9x_attach(sc);
}

/*
 * Glue functions.
 */
uint8_t
esp_isa_read_reg(struct ncr53c9x_softc *sc, int reg)
{
	struct esp_isa_softc *esc = (struct esp_isa_softc *)sc;
	uint8_t v;

	v =  bus_space_read_1(esc->sc_iot, esc->sc_ioh, reg);

	ESP_REGS(("[esp_isa_read_reg CRS%c 0x%02x=0x%02x] ",
	    (bus_space_read_1(esc->sc_iot, esc->sc_ioh, NCR_CFG4) &
	    NCRCFG4_CRS1) ? '1' : '0', reg, v));

	return v;
}

void
esp_isa_write_reg(struct ncr53c9x_softc *sc, int reg, uint8_t val)
{
	struct esp_isa_softc *esc = (struct esp_isa_softc *)sc;
	uint8_t v = val;

	if (reg == NCR_CMD && v == (NCRCMD_TRANS|NCRCMD_DMA)) {
		v = NCRCMD_TRANS;
	}

	ESP_REGS(("[esp_isa_write_reg CRS%c 0x%02x=0x%02x] ",
	    (bus_space_read_1(esc->sc_iot, esc->sc_ioh, NCR_CFG4) &
	    NCRCFG4_CRS1) ? '1' : '0', reg, v));

	bus_space_write_1(esc->sc_iot, esc->sc_ioh, reg, v);
}

int
esp_isa_dma_isintr(struct ncr53c9x_softc *sc)
{

	ESP_TRACE(("[esp_isa_dma_isintr] "));

	return NCR_READ_REG(sc, NCR_STAT) & NCRSTAT_INT;
}

void
esp_isa_dma_reset(struct ncr53c9x_softc *sc)
{
	struct esp_isa_softc *esc = (struct esp_isa_softc *)sc;

	ESP_TRACE(("[esp_isa_dma_reset] "));

	esc->sc_active = 0;
	esc->sc_tc = 0;
}

int
esp_isa_dma_intr(struct ncr53c9x_softc *sc)
{
	struct esp_isa_softc *esc = (struct esp_isa_softc *)sc;
	uint8_t*p;
	u_int	espphase, espstat, espintr;
	int	cnt;

	ESP_TRACE(("[esp_isa_dma_intr] "));

	if (esc->sc_active == 0) {
		printf("%s: dma_intr--inactive DMA\n",
		    device_xname(sc->sc_dev));
		return -1;
	}

	if ((sc->sc_espintr & NCRINTR_BS) == 0) {
		esc->sc_active = 0;
		return 0;
	}

	cnt = *esc->sc_pdmalen;
	if (*esc->sc_pdmalen == 0) {
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
			if (espphase == DATA_IN_PHASE) {
				NCR_WRITE_REG(sc, NCR_CMD, NCRCMD_TRANS);
			} else {
				esc->sc_active = 0;
			}
	 	} else {
			if ((espphase == DATA_OUT_PHASE) ||
			    (espphase == MESSAGE_OUT_PHASE)) {
				NCR_WRITE_REG(sc, NCR_FIFO, *p++);
				cnt--;
				NCR_WRITE_REG(sc, NCR_CMD, NCRCMD_TRANS);
			} else {
				esc->sc_active = 0;
			}
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
	*esc->sc_pdmalen = cnt;

	if (*esc->sc_pdmalen == 0) {
		esc->sc_tc = NCRSTAT_TC;
	}
	sc->sc_espstat |= esc->sc_tc;
	return 0;
}

int
esp_isa_dma_setup(struct ncr53c9x_softc *sc, uint8_t **addr, size_t *len,
    int datain, size_t *dmasize)
{
	struct esp_isa_softc *esc = (struct esp_isa_softc *)sc;

	ESP_TRACE(("[esp_isa_dma_setup] "));

	esc->sc_dmaaddr = addr;
	esc->sc_pdmalen = len;
	esc->sc_datain = datain;
	esc->sc_dmasize = *dmasize;
	esc->sc_tc = 0;

	return 0;
}

void
esp_isa_dma_go(struct ncr53c9x_softc *sc)
{
	struct esp_isa_softc *esc = (struct esp_isa_softc *)sc;

	ESP_TRACE(("[esp_isa_dma_go] "));

	esc->sc_active = 1;
}

void
esp_isa_dma_stop(struct ncr53c9x_softc *sc)
{
	ESP_TRACE(("[esp_isa_dma_stop] "));
}

int
esp_isa_dma_isactive(struct ncr53c9x_softc *sc)
{
	struct esp_isa_softc *esc = (struct esp_isa_softc *)sc;

	ESP_TRACE(("[esp_isa_dma_isactive] "));

	return esc->sc_active;
}
