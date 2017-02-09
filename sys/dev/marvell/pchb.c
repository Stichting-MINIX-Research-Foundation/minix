/*	$NetBSD: pchb.c,v 1.2 2012/10/27 17:18:26 chs Exp $	*/
/*
 * Copyright (c) 2008 KIYOHARA Takashi
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pchb.c,v 1.2 2012/10/27 17:18:26 chs Exp $");

#include "opt_pci.h"
#include "pci.h"

#include <sys/param.h>
#include <sys/device.h>
#include <sys/errno.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/marvell/marvellreg.h>
#include <dev/marvell/marvellvar.h>


static int pchb_match(device_t, cfdata_t, void *);
static void pchb_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(pchb, 0,
    pchb_match, pchb_attach, NULL, NULL);


/* ARGSUSED */
static int
pchb_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_MARVELL)
		switch (PCI_PRODUCT(pa->pa_id)) {
		case MARVELL_ORION_1_88F1181:
		case MARVELL_ORION_1_88F5082:
		case MARVELL_ORION_1_88F5181:
		case MARVELL_ORION_1_88F5182:
		case MARVELL_ORION_2_88F5281:
		case MARVELL_ORION_1_88W8660:
			return 1;
		}

	return 0;
}

/* ARGSUSED */
static void
pchb_attach(device_t parent, device_t self, void *aux)
{

	aprint_normal("\n");
	aprint_naive("\n");
}
