/* $NetBSD: cxdtv_boards.h,v 1.1 2011/07/11 00:46:04 jakllsch Exp $ */

/*
 * Copyright (c) 2008 Jonathan A. Kollasch
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

#ifndef _DEV_PCI_CXDTV_BOARDS_H
#define _DEV_PCI_CXDTV_BOARDS_H

typedef enum cxdtv_tuner_type {
	CXDTV_TUNER_PLL,
} cxdtv_tuner_type_t;

typedef enum cxdtv_demod_type {
	CXDTV_DEMOD_NXT2004,
	CXDTV_DEMOD_LG3303,
} cxdtv_demod_type_t;

struct cxdtv_board {
	const char		*cb_name;
	pci_vendor_id_t		cb_vendor;
	pci_product_id_t	cb_product;
	cxdtv_tuner_type_t	cb_tuner;
	cxdtv_demod_type_t	cb_demod;
};

const struct cxdtv_board *cxdtv_board_lookup(pci_vendor_id_t,
    pci_product_id_t);

#endif /* !_DEV_PCI_CXDTV_BOARDS_H */
