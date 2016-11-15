/*      $NetBSD: svvar.h,v 1.8 2012/10/27 17:18:35 chs Exp $ */

/*
 * Copyright (c) 1998 Constantine Paul Sapuntzakis
 * All rights reserved
 *
 * Author: Constantine Paul Sapuntzakis (csapuntz@cvs.openbsd.org)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The author's name or those of the contributors may be used to
 *    endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) AND CONTRIBUTORS
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

struct sv_softc {
	kmutex_t sc_lock;
	kmutex_t sc_intr_lock;
	void *sc_ih;			/* interrupt vectoring */

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
	bus_space_handle_t sc_dmaa_ioh;
	bus_space_handle_t sc_dmac_ioh;
	bus_dma_tag_t sc_dmatag;	/* DMA tag */

	bus_space_tag_t sc_opliot;
	bus_space_handle_t sc_oplioh;

	bus_space_tag_t sc_midiiot;
	bus_space_handle_t sc_midiioh;

	struct sv_dma *sc_dmas;

	void	(*sc_pintr)(void *);	/* DMA completion intr handler */
	void	*sc_parg;		/* arg for sc_intr() */

	void	(*sc_rintr)(void *);	/* DMA completion intr handler */
	void	*sc_rarg;		/* arg for sc_intr() */

	u_int	sc_record_source;	/* recording source mask */

	struct pci_attach_args sc_pa;
	char	sc_dmaset;
};

