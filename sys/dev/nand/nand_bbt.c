/*	$NetBSD: nand_bbt.c,v 1.7 2013/10/22 01:01:27 htodd Exp $	*/

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
 * Implementation of Bad Block Tables (BBTs).
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: nand_bbt.c,v 1.7 2013/10/22 01:01:27 htodd Exp $");

#include <sys/param.h>
#include <sys/kmem.h>

#include "nand.h"
#include "nand_bbt.h"

void
nand_bbt_init(device_t self)
{
	struct nand_softc *sc = device_private(self);
	struct nand_chip *chip = &sc->sc_chip;
	struct nand_bbt *bbt = &sc->sc_bbt;

	bbt->nbbt_size = chip->nc_size / chip->nc_block_size / 4;
	bbt->nbbt_bitmap = kmem_alloc(bbt->nbbt_size, KM_SLEEP);

	memset(bbt->nbbt_bitmap, 0xff, bbt->nbbt_size);
}

void
nand_bbt_detach(device_t self)
{
	struct nand_softc *sc = device_private(self);
	struct nand_bbt *bbt = &sc->sc_bbt;

	kmem_free(bbt->nbbt_bitmap, bbt->nbbt_size);
}

void
nand_bbt_scan(device_t self)
{
	struct nand_softc *sc = device_private(self);
	struct nand_chip *chip = &sc->sc_chip;
	flash_off_t i, blocks, addr;

	blocks = chip->nc_size / chip->nc_block_size;

	aprint_normal_dev(self, "scanning for bad blocks\n");

	addr = 0;
	for (i = 0; i < blocks; i++) {
		if (nand_isfactorybad(self, addr)) {
			nand_bbt_block_markfactorybad(self, i);
		} else if (nand_iswornoutbad(self, addr)) {
			nand_bbt_block_markbad(self, i);
		}

		addr += chip->nc_block_size;
	}
}

bool
nand_bbt_update(device_t self)
{
	return true;
}

static bool
nand_bbt_page_has_bbt(device_t self, flash_off_t addr) {
	struct nand_softc *sc = device_private(self);
	struct nand_chip *chip = &sc->sc_chip;
	uint8_t *oob = chip->nc_oob_cache;

	nand_read_oob(self, addr, oob);

	if (oob[NAND_BBT_OFFSET] == 'B' &&
	    oob[NAND_BBT_OFFSET + 1] == 'b' &&
	    oob[NAND_BBT_OFFSET + 2] == 't') {
		return true;
	} else {
		return false;
	}
}

static bool
nand_bbt_get_bbt_from_page(device_t self, flash_off_t addr)
{
	struct nand_softc *sc = device_private(self);
	struct nand_chip *chip = &sc->sc_chip;
	struct nand_bbt *bbt = &sc->sc_bbt;
	uint8_t *bbtp, *buf = chip->nc_page_cache;
	size_t left, bbt_pages, i;

	bbt_pages = bbt->nbbt_size / chip->nc_page_size;
	if (bbt->nbbt_size % chip->nc_page_size)
		bbt_pages++;
	
	if (nand_isbad(self, addr)) {
		return false;
	}

	if (nand_bbt_page_has_bbt(self, addr)) {
		bbtp = bbt->nbbt_bitmap;
		left = bbt->nbbt_size;

		for (i = 0; i < bbt_pages; i++) {
			nand_read_page(self, addr, buf);

			if (i == bbt_pages - 1) {
				KASSERT(left <= chip->nc_page_size);
				memcpy(bbtp, buf, left);
			} else {
				memcpy(bbtp, buf, chip->nc_page_size);
			}

			bbtp += chip->nc_page_size;
			left -= chip->nc_page_size;
			addr += chip->nc_page_size;
		}

		return true;
	} else {
		return false;
	}
}

bool
nand_bbt_load(device_t self)
{
	struct nand_softc *sc = device_private(self);
	struct nand_chip *chip = &sc->sc_chip;
	flash_off_t blockaddr;
	int n;

	blockaddr = chip->nc_size - chip->nc_block_size;
	/* XXX currently we check the last 4 blocks */
	for (n = 0; n < 4; n++) {
		if (nand_bbt_get_bbt_from_page(self, blockaddr)) {
			break;
		} else {
			blockaddr -= chip->nc_block_size;
		}
	}

	return true;
}

void
nand_bbt_block_markbad(device_t self, flash_off_t block)
{
	if (nand_bbt_block_isbad(self, block)) {
		aprint_error_dev(self,
		    "trying to mark block bad already marked in bbt\n");
	}
	/* XXX check if this is the correct marker */
	nand_bbt_block_mark(self, block, NAND_BBT_MARKER_WORNOUT_BAD);
}

void
nand_bbt_block_markfactorybad(device_t self, flash_off_t block)
{
	if (nand_bbt_block_isbad(self, block)) {
		aprint_error_dev(self,
		    "trying to mark block factory bad already"
		    " marked in bbt\n");
	}
	nand_bbt_block_mark(self, block, NAND_BBT_MARKER_FACTORY_BAD);
}

void
nand_bbt_block_mark(device_t self, flash_off_t block, uint8_t marker)
{
	struct nand_softc *sc = device_private(self);
	struct nand_chip *chip = &sc->sc_chip;
	struct nand_bbt *bbt = &sc->sc_bbt;
	uint8_t clean;

	__USE(chip);
	KASSERT(block < chip->nc_size / chip->nc_block_size);

	clean = (~0x03 << ((block % 4) * 2));
	marker = (marker << ((block % 4) * 2));

	/* set byte containing the 2 bit marker for this block */
	bbt->nbbt_bitmap[block / 4] &= clean;
	bbt->nbbt_bitmap[block / 4] |= marker;
}

bool
nand_bbt_block_isbad(device_t self, flash_off_t block)
{
	struct nand_softc *sc = device_private(self);
	struct nand_chip *chip = &sc->sc_chip;
	struct nand_bbt *bbt = &sc->sc_bbt;
	uint8_t byte, marker;
	bool result;

	__USE(chip);
	KASSERT(block < chip->nc_size / chip->nc_block_size);

	/* get byte containing the 2 bit marker for this block */
	byte = bbt->nbbt_bitmap[block / 4];

	/* extract the 2 bit marker from the byte */
	marker = (byte >> ((block % 4) * 2)) & 0x03;

	switch (marker) {
	case NAND_BBT_MARKER_FACTORY_BAD:
	case NAND_BBT_MARKER_WORNOUT_BAD:
	case NAND_BBT_MARKER_RESERVED:
		result = true;
		break;
	case NAND_BBT_MARKER_GOOD:
		result = false;
		break;
	default:
		panic("error in marker extraction");
	}

	return result;
}
