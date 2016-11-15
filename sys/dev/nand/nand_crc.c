/*	$NetBSD: nand_crc.c,v 1.1 2011/02/26 18:07:31 ahoka Exp $	*/

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

/* Implements CRC-16 as required by the ONFI 2.3 specification */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: nand_crc.c,v 1.1 2011/02/26 18:07:31 ahoka Exp $");

#include "nand_crc.h"

uint16_t
nand_crc16(uint8_t *data, size_t len)
{
	const uint16_t init = 0x4f4e;
	const uint16_t polynom = 0x8005;
	const uint16_t highbit = 0x0001 << 15;
	uint16_t crc = init;
	size_t i;
	int j;

	for (i = 0; i < len; i++) {
		crc ^= data[i] << 8;
		
		for (j = 0; j < 8; j++) {
			if ((crc & highbit) != 0x00)
				crc = (crc << 1) ^ polynom;
			else
				crc <<= 1;
		}
	}

	return crc;
}
