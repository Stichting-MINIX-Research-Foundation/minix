/*	$NetBSD: nca_isa.c,v 1.22 2012/10/27 17:18:25 chs Exp $	*/

/*-
 * Copyright (c) 1998, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by John M. Ruschmeyer.
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
 * FreeBSD generic NCR-5380/NCR-53C400 SCSI driver
 *
 * Copyright (C) 1994 Serge Vakulenko (vak@cronyx.ru)
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
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPERS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE DEVELOPERS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: nca_isa.c,v 1.22 2012/10/27 17:18:25 chs Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/buf.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/ic/ncr5380reg.h>
#include <dev/ic/ncr5380var.h>
#include <dev/ic/ncr53c400reg.h>

struct nca_isa_softc {
	struct ncr5380_softc	sc_ncr5380;	/* glue to MI code */

        void *sc_ih;
        int sc_irq;
	int sc_options;
};

struct nca_isa_probe_data {
	int sc_reg_offset;
	int sc_host_type;
};

int	nca_isa_find(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    struct nca_isa_probe_data *);
int	nca_isa_match(device_t, cfdata_t, void *);
void	nca_isa_attach(device_t, device_t, void *);
int	nca_isa_test(bus_space_tag_t, bus_space_handle_t, bus_size_t);

CFATTACH_DECL_NEW(nca_isa, sizeof(struct nca_isa_softc),
    nca_isa_match, nca_isa_attach, NULL, NULL);


/* Supported controller types */
#define MAX_NCA_CONTROLLER	3
#define CTLR_NCR_5380	1
#define	CTLR_NCR_53C400	2
#define CTLR_PAS16	3

#define NCA_ISA_IOSIZE 16
#define MIN_DMA_LEN 128

/* Options for disconnect/reselect, DMA, and interrupts. */
#define NCA_NO_DISCONNECT    0xff
#define NCA_NO_PARITY_CHK  0xff00
#define NCA_FORCE_POLLING 0x10000


/*
 * Initialization and test function used by nca_isa_find()
 */
int
nca_isa_test(bus_space_tag_t iot, bus_space_handle_t ioh, bus_size_t reg_offset)
{

	/* Reset the SCSI bus. */
	bus_space_write_1(iot, ioh, reg_offset + C80_ICR, SCI_ICMD_RST);
	bus_space_write_1(iot, ioh, reg_offset + C80_ODR, 0);
	/* Hold reset for at least 25 microseconds. */
	delay(500);
	/* Check that status cleared. */
	if (bus_space_read_1(iot, ioh, reg_offset + C80_CSBR) != SCI_BUS_RST) {
#ifdef DEBUG
		printf("%s: reset status not cleared [0x%x]\n",
		    __func__, bus_space_read_1(iot, ioh, reg_offset+C80_CSBR));
#endif
		bus_space_write_1(iot, ioh, reg_offset+C80_ICR, 0);
		return 0;
	}
	/* Clear reset. */
	bus_space_write_1(iot, ioh, reg_offset + C80_ICR, 0);
	/* Wait a Bus Clear Delay (800 ns + bus free delay 800 ns). */
	delay(16000);

	/* Read RPI port, resetting parity/interrupt state. */
	bus_space_read_1(iot, ioh, reg_offset + C80_RPIR);

	/* Test BSR: parity error, interrupt request and busy loss state
	 * should be cleared. */
	if (bus_space_read_1(iot, ioh, reg_offset + C80_BSR) & (SCI_CSR_PERR |
	    SCI_CSR_INT | SCI_CSR_DISC)) {
#ifdef DEBUG
		printf("%s: Parity/Interrupt/Busy not cleared [0x%x]\n",
		    __func__, bus_space_read_1(iot, ioh, reg_offset+C80_BSR));
#endif
		return 0;
	}

	/* We must have found one */
	return 1;
}


/*
 * Look for the board
 */
