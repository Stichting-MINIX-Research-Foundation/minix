/* $NetBSD: pckbportvar.h,v 1.9 2011/09/09 14:00:01 jakllsch Exp $ */

/*
 * Copyright (c) 2004 Ben Harris
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

#ifndef _DEV_PCKBPORT_PCKBPORTVAR_H_
#define _DEV_PCKBPORT_PCKBPORTVAR_H_

#include <sys/callout.h>

typedef struct pckbport_tag *pckbport_tag_t;
typedef int pckbport_slot_t;

#define	PCKBPORT_KBD_SLOT	0
#define	PCKBPORT_AUX_SLOT	1
#define	PCKBPORT_NSLOTS	2

typedef void (*pckbport_inputfcn)(void *, int);

struct pckbport_accessops {
	/* Functions to be provided by controller driver (eg pckbc) */
	int	(*t_xt_translation)(void *, pckbport_slot_t, int);
	int	(*t_send_devcmd)   (void *, pckbport_slot_t, u_char);
	int	(*t_poll_data1)    (void *, pckbport_slot_t);
	void	(*t_slot_enable)   (void *, pckbport_slot_t, int);
	void	(*t_intr_establish)(void *, pckbport_slot_t);
	void	(*t_set_poll)      (void *, pckbport_slot_t, int);
};

/*
 * external representation (pckbport_tag_t),
 * needed early for console operation
 */
struct pckbport_tag {
	struct pckbport_slotdata *t_slotdata[PCKBPORT_NSLOTS];

	struct callout t_cleanup;

	pckbport_inputfcn t_inputhandler[PCKBPORT_NSLOTS];
	void *t_inputarg[PCKBPORT_NSLOTS];
	const char *t_subname[PCKBPORT_NSLOTS];

	struct pckbport_accessops const *t_ops;
	/* First argument to all those */
	void	*t_cookie;
};

struct pckbport_attach_args {
	pckbport_tag_t pa_tag;
	pckbport_slot_t pa_slot;
};

extern struct pckbport_tag pckbport_consdata;
extern int pckbport_console_attached;

/* Calls from pckbd etc */
void pckbport_set_inputhandler(pckbport_tag_t, pckbport_slot_t,
				 pckbport_inputfcn, void *, const char *);

void pckbport_flush(pckbport_tag_t, pckbport_slot_t);
int pckbport_poll_cmd(pckbport_tag_t, pckbport_slot_t, const u_char *, int,
			int, u_char *, int);
int pckbport_enqueue_cmd(pckbport_tag_t, pckbport_slot_t, const u_char *, int,
			   int, int, u_char *);
int pckbport_poll_data(pckbport_tag_t, pckbport_slot_t);
void pckbport_set_poll(pckbport_tag_t, pckbport_slot_t, int);
int pckbport_xt_translation(pckbport_tag_t, pckbport_slot_t, int);
void pckbport_slot_enable(pckbport_tag_t, pckbport_slot_t, int);

/* calls from pckbc etc */
int pckbport_cnattach(void *, struct pckbport_accessops const *,
			      pckbport_slot_t);
pckbport_tag_t pckbport_attach(void *,
				       struct pckbport_accessops const *);
device_t pckbport_attach_slot(device_t, pckbport_tag_t, pckbport_slot_t);
void pckbportintr(pckbport_tag_t, pckbport_slot_t, int);

/* md hook for use without mi wscons */
int pckbport_machdep_cnattach(pckbport_tag_t, pckbport_slot_t);

#endif /* _DEV_PCKBPORT_PCKBPORTVAR_H_ */
