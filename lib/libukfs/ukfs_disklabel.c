/*	$NetBSD: ukfs_disklabel.c,v 1.3 2011/02/22 15:42:15 pooka Exp $	*/

/*
 * Local copies of libutil disklabel routines.  This uncouples libukfs
 * from the NetBSD-only libutil.  All identifiers are prefixed with
 * ukfs or UKFS, otherwise the routines are the same.
 */

/*
 * From:
 * NetBSD: disklabel_scan.c,v 1.3 2009/01/18 12:13:03 lukem Exp
 */

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Roland C. Dowdeswell.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>

#include <string.h>
#include <unistd.h>

#include "ukfs_int_disklabel.h"

#define SCAN_INCR	4

int
ukfs__disklabel_scan(struct ukfs__disklabel *lp, int *isswapped,
	char *buf, size_t buflen)
{
	size_t i;
	int imswapped;
	uint16_t npart;

	/* scan for the correct magic numbers. */

	for (i=0; i <= buflen - sizeof(*lp); i += SCAN_INCR) {
		memcpy(lp, buf + i, sizeof(*lp));
		if (lp->d_magic == UKFS_DISKMAGIC &&
		    lp->d_magic2 == UKFS_DISKMAGIC) {
			imswapped = 0;
			goto sanity;
		}
		if (lp->d_magic == bswap32(UKFS_DISKMAGIC) &&
		    lp->d_magic2 == bswap32(UKFS_DISKMAGIC)) {
			imswapped = 1;
			goto sanity;
		}
	}

	return 1;

sanity:
	if (imswapped)
		npart = bswap16(lp->d_npartitions);
	else
		npart = lp->d_npartitions;
	/* we've found something, let's sanity check it */
	if (npart > UKFS_MAXPARTITIONS
	    || ukfs__disklabel_dkcksum(lp, imswapped))
		return 1;

	*isswapped = imswapped;
	return 0;
}


/*
 * From:
 *	$NetBSD: disklabel_dkcksum.c,v 1.4 2005/05/15 21:01:34 thorpej Exp
 */

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

uint16_t
ukfs__disklabel_dkcksum(struct ukfs__disklabel *lp, int imswapped)
{
	uint16_t *start, *end;
	uint16_t sum;
	uint16_t npart;

	if (imswapped)
		npart = bswap16(lp->d_npartitions);
	else
		npart = lp->d_npartitions;

	sum = 0;
	start = (uint16_t *)(void *)lp;
	end = (uint16_t *)(void *)&lp->d_partitions[npart];
	while (start < end) {
		if (imswapped)
			sum ^= bswap16(*start);
		else
			sum ^= *start;
		start++;
	}
	return (sum);
}
