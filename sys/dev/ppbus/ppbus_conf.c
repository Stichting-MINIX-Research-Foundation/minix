/* $NetBSD: ppbus_conf.c,v 1.20 2012/10/27 17:18:37 chs Exp $ */

/*-
 * Copyright (c) 1997, 1998, 1999 Nicolas Souchu
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * FreeBSD: src/sys/dev/ppbus/ppbconf.c,v 1.17.2.1 2000/05/24 00:20:57 n_hibma Exp
 *
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ppbus_conf.c,v 1.20 2012/10/27 17:18:37 chs Exp $");

#include "opt_ppbus.h"
#include "opt_ppbus_1284.h"

#include "gpio.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/proc.h>

#include <dev/ppbus/ppbus_1284.h>
#include <dev/ppbus/ppbus_base.h>
#include <dev/ppbus/ppbus_conf.h>
#include <dev/ppbus/ppbus_device.h>
#include <dev/ppbus/ppbus_var.h>

/* Probe, attach, and detach functions for ppbus. */
static int ppbus_probe(device_t, cfdata_t, void *);
static void ppbus_attach(device_t, device_t, void *);
static void ppbus_childdet(device_t, device_t);
static int ppbus_detach(device_t, int);

/* Utility function prototypes */
static int ppbus_search_children(device_t, cfdata_t,
				 const int *, void *);


CFATTACH_DECL2_NEW(ppbus, sizeof(struct ppbus_softc), ppbus_probe, ppbus_attach,
	ppbus_detach, NULL, NULL, ppbus_childdet);

/* Probe function for ppbus. */
static int
ppbus_probe(device_t parent, cfdata_t cf, void *aux)
{
	struct parport_adapter *sc_link = aux;

	/* Check adapter for consistency */
	if (
		/* Required methods for all parports */
		sc_link->parport_io == NULL ||
		sc_link->parport_exec_microseq == NULL ||
		sc_link->parport_setmode == NULL ||
		sc_link->parport_getmode == NULL ||
		sc_link->parport_read == NULL ||
		sc_link->parport_write == NULL ||
		sc_link->parport_read_ivar == NULL ||
		sc_link->parport_write_ivar == NULL ||
		/* Methods which conditional exist based on capabilities */
		((sc_link->capabilities & PPBUS_HAS_EPP) &&
		(sc_link->parport_reset_epp_timeout == NULL)) ||
		((sc_link->capabilities & PPBUS_HAS_ECP) &&
		(sc_link->parport_ecp_sync == NULL)) ||
		((sc_link->capabilities & PPBUS_HAS_DMA) &&
		(sc_link->parport_dma_malloc == NULL ||
		sc_link->parport_dma_free == NULL)) ||
		((sc_link->capabilities & PPBUS_HAS_INTR) &&
		(sc_link->parport_add_handler == NULL ||
		sc_link->parport_remove_handler == NULL))
		) {

#ifdef PPBUS_DEBUG
		printf("%s(%s): parport_adaptor is incomplete. Child device "
			"probe failed.\n", __func__, device_xname(parent));
#endif
		return 0;
	} else {
		return 1;
	}
}

