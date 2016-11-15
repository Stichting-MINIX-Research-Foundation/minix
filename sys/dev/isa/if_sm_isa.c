/*	$NetBSD: if_sm_isa.c,v 1.23 2012/10/27 17:18:24 chs Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_sm_isa.c,v 1.23 2012/10/27 17:18:24 chs Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/select.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#include <sys/intr.h>
#include <sys/bus.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/ic/smc91cxxreg.h>
#include <dev/ic/smc91cxxvar.h>

#include <dev/isa/isavar.h>

int	sm_isa_match(device_t, cfdata_t, void *);
void	sm_isa_attach(device_t, device_t, void *);

struct sm_isa_softc {
	struct	smc91cxx_softc sc_smc;		/* real "smc" softc */

	/* ISA-specific goo. */
	void	*sc_ih;				/* interrupt cookie */
};

CFATTACH_DECL_NEW(sm_isa, sizeof(struct sm_isa_softc),
    sm_isa_match, sm_isa_attach, NULL, NULL);

int
sm_isa_match(device_t parent, cfdata_t match, void *aux)
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	u_int16_t tmp;
	int rv = 0;
	extern const char *smc91cxx_idstrs[];

	if (ia->ia_nio < 1)
		return (0);
	if (ia->ia_nirq < 1)
		return (0);

	if (ISA_DIRECT_CONFIG(ia))
		return (0);

	/* Disallow wildcarded values. */
	if (ia->ia_io[0].ir_addr == ISA_UNKNOWN_PORT)
		return (0);
	if (ia->ia_irq[0].ir_irq == ISA_UNKNOWN_IRQ)
		return (0);

	/* Map i/o space. */
	if (bus_space_map(iot, ia->ia_io[0].ir_addr, SMC_IOSIZE, 0, &ioh))
		return (0);

	/* Check that high byte of BANK_SELECT is what we expect. */
	tmp = bus_space_read_2(iot, ioh, BANK_SELECT_REG_W);
	if ((tmp & BSR_DETECT_MASK) != BSR_DETECT_VALUE)
		goto out;

	/*
	 * Switch to bank 0 and perform the test again.
	 * XXX INVASIVE!
	 */
	bus_space_write_2(iot, ioh, BANK_SELECT_REG_W, 0);
	tmp = bus_space_read_2(iot, ioh, BANK_SELECT_REG_W);
	if ((tmp & BSR_DETECT_MASK) != BSR_DETECT_VALUE)
		goto out;

	/*
	 * Switch to bank 1 and check the base address register.
	 * XXX INVASIVE!
	 */
	bus_space_write_2(iot, ioh, BANK_SELECT_REG_W, 1);
	tmp = bus_space_read_2(iot, ioh, BASE_ADDR_REG_W);
	if (ia->ia_io[0].ir_addr != ((tmp >> 3) & 0x3e0))
		goto out;

	/*
	 * Check for a recognized chip id.
	 * XXX INVASIVE!
	 */
	bus_space_write_2(iot, ioh, BANK_SELECT_REG_W, 3);
	tmp = bus_space_read_2(iot, ioh, REVISION_REG_W);
	if (smc91cxx_idstrs[RR_ID(tmp)] == NULL)
		goto out;

	/*
	 * Assume we have an SMC91Cxx.
	 */
	ia->ia_nio = 1;
	ia->ia_io[0].ir_size = SMC_IOSIZE;

	ia->ia_nirq = 1;

	ia->ia_niomem = 0;
	ia->ia_ndrq = 0;

	rv = 1;

 out:
	bus_space_unmap(iot, ioh, SMC_IOSIZE);
	return (rv);
}

void
sm_isa_attach(device_t parent, device_t self, void *aux)
{
	struct sm_isa_softc *isc = device_private(self);
	struct smc91cxx_softc *sc = &isc->sc_smc;
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;

	printf("\n");

	/* Map i/o space. */
	if (bus_space_map(iot, ia->ia_io[0].ir_addr, SMC_IOSIZE, 0, &ioh))
		panic("sm_isa_attach: can't map i/o space");

	sc->sc_dev = self;
	sc->sc_bst = iot;
	sc->sc_bsh = ioh;

	/* should always be enabled */
	sc->sc_flags |= SMC_FLAGS_ENABLED;

	/* XXX Should get Ethernet address from EEPROM!! */

	/* Perform generic intialization. */
	smc91cxx_attach(sc, NULL);

	/* Establish the interrupt handler. */
	isc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq[0].ir_irq,
	    IST_EDGE, IPL_NET, smc91cxx_intr, sc);
	if (isc->sc_ih == NULL)
		aprint_error_dev(self, "couldn't establish interrupt handler\n");
}
