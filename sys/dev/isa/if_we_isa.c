/*	$NetBSD: if_we_isa.c,v 1.23 2014/10/18 08:33:28 snj Exp $	*/

/*-
 * Copyright (c) 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 * Device driver for National Semiconductor DS8390/WD83C690 based ethernet
 * adapters.
 *
 * Copyright (c) 1994, 1995 Charles M. Hannum.  All rights reserved.
 *
 * Copyright (C) 1993, David Greenman.  This software may be used, modified,
 * copied, distributed, and sold, in both source and binary form provided that
 * the above copyright and these terms are retained.  Under no circumstances is
 * the author responsible for the proper functioning of this software, nor does
 * the author assume any responsibility for damages incurred with its use.
 */

/*
 * Device driver for the Western Digital/SMC 8003 and 8013 series,
 * and the SMC Elite Ultra (8216).
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_we_isa.c,v 1.23 2014/10/18 08:33:28 snj Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/socket.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_media.h>

#include <net/if_ether.h>

#include <sys/bus.h>
#include <sys/bswap.h>
#include <sys/intr.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#include <dev/ic/dp8390reg.h>
#include <dev/ic/dp8390var.h>
#include <dev/ic/wereg.h>
#include <dev/ic/wevar.h>

#include "ioconf.h"

#ifndef __BUS_SPACE_HAS_STREAM_METHODS
#define	bus_space_read_region_stream_2	bus_space_read_region_2
#define	bus_space_write_stream_2	bus_space_write_2
#define	bus_space_write_region_stream_2	bus_space_write_region_2
#endif

int	we_isa_probe(device_t, cfdata_t , void *);
void	we_isa_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(we_isa, sizeof(struct we_softc),
    we_isa_probe, we_isa_attach, NULL, NULL);

static const char *we_params(bus_space_tag_t, bus_space_handle_t,
    uint8_t *, bus_size_t *, uint8_t *, int *);

static const int we_584_irq[] = {
	9, 3, 5, 7, 10, 11, 15, 4,
};

static const int we_790_irq[] = {
	ISA_UNKNOWN_IRQ, 9, 3, 5, 7, 10, 11, 15,
};

int
we_isa_probe(device_t parent, cfdata_t cf, void *aux)
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t asict, memt;
	bus_space_handle_t asich, memh;
	bus_size_t memsize;
	int asich_valid, memh_valid;
	int i, is790, rv = 0;
	uint8_t x, type;

	asict = ia->ia_iot;
	memt = ia->ia_memt;

	asich_valid = memh_valid = 0;

	if (ia->ia_nio < 1)
		return 0;
	if (ia->ia_niomem < 1)
		return 0;
	if (ia->ia_nirq < 1)
		return 0;

	if (ISA_DIRECT_CONFIG(ia))
		return 0;

	/* Disallow wildcarded i/o addresses. */
	if (ia->ia_io[0].ir_addr == ISA_UNKNOWN_PORT)
		return 0;

	/* Disallow wildcarded mem address. */
	if (ia->ia_iomem[0].ir_addr == ISA_UNKNOWN_IOMEM)
		return 0;

	/* Attempt to map the device. */
	if (bus_space_map(asict, ia->ia_io[0].ir_addr, WE_NPORTS, 0, &asich))
		goto out;
	asich_valid = 1;

#ifdef TOSH_ETHER
	bus_space_write_1(asict, asich, WE_MSR, WE_MSR_POW);
#endif

	/*
	 * Attempt to do a checksum over the station address PROM.
	 * If it fails, it's probably not a WD/SMC board.  There is
	 * a problem with this, though.  Some clone WD8003E boards
	 * (e.g. Danpex) won't pass the checksum.  In this case,
	 * the checksum byte always seems to be 0.
	 */
	for (x = 0, i = 0; i < 8; i++)
		x += bus_space_read_1(asict, asich, WE_PROM + i);

	if (x != WE_ROM_CHECKSUM_TOTAL) {
		/* Make sure it's an 8003E clone... */
		if (bus_space_read_1(asict, asich, WE_CARD_ID) !=
		    WE_TYPE_WD8003E)
			goto out;

		/* Check the checksum byte. */
		if (bus_space_read_1(asict, asich, WE_PROM + 7) != 0)
			goto out;
	}

	/*
	 * Reset the card to force it into a known state.
	 */
#ifdef TOSH_ETHER
	bus_space_write_1(asict, asich, WE_MSR, WE_MSR_RST | WE_MSR_POW);
#else
	bus_space_write_1(asict, asich, WE_MSR, WE_MSR_RST);
