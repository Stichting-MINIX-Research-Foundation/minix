/* $NetBSD: upcvar.h,v 1.6 2012/10/27 17:18:23 chs Exp $ */
/*-
 * Copyright (c) 2000, 2003 Ben Harris
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#ifndef __UPCVAR_H
#define __UPCVAR_H

#include <sys/bus.h>

struct upc_irqhandle {
	int	(*uih_func)(void *);
	void	*uih_arg;
	int	uih_level;
};

struct upc_softc {
	device_t		sc_dev;
	/* These fields are filled in by the bus attachment. */
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	/*
	 * These fields are filled in by upc_attach().  The bus attachment
	 * for upc is expected to establish them according to the way the
	 * chip is wired.
	 */
	struct upc_irqhandle	sc_irq3;
	struct upc_irqhandle	sc_irq4;
	struct upc_irqhandle	sc_pintr;
	struct upc_irqhandle	sc_fintr;
	struct upc_irqhandle	sc_wintr;
};

extern void upc_attach(struct upc_softc *);
extern int upc1_read_config(struct upc_softc *, int);
extern void upc1_write_config(struct upc_softc *, int, int);
extern int upc2_read_config(struct upc_softc *, int);
extern void upc2_write_config(struct upc_softc *, int, int);

/* This is the structure passed to children of upc. */
struct upc_attach_args {
	char const		*ua_devtype;
	int			ua_offset;
	bus_space_tag_t		ua_iot;
	bus_space_handle_t	ua_ioh;
	bus_space_handle_t	ua_ioh2; /* for wdc */
	struct upc_irqhandle	*ua_irqhandle;
};

extern void upc_intr_establish(struct upc_irqhandle *, int, int (*)(void *),
			       void *arg);

#endif
