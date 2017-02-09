/*	$NetBSD: if_cs_isa.c,v 1.27 2015/04/13 16:33:24 riastradh Exp $	*/

/*
 * Copyright 1997
 * Digital Equipment Corporation. All rights reserved.
 *
 * This software is furnished under license and may be used and
 * copied only in accordance with the following terms and conditions.
 * Subject to these conditions, you may download, copy, install,
 * use, modify and distribute this software in source and/or binary
 * form. No title or ownership is transferred hereby.
 *
 * 1) Any source code used, modified or distributed must reproduce
 *    and retain this copyright notice and list of conditions as
 *    they appear in the source file.
 *
 * 2) No right is granted to use any trade name, trademark, or logo of
 *    Digital Equipment Corporation. Neither the "Digital Equipment
 *    Corporation" name nor any trademark or logo of Digital Equipment
 *    Corporation may be used to endorse or promote products derived
 *    from this software without the prior written permission of
 *    Digital Equipment Corporation.
 *
 * 3) This software is provided "AS-IS" and any express or implied
 *    warranties, including but not limited to, any implied warranties
 *    of merchantability, fitness for a particular purpose, or
 *    non-infringement are disclaimed. In no event shall DIGITAL be
 *    liable for any damages whatsoever, and in particular, DIGITAL
 *    shall not be liable for special, indirect, consequential, or
 *    incidental damages or damages for lost profits, loss of
 *    revenue or loss of use, whether such damages arise in contract,
 *    negligence, tort, under statute, in equity, at law or otherwise,
 *    even if advised of the possibility of such damage.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_cs_isa.c,v 1.27 2015/04/13 16:33:24 riastradh Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/ic/cs89x0reg.h>
#include <dev/ic/cs89x0var.h>
#include <dev/isa/cs89x0isavar.h>

static int	cs_isa_probe(device_t, cfdata_t, void *);
static void	cs_isa_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(cs_isa, sizeof(struct cs_softc_isa),
    cs_isa_probe, cs_isa_attach, NULL, NULL);

int
cs_isa_probe(device_t parent, cfdata_t cf, void *aux)
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_tag_t memt = ia->ia_memt;
	bus_space_handle_t ioh, memh;
	struct cs_softc sc;
	int rv = 0, have_io = 0, have_mem = 0;
	u_int16_t isa_cfg, isa_membase;
	int maddr, irq;

	if (ia->ia_nio < 1)
		return (0);
	if (ia->ia_nirq < 1)
		return (0);

	if (ISA_DIRECT_CONFIG(ia))
		return (0);

	/*
	 * Disallow wildcarded I/O base.
	 */
	if (ia->ia_io[0].ir_addr == ISA_UNKNOWN_PORT)
		return (0);

	if (ia->ia_niomem > 0)
		maddr = ia->ia_iomem[0].ir_addr;
	else
		maddr = ISA_UNKNOWN_IOMEM;

	/*
	 * Map the I/O space.
	 */
	if (bus_space_map(ia->ia_iot, ia->ia_io[0].ir_addr, CS8900_IOSIZE,
	    0, &ioh))
		goto out;
	have_io = 1;

	memset(&sc, 0, sizeof sc);
	sc.sc_iot = iot;
	sc.sc_ioh = ioh;
	/* Verify that it's a Crystal product. */
	if (CS_READ_PACKET_PAGE_IO(&sc, PKTPG_EISA_NUM) !=
	    EISA_NUM_CRYSTAL)
		goto out;

	/*
	 * Verify that it's a supported chip.
	 */
	switch (CS_READ_PACKET_PAGE_IO(&sc, PKTPG_PRODUCT_ID) &
	    PROD_ID_MASK) {
	case PROD_ID_CS8900:
#ifdef notyet
	case PROD_ID_CS8920:
	case PROD_ID_CS8920M:
#endif
		break;
	default:
		/* invalid product ID */
		goto out;
	}

	/*
	 * If the IRQ or memory address were not specified, read the
	 * ISA_CFG EEPROM location.
	 */
	if (maddr == ISA_UNKNOWN_IOMEM ||
	    ia->ia_irq[0].ir_irq == ISA_UNKNOWN_IRQ) {
		if (cs_verify_eeprom(&sc) == CS_ERROR) {
			printf("cs_isa_probe: EEPROM bad or missing\n");
			goto out;
		}
		if (cs_read_eeprom(&sc, EEPROM_ISA_CFG, &isa_cfg)
		    == CS_ERROR) {
			printf("cs_isa_probe: unable to read ISA_CFG\n");
			goto out;
		}
	}

	/*
	 * If the IRQ wasn't specified, get it from the EEPROM.
	 */
	if (ia->ia_irq[0].ir_irq == ISA_UNKNOWN_IRQ) {
		irq = isa_cfg & ISA_CFG_IRQ_MASK;
		if (irq == 3)
			irq = 5;
		else
			irq += 10;
	} else
		irq = ia->ia_irq[0].ir_irq;

	/*
	 * If the memory address wasn't specified, get it from the EEPROM.
	 */
	if (maddr == ISA_UNKNOWN_IOMEM) {
		if ((isa_cfg & ISA_CFG_MEM_MODE) == 0) {
			/* EEPROM says don't use memory mode. */
			goto out;
		}
		if (cs_read_eeprom(&sc, EEPROM_MEM_BASE, &isa_membase)
		    == CS_ERROR) {
			printf("cs_isa_probe: unable to read MEM_BASE\n");
			goto out;
		}

		isa_membase &= MEM_BASE_MASK;
		maddr = (int)isa_membase << 8;
	}

	/*
	 * We now have a valid mem address; attempt to map it.
	 */
	if (bus_space_map(ia->ia_memt, maddr, CS8900_MEMSIZE, 0, &memh)) {
		/* Can't map it; fall back on i/o-only mode. */
		printf("cs_isa_probe: unable to map memory space\n");
		maddr = ISA_UNKNOWN_IOMEM;
	} else
		have_mem = 1;

	ia->ia_nio = 1;
	ia->ia_io[0].ir_size = CS8900_IOSIZE;

	if (maddr == ISA_UNKNOWN_IOMEM)
		ia->ia_niomem = 0;
	else {
		ia->ia_niomem = 1;
		ia->ia_iomem[0].ir_addr = maddr;
		ia->ia_iomem[0].ir_size = CS8900_MEMSIZE;
	}

	ia->ia_nirq = 1;
	ia->ia_irq[0].ir_irq = irq;

	rv = 1;

 out:
	if (have_io)
		bus_space_unmap(iot, ioh, CS8900_IOSIZE);
	if (have_mem)
		bus_space_unmap(memt, memh, CS8900_MEMSIZE);

	return (rv);
}