#endif
	delay(100);

	bus_space_write_1(asict, asich, WE_MSR,
	    bus_space_read_1(asict, asich, WE_MSR) & ~WE_MSR_RST);

	/* Wait in case the card is reading its EEPROM. */
	delay(5000);

	/*
	 * Get parameters.
	 */
	if (we_params(asict, asich, &type, &memsize, NULL, &is790) == NULL)
		goto out;

	/* Allow user to override probed value. */
	if (ia->ia_iomem[0].ir_size)
		memsize = ia->ia_iomem[0].ir_size;

	/* Attempt to map the memory space. */
	if (bus_space_map(memt, ia->ia_iomem[0].ir_addr, memsize, 0, &memh))
		goto out;
	memh_valid = 1;

	/*
	 * If possible, get the assigned interrupt number from the card
	 * and use it.
	 */
	if (is790) {
		uint8_t hwr;

		/* Assemble together the encoded interrupt number. */
		hwr = bus_space_read_1(asict, asich, WE790_HWR);
		bus_space_write_1(asict, asich, WE790_HWR,
		    hwr | WE790_HWR_SWH);

		x = bus_space_read_1(asict, asich, WE790_GCR);
		i = ((x & WE790_GCR_IR2) >> 4) |
		    ((x & (WE790_GCR_IR1|WE790_GCR_IR0)) >> 2);
		bus_space_write_1(asict, asich, WE790_HWR,
		    hwr & ~WE790_HWR_SWH);

		if (ia->ia_irq[0].ir_irq != ISA_UNKNOWN_IRQ &&
		    ia->ia_irq[0].ir_irq != we_790_irq[i])
			aprint_error("%s%d: overriding configured "
			    "IRQ %d to %d\n", we_cd.cd_name, cf->cf_unit,
			    ia->ia_irq[0].ir_irq, we_790_irq[i]);
		ia->ia_irq[0].ir_irq = we_790_irq[i];
	} else if (type & WE_SOFTCONFIG) {
		/* Assemble together the encoded interrupt number. */
		i = (bus_space_read_1(asict, asich, WE_ICR) & WE_ICR_IR2) |
		    ((bus_space_read_1(asict, asich, WE_IRR) &
		      (WE_IRR_IR0 | WE_IRR_IR1)) >> 5);

		if (ia->ia_irq[0].ir_irq != ISA_UNKNOWN_IRQ &&
		    ia->ia_irq[0].ir_irq != we_584_irq[i])
			aprint_error("%s%d: overriding configured "
			    "IRQ %d to %d\n", we_cd.cd_name, cf->cf_unit,
			    ia->ia_irq[0].ir_irq, we_584_irq[i]);
		ia->ia_irq[0].ir_irq = we_584_irq[i];
	}

	/* So, we say we've found it! */
	ia->ia_nio = 1;
	ia->ia_io[0].ir_size = WE_NPORTS;

	ia->ia_niomem = 1;
	ia->ia_iomem[0].ir_size = memsize;

	ia->ia_nirq = 1;

	ia->ia_ndrq = 0;

	rv = 1;

 out:
	if (asich_valid)
		bus_space_unmap(asict, asich, WE_NPORTS);
	if (memh_valid)
		bus_space_unmap(memt, memh, memsize);
	return rv;
}

void
we_isa_attach(device_t parent, device_t self, void *aux)
{
	struct we_softc *wsc = device_private(self);
	struct dp8390_softc *sc = &wsc->sc_dp8390;
	struct isa_attach_args *ia = aux;
	bus_space_tag_t nict, asict, memt;
	bus_space_handle_t nich, asich, memh;
	const char *typestr;

	aprint_normal("\n");

	sc->sc_dev = self;
	nict = asict = ia->ia_iot;
	memt = ia->ia_memt;

	/* Map the device. */
	if (bus_space_map(asict, ia->ia_io[0].ir_addr, WE_NPORTS, 0, &asich)) {
		aprint_error_dev(self, "can't map nic i/o space\n");
		return;
	}

	if (bus_space_subregion(asict, asich, WE_NIC_OFFSET, WE_NIC_NPORTS,
	    &nich)) {
		aprint_error_dev(self, "can't subregion i/o space\n");
		return;
	}

	typestr = we_params(asict, asich, &wsc->sc_type, NULL,
	    &wsc->sc_flags, &sc->is790);
	if (typestr == NULL) {
		aprint_error_dev(self, "where did the card go?\n");
		return;
	}

	/*
	 * Map memory space.  Note we use the size that might have
	 * been overridden by the user.
	 */
	if (bus_space_map(memt, ia->ia_iomem[0].ir_addr,
	    ia->ia_iomem[0].ir_size, 0, &memh)) {
		aprint_error_dev(self, "can't map shared memory\n");
		return;
	}

	wsc->sc_asict = asict;
	wsc->sc_asich = asich;

	sc->sc_regt = nict;
	sc->sc_regh = nich;

	sc->sc_buft = memt;
	sc->sc_bufh = memh;

	wsc->sc_maddr = ia->ia_iomem[0].ir_addr;
	sc->mem_size = ia->ia_iomem[0].ir_size;

	/* Interface is always enabled. */
	sc->sc_enabled = 1;

	if (we_config(self, wsc, typestr))
		return;

	/*
	 * Enable the configured interrupt.
	 */
	if (sc->is790)
		bus_space_write_1(asict, asich, WE790_ICR,
		    bus_space_read_1(asict, asich, WE790_ICR) |
		    WE790_ICR_EIL);
	else if (wsc->sc_type & WE_SOFTCONFIG)
		bus_space_write_1(asict, asich, WE_IRR,
		    bus_space_read_1(asict, asich, WE_IRR) | WE_IRR_IEN);
	else if (ia->ia_irq[0].ir_irq == ISA_UNKNOWN_IRQ) {
		aprint_error_dev(self, "can't wildcard IRQ on a %s\n",
		    typestr);
		return;
	}

	/* Establish interrupt handler. */
	wsc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq[0].ir_irq,
	    IST_EDGE, IPL_NET, dp8390_intr, sc);
	if (wsc->sc_ih == NULL)
		aprint_error_dev(self, "can't establish interrupt\n");
}

