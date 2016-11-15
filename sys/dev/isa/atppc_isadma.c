/* $NetBSD: atppc_isadma.c,v 1.7 2008/04/15 15:02:28 cegger Exp $ */

/*-
 * Copyright (c) 2001 Alcove - Nicolas Souchu
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * FreeBSD: src/sys/isa/ppc.c,v 1.26.2.5 2001/10/02 05:21:45 nsouch Exp
 *
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: atppc_isadma.c,v 1.7 2008/04/15 15:02:28 cegger Exp $");

#include "opt_atppc.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>

#include <sys/intr.h>
#include <sys/bus.h>

#include <dev/ic/atppcreg.h>
#include <dev/ic/atppcvar.h>

#include <dev/isa/isadmavar.h>
#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#include <dev/isa/atppc_isadma.h>

/* Enable DMA */
int
atppc_isadma_setup(struct atppc_softc * lsc, isa_chipset_tag_t ic, int drq)
{
	int error = 1;

	/* Reserve DRQ */
	if (isa_drq_alloc(ic, drq)) {
		ATPPC_DPRINTF(("%s(%s): cannot reserve DRQ line.\n", __func__,
			device_xname(lsc->sc_dev)));
		return error;
	}

	/* Get maximum DMA size for isa bus */
	lsc->sc_dma_maxsize = isa_dmamaxsize(ic, drq);

	/* Create dma mapping */
	error = isa_dmamap_create(ic, drq, lsc->sc_dma_maxsize,
		BUS_DMA_NOWAIT|BUS_DMA_ALLOCNOW);

	return error;
}

/* Start DMA operation over ISA bus */
int
atppc_isadma_start(isa_chipset_tag_t ic, int drq, void *buf, u_int nbytes,
	u_int8_t mode)
{
	return (isa_dmastart(ic, drq, buf, nbytes, NULL,
		((mode == ATPPC_DMA_MODE_WRITE) ? DMAMODE_WRITE :
		DMAMODE_READ) | DMAMODE_DEMAND, ((mode == ATPPC_DMA_MODE_WRITE)
		? BUS_DMA_WRITE : BUS_DMA_READ) | BUS_DMA_NOWAIT));
}

/* Stop DMA operation over ISA bus */
int
atppc_isadma_finish(isa_chipset_tag_t ic, int drq)
{
	isa_dmadone(ic, drq);
	return 0;
}

/* Abort DMA operation over ISA bus */
int
atppc_isadma_abort(isa_chipset_tag_t ic, int drq)
{
	isa_dmaabort(ic, drq);
	return 0;
}

/* Allocate memory for DMA over ISA bus */
int
atppc_isadma_malloc(isa_chipset_tag_t ic, int drq, void **buf, bus_addr_t *bus_addr, bus_size_t size)
{
	int error;

	error = isa_dmamem_alloc(ic, drq, size, bus_addr, BUS_DMA_WAITOK);
	if (error)
		return error;

	error = isa_dmamem_map(ic, drq, *bus_addr, size, buf, BUS_DMA_WAITOK);
	if (error)
		isa_dmamem_free(ic, drq, *bus_addr, size);

	return error;
}

/* Free memory allocated by atppc_isadma_malloc() */
void
atppc_isadma_free(isa_chipset_tag_t ic, int drq, void ** buf, bus_addr_t * bus_addr, bus_size_t size)
{
	isa_dmamem_unmap(ic, drq, *buf, size);
	isa_dmamem_free(ic, drq, *bus_addr, size);
}
