/*	$NetBSD: irframevar.h,v 1.19 2009/05/12 14:29:42 cegger Exp $	*/

/*
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net).
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

struct irframe_methods {
	int (*im_open)(void *, int, int, struct lwp *);
	int (*im_close)(void *, int, int, struct lwp *);
	int (*im_read)(void *, struct uio *, int);
	int (*im_write)(void *, struct uio *, int);
	int (*im_poll)(void *, int, struct lwp *);
	int (*im_kqfilter)(void *, struct knote *);
	int (*im_set_params)(void *, struct irda_params *);
	int (*im_get_speeds)(void *, int *);
	int (*im_get_turnarounds)(void *, int *);
};

struct irframe_softc {
	device_t			sc_dev;
	const struct irframe_methods	*sc_methods;
	void				*sc_handle;
	char				sc_open;
	struct irda_params		sc_params;
	u_int				sc_speedmask;
	u_int				sc_speed;	/* DIAGNOSTIC only */
};

#define IRDA_DEFAULT_SPEED	9600
#define IRDA_DEFAULT_EBOFS	12
#define IRDA_DEFAULT_SIZE	386

#define IRDA_MAX_FRAME_SIZE	2048
#define IRDA_MAX_EBOFS		64

void irframe_attach(device_t, device_t, void *);
int irframe_detach(device_t, int);
