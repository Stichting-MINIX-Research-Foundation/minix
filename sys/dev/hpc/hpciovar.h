/*	$NetBSD: hpciovar.h,v 1.6 2005/12/11 12:21:22 christos Exp $	*/

/*-
 * Copyright (c) 2001 TAKEMURA Shin.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

#ifndef _DEV_HPC_HPCIOVAR_H_
#define _DEV_HPC_HPCIOVAR_H_

struct hpcio_chip;
typedef struct hpcio_chip *hpcio_chip_t;
typedef void *hpcio_intr_handle_t;
struct hpcio_chip {
	int hc_chipid;
	const char *hc_name;
	void *hc_sc;
	int (*hc_portread)(hpcio_chip_t, int);
	void (*hc_portwrite)(hpcio_chip_t, int, int);
	hpcio_intr_handle_t(*hc_intr_establish)(hpcio_chip_t, int, int,
	    int (*)(void *), void *);
	void (*hc_intr_disestablish)(hpcio_chip_t, hpcio_intr_handle_t);
	void (*hc_intr_clear)(hpcio_chip_t, hpcio_intr_handle_t);
	void (*hc_register_iochip)(hpcio_chip_t, hpcio_chip_t);
	void (*hc_update)(hpcio_chip_t);
	void (*hc_dump)(hpcio_chip_t);
};

struct hpcio_attach_args {
	const char *haa_busname;
	void *haa_sc;
	hpcio_chip_t (*haa_getchip)(void*, int);
	bus_space_tag_t haa_iot;	/* I/O space tag */
};
#define HPCIO_BUSNAME	"hpcioif"

#define hpcio_portread(hc, port)					\
		((*(hc)->hc_portread)((hc), (port)))
#define hpcio_portwrite(hc, port, data)					\
		((*(hc)->hc_portwrite)((hc), (port), (data)))
#define hpcio_intr_establish(hc, port, mode, func, arg)			\
		((*(hc)->hc_intr_establish)((hc),(port),(mode),(func),(arg)))
#define hpcio_intr_disestablish(hc, handle)				\
		((*(hc)->hc_intr_disestablish)((hc), (handle)))
#define hpcio_intr_clear(hc, handle)					\
		((*(hc)->hc_intr_clear)((hc), (handle)))
#define hpcio_register_iochip(hc, iochip)				\
		((*(hc)->hc_register_iochip)((hc), (iochip)))
#define hpcio_update(hc)						\
		((*(hc)->hc_update)(hc))
#define hpcio_dump(hc)							\
		((*(hc)->hc_dump)(hc))

/* interrupt trigger options. */
#define HPCIO_INTR_EDGE		(1<<0)
#define HPCIO_INTR_LEVEL	(0<<0)
#define HPCIO_INTR_HOLD		(1<<1)
#define HPCIO_INTR_THROUGH	(0<<1)
#define HPCIO_INTR_HIGH		(1<<2)
#define HPCIO_INTR_LOW		(0<<2)
#define HPCIO_INTR_POSEDGE	(1<<3)
#define HPCIO_INTR_NEGEDGE	(1<<4)

#define HPCIO_INTR_LEVEL_HIGH_HOLD					\
		(HPCIO_INTR_LEVEL|HPCIO_INTR_HIGH|HPCIO_INTR_HOLD)
#define HPCIO_INTR_LEVEL_HIGH_THROUGH					\
		(HPCIO_INTR_LEVEL|HPCIO_INTR_HIGH|HPCIO_INTR_THROUGH)
#define HPCIO_INTR_LEVEL_LOW_HOLD					\
		(HPCIO_INTR_LEVEL|HPCIO_INTR_LOW|HPCIO_INTR_HOLD)
#define HPCIO_INTR_LEVEL_LOW_THROUGH					\
		(HPCIO_INTR_LEVEL|HPCIO_INTR_LOW|HPCIO_INTR_THROUGH)
#define HPCIO_INTR_EDGE_HOLD						\
		(HPCIO_INTR_EDGE|HPCIO_INTR_HOLD)
#define HPCIO_INTR_EDGE_THROUGH						\
		(HPCIO_INTR_EDGE|HPCIO_INTR_THROUGH)

#endif /* !_DEV_HPC_HPCIOVAR_H_ */