void
cs_isa_attach(device_t parent, device_t self, void *aux)
{
	struct cs_softc_isa *isc = device_private(self);
	struct cs_softc *sc = &isc->sc_cs;
	struct isa_attach_args *ia = aux;

	sc->sc_dev = self;
	isc->sc_ic = ia->ia_ic;
	sc->sc_iot = ia->ia_iot;
	sc->sc_memt = ia->ia_memt;

	if (ia->ia_ndrq > 0)
		isc->sc_drq = ia->ia_drq[0].ir_drq;
	else
		isc->sc_drq = -1;

	sc->sc_irq = ia->ia_irq[0].ir_irq;

	printf("\n");

	/*
	 * Map the device.
	 */
	if (bus_space_map(sc->sc_iot, ia->ia_io[0].ir_addr, CS8900_IOSIZE,
	    0, &sc->sc_ioh)) {
		aprint_error_dev(self, "unable to map i/o space\n");
		return;
	}

	/*
	 * Validate IRQ.
	 */
	if (CS8900_IRQ_ISVALID(sc->sc_irq) == 0) {
		aprint_error_dev(self, "invalid IRQ %d\n", sc->sc_irq);
		return;
	}

	/*
	 * Map the memory space if it was specified.  If we can do this,
	 * we set ourselves up to use memory mode forever.  Otherwise,
	 * we fall back on I/O mode.
	 */
	if (ia->ia_iomem[0].ir_addr != ISA_UNKNOWN_IOMEM &&
	    ia->ia_iomem[0].ir_size == CS8900_MEMSIZE &&
	    CS8900_MEMBASE_ISVALID(ia->ia_iomem[0].ir_addr)) {
		if (bus_space_map(sc->sc_memt, ia->ia_iomem[0].ir_addr,
		    CS8900_MEMSIZE, 0, &sc->sc_memh)) {
			aprint_error_dev(self, "unable to map memory space\n");
		} else {
			sc->sc_cfgflags |= CFGFLG_MEM_MODE;
			sc->sc_pktpgaddr = ia->ia_iomem[0].ir_addr;
		}
	}

	sc->sc_ih = isa_intr_establish(ia->ia_ic, sc->sc_irq, IST_EDGE,
	    IPL_NET, cs_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(self, "unable to establish interrupt\n");
		return;
	}

	sc->sc_dma_chipinit = cs_isa_dma_chipinit;
	sc->sc_dma_attach = cs_isa_dma_attach;
	sc->sc_dma_process_rx = cs_process_rx_dma;

	cs_attach(sc, NULL, NULL, 0, 0);
}