static const char *
we_params(bus_space_tag_t asict, bus_space_handle_t asich, uint8_t *typep,
    bus_size_t *memsizep, uint8_t *flagp, int *is790p)
{
	const char *typestr;
	bus_size_t memsize;
	int is16bit, is790;
	uint8_t type;

	memsize = 8192;
	is16bit = is790 = 0;

	type = bus_space_read_1(asict, asich, WE_CARD_ID);
	switch (type) {
	case WE_TYPE_WD8003S:
		typestr = "WD8003S";
		break;
	case WE_TYPE_WD8003E:
		typestr = "WD8003E";
		break;
	case WE_TYPE_WD8003EB:
		typestr = "WD8003EB";
		break;
	case WE_TYPE_WD8003W:
		typestr = "WD8003W";
		break;
	case WE_TYPE_WD8013EBT:
		typestr = "WD8013EBT";
		memsize = 16384;
		is16bit = 1;
		break;
	case WE_TYPE_WD8013W:
		typestr = "WD8013W";
		memsize = 16384;
		is16bit = 1;
		break;
	case WE_TYPE_WD8013EP:		/* also WD8003EP */
		if (bus_space_read_1(asict, asich, WE_ICR) & WE_ICR_16BIT) {
			is16bit = 1;
			memsize = 16384;
			typestr = "WD8013EP";
		} else
			typestr = "WD8003EP";
		break;
	case WE_TYPE_WD8013WC:
		typestr = "WD8013WC";
		memsize = 16384;
		is16bit = 1;
		break;
	case WE_TYPE_WD8013EBP:
		typestr = "WD8013EBP";
		memsize = 16384;
		is16bit = 1;
		break;
	case WE_TYPE_WD8013EPC:
		typestr = "WD8013EPC";
		memsize = 16384;
		is16bit = 1;
		break;
	case WE_TYPE_SMC8216C:
	case WE_TYPE_SMC8216T:
	    {
		uint8_t hwr;

		typestr = (type == WE_TYPE_SMC8216C) ?
		    "SMC8216/SMC8216C" : "SMC8216T";

		hwr = bus_space_read_1(asict, asich, WE790_HWR);
		bus_space_write_1(asict, asich, WE790_HWR,
		    hwr | WE790_HWR_SWH);
		switch (bus_space_read_1(asict, asich, WE790_RAR) &
		    WE790_RAR_SZ64) {
		case WE790_RAR_SZ64:
			memsize = 65536;
			break;
		case WE790_RAR_SZ32:
			memsize = 32768;
			break;
		case WE790_RAR_SZ16:
			memsize = 16384;
			break;
		case WE790_RAR_SZ8:
			/* 8216 has 16K shared mem -- 8416 has 8K */
			typestr = (type == WE_TYPE_SMC8216C) ?
			    "SMC8416C/SMC8416BT" : "SMC8416T";
			memsize = 8192;
			break;
		}
		bus_space_write_1(asict, asich, WE790_HWR, hwr);

		is16bit = 1;
		is790 = 1;
		break;
	    }
#ifdef TOSH_ETHER
	case WE_TYPE_TOSHIBA1:
		typestr = "Toshiba1";
		memsize = 32768;
		is16bit = 1;
		break;
	case WE_TYPE_TOSHIBA4:
		typestr = "Toshiba4";
		memsize = 32768;
		is16bit = 1;
		break;
#endif
	default:
		/* Not one we recognize. */
		return NULL;
	}

	/*
	 * Make some adjustments to initial values depending on what is
	 * found in the ICR.
	 */
	if (is16bit && (type != WE_TYPE_WD8013EBT) &&
#ifdef TOSH_ETHER
	    (type != WE_TYPE_TOSHIBA1 && type != WE_TYPE_TOSHIBA4) &&
#endif
	    (bus_space_read_1(asict, asich, WE_ICR) & WE_ICR_16BIT) == 0) {
		is16bit = 0;
		memsize = 8192;
	}

#ifdef WE_DEBUG
	{
		int i;

		printf("we_params: type = 0x%x, typestr = %s, is16bit = %d, "
		    "memsize = %ld\n", type, typestr, is16bit, (u_long)memsize);
		for (i = 0; i < 8; i++)
			printf("     %d -> 0x%x\n", i,
			    bus_space_read_1(asict, asich, i));
	}
#endif

	if (typep != NULL)
		*typep = type;
	if (memsizep != NULL)
		*memsizep = memsize;
	if (flagp != NULL && is16bit)
		*flagp |= WE_16BIT_ENABLE;
	if (is790p != NULL)
		*is790p = is790;
	return typestr;
}
