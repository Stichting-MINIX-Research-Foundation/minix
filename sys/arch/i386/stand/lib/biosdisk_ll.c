/*	$NetBSD: biosdisk_ll.c,v 1.31 2011/02/21 02:58:02 jakllsch Exp $	 */

/*-
 * Copyright (c) 2005 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Bang Jun-Young.
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

/*
 * Copyright (c) 1996
 * 	Matthias Drochner.  All rights reserved.
 * Copyright (c) 1996
 * 	Perry E. Metzger.  All rights reserved.
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
 *    must display the following acknowledgements:
 *	This product includes software developed for the NetBSD Project
 *	by Matthias Drochner.
 *	This product includes software developed for the NetBSD Project
 *	by Perry E. Metzger.
 * 4. The names of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

/*
 * shared by bootsector startup (bootsectmain) and biosdisk.c
 * needs lowlevel parts from bios_disk.S
 */

#include <lib/libkern/libkern.h>
#include <lib/libsa/stand.h>

#include "biosdisk_ll.h"
#include "diskbuf.h"
#include "libi386.h"

static int do_read(struct biosdisk_ll *, daddr_t, int, char *);

/*
 * we get from get_diskinfo():
 *      %ah      %ch      %cl      %dh (registers after int13/8), ie
 * xxxxxxxx cccccccc CCssssss hhhhhhhh
 */
#define STATUS(di)	((di)>>24)
#define SPT(di)		(((di)>>8)&0x3f)
#define HEADS(di)	(((di)&0xff)+1)
#define CYL(di)		(((((di)>>16)&0xff)|(((di)>>6)&0x300))+1)

#ifndef BIOSDISK_RETRIES
#define BIOSDISK_RETRIES 5
#endif

int
set_geometry(struct biosdisk_ll *d, struct biosdisk_extinfo *ed)
{
	int diskinfo;

	diskinfo = biosdisk_getinfo(d->dev);
	d->sec = SPT(diskinfo);
	d->head = HEADS(diskinfo);
	d->cyl = CYL(diskinfo);
	d->chs_sectors = d->sec * d->head * d->cyl;

	if (d->dev >= 0x80 + get_harddrives()) {
		d->secsize = 2048;
		d->type = BIOSDISK_TYPE_CD;
	} else {
		d->secsize = 512;
		if (d->dev & 0x80)
			d->type = BIOSDISK_TYPE_HD;
		else
			d->type = BIOSDISK_TYPE_FD;
	}

	/*
	 * Some broken BIOSes such as one found on Soltek SL-75DRV2 report
	 * that they don't support int13 extension for CD-ROM drives while
	 * they actually do. As a workaround, if the boot device is a CD we
	 * assume that the extension is available. Note that only very old
	 * BIOSes don't support the extended mode, and they don't work with
	 * ATAPI CD-ROM drives, either. So there's no problem.
	 */
	d->flags = 0;
	if (d->type == BIOSDISK_TYPE_CD ||
	    (d->type == BIOSDISK_TYPE_HD && biosdisk_int13ext(d->dev))) {
		d->flags |= BIOSDISK_INT13EXT;
		if (ed != NULL) {
			ed->size = sizeof(*ed);
			if (biosdisk_getextinfo(d->dev, ed) != 0)
				return -1;
		}
	}

	/*
	 * If the drive is 2.88MB floppy drive, check that we can actually
	 * read sector >= 18. If not, assume 1.44MB floppy disk.
	 */
	if (d->type == BIOSDISK_TYPE_FD && SPT(diskinfo) == 36) {
		char buf[512];

		if (biosdisk_read(d->dev, 0, 0, 18, 1, buf)) {
			d->sec = 18;
			d->chs_sectors /= 2;
		}
	}

	return 0;
}

/*
 * Global shared "diskbuf" is used as read ahead buffer.  For reading from
 * floppies, the bootstrap has to be loaded on a 64K boundary to ensure that
 * this buffer doesn't cross a 64K DMA boundary.
 */
static int      ra_dev;
static daddr_t  ra_end;
static daddr_t  ra_first;

/*
 * Because some older BIOSes have bugs in their int13 extensions, we
 * only try to use the extended read if the I/O request can't be addressed
 * using CHS.
 *
 * Of course, some BIOSes have bugs in ths CHS read, such as failing to
 * function properly if the MBR table has a different geometry than the
 * BIOS would generate internally for the device in question, and so we
 * provide a way to force the extended on hard disks via a compile-time
 * option.
 */
