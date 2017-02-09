/*	$NetBSD: sbusvar.h,v 1.27 2009/09/17 16:28:12 tsutsui Exp $ */

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

#ifndef _SBUS_VAR_H
#define _SBUS_VAR_H

#if defined(_KERNEL_OPT) && (defined(__sparc__) || defined(__sparc64__))
#include "opt_sparc_arch.h"
#endif

#include <machine/bsd_openprom.h>

struct sbus_softc;

typedef u_int32_t sbus_slot_t;
typedef u_int32_t sbus_offset_t;

/*
 * Sbus driver attach arguments.
 */
struct sbus_attach_args {
	int		sa_placeholder;	/* for obio attach args sharing */
	bus_space_tag_t	sa_bustag;
	bus_dma_tag_t	sa_dmatag;
	char		*sa_name;	/* PROM node name */
	int		sa_node;	/* PROM handle */
	struct openprom_addr *sa_reg;	/* Sbus register space for device */
	int		sa_nreg;	/* Number of Sbus register spaces */
#define sa_slot		sa_reg[0].oa_space
#define sa_offset	sa_reg[0].oa_base
#define sa_size		sa_reg[0].oa_size

	struct openprom_intr *sa_intr;	/* Sbus interrupts for device */
	int		sa_nintr;	/* Number of interrupts */
#define sa_pri		sa_intr[0].oi_pri

	u_int32_t	*sa_promvaddrs;/* PROM-supplied virtual addresses -- 32-bit */
	int		sa_npromvaddrs;	/* Number of PROM VAs */
#define sa_promvaddr	sa_promvaddrs[0]
	int		sa_frequency;	/* SBus clockrate */
};

/* sbus_attach_internal() is also used from obio.c */
void	sbus_attach_common(struct sbus_softc *, const char *, int,
				const char * const *);
int	sbus_print(void *, const char *);

int	sbus_setup_attach_args(
		struct sbus_softc *,
		bus_space_tag_t,
		bus_dma_tag_t,
		int,			/*node*/
		struct sbus_attach_args *);

void	sbus_destroy_attach_args(struct sbus_attach_args *);

#define sbus_bus_map(tag, slot, offset, sz, flags, hp) \
	bus_space_map(tag, BUS_ADDR(slot,offset), sz, flags, hp)
bus_addr_t	sbus_bus_addr(bus_space_tag_t, u_int, u_int);
void	sbus_promaddr_to_handle(bus_space_tag_t, u_int,
	bus_space_handle_t *);

#if notyet
/* variables per Sbus */
struct sbus_softc {
	device_t sc_dev;		/* base device */
	bus_space_tag_t	sc_bustag;
	bus_dma_tag_t	sc_dmatag;
	int	sc_clockfreq;		/* clock frequency (in Hz) */
	struct	openprom_range *sc_range;
	int	sc_nrange;
	int	sc_burst;		/* burst transfer sizes supported */
	/* machdep stuff follows here */
	int	*sc_intr2ipl;		/* Interrupt level translation */
};
#endif


/*
 * PROM-reported DMA burst sizes for the SBus
 */
#define SBUS_BURST_1	0x1
#define SBUS_BURST_2	0x2
#define SBUS_BURST_4	0x4
#define SBUS_BURST_8	0x8
#define SBUS_BURST_16	0x10
#define SBUS_BURST_32	0x20
#define SBUS_BURST_64	0x40

/* We use #defined(SUN4*) here while the ports are in flux */
#if defined(SUN4) || defined(SUN4C) || defined(SUN4M) || defined(SUN4D)
#include <sparc/dev/sbusvar.h>
#elif defined(SUN4U)
#include <sparc64/dev/sbusvar.h>
#endif

#endif /* _SBUS_VAR_H */