int
nca_isa_find(bus_space_tag_t iot, bus_space_handle_t ioh,
    bus_size_t max_offset, struct nca_isa_probe_data *epd)
{
	/*
	 * We check for the existence of a board by trying to initialize it,
	 * Then sending the commands to reset the SCSI bus.
	 * (Unfortunately, this duplicates code which is already in the MI
	 * driver. Unavoidable as that code is not suited to this task.)
	 * This is largely stolen from FreeBSD.
	 */
	int 		cont_type;
	bus_size_t	base_offset, reg_offset = 0;

	/*
	 * Some notes:
	 * In the case of a port-mapped board, we should be pointing
	 * right at the chip registers (if they are there at all).
	 * For a memory-mapped card, we loop through the 16K paragraph,
	 * 8 bytes at a time, until we either find it or run out
	 * of region. This means we will probably be doing things like
	 * trying to write to ROMS, etc. Hopefully, this is not a problem.
	 */

	for (base_offset = 0; base_offset < max_offset; base_offset += 0x08) {
#ifdef DEBUG
		printf("%s: testing offset 0x%x\n", __func__, (int)base_offset);
#endif

		/* See if anything is there */
		if (bus_space_read_1(iot, ioh, base_offset) == 0xff)
			continue;

		/* Loop around for each board type */
		for (cont_type = 1; cont_type <= MAX_NCA_CONTROLLER;
		    cont_type++) {
			/* Per-controller initialization */
			switch (cont_type) {
			case CTLR_NCR_5380:
				/* No special inits */
				reg_offset = 0;
				break;
			case CTLR_NCR_53C400:
				/* Reset into 5380-compat. mode */
				bus_space_write_1(iot, ioh,
				    base_offset + C400_CSR,
				    C400_CSR_5380_ENABLE);
				reg_offset = C400_5380_REG_OFFSET;
				break;
			case CTLR_PAS16:
				/* Not currently supported */
				reg_offset = 0;
				cont_type = 0;
				continue;
			}

			/* Initialize controller and bus */
			if (nca_isa_test(iot, ioh, base_offset+reg_offset)) {
				epd->sc_reg_offset = base_offset;
				epd->sc_host_type = cont_type;
				return cont_type;	/* This must be it */
			}
		}
	}

	/* If we got here, we didn't find one */
	return 0;
}


/*
 * See if there is anything at the config'd address.
 * If so, call the real probe to see what it is.
 */
int
nca_isa_match(device_t parent, cfdata_t cf, void *aux)
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_tag_t memt = ia->ia_memt;
	bus_space_handle_t ioh;
	struct nca_isa_probe_data epd;
	int rv = 0;

	if (ISA_DIRECT_CONFIG(ia))
		return 0;

	/* See if we are looking for a port- or memory-mapped adapter */
	if (ia->ia_nio > 0 || ia->ia_io[0].ir_addr != ISA_UNKNOWN_PORT) {
		/* Port-mapped card */
		if (bus_space_map(iot, ia->ia_io[0].ir_addr, NCA_ISA_IOSIZE,
		    0, &ioh))
			return 0;

		/* See if a 53C80/53C400 is there */
		rv = nca_isa_find(iot, ioh, 0x07, &epd);

		bus_space_unmap(iot, ioh, NCA_ISA_IOSIZE);

		if (rv) {
			ia->ia_nio = 1;
			ia->ia_io[0].ir_size = NCA_ISA_IOSIZE;

			ia->ia_niomem = 0;
			ia->ia_ndrq = 0;
		}
	} else if (ia->ia_niomem > 0) {
		/* Memory-mapped card */
		if (bus_space_map(memt, ia->ia_iomem[0].ir_addr, 0x4000,
		    0, &ioh))
			return 0;

		/* See if a 53C80/53C400 is somewhere in this para. */
		rv = nca_isa_find(memt, ioh, 0x03ff0, &epd);

		bus_space_unmap(memt, ioh, 0x04000);

		if (rv) {
			ia->ia_niomem = 1;
			ia->ia_iomem[0].ir_addr += epd.sc_reg_offset;
			ia->ia_iomem[0].ir_size = NCA_ISA_IOSIZE;

			ia->ia_nio = 0;
			ia->ia_ndrq = 0;
		}
	}

	return rv;
}

/*
 * Attach this instance, and then all the sub-devices
 */
