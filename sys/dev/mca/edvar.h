/*	$NetBSD: edvar.h,v 1.18 2015/04/14 20:32:36 riastradh Exp $	*/

/*
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jaromir Dolecek.
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

#include <sys/mutex.h>
#include <sys/rndsource.h>

struct edc_mca_softc;

struct ed_softc {
	device_t sc_dev;

	/* General disk infos */
	struct disk sc_dk;
	struct bufq_state *sc_q;
	kmutex_t sc_q_lock;

	struct edc_mca_softc *edc_softc;   /* pointer to our controller */

	int sc_flags;
#define WDF_WLABEL	0x001 /* label is writable */
#define WDF_LABELLING   0x002 /* writing label */
#define WDF_LOADED	0x004 /* parameters loaded */
#define WDF_KLABEL	0x008 /* retain label after 'full' close */
#define EDF_INIT	0x100 /* disk initialized */

	/* actual drive parameters */
	int sc_capacity;
	int sc_devno;
	u_int16_t sense_data[4];	/* sensed drive parameters */
	u_int16_t cyl;
	u_int8_t heads;
	u_int8_t sectors;
	u_int8_t spares;	/* spares per cylinder */
	u_int32_t rba;		/* # of RBAs */

	krndsource_t	rnd_source;
};