#if defined(FORCE_INT13EXT)
#define	NEED_INT13EXT(d, dblk, num)				\
	(((d)->dev & 0x80) != 0)
#else
#define	NEED_INT13EXT(d, dblk, num)				\
	(((d)->type == BIOSDISK_TYPE_CD) ||                     \
	 ((d)->type == BIOSDISK_TYPE_HD &&			\
	  ((dblk) + (num)) >= (d)->chs_sectors))
#endif

static int
do_read(struct biosdisk_ll *d, daddr_t dblk, int num, char *buf)
{

	if (NEED_INT13EXT(d, dblk, num)) {
		struct {
			int8_t size;
			int8_t resvd;
			int16_t cnt;
			int16_t off;
			int16_t seg;
			int64_t sec;
		} ext;

		if (!(d->flags & BIOSDISK_INT13EXT))
			return -1;
		ext.size = sizeof(ext);
		ext.resvd = 0;
		ext.cnt = num;
		/* seg:off of physical address */
		ext.off = (int)buf & 0xf;
		ext.seg = vtophys(buf) >> 4;
		ext.sec = dblk;

		if (biosdisk_extread(d->dev, &ext)) {
			(void)biosdisk_reset(d->dev);
			return -1;
		}

		return ext.cnt;
	} else {
		int cyl, head, sec, nsec, spc, dblk32;

		dblk32 = (int)dblk;
		spc = d->head * d->sec;
		cyl = dblk32 / spc;
		head = (dblk32 % spc) / d->sec;
		sec = dblk32 % d->sec;
		nsec = d->sec - sec;

		if (nsec > num)
			nsec = num;

		if (biosdisk_read(d->dev, cyl, head, sec, nsec, buf)) {
			(void)biosdisk_reset(d->dev);
			return -1;
		}

		return nsec;
	}
}

/*
 * NB if 'cold' is set below not all of the program is loaded, so
 * mustn't use data segment, bss, call library functions or do read-ahead.
 */
int
readsects(struct biosdisk_ll *d, daddr_t dblk, int num, char *buf, int cold)
{
#ifdef BOOTXX
#define cold 1		/* collapse out references to diskbufp */
#endif
	while (num) {
		int nsec;

		/* check for usable data in read-ahead buffer */
		if (cold || diskbuf_user != &ra_dev || d->dev != ra_dev
		    || dblk < ra_first || dblk >= ra_end) {

			/* no, read from disk */
			char *trbuf;
			int maxsecs;
			int retries = BIOSDISK_RETRIES;

			if (cold) {
				/* transfer directly to buffer */
				trbuf = buf;
				maxsecs = num;
			} else {
				/* fill read-ahead buffer */
				trbuf = alloc_diskbuf(0); /* no data yet */
				maxsecs = DISKBUFSIZE / d->secsize;
			}

			while ((nsec = do_read(d, dblk, maxsecs, trbuf)) < 0) {
#ifdef DISK_DEBUG
				if (!cold)
					printf("read error dblk %"PRId64"-%"PRId64"\n",
					    dblk, (dblk + maxsecs - 1));
#endif
				if (--retries >= 0)
					continue;
				return -1;	/* XXX cannot output here if
						 * (cold) */
			}
			if (!cold) {
				ra_dev = d->dev;
				ra_first = dblk;
				ra_end = dblk + nsec;
				diskbuf_user = &ra_dev;
			}
		} else		/* can take blocks from end of read-ahead
				 * buffer */
			nsec = ra_end - dblk;

		if (!cold) {
			/* copy data from read-ahead to user buffer */
			if (nsec > num)
				nsec = num;
			memcpy(buf,
			       diskbufp + (dblk - ra_first) * d->secsize,
			       nsec * d->secsize);
		}
		buf += nsec * d->secsize;
		num -= nsec;
		dblk += nsec;
	}

	return 0;
}

/*
 * Return the number of hard disk drives.
 */
int
get_harddrives(void)
{
	/*
	 * Some BIOSes are buggy so that they return incorrect number
	 * of hard drives with int13/ah=8. We read a byte at 0040:0075
	 * instead, which is known to be always correct.
	 */
	int n = 0;

	pvbcopy((void *)0x475, &n, 1);

	return n;
}
