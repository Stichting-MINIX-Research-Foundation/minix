/*	$NetBSD: uhidev.h,v 1.18 2015/04/13 16:33:25 riastradh Exp $	*/

/*
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
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


#include <sys/rndsource.h>

struct uhidev_softc {
	device_t sc_dev;		/* base device */
	usbd_device_handle sc_udev;
	usbd_interface_handle sc_iface;	/* interface */
	usbd_pipe_handle sc_ipipe;	/* input interrupt pipe */
	int sc_iep_addr;

	u_char *sc_ibuf;
	u_int sc_isize;

	usbd_pipe_handle sc_opipe;	/* output interrupt pipe */
	usbd_xfer_handle sc_oxfer;	/* write request */
	int sc_oep_addr;

	void *sc_repdesc;
	int sc_repdesc_size;

	u_int sc_nrepid;
	device_t *sc_subdevs;

	int sc_refcnt;
	u_char sc_dying;

	kmutex_t sc_lock;		/* protects writes to sc_state */

	u_int sc_flags;
#define UHIDEV_F_XB1	0x0001	/* Xbox 1 controller */
};

struct uhidev {
	device_t sc_dev;		/* base device */
	struct uhidev_softc *sc_parent;
	uByte sc_report_id;
	u_int8_t sc_state;
	int sc_in_rep_size;
#define	UHIDEV_OPEN	0x01	/* device is open */
	void (*sc_intr)(struct uhidev *, void *, u_int);
        krndsource_t     rnd_source;
};

struct uhidev_attach_arg {
	struct usbif_attach_arg *uaa;
	struct uhidev_softc *parent;
	int reportid;
	int reportsize;
};

void uhidev_get_report_desc(struct uhidev_softc *, void **, int *);
int uhidev_open(struct uhidev *);
void uhidev_stop(struct uhidev *);
void uhidev_close(struct uhidev *);
usbd_status uhidev_set_report(struct uhidev *, int, void *, int);
usbd_status uhidev_get_report(struct uhidev *, int, void *, int);
usbd_status uhidev_write(struct uhidev_softc *, void *, int);
