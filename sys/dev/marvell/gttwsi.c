/*	$NetBSD: gttwsi.c,v 1.11 2013/09/06 00:56:12 matt Exp $	*/
/*
 * Copyright (c) 2008 Eiji Kawauchi.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project by
 *      Eiji Kawauchi.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
/*
 * Copyright (c) 2005 Brocade Communcations, inc.
 * All rights reserved.
 *
 * Written by Matt Thomas for Brocade Communcations, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Brocade Communications, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BROCADE COMMUNICATIONS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL EITHER BROCADE COMMUNICATIONS, INC. BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */
//#define TWSI_DEBUG

/*
 * Marvell Two-Wire Serial Interface (aka I2C) master driver
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: gttwsi.c,v 1.11 2013/09/06 00:56:12 matt Exp $");
#include "locators.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/condvar.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/mutex.h>
#include <sys/systm.h>

#include <machine/cpu.h>
#include <machine/param.h>

#include <dev/i2c/i2cvar.h>
#include <dev/i2c/gttwsireg.h>
#include <dev/i2c/gttwsivar.h>

#include <dev/marvell/marvellvar.h>

static int	gttwsi_match(device_t, cfdata_t, void *);
static void	gttwsi_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(gttwsi_gt, sizeof(struct gttwsi_softc),
    gttwsi_match, gttwsi_attach, NULL, NULL);
CFATTACH_DECL_NEW(gttwsi_mbus, sizeof(struct gttwsi_softc),
    gttwsi_match, gttwsi_attach, NULL, NULL);

/* ARGSUSED */
static int
gttwsi_match(device_t parent, cfdata_t match, void *aux)
{
	struct marvell_attach_args *mva = aux;

	if (strcmp(mva->mva_name, match->cf_name) != 0)
		return 0;
	if (mva->mva_offset == MVA_OFFSET_DEFAULT ||
	    mva->mva_irq == MVA_IRQ_DEFAULT)
		return 0;

	mva->mva_size = GTTWSI_SIZE;
	return 1;
}

/* ARGSUSED */
static void
gttwsi_attach(device_t parent, device_t self, void *args)
{
	struct marvell_attach_args *mva = args;
	bus_space_handle_t ioh;

	if (bus_space_subregion(mva->mva_iot, mva->mva_ioh, mva->mva_offset,
	    mva->mva_size, &ioh)) {
		aprint_error(": cannot map registers\n");
		return;
	}

	gttwsi_attach_subr(self, mva->mva_iot, ioh);

	marvell_intr_establish(mva->mva_irq, IPL_BIO, gttwsi_intr,
	    device_private(self));

	gttwsi_config_children(self);
}
