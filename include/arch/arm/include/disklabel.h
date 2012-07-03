/*	$NetBSD: disklabel.h,v 1.9 2009/11/23 13:40:09 pooka Exp $	*/

/*
 * Copyright (c) 1994 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * disklabel.h
 *
 * machine specific disk label info
 *
 * Created      : 04/10/94
 */

#ifndef _ARM_DISKLABEL_H_
#define _ARM_DISKLABEL_H_

#define LABELSECTOR	1		/* sector containing label */
#define LABELOFFSET	0		/* offset of label in sector */
#define MAXPARTITIONS	8		/* number of partitions */
#define RAW_PART	2		/* raw partition: XX?c */

#if HAVE_NBTOOL_CONFIG_H
#include <nbinclude/sys/dkbad.h>
#include <nbinclude/sys/disklabel_acorn.h>
#include <nbinclude/sys/bootblock.h>
#else
#include <sys/dkbad.h>
#include <sys/disklabel_acorn.h>
#include <sys/bootblock.h>
#endif /* HAVE_NBTOOL_CONFIG_H */

struct cpu_disklabel {
	struct mbr_partition mbrparts[MBR_PART_COUNT];
#define __HAVE_DISKLABEL_DKBAD
	struct dkbad bad;
};

#ifdef _KERNEL
struct buf;
struct disklabel;

/* for readdisklabel.  rv != 0 -> matches, msg == NULL -> success */
int	mbr_label_read(dev_t, void (*)(struct buf *), struct disklabel *,
	    struct cpu_disklabel *, const char **, int *, int *);

/* for writedisklabel.  rv == 0 -> dosen't match, rv > 0 -> success */
int	mbr_label_locate(dev_t, void (*)(struct buf *),
	    struct disklabel *, struct cpu_disklabel *, int *, int *);
#endif /* _KERNEL */

#endif /* _ARM_DISKLABEL_H_ */
