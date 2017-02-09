/*	$NetBSD: pcctwovar.h,v 1.5 2012/10/27 17:18:27 chs Exp $	*/

/*-
 * Copyright (c) 2000, 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Steve C. Woodford
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

#ifndef	_MVME_PCCTWOVAR_H
#define	_MVME_PCCTWOVAR_H

/*
 * Structure used to attach PCC devices.
 */
struct pcctwo_attach_args {
	const char	*pa_name;	/* name of device */
	int		pa_ipl;		/* interrupt level */
	bus_dma_tag_t	pa_dmat;	/* DMA tag */
	bus_space_tag_t	pa_bust;	/* Bus tag */
	bus_addr_t	pa_offset;	/* Offset with 'Bus tag' bus space */

	bus_addr_t	_pa_base;
};

/*
 * Structure used to describe a device for autoconfiguration purposes.
 */
struct pcctwo_device {
	const char *pcc_name;	/* name of device (e.g. "clock") */
	bus_addr_t pcc_offset;	/* offset from PCC2 base */
};

/*
 * Macroes to make life easy when converting vector offset to interrupt
 * control register, and how to initialise the ICSR.
 */
#define VEC2ICSR(r,v)		((r) | (((v) | PCCTWO_ICR_IEN) << 8))
#define VEC2ICSR_REG(x)		((x) & 0xff)
#define VEC2ICSR_INIT(x)	((x) >> 8)

/* Shorthand for locators. */
#include "locators.h"
#define pcctwocf_ipl	cf_loc[PCCTWOCF_IPL]

/*
 * PCCChip2 driver's soft state structure
 */
struct pcctwo_softc {
	device_t		sc_dev;
	bus_space_tag_t		sc_bust;	/* PCCChip2's register tag */
	bus_space_handle_t	sc_bush;	/* PCCChip2's register handle */
	bus_dma_tag_t		sc_dmat;	/* PCCChip2's dma tag (unused)*/
	int			*sc_vec2icsr;	/* Translate vector to ICSR */
	int			sc_vecbase;
	void			*sc_isrcookie;
	void			(*sc_isrlink)(void *, int (*)(void *), void *,
				    int, int, struct evcnt *);
	void			(*sc_isrunlink)(void *, int);
	struct evcnt *		(*sc_isrevcnt)(void *, int);
#if defined(MVME162) || defined(MVME172)
	struct evcnt		sc_evcnt;
#endif
};

extern struct pcctwo_softc *sys_pcctwo;

extern void pcctwo_init(struct pcctwo_softc *,
		const struct pcctwo_device *, int);
extern struct evcnt *pcctwointr_evcnt(int);
extern void pcctwointr_establish(int, int (*)(void *), int, void *,
		struct evcnt *);
extern void pcctwointr_disestablish(int);

#endif	/* _MVME_PCCTWOVAR_H */
