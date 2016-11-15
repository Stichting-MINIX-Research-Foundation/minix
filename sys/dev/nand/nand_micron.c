/*	$NetBSD: nand_micron.c,v 1.8 2012/11/03 12:12:48 ahoka Exp $	*/

/*-
 * Copyright (c) 2011 Department of Software Engineering,
 *		      University of Szeged, Hungary
 * Copyright (c) 2011 Adam Hoka <ahoka@NetBSD.org>
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by the Department of Software Engineering, University of Szeged, Hungary
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Device specific functions for legacy Micron NAND chips
 *
 * Currently supported:
 * MT29F2G08AACWP, MT29F4G08BACWP, MT29F8G08FACWP
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: nand_micron.c,v 1.8 2012/11/03 12:12:48 ahoka Exp $");

#include "nand.h"
#include "onfi.h"

#define MT29F2G08AAC	0xda
#define MT29F2G08ABC	0xaa
#define MT29F2G16AAC	0xca
#define MT29F2G16ABC	0xba
#define MT29F4G08BAC	0xdc
#define MT29F8G08FAC	0xdc		/* each 4GB section */

#define MT29FxG_PARAM_WIDTH(p)		(((p) >> 1) & __BIT(0))
#define MT29FxG_PAGESIZE		(2 * 1024)
#define MT29FxG_BLOCK_PAGES		64		/* pages per block */
#define MT29FxG_BLOCKSIZE		(128 * 1024)	/* not including spares */
#define MT29FxG_SPARESIZE		64

struct nand_micron_devices {
	const char *name;
	uint8_t	id;
	uint8_t width;			/* bus width */
	u_int lun_blocks;		/* number of blocks per LUN */
	u_int num_luns;			/* number LUNs */
};

static const struct nand_micron_devices nand_micron_devices[] = {
	{ "MT29F2G08AAC", MT29F2G08AAC,  8,  2048, 1 },
	{ "MT29F2G08ABC", MT29F2G08ABC,  8,  2048, 1 },
	{ "MT29F2G16AAC", MT29F2G16AAC, 16,  2048, 1 },
	{ "MT29F2G16ABC", MT29F2G16ABC, 16,  2048, 1 },
	{ "MT29F4G08BAC", MT29F4G08BAC,  8,  4096, 1 },
#ifdef NOTYET
	/* how do we recognize/match this? */
	{ "MT29F8G08FAC", MT29F8G08FAC,  8,  4096, 2 },
#endif
};

static int mt29fxgx_parameters(device_t, struct nand_chip *, u_int8_t, uint8_t);

static const struct nand_micron_devices *
nand_micron_device_lookup(u_int8_t id)
{
	for (int i=0; i < __arraycount(nand_micron_devices); i++)
		if (nand_micron_devices[i].id == id)
			return &nand_micron_devices[i];
	return NULL;
}

int
nand_read_parameters_micron(device_t self, struct nand_chip * const chip)
{
	uint8_t mfgrid;
	uint8_t devid;
	uint8_t dontcare;
	uint8_t params;

	KASSERT(chip->nc_manf_id == NAND_MFR_MICRON);
	switch (chip->nc_manf_id) {
	case NAND_MFR_MICRON:
		break;
	default:
		return 1;
	}

	nand_select(self, true);
	nand_command(self, ONFI_READ_ID);
	nand_address(self, 0x00);
	nand_read_1(self, &mfgrid);
	nand_read_1(self, &devid);
	nand_read_1(self, &dontcare);
	nand_read_1(self, &params);
	nand_select(self, false);

	KASSERT(chip->nc_manf_id == mfgrid);

	switch(devid) {
	case MT29F2G08AAC:
	case MT29F2G08ABC:
	case MT29F2G16AAC:
	case MT29F2G16ABC:
	case MT29F4G08BAC:
		return mt29fxgx_parameters(self, chip, devid, params);
	default:
		aprint_error_dev(self, "unsupported device id %#x\n", devid);
		return 1;
	}
}

static int
mt29fxgx_parameters(device_t self, struct nand_chip * const chip,
	u_int8_t devid, uint8_t params)
{
	const struct nand_micron_devices *dp;
	const char *vendor = "Micron";

	dp = nand_micron_device_lookup(devid);
	if (dp == NULL) {
		aprint_error_dev(self, "unknown device id %#x\n", devid);
		return 1;
	}

	/*
	 * MT29FxGx params across models are the same
	 * except for luns, blocks per lun, and bus width
	 * (and voltage)
	 */
	chip->nc_addr_cycles_column = 2;	/* XXX */
	chip->nc_addr_cycles_row = 3;		/* XXX */
	if (dp->width == 16)
		chip->nc_flags |= NC_BUSWIDTH_16;
	chip->nc_page_size = MT29FxG_PAGESIZE;
	chip->nc_block_size = MT29FxG_BLOCK_PAGES * MT29FxG_PAGESIZE;
	chip->nc_spare_size = MT29FxG_SPARESIZE;
	chip->nc_lun_blocks = dp->lun_blocks;
	chip->nc_num_luns = dp->num_luns;
	chip->nc_size = MT29FxG_PAGESIZE * MT29FxG_BLOCK_PAGES *
		dp->lun_blocks * dp->num_luns;

	aprint_normal_dev(self, "%s %s, size %" PRIu64 "MB\n",
		vendor, dp->name, chip->nc_size >> 20);

	return 0;
}
