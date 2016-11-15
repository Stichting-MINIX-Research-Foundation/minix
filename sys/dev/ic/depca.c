/*	$NetBSD: depca.c,v 1.17 2008/04/28 20:23:49 martin Exp $	*/

/*-
 * Copyright (c) 1997, 1998, 2000 The NetBSD Foundation, Inc.
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

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell and Rick Macklem.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)if_le.c	8.2 (Berkeley) 11/16/93
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: depca.c,v 1.17 2008/04/28 20:23:49 martin Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#include <sys/cpu.h>
#include <sys/intr.h>
#include <sys/bus.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#include <dev/ic/lancereg.h>
#include <dev/ic/lancevar.h>
#include <dev/ic/am7990reg.h>
#include <dev/ic/am7990var.h>
#include <dev/ic/depcareg.h>
#include <dev/ic/depcavar.h>

struct le_depca_softc {
	struct am7990_softc sc_am7990;	/* glue to MI code */

	void *sc_ih;
};

int	le_depca_match(device_t, cfdata_t, void *);
void	le_depca_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(le_depca, sizeof(struct le_depca_softc),
    le_depca_match, le_depca_attach, NULL, NULL);

void	depca_copytobuf(struct lance_softc *, void *, int, int);
void	depca_copyfrombuf(struct lance_softc *, void *, int, int);
void	depca_zerobuf(struct lance_softc *, int, int);

struct depca_attach_args {
	const char *da_name;
};

int	depca_print(void *, const char *);

void
depca_attach(struct depca_softc *sc)
{
	struct depca_attach_args da;

	da.da_name = "le";

	(void)config_found(sc->sc_dev, &da, depca_print);
}

int
depca_print(void *aux, const char *pnp)
{
	struct depca_attach_args *da = aux;

	if (pnp)
		aprint_normal("%s at %s", da->da_name, pnp);

	return (UNCONF);
}

void
depca_wrcsr(struct lance_softc *sc, uint16_t port, uint16_t val)
{
	struct depca_softc *dsc = device_private(device_parent(sc->sc_dev));

	bus_space_write_2(dsc->sc_iot, dsc->sc_ioh, DEPCA_RAP, port);
	bus_space_write_2(dsc->sc_iot, dsc->sc_ioh, DEPCA_RDP, val);
}

uint16_t
depca_rdcsr(struct lance_softc *sc, uint16_t port)
{
	struct depca_softc *dsc = device_private(device_parent(sc->sc_dev));

	bus_space_write_2(dsc->sc_iot, dsc->sc_ioh, DEPCA_RAP, port);
	return (bus_space_read_2(dsc->sc_iot, dsc->sc_ioh, DEPCA_RDP));
}

int
depca_readprom(bus_space_tag_t iot, bus_space_handle_t ioh, uint8_t *laddr)
{
	int port, i;

	/*
	 * Extract the physical MAC address from the ROM.
	 *
	 * The address PROM is 32 bytes wide, and we access it through
	 * a single I/O port.  On each read, it rotates to the next
	 * position.  We find the ethernet address by looking for a
	 * particular sequence of bytes (0xff, 0x00, 0x55, 0xaa, 0xff,
	 * 0x00, 0x55, 0xaa), and then reading the next 8 bytes (the
	 * ethernet address and a checksum).
	 *
	 * It appears that the PROM can be at one of two locations, so
	 * we just try both.
	 */
	port = DEPCA_ADP;
	for (i = 0; i < 32; i++)
		if (bus_space_read_1(iot, ioh, port) == 0xff &&
		    bus_space_read_1(iot, ioh, port) == 0x00 &&
		    bus_space_read_1(iot, ioh, port) == 0x55 &&
		    bus_space_read_1(iot, ioh, port) == 0xaa &&
		    bus_space_read_1(iot, ioh, port) == 0xff &&
		    bus_space_read_1(iot, ioh, port) == 0x00 &&
		    bus_space_read_1(iot, ioh, port) == 0x55 &&
		    bus_space_read_1(iot, ioh, port) == 0xaa)
			goto found;
	port = DEPCA_ADP + 1;
	for (i = 0; i < 32; i++)
		if (bus_space_read_1(iot, ioh, port) == 0xff &&
		    bus_space_read_1(iot, ioh, port) == 0x00 &&
		    bus_space_read_1(iot, ioh, port) == 0x55 &&
		    bus_space_read_1(iot, ioh, port) == 0xaa &&
		    bus_space_read_1(iot, ioh, port) == 0xff &&
		    bus_space_read_1(iot, ioh, port) == 0x00 &&
		    bus_space_read_1(iot, ioh, port) == 0x55 &&
		    bus_space_read_1(iot, ioh, port) == 0xaa)
			goto found;
	aprint_error("depca: address not found\n");
	return (-1);

found:

	if (laddr) {
		for (i = 0; i < 6; i++)
			laddr[i] = bus_space_read_1(iot, ioh, port);
	}

#if 0
	sum =
	    (laddr[0] <<  2) +
	    (laddr[1] << 10) +
	    (laddr[2] <<  1) +
	    (laddr[3] <<  9) +
	    (laddr[4] <<  0) +
	    (laddr[5] <<  8);
	sum = (sum & 0xffff) + (sum >> 16);
	sum = (sum & 0xffff) + (sum >> 16);

	rom_sum = bus_space_read_1(iot, ioh, port);
	rom_sum |= bus_space_read_1(iot, ioh, port) << 8;

	if (sum != rom_sum) {
		aprint_error("depca: checksum mismatch; "
		    "calculated %04x != read %04x", sum, rom_sum);
		return (-1);
	}
#endif

	return (0);
}

