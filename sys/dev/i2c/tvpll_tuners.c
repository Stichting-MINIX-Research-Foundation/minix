/* $NetBSD: tvpll_tuners.c,v 1.3 2011/07/15 03:31:37 jmcneill Exp $ */

/*
 * Copyright (c) 2008, 2011 Jonathan A. Kollasch
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
__KERNEL_RCSID(0, "$NetBSD: tvpll_tuners.c,v 1.3 2011/07/15 03:31:37 jmcneill Exp $");

#include <sys/param.h>
#include <dev/i2c/tvpllvar.h>
#include <dev/i2c/tvpll_tuners.h>

static struct tvpll_entry tuv1236d_pll_entries[] = {  
	{ 157250000, 62500, 0xc6, 0x41, TVPLL_IGNORE_AUX },
	{ 454000000, 62500, 0xc6, 0x42, TVPLL_IGNORE_AUX },
	{ 999999999, 62500, 0xc6, 0x44, TVPLL_IGNORE_AUX },
};
struct tvpll_data tvpll_tuv1236d_pll = {
	"Philips TUV1236D",
	54000000, 864000000, 44000000,
	NULL, NULL,
	__arraycount(tuv1236d_pll_entries), tuv1236d_pll_entries
};

static uint8_t tdvs_h06xf_initdata[] = { 2, 0xdf, 0x50 };
static struct tvpll_entry tdvs_h06xf_pll_entries[] = {
	{ 165000000, 62500, 0xce, 0x01, 0x50 },
	{ 450000000, 62500, 0xce, 0x02, 0x50 },
	{ 999999999, 62500, 0xce, 0x04, 0x50 },
};
struct tvpll_data tvpll_tdvs_h06xf_pll = {
	"LG TDVS-H06xF",
	54000000, 863000000, 44000000,
	tdvs_h06xf_initdata, NULL,
	__arraycount(tdvs_h06xf_pll_entries), tdvs_h06xf_pll_entries
};
