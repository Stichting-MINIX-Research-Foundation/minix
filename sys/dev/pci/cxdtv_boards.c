/* $NetBSD: cxdtv_boards.c,v 1.2 2011/07/14 23:47:45 jmcneill Exp $ */

/*
 * Copyright (c) 2008, 2011 Jonathan A. Kollasch
 * Copyright (c) 2008 Jared D. McNeill <jmcneill@invisible.ca>
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: cxdtv_boards.c,v 1.2 2011/07/14 23:47:45 jmcneill Exp $");

#include <sys/types.h>
#include <sys/device.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/cxdtvreg.h>
#include <dev/pci/cxdtv_boards.h>

static const struct cxdtv_board cxdtv_boards[] = {
	{
		.cb_name = "ATI HDTV Wonder (digital-only)",
		.cb_vendor = PCI_VENDOR_ATI,
		.cb_product = PCI_SUBSYSTEM_ATI_HDTV_WONDER_HP_Z556_MC,
		.cb_tuner = CXDTV_TUNER_PLL,
		.cb_demod = CXDTV_DEMOD_NXT2004,
	},
	{
		.cb_name = "pcHDTV HD5500",
		.cb_vendor = PCI_VENDOR_PCHDTV,
		.cb_product = PCI_PRODUCT_PCHDTV_HD5500,
		.cb_tuner = CXDTV_TUNER_PLL,
		.cb_demod = CXDTV_DEMOD_LG3303,
	},
};

const struct cxdtv_board *
cxdtv_board_lookup(pci_vendor_id_t vendor, pci_product_id_t product)
{
	const struct cxdtv_board *cb;
	unsigned int i;

	for (i = 0; i < __arraycount(cxdtv_boards); i++) {
		cb = &cxdtv_boards[i];
		if (vendor == cb->cb_vendor && product == cb->cb_product)
			return cb;
	}

	return NULL;
}
