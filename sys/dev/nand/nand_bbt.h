/*	$NetBSD: nand_bbt.h,v 1.2 2011/04/04 14:25:10 ahoka Exp $	*/

/*-
 * Copyright (c) 2010 Department of Software Engineering,
 *		      University of Szeged, Hungary
 * Copyright (c) 2010 Adam Hoka <ahoka@NetBSD.org>
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

#ifndef _NAND_BBT_H_
#define _NAND_BBT_H_

enum {
	NAND_BBT_MARKER_FACTORY_BAD = 0x00,	/* 00 */
	NAND_BBT_MARKER_WORNOUT_BAD = 0x01,	/* 01 */
	NAND_BBT_MARKER_RESERVED = 0x02,	/* 10 */
	NAND_BBT_MARKER_GOOD = 0x03		/* 11 */
};

enum {
	NAND_BBT_OFFSET = 0x0e
};

void nand_bbt_init(device_t);
void nand_bbt_detach(device_t);
void nand_bbt_scan(device_t);
bool nand_bbt_update(device_t);
bool nand_bbt_load(device_t);
void nand_bbt_block_mark(device_t, flash_off_t, uint8_t);
void nand_bbt_block_markbad(device_t, flash_off_t);
void nand_bbt_block_markfactorybad(device_t, flash_off_t);
bool nand_bbt_block_isbad(device_t, flash_off_t);

#endif
