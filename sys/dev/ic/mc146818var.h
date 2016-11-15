/*	$NetBSD: mc146818var.h,v 1.7 2008/05/14 13:29:29 tsutsui Exp $	*/

/*-
 * Copyright (c) 2003 Izumi Tsutsui.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

struct mc146818_softc {
	device_t sc_dev;

	bus_space_tag_t sc_bst;			/* bus space tag */
	bus_space_handle_t sc_bsh;		/* bus space handle */

	struct todr_chip_handle sc_handle;	/* TODR handle */
	u_int sc_year0;				/* year counter offset */
	u_int sc_flag;				/* MD flags */
#define MC146818_NO_CENT_ADJUST	0x0001		/* don't adjust century */
#define MC146818_BCD		0x0002		/* use BCD mode */
#define MC146818_12HR		0x0004		/* use AM/PM mode */

	/* MD chip register read/write functions */
	u_int (*sc_mcread)(struct mc146818_softc *, u_int);
	void (*sc_mcwrite)(struct mc146818_softc *, u_int, u_int);
	/* MD century get/set functions */
	u_int (*sc_getcent)(struct mc146818_softc *);
	void (*sc_setcent)(struct mc146818_softc *, u_int);
};

void mc146818_attach(struct mc146818_softc *);