/* Attach function for ppbus. */
static void
ppbus_attach(device_t parent, device_t self, void *aux)
{
	struct ppbus_softc *ppbus = device_private(self);
	struct parport_adapter *sc_link = aux;
	struct ppbus_attach_args args;

	printf("\n");

	/* Initialize config data from adapter (bus + device methods) */
        ppbus->sc_dev = self;
	args.capabilities = ppbus->sc_capabilities = sc_link->capabilities;
	ppbus->ppbus_io = sc_link->parport_io;
	ppbus->ppbus_exec_microseq = sc_link->parport_exec_microseq;
	ppbus->ppbus_reset_epp_timeout = sc_link->
		parport_reset_epp_timeout;
	ppbus->ppbus_setmode = sc_link->parport_setmode;
	ppbus->ppbus_getmode = sc_link->parport_getmode;
	ppbus->ppbus_ecp_sync = sc_link->parport_ecp_sync;
	ppbus->ppbus_read = sc_link->parport_read;
	ppbus->ppbus_write = sc_link->parport_write;
        ppbus->ppbus_read_ivar = sc_link->parport_read_ivar;
	ppbus->ppbus_write_ivar = sc_link->parport_write_ivar;
	ppbus->ppbus_dma_malloc = sc_link->parport_dma_malloc;
	ppbus->ppbus_dma_free = sc_link->parport_dma_free;
	ppbus->ppbus_add_handler = sc_link->parport_add_handler;
	ppbus->ppbus_remove_handler = sc_link->parport_remove_handler;

	/* Initially there is no device owning the bus */
	ppbus->ppbus_owner = NULL;

	/* Initialize locking structures */
	mutex_init(&(ppbus->sc_lock), MUTEX_DEFAULT, IPL_NONE);

	/* Set up bus mode and ieee state */
	ppbus->sc_mode = ppbus->ppbus_getmode(device_parent(self));
	ppbus->sc_use_ieee = 1;
	ppbus->sc_1284_state = PPBUS_FORWARD_IDLE;
	ppbus->sc_1284_error = PPBUS_NO_ERROR;

	/* Record device's sucessful attachment */
	ppbus->sc_dev_ok = PPBUS_OK;

#ifndef DONTPROBE_1284
	/* detect IEEE1284 compliant devices */
	if (ppbus_scan_bus(self)) {
		printf("%s: No IEEE1284 device found.\n", device_xname(self));
	} else {
		printf("%s: IEEE1284 device found.\n", device_xname(self));
		/*
		 * Detect device ID (interrupts must be disabled because we
		 * cannot do a block to wait for it - no context)
		 */
		if (args.capabilities & PPBUS_HAS_INTR) {
			int val = 0;
			if(ppbus_write_ivar(self, PPBUS_IVAR_INTR, &val) != 0) {
				printf(" <problem initializing interrupt "
					"usage>");
			}
		}
		ppbus_pnp_detect(self);
	}
#endif /* !DONTPROBE_1284 */

	/* Configure child devices */
	SLIST_INIT(&(ppbus->sc_childlist_head));
	config_search_ia(ppbus_search_children, self, "ppbus", &args);

#if NGPIO > 0
	gpio_ppbus_attach(ppbus);
#endif
	return;
}

static void
ppbus_childdet(device_t self, device_t target)
{
	struct ppbus_softc * ppbus = device_private(self);
	struct ppbus_device_softc * child;

	SLIST_FOREACH(child, &ppbus->sc_childlist_head, entries) {
		if (child->sc_dev == target)
			break;
	}
	if (child != NULL)
		SLIST_REMOVE(&ppbus->sc_childlist_head, child,
		    ppbus_device_softc, entries);
}

/* Detach function for ppbus. */
static int
ppbus_detach(device_t self, int flag)
{
	struct ppbus_softc * ppbus = device_private(self);
	struct ppbus_device_softc * child;

	if (ppbus->sc_dev_ok != PPBUS_OK) {
		if (!(flag & DETACH_QUIET))
			printf("%s: detach called on unattached device.\n",
				device_xname(ppbus->sc_dev));
		if (!(flag & DETACH_FORCE))
			return 0;
		if (!(flag & DETACH_QUIET))
			printf("%s: continuing detach (DETACH_FORCE).\n",
				device_xname(ppbus->sc_dev));
	}

	mutex_destroy(&(ppbus->sc_lock));

	/* Detach children devices */
	while ((child = SLIST_FIRST(&ppbus->sc_childlist_head)) != NULL) {
		if (config_detach(child->sc_dev, flag)) {
			if(!(flag & DETACH_QUIET))
				aprint_error_dev(ppbus->sc_dev, "error detaching %s.",
					device_xname(child->sc_dev));
			if(!(flag & DETACH_FORCE))
				return 0;
			if(!(flag & DETACH_QUIET))
				printf("%s: continuing (DETACH_FORCE).\n",
					device_xname(ppbus->sc_dev));
		}
	}

	if (!(flag & DETACH_QUIET))
		printf("%s: detached.\n", device_xname(ppbus->sc_dev));

	return 1;
}

/* Search for children device and add to list */
static int
ppbus_search_children(device_t parent, cfdata_t cf, const int *ldesc, void *aux)
{
	struct ppbus_softc *ppbus = device_private(parent);
	struct ppbus_device_softc *child;
	device_t dev;
	int rval = 0;

	if (config_match(parent, cf, aux) > 0) {
		dev = config_attach(parent, cf, aux, NULL);
		if (dev) {
			child = device_private(dev);
			SLIST_INSERT_HEAD(&(ppbus->sc_childlist_head),
				child, entries);
			rval = 1;
		}
	}

	return rval;
}

