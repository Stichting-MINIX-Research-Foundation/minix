/*	$NetBSD: ohcivar.h,v 1.55 2014/01/28 17:24:42 skrll Exp $	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
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

#ifndef _OHCIVAR_H_
#define _OHCIVAR_H_

#include <sys/pool.h>

typedef struct ohci_soft_ed {
	ohci_ed_t ed;
	struct ohci_soft_ed *next;
	ohci_physaddr_t physaddr;
	usb_dma_t dma;
	int offs;
} ohci_soft_ed_t;
#define OHCI_SED_SIZE ((sizeof (struct ohci_soft_ed) + OHCI_ED_ALIGN - 1) / OHCI_ED_ALIGN * OHCI_ED_ALIGN)
#define OHCI_SED_CHUNK 128


typedef struct ohci_soft_td {
	ohci_td_t td;
	struct ohci_soft_td *nexttd; /* mirrors nexttd in TD */
	struct ohci_soft_td *dnext; /* next in done list */
	ohci_physaddr_t physaddr;
	usb_dma_t dma;
	int offs;
	LIST_ENTRY(ohci_soft_td) hnext;
	usbd_xfer_handle xfer;
	u_int16_t len;
	u_int16_t flags;
#define OHCI_CALL_DONE	0x0001
#define OHCI_ADD_LEN	0x0002
} ohci_soft_td_t;
#define OHCI_STD_SIZE ((sizeof (struct ohci_soft_td) + OHCI_TD_ALIGN - 1) / OHCI_TD_ALIGN * OHCI_TD_ALIGN)
#define OHCI_STD_CHUNK 128


typedef struct ohci_soft_itd {
	ohci_itd_t itd;
	struct ohci_soft_itd *nextitd; /* mirrors nexttd in ITD */
	struct ohci_soft_itd *dnext; /* next in done list */
	ohci_physaddr_t physaddr;
	usb_dma_t dma;
	int offs;
	LIST_ENTRY(ohci_soft_itd) hnext;
	usbd_xfer_handle xfer;
	u_int16_t flags;
	char isdone;	/* used only when DIAGNOSTIC is defined */
} ohci_soft_itd_t;
#define OHCI_SITD_SIZE ((sizeof (struct ohci_soft_itd) + OHCI_ITD_ALIGN - 1) / OHCI_ITD_ALIGN * OHCI_ITD_ALIGN)
#define OHCI_SITD_CHUNK 64


#define OHCI_NO_EDS (2*OHCI_NO_INTRS-1)

#define OHCI_HASH_SIZE 128

typedef struct ohci_softc {
	device_t sc_dev;
	struct usbd_bus sc_bus;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	bus_size_t sc_size;

	kmutex_t sc_lock;
	kmutex_t sc_intr_lock;
	void *sc_rhsc_si;

	usb_dma_t sc_hccadma;
	struct ohci_hcca *sc_hcca;
	ohci_soft_ed_t *sc_eds[OHCI_NO_EDS];
	u_int sc_bws[OHCI_NO_INTRS];

	u_int32_t sc_eintrs;
	ohci_soft_ed_t *sc_isoc_head;
	ohci_soft_ed_t *sc_ctrl_head;
	ohci_soft_ed_t *sc_bulk_head;

	LIST_HEAD(, ohci_soft_td)  sc_hash_tds[OHCI_HASH_SIZE];
	LIST_HEAD(, ohci_soft_itd) sc_hash_itds[OHCI_HASH_SIZE];

	int sc_noport;
	u_int8_t sc_addr;		/* device address */
	u_int8_t sc_conf;		/* device configuration */

	int sc_endian;
#define	OHCI_LITTLE_ENDIAN	0	/* typical (uninitialized default) */
#define	OHCI_BIG_ENDIAN		1	/* big endian OHCI? never seen it */
#define	OHCI_HOST_ENDIAN	2	/* if OHCI always matches CPU */

	int sc_flags;
#define OHCIF_SUPERIO		0x0001

	char sc_softwake;
	kcondvar_t sc_softwake_cv;

	ohci_soft_ed_t *sc_freeeds;
	ohci_soft_td_t *sc_freetds;
	ohci_soft_itd_t *sc_freeitds;

	pool_cache_t sc_xferpool;	/* free xfer pool */

	usbd_xfer_handle sc_intrxfer;

	char sc_vendor[32];
	int sc_id_vendor;

	u_int32_t sc_control;		/* Preserved during suspend/standby */
	u_int32_t sc_intre;

	u_int sc_overrun_cnt;
	struct timeval sc_overrun_ntc;

	struct callout sc_tmo_rhsc;
	device_t sc_child;
	char sc_dying;
	struct usb_dma_reserve sc_dma_reserve;
} ohci_softc_t;

struct ohci_xfer {
	struct usbd_xfer xfer;
	struct usb_task	abort_task;
};

usbd_status	ohci_init(ohci_softc_t *);
int		ohci_intr(void *);
int		ohci_detach(ohci_softc_t *, int);
bool		ohci_shutdown(device_t, int);
void		ohci_childdet(device_t, device_t);
int		ohci_activate(device_t, enum devact);
bool		ohci_resume(device_t, const pmf_qual_t *);
bool		ohci_suspend(device_t, const pmf_qual_t *);

#endif /* _OHCIVAR_H_ */