int
le_depca_match(device_t parent, cfdata_t cf, void *aux)
{
	struct depca_attach_args *da = aux;

	return (strcmp(da->da_name, cf->cf_name) == 0);
}

void
le_depca_attach(device_t parent, device_t self, void *aux)
{
	struct depca_softc *dsc = device_private(parent);
	struct le_depca_softc *lesc = device_private(self);
	struct lance_softc *sc = &lesc->sc_am7990.lsc;
	uint8_t val;
	int i;

	sc->sc_dev = self;
	aprint_error("\n");

	/* I/O and memory spaces already mapped. */

	if (depca_readprom(dsc->sc_iot, dsc->sc_ioh, sc->sc_enaddr)) {
		aprint_error_dev(self, "can't read PROM\n");
		return;
	}

	bus_space_write_2(dsc->sc_iot, dsc->sc_ioh, DEPCA_CSR,
	    DEPCA_CSR_DUM | DEPCA_CSR_IEN | DEPCA_CSR_SHE |
	    (dsc->sc_memsize == 32*1024 ? DEPCA_CSR_LOW32K : 0));

	val = 0xff;
	for (;;) {
		uint8_t cv;

		bus_space_set_region_1(dsc->sc_memt, dsc->sc_memh, 0, val,
		    dsc->sc_memsize);
		for (i = 0; i < dsc->sc_memsize; i++) {
			cv = bus_space_read_1(dsc->sc_memt, dsc->sc_memh, i);
			if (cv != val) {
				aprint_error_dev(self,
				    "failed to clear memory at %d "
				    "(0x%02x != 0x%02x)\n",
				    i, cv, val);
				return;
			}
		}
		if (val == 0x00)
			break;
		val -= 0x55;
	}

	sc->sc_conf3 = LE_C3_ACON;
	sc->sc_mem = 0;			/* Not used. */
	sc->sc_addr = 0;
	sc->sc_memsize = dsc->sc_memsize;

	sc->sc_copytodesc = depca_copytobuf;
	sc->sc_copyfromdesc = depca_copyfrombuf;
	sc->sc_copytobuf = depca_copytobuf;
	sc->sc_copyfrombuf = depca_copyfrombuf;
	sc->sc_zerobuf = depca_zerobuf;

	sc->sc_rdcsr = depca_rdcsr;
	sc->sc_wrcsr = depca_wrcsr;
	sc->sc_hwinit = NULL;

	aprint_error("%s", device_xname(self));
	am7990_config(&lesc->sc_am7990);

	lesc->sc_ih = (*dsc->sc_intr_establish)(dsc, sc);
}

/*
 * Controller interrupt.
 */
int
depca_intredge(void *arg)
{

	if (am7990_intr(arg) == 0)
		return (0);
	for (;;)
		if (am7990_intr(arg) == 0)
			return (1);
}

/*
 * DEPCA shared memory access functions.
 */

void
depca_copytobuf(struct lance_softc *sc, void *from, int boff, int len)
{
	struct depca_softc *dsc = device_private(device_parent(sc->sc_dev));

	bus_space_write_region_1(dsc->sc_memt, dsc->sc_memh, boff,
	    from, len);
}

void
depca_copyfrombuf(struct lance_softc *sc, void *to, int boff, int len)
{
	struct depca_softc *dsc = device_private(device_parent(sc->sc_dev));

	bus_space_read_region_1(dsc->sc_memt, dsc->sc_memh, boff,
	    to, len);
}

void
depca_zerobuf(struct lance_softc *sc, int boff, int len)
{
	struct depca_softc *dsc = device_private(device_parent(sc->sc_dev));

	bus_space_set_region_1(dsc->sc_memt, dsc->sc_memh, boff,
	    0x00, len);
}
