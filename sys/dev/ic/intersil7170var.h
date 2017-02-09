/*	$NetBSD: intersil7170var.h,v 1.4 2008/04/28 20:23:50 martin Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Adam Glass.
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

#ifndef	_INTERSIL7170VAR_H_
#define	_INTERSIL7170VAR_H_

/*
 * Driver support for the intersil7170 used in sun[34]s to provide
 * real time clock and time-of-day support.
 */

struct intersil7170_softc {
	device_t	sc_dev;

	bus_space_tag_t	sc_bst;			/* bus_space(9) tag */
	bus_space_handle_t sc_bsh;		/* bus_space(9) handle */
	struct todr_chip_handle sc_handle;	/* todr(9) handle */
	u_int		sc_year0;		/* what year is represented on
						   the system by the chip's
						   year counter at 0 */
	u_int		sc_flag;
#define INTERSIL7170_NO_CENT_ADJUST	0x0001
};

/* common attach function */
void intersil7170_attach(struct intersil7170_softc *);

#endif	/* _INTERSIL7170VAR_H_ */
