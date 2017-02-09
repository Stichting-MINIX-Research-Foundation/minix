/*	$NetBSD: iic_cpcbus.c,v 1.13 2012/10/27 17:18:20 chs Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: iic_cpcbus.c,v 1.13 2012/10/27 17:18:20 chs Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>

#include <sys/bus.h>
#include <dev/ic/cpc700reg.h>
#include <dev/ic/cpc700var.h>

struct iic_cpcbus_softc {
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
	void *sc_ih;
};

static int	iic_cpcbus_match(device_t, cfdata_t, void *);
static void	iic_cpcbus_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(iic_cpcbus, sizeof(struct iic_cpcbus_softc),
    iic_cpcbus_match, iic_cpcbus_attach, NULL, NULL);

int
iic_cpcbus_match(device_t parent, cfdata_t cf, void *aux)
{
	struct cpcbus_attach_args *caa = aux;

	return (strcmp(caa->cpca_name, "cpciic") == 0);
}

void
iic_cpcbus_attach(device_t parent, device_t self, void *aux)
{
	struct cpcbus_attach_args *caa = aux;
	struct iic_cpcbus_softc *sc = device_private(self);

	sc->sc_iot = caa->cpca_tag;
	if (bus_space_map(sc->sc_iot, caa->cpca_addr, CPC_IIC_SIZE, 0,
			  &sc->sc_ioh)) {
		aprint_error_dev(self, "can't map i/o space\n");
		return;
	}

	printf(": driver not implemented\n");
}
