/*	$NetBSD: getextmemx.c,v 1.10 2011/06/16 13:27:59 joerg Exp $	*/

/*
 * Copyright (c) 1997, 1999
 *	Matthias Drochner.  All rights reserved.
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * Try 2 more fancy BIOS calls to get the size of extended
 * memory besides the classical int15/88, take maximum.
 * needs lowlevel parts from biosmemx.S and biosmem.S
 */

#include <lib/libsa/stand.h>
#include "libi386.h"

int
getextmemx(void)
{
	int buf[5], i;
	int extmem = getextmem1();
#ifdef SUPPORT_PS2
	struct {
		uint16_t len;
		uint32_t dta[8];
		/* pad to 64 bytes - without this, machine would reset */
		uint8_t __pad[30];
	} __packed bufps2;
#endif

#ifdef DEBUG_MEMSIZE
	printf("extmem1: %xk\n", extmem);
#endif
	if (!getextmem2(buf)) {
#ifdef DEBUG_MEMSIZE
		printf("extmem2: %xk + %xk\n", buf[0], buf[1] * 64);
#endif
		if (buf[0] <= 15 * 1024) {
			int help = buf[0];
			if (help == 15 * 1024)
				help += buf[1] * 64;
			if (extmem < help)
				extmem = help;
		}
	}

	i = 0;
	do {
		if (getmementry(&i, buf))
			break;
#ifdef DEBUG_MEMSIZE
		printf("mementry: (%d) %x %x %x %x %x\n",
			i, buf[0], buf[1], buf[2], buf[3], buf[4]);
#endif
		if ((buf[4] == 1 && buf[0] == 0x100000)
		    && extmem < buf[2] / 1024)
			extmem = buf[2] / 1024;
	} while (i);

#ifdef SUPPORT_PS2
	/* use local memory information from RETURN MEMORY-MAP INFORMATION */
	if (!getextmemps2((void *) &bufps2)) {
		int help = bufps2.dta[0];
		if (help == 15 * 1024)
			help += bufps2.dta[1];
		if (extmem < help)
			extmem = help;
	}
#endif

	return extmem;
}
