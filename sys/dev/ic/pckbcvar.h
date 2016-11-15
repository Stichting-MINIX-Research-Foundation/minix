/* $NetBSD: pckbcvar.h,v 1.21 2015/04/13 16:33:24 riastradh Exp $ */

/*
 * Copyright (c) 1998
 *	Matthias Drochner.  All rights reserved.
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
 *
 */

#ifndef _DEV_IC_PCKBCVAR_H_
#define _DEV_IC_PCKBCVAR_H_

#include <sys/callout.h>
#include <sys/pmf.h>

#include <dev/pckbport/pckbportvar.h>

typedef void *pckbc_tag_t;
typedef int pckbc_slot_t;
#define	PCKBC_KBD_SLOT	0
#define	PCKBC_AUX_SLOT	1
#define	PCKBC_NSLOTS	2

/* Simple cmd/slot ringbuffer structure */
struct pckbc_rbuf_item {
	int data;
	int slot;
};

#define	PCKBC_RBUF_SIZE	32	/* number of rbuf entries */

/*
 * external representation (pckbc_tag_t),
 * needed early for console operation
 */
struct pckbc_internal {
	pckbport_tag_t t_pt;
	bus_space_tag_t t_iot;
	bus_space_handle_t t_ioh_d, t_ioh_c; /* data port, cmd port */
	bus_addr_t t_addr;
	u_char t_cmdbyte; /* shadow */
	int t_flags;
#define	PCKBC_CANT_TRANSLATE	0x0001	/* can't translate to XT scancodes */
#define	PCKBC_NEED_AUXWRITE	0x0002	/* need auxwrite command to find aux */

	int t_haveaux; /* controller has an aux port */
	struct pckbc_slotdata *t_slotdata[PCKBC_NSLOTS];

	struct pckbc_softc *t_sc; /* back pointer */

	struct callout t_cleanup;

	struct pckbc_rbuf_item rbuf[PCKBC_RBUF_SIZE];
	int rbuf_read;
	int rbuf_write;
};

typedef void (*pckbc_inputfcn)(void *, int);

/*
 * State per device.
 */
struct pckbc_softc {
	device_t sc_dv;
	struct pckbc_internal *id;

	void (*intr_establish)(struct pckbc_softc *, pckbc_slot_t);
};

struct pckbc_attach_args {
	pckbc_tag_t pa_tag;
	pckbc_slot_t pa_slot;
};

extern const char * const pckbc_slot_names[];
extern struct pckbc_internal pckbc_consdata;
extern int pckbc_console_attached;

/* These functions are sometimes called by match routines */
int pckbc_send_cmd(bus_space_tag_t, bus_space_handle_t, u_char);
int pckbc_poll_data1(void *, pckbc_slot_t);

/* More normal calls from attach routines */
void pckbc_attach(struct pckbc_softc *);
int pckbc_cnattach(bus_space_tag_t, bus_addr_t, bus_size_t, pckbc_slot_t, int);
int pckbc_is_console(bus_space_tag_t, bus_addr_t);
int pckbcintr(void *);
int pckbcintr_hard(void *);
void pckbcintr_soft(void *);

/* md hook for use without mi wscons */
int pckbc_machdep_cnattach(pckbc_tag_t, pckbc_slot_t);

/* power management */
bool pckbc_resume(device_t, const pmf_qual_t *);

#endif /* _DEV_IC_PCKBCVAR_H_ */
