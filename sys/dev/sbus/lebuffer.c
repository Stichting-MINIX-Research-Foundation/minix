/*	$NetBSD: lebuffer.c,v 1.36 2009/09/17 17:53:35 tsutsui Exp $ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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
__KERNEL_RCSID(0, "$NetBSD: lebuffer.c,v 1.36 2009/09/17 17:53:35 tsutsui Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <sys/bus.h>
#include <machine/autoconf.h>
#include <sys/cpu.h>

#include <dev/sbus/sbusvar.h>
#include <dev/sbus/lebuffervar.h>

int	lebufprint(void *, const char *);
int	lebufmatch(device_t, cfdata_t, void *);
void	lebufattach(device_t, device_t, void *);

CFATTACH_DECL_NEW(lebuffer, sizeof(struct lebuf_softc),
    lebufmatch, lebufattach, NULL, NULL);

int
lebufprint(void *aux, const char *busname)
{

	sbus_print(aux, busname);
	return (UNCONF);
}

int
lebufmatch(device_t parent, cfdata_t cf, void *aux)
{
	struct sbus_attach_args *sa = aux;

	return (strcmp(cf->cf_name, sa->sa_name) == 0);
}

/*
 * Attach all the sub-devices we can find
 */
void
lebufattach(device_t parent, device_t self, void *aux)
{
	struct sbus_attach_args *sa = aux;
	struct lebuf_softc *sc = device_private(self);
	struct sbus_softc *sbsc = device_private(parent);
	int node;
	int sbusburst;
	bus_space_tag_t bt = sa->sa_bustag;
	bus_dma_tag_t	dt = sa->sa_dmatag;
	bus_space_handle_t bh;

	sc->sc_dev = self;

	if (sbus_bus_map(bt, sa->sa_slot, sa->sa_offset, sa->sa_size,
			 BUS_SPACE_MAP_LINEAR, &bh) != 0) {
		aprint_error(": cannot map registers\n");
		return;
	}

	/*
	 * This device's "register space" is just a buffer where the
	 * Lance ring-buffers can be stored. Note the buffer's location
	 * and size, so the `le' driver can pick them up.
	 */
	sc->sc_buffer = bus_space_vaddr(bt, bh);
	sc->sc_bufsiz = sa->sa_size;

	node = sc->sc_node = sa->sa_node;

	/*
	 * Get transfer burst size from PROM
	 */
	sbusburst = sbsc->sc_burst;
	if (sbusburst == 0)
		sbusburst = SBUS_BURST_32 - 1; /* 1->16 */

	sc->sc_burst = prom_getpropint(node, "burst-sizes", -1);
	if (sc->sc_burst == -1)
		/* take SBus burst sizes */
		sc->sc_burst = sbusburst;

	/* Clamp at parent's burst sizes */
	sc->sc_burst &= sbusburst;

	printf(": %dK memory\n", sc->sc_bufsiz / 1024);

	/* search through children */
	for (node = firstchild(node); node; node = nextsibling(node)) {
		struct sbus_attach_args sax;
		sbus_setup_attach_args(sbsc,
				       bt, dt, node, &sax);
		(void)config_found(self, (void *)&sax, lebufprint);
		sbus_destroy_attach_args(&sax);
	}
}