void
nca_isa_attach(device_t parent, device_t self, void *aux)
{
	struct nca_isa_softc *esc = device_private(self);
	struct ncr5380_softc *sc = &esc->sc_ncr5380;
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	struct nca_isa_probe_data epd;
	isa_chipset_tag_t ic = ia->ia_ic;

	sc->sc_dev = self;
	aprint_normal("\n");

	if (ia->ia_nio > 0) {
		iot = ia->ia_iot;
		if (bus_space_map(iot, ia->ia_io[0].ir_addr, NCA_ISA_IOSIZE,
		    0, &ioh)) {
			aprint_error_dev(self, "can't map i/o space\n");
			return;
		}
	} else {
		KASSERT(ia->ia_niomem > 0);
		iot = ia->ia_memt;
		if (bus_space_map(iot, ia->ia_iomem[0].ir_addr, NCA_ISA_IOSIZE,
		    0, &ioh)) {
			aprint_error_dev(self, "can't map mem space\n");
			return;
		}
	}

	switch (nca_isa_find(iot, ioh, NCA_ISA_IOSIZE, &epd)) {
	case 0:
		/* Not found- must have gone away */
		aprint_error_dev(self, "nca_isa_find failed\n");
		return;
	case CTLR_NCR_5380:
		aprint_normal_dev(self, "NCR 53C80 detected\n");
		sc->sci_r0 = 0;
		sc->sci_r1 = 1;
		sc->sci_r2 = 2;
		sc->sci_r3 = 3;
		sc->sci_r4 = 4;
		sc->sci_r5 = 5;
		sc->sci_r6 = 6;
		sc->sci_r7 = 7;
		sc->sc_rev = NCR_VARIANT_NCR5380;
		break;
	case CTLR_NCR_53C400:
		aprint_normal_dev(self, "NCR 53C400 detected\n");
		sc->sci_r0 = C400_5380_REG_OFFSET + 0;
		sc->sci_r1 = C400_5380_REG_OFFSET + 1;
		sc->sci_r2 = C400_5380_REG_OFFSET + 2;
		sc->sci_r3 = C400_5380_REG_OFFSET + 3;
		sc->sci_r4 = C400_5380_REG_OFFSET + 4;
		sc->sci_r5 = C400_5380_REG_OFFSET + 5;
		sc->sci_r6 = C400_5380_REG_OFFSET + 6;
		sc->sci_r7 = C400_5380_REG_OFFSET + 7;
		sc->sc_rev = NCR_VARIANT_NCR53C400;
		break;
	case CTLR_PAS16:
		aprint_normal_dev(self, "ProAudio Spectrum 16 detected\n");
		sc->sc_rev = NCR_VARIANT_PAS16;
		break;
	}

	/*
	 * MD function pointers used by the MI code.
	 */
	sc->sc_pio_out = ncr5380_pio_out;
	sc->sc_pio_in =  ncr5380_pio_in;
	sc->sc_dma_alloc = NULL;
	sc->sc_dma_free  = NULL;
	sc->sc_dma_setup = NULL;
	sc->sc_dma_start = NULL;
	sc->sc_dma_poll  = NULL;
	sc->sc_dma_eop   = NULL;
	sc->sc_dma_stop  = NULL;
	sc->sc_intr_on   = NULL;
	sc->sc_intr_off  = NULL;

	if (ia->ia_nirq > 0 && ia->ia_irq[0].ir_irq != ISA_UNKNOWN_IRQ) {
		esc->sc_ih = isa_intr_establish(ic, ia->ia_irq[0].ir_irq,
		    IST_EDGE, IPL_BIO, ncr5380_intr, esc);
		if (esc->sc_ih == NULL) {
			aprint_error_dev(self,
			    "couldn't establish interrupt\n");
			return;
		}
	} else
		sc->sc_flags |= NCR5380_FORCE_POLLING;


	/*
	 * Support the "options" (config file flags).
	 * Disconnect/reselect is a per-target mask.
	 * Interrupts and DMA are per-controller.
	 */
#if 0
	esc->sc_options = 0x00000;	/* no options */
#else
	esc->sc_options = 0x0ffff;	/* all options except force poll */
#endif

	sc->sc_no_disconnect = (esc->sc_options & NCA_NO_DISCONNECT);
	sc->sc_parity_disable = (esc->sc_options & NCA_NO_PARITY_CHK) >> 8;
	if (esc->sc_options & NCA_FORCE_POLLING)
		sc->sc_flags |= NCR5380_FORCE_POLLING;
	sc->sc_min_dma_len = MIN_DMA_LEN;


	/*
	 * Initialize fields used by the MI code
	 */
	sc->sc_regt = iot;
	sc->sc_regh = ioh;

	/*
	 * Fill in our portion of the scsipi_adapter.
	 */
	sc->sc_adapter.adapt_request = ncr5380_scsipi_request;
	sc->sc_adapter.adapt_minphys = minphys;

	/*
	 * Fill in our portion of the scsipi_channel.
	 */

	sc->sc_channel.chan_id = 7;

	/*
	 *  Initialize nca board itself.
	 */
	ncr5380_attach(sc);
}
