/*	$NetBSD: ad1848var.h,v 1.44 2011/11/23 23:07:32 jmcneill Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Ken Hornstein and John Kohl.
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
 * Copyright (c) 1994 John Brezak
 * Copyright (c) 1991-1993 Regents of the University of California.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
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
 */

#define AD1848_NPORT	4

#include <dev/ic/ad1848var.h>

struct ad1848_isa_softc {
	struct	ad1848_softc sc_ad1848;	/* AD1848 device */
	void	*sc_ih;			/* interrupt vectoring */
	isa_chipset_tag_t sc_ic;	/* ISA chipset info */

	char	sc_playrun;		/* running in continuous mode */
	char	sc_recrun;		/* running in continuous mode */

	int	sc_irq;			/* interrupt */
	int	sc_playdrq;		/* playback DMA */
	bus_size_t sc_play_maxsize;	/* playback DMA size */
	int	sc_recdrq;		/* record/capture DMA */
	bus_size_t sc_rec_maxsize;	/* record/capture DMA size */

	u_long	sc_interrupts;		/* number of interrupts taken */
	void	(*sc_pintr)(void *);	/* play DMA completion intr handler */
	void	*sc_parg;		/* arg for sc_pintr() */
	void	(*sc_rintr)(void *);	/* rec. DMA completion intr handler */
	void	*sc_rarg;		/* arg for sc_rintr() */

	/* Only used by gus XXX */
	int	sc_iobase;

#ifndef AUDIO_NO_POWER_CTL
	int	(*powerctl)(void *, int);
	void	*powerarg;
#endif
};

#ifdef _KERNEL
int	ad1848_isa_mapprobe(struct ad1848_isa_softc *, int);
int	ad1848_isa_probe(struct ad1848_isa_softc *);
void	ad1848_isa_unmap(struct ad1848_isa_softc *);
void	ad1848_isa_attach(struct ad1848_isa_softc *);

int	ad1848_isa_open(void *, int);
void	ad1848_isa_close(void *);

int	ad1848_isa_trigger_output(void *, void *, void *, int,
	    void (*)(void *), void *, const audio_params_t *);
int	ad1848_isa_trigger_input(void *, void *, void *, int,
	    void (*)(void *), void *, const audio_params_t *);
int	ad1848_isa_halt_output(void *);
int	ad1848_isa_halt_input(void *);

int	ad1848_isa_intr(void *);

void   *ad1848_isa_malloc(void *, int, size_t);
void	ad1848_isa_free(void *, void *, size_t);
size_t	ad1848_isa_round_buffersize(void *, int, size_t);
paddr_t	ad1848_isa_mappage(void *, void *, off_t, int);
int	ad1848_isa_get_props(void *);
#endif
