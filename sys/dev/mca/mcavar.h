/*	$NetBSD: mcavar.h,v 1.12 2012/10/27 17:18:26 chs Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * Copyright (c) 1996-1999 Scott D. Telford.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Scott Telford <s.telford@ed.ac.uk>.
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

#ifndef _DEV_MCA_MCAVAR_H_
#define	_DEV_MCA_MCAVAR_H_

/*
 * Definitions for MCA autoconfiguration.
 *
 * This file describes types and functions which are used for MCA
 * configuration.  Some of this information is machine-specific, and is
 * separated into mca_machdep.h.
 */

#include <sys/bus.h>
#include <machine/mca_machdep.h>

struct mcabus_attach_args {
	const char *_mba_busname;	/* XXX placeholder */
	bus_space_tag_t mba_iot;	/* MCA I/O space tag */
	bus_space_tag_t mba_memt;	/* MCA mem space tag */
	bus_dma_tag_t mba_dmat;		/* MCA DMA tag */
	mca_chipset_tag_t mba_mc;	/* currently unused */
	int		mba_bus;	/* MCA bus number */
};


struct mca_attach_args {
	bus_space_tag_t ma_iot;		/* MCA I/O space tag */
	bus_space_tag_t ma_memt;	/* MCA mem space tag */
	bus_dma_tag_t ma_dmat;		/* MCA DMA tag */
	mca_chipset_tag_t ma_mc;	/* currently unused */

	int ma_slot;			/* MCA slot number */
	int ma_pos[8];			/* MCA POS register values */
	int ma_id;			/* MCA device ID (POS1 + POS2<<8) */
};

int	mcabusprint(void *, const char *);

void	mca_devinfo(int, char *, size_t);
int	mca_match_disabled(int);

#endif /* _DEV_MCA_MCAVAR_H_ */
