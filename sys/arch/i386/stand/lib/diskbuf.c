/*	$NetBSD: diskbuf.c,v 1.6 2005/12/11 12:17:48 christos Exp $	*/

/*
 * Copyright (c) 1996
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

/* data buffer for BIOS disk / DOS I/O  */

#include <lib/libsa/stand.h>
#include "diskbuf.h"

char *diskbufp;		/* allocated from heap */

const void *diskbuf_user;

/*
 * Global shared "diskbuf" is used as read ahead buffer.
 * This MAY have to not cross a 64k boundary.
 * In practise it is allocated out of the heap early on...
 * NB a statically allocated diskbuf is not guaranteed to not
 * cross a 64k boundary.
 */
char *
alloc_diskbuf(const void *user)
{
	diskbuf_user = user;
	if (!diskbufp) {
		diskbufp = alloc(DISKBUFSIZE);
		if (((int)diskbufp & 0xffff) + DISKBUFSIZE > 0x10000) {
			printf("diskbufp %x\n", (unsigned)diskbufp);
			panic("diskbuf crosses 64k boundary");
		}
	}
	return diskbufp;
}
