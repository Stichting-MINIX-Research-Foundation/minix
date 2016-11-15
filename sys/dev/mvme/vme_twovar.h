/*	$NetBSD: vme_twovar.h,v 1.3 2008/04/28 20:23:54 martin Exp $	*/

/*-
 * Copyright (c) 1999, 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Steve C. Woodford.
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

#ifndef _MVME_VME_TWOVAR_H
#define _MVME_VME_TWOVAR_H

#define	VME2_MASTER_FIXED_A16	0
#define	VME2_MASTER_FIXED_A24	1
#define	VME2_MASTER_FIXED_A32	2
#define VME2_MASTER_PROG_START	3
#define VME2_SLAVE_OFFBOARD_RAM	0
#define VME2_SLAVE_PROG_START	1
#define VME2_SLAVE_A16		(VME2_SLAVE_PROG_START+(VME2_SLAVE_WINDOWS*2))

#define VME2_NMASTERS		(VME2_MASTER_PROG_START + VME2_MASTER_WINDOWS)
#define VME2_NSLAVES		(VME2_SLAVE_A16 + 1)

struct vmetwo_softc {
	struct mvmebus_softc	sc_mvmebus;
	bus_space_handle_t	sc_lcrh;
	bus_space_handle_t	sc_gcrh;
	void			*sc_isrcookie;
	void			(*sc_isrlink)(void *, int (*)(void *),
				    void *, int, int, struct evcnt *);
	void			(*sc_isrunlink)(void *, int);
	struct evcnt *		(*sc_isrevcnt)(void *, int);
#if NVMETWO > 0
	struct mvmebus_range	sc_master[VME2_NMASTERS];
	struct mvmebus_range	sc_slave[VME2_NSLAVES];
#endif
};

extern int vmetwo_not_present;

void	vmetwo_init(struct vmetwo_softc *);
void	vmetwo_md_intr_init(struct vmetwo_softc *);
int	vmetwo_probe(bus_space_tag_t, bus_addr_t);
void	vmetwo_intr_init(struct vmetwo_softc *);
void	vmetwo_intr_establish(void *, int, int, int, int,
	    int (*)(void *), void *, struct evcnt *);
void	vmetwo_intr_disestablish(void *, int, int, int, struct evcnt *);
void	vmetwo_local_intr_establish(int, int,
	    int (*)(void *), void *, struct evcnt *);

#endif /* _MVME_VME_TWOVAR_H */
