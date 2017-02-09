/*	$NetBSD: ne2000var.h,v 1.27 2013/08/11 12:34:16 rkujawa Exp $	*/

/*-
 * Copyright (c) 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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

#ifndef _DEV_IC_NE2000VAR_H_
#define	_DEV_IC_NE2000VAR_H_

struct ne2000_softc {
	struct dp8390_softc sc_dp8390;

	bus_space_tag_t sc_asict;	/* space tag for ASIC */
	bus_space_handle_t sc_asich;	/* space handle for ASIC */

	enum {
		NE2000_TYPE_UNKNOWN = 0,
		NE2000_TYPE_NE1000,
		NE2000_TYPE_NE2000,
		NE2000_TYPE_DL10019,
		NE2000_TYPE_DL10022,
		NE2000_TYPE_AX88190,
		NE2000_TYPE_AX88790,
		NE2000_TYPE_RTL8019,
		NE2000_TYPE_AX88796
	} sc_type;
	int sc_useword;
	u_int sc_quirk;			/* quirks passed from attachments */
#define	NE2000_QUIRK_8BIT	0x0001	/* force 8bit mode even on NE2000 */
};

int	ne2000_attach(struct ne2000_softc *, uint8_t *);
int	ne2000_detect(bus_space_tag_t, bus_space_handle_t,
	    bus_space_tag_t, bus_space_handle_t);
int	ne2000_detach(struct ne2000_softc *, int);

#ifdef IPKDB_NE
int	ne2000_ipkdb_attach(struct ipkdb_if *);
#endif

/* pmf(9) */
bool ne2000_suspend(device_t, const pmf_qual_t *);
bool ne2000_resume(device_t, const pmf_qual_t *);

#endif /* _DEV_IC_NE2000VAR_H_ */
