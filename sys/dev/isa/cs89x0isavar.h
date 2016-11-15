/* $NetBSD: cs89x0isavar.h,v 1.5 2007/03/04 06:02:10 christos Exp $ */

/*-
 * Copyright (c)2001 YAMAMOTO Takashi,
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
 */

void	cs_process_rx_dma(struct cs_softc *);
void	cs_isa_dma_chipinit(struct cs_softc *);
void	cs_isa_dma_attach(struct cs_softc *);

struct cs_softc_isa {
	struct cs_softc sc_cs;

	isa_chipset_tag_t sc_ic;	/* ISA chipset */
	int	sc_drq;			/* DRQ line */

	bus_size_t sc_dmasize;		/* DMA size (16k or 64k) */
	bus_addr_t sc_dmaaddr;		/* DMA address */
	char *sc_dmabase;		/* base DMA address (KVA) */
	char *sc_dmacur;		/* current DMA address (KVA) */
};
