/*	$NetBSD: com_cpcbus.c,v 1.11 2008/04/28 20:23:49 martin Exp $	*/

/*
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at Sandburst Corp.
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
__KERNEL_RCSID(0, "$NetBSD: com_cpcbus.c,v 1.11 2008/04/28 20:23:49 martin Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/tty.h>
#include <sys/systm.h>

#include <sys/bus.h>

#include <dev/ic/cpc700reg.h>
#include <dev/ic/cpc700var.h>

#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>

struct com_cpc_softc {
	struct com_softc sc_com;
	void *sc_ih;
};

static int	com_cpc_match(device_t, cfdata_t , void *);
static void	com_cpc_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(com_cpcbus, sizeof(struct com_cpc_softc),
    com_cpc_match, com_cpc_attach, NULL, NULL);

int
com_cpc_match(device_t parent, cfdata_t cf, void *aux)
{
	struct cpcbus_attach_args *caa = aux;

	return (strcmp(caa->cpca_name, "com") == 0);
}

void
com_cpc_attach(device_t parent, device_t self, void *aux)
{
	struct cpcbus_attach_args *caa = aux;
	struct com_cpc_softc *sc = device_private(self);
	int iobase = caa->cpca_addr;
	int irq = caa->cpca_irq;
	bus_space_handle_t ioh;

	sc->sc_com.sc_dev = self;

	if (!com_is_console(caa->cpca_tag, iobase, &ioh) &&
	    bus_space_map(caa->cpca_tag, iobase, COM_NPORTS, 0, &ioh)) {
		aprint_error_dev(self, "can't map i/o space\n");
		return;
	}
	COM_INIT_REGS(sc->sc_com.sc_regs, caa->cpca_tag, ioh, iobase);

	sc->sc_com.sc_frequency = CPC_COM_SPEED(caa->cpca_freq);

	com_attach_subr(&sc->sc_com);

	sc->sc_ih = intr_establish(irq, IST_LEVEL, IPL_SERIAL, comintr,
				   &sc->sc_com);
}
