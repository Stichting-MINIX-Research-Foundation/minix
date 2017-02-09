/*	$NetBSD: tms320av110var.h,v 1.12 2012/10/27 17:18:23 chs Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Ignatios Souvatzis.
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
 * Machine independent definitions, declarations and data structures for
 * access to the TMS320AV110 data sheet.
 *
 * Currently, only minimum support for audio output. For audio/video
 * synchronization, more is needed.
 */

#ifndef _TMS320AV110_VAR_H_
#define _TMS320AV110_VAR_H_

#include <sys/bus.h>

/* softc */

struct tav_softc {
	device_t	sc_dev;
	kmutex_t	sc_lock;
	kmutex_t	sc_intr_lock;
	kcondvar_t	sc_cv;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;

	/* above audio callback function */
	void		(*sc_intr)(void *);
	void		*sc_intrarg;
	int		sc_bsize;

	/* below audio interrupt acknowledge function. Ignored if NULL */
	void		(*sc_intack)(struct tav_softc *);

	/* initialization from below */

	uint8_t		sc_pcm_div;	/* passed in */
	uint8_t		sc_pcm_ord;	/* passed in */
	uint8_t		sc_pcm_18;	/* passed in */
	uint8_t		sc_dif;	/* passed in */
};

/* prototypes */

void tms320av110_attach_mi(struct tav_softc *);
int tms320av110_intr(void *);

static void tav_write_short(bus_space_tag_t, bus_space_handle_t,
    bus_size_t, uint16_t);

/* access functions/macros: */
/* XXX shouldn't these be in the reg.h file? */

#define tav_read_byte(ioh, iot, off) bus_space_read_1(ioh, iot, off)

#define tav_read_short(ioh, iot, off)	(		\
	bus_space_read_1((ioh), (iot), (off))	|	\
	bus_space_read_1((ioh), (iot), (off)+1) << 8)

#define tav_read_long(ioh, iot, off)	(		\
	bus_space_read_1((ioh), (iot), (off))	|	\
	bus_space_read_1((ioh), (iot), (off)+1) << 8 |	\
	bus_space_read_1((ioh), (iot), (off)+2) << 16 |	\
	bus_space_read_1((ioh), (iot), (off)+3))

#define tav_read_time(ioh, iot, off)	(		\
	bus_space_read_1((ioh), (iot), (off))	|	\
	bus_space_read_1((ioh), (iot), (off)+1) << 8 |	\
	bus_space_read_1((ioh), (iot), (off)+2) << 16 |	\
	bus_space_read_1((ioh), (iot), (off)+3) << 24 |	\
	bus_space_read_1((ioh), (iot), (off)+4) << 32)

#define tav_write_byte(ioh, iot, off, v) bus_space_write_1(ioh, iot, off, v)

static __inline void
tav_write_short(bus_space_tag_t iot, bus_space_handle_t ioh,
    bus_size_t off, uint16_t val)
{

	bus_space_write_1(iot, ioh, off+1, (val)>>8);
	bus_space_write_1(iot, ioh, off,  (uint8_t)val);
}

#endif /* _TMS320AV110_VAR_H_ */
