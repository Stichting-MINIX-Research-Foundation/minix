/*	$NetBSD: dkio.h,v 1.18 2012/10/19 17:09:07 drochner Exp $	*/

/*
 * Copyright (c) 1987, 1988, 1993
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

#ifndef _SYS_DKIO_H_
#define _SYS_DKIO_H_

#include <sys/ioccom.h>
#include <prop/plistref.h>

/*
 * Disk-specific ioctls.
 */
		/* get and set disklabel; DIOCGPART used internally */
#define DIOCGDINFO	_IOR('d', 101, struct disklabel)/* get */
#define DIOCSDINFO	_IOW('d', 102, struct disklabel)/* set */
#define DIOCWDINFO	_IOW('d', 103, struct disklabel)/* set, update disk */

#ifdef _KERNEL
#define DIOCGDINFO32	(DIOCGDINFO - (sizeof(uint32_t) << IOCPARM_SHIFT))
#define DIOCGPART	_IOW('d', 104, struct partinfo)	/* get partition */
#endif

#if defined(__HAVE_OLD_DISKLABEL) && defined(_KERNEL)
#define ODIOCGDINFO	_IOR('d', 101, struct olddisklabel)/* get */
#define ODIOCSDINFO	_IOW('d', 102, struct olddisklabel)/* set */
#define ODIOCWDINFO	_IOW('d', 103, struct olddisklabel)/* set, update dk */
#endif

/* do format operation, read or write */
#define DIOCRFORMAT	_IOWR('d', 105, struct format_op)
#define DIOCWFORMAT	_IOWR('d', 106, struct format_op)

#define DIOCSSTEP	_IOW('d', 107, int)	/* set step rate */
#define DIOCSRETRIES	_IOW('d', 108, int)	/* set # of retries */
#define DIOCKLABEL	_IOW('d', 119, int)	/* keep/drop label on close? */
#define DIOCWLABEL	_IOW('d', 109, int)	/* write en/disable label */

#define DIOCSBAD	_IOW('d', 110, struct dkbad)	/* set kernel dkbad */
#define DIOCEJECT	_IOW('d', 112, int)	/* eject removable disk */
#define ODIOCEJECT	_IO('d', 112)		/* eject removable disk */
#define DIOCLOCK	_IOW('d', 113, int)	/* lock/unlock pack */

		/* get default label, clear label */
#define	DIOCGDEFLABEL	_IOR('d', 114, struct disklabel)
#define	DIOCCLRLABEL	_IO('d', 115)

#if defined(__HAVE_OLD_DISKLABEL) && defined(_KERNEL)
#define	ODIOCGDEFLABEL	_IOR('d', 114, struct olddisklabel)
#endif

		/* disk cache enable/disable */
#define	DIOCGCACHE	_IOR('d', 116, int)	/* get cache enables */
#define	DIOCSCACHE	_IOW('d', 117, int)	/* set cache enables */

#define	DKCACHE_READ	0x000001 /* read cache enabled */
#define	DKCACHE_WRITE	0x000002 /* write(back) cache enabled */
#define	DKCACHE_RCHANGE	0x000100 /* read enable is changeable */
#define	DKCACHE_WCHANGE	0x000200 /* write enable is changeable */
#define	DKCACHE_SAVE	0x010000 /* cache parameters are savable/save them */

		/* sync disk cache */
#define	DIOCCACHESYNC	_IOW('d', 118, int)	/* sync cache (force?) */

		/* bad sector list */
#define	DIOCBSLIST	_IOWR('d', 119, struct disk_badsecinfo)	/* get list */
#define	DIOCBSFLUSH	_IO('d', 120)			/* flush list */

		/* wedges */
#define	DIOCAWEDGE	_IOWR('d', 121, struct dkwedge_info) /* add wedge */
#define	DIOCGWEDGEINFO	_IOR('d', 122, struct dkwedge_info)  /* get wedge inf */
#define	DIOCDWEDGE	_IOW('d', 123, struct dkwedge_info)  /* del wedge */
#define	DIOCLWEDGES	_IOWR('d', 124, struct dkwedge_list) /* list wedges */

		/* disk buffer queue strategy */
#define	DIOCGSTRATEGY	_IOR('d', 125, struct disk_strategy)
#define	DIOCSSTRATEGY	_IOW('d', 126, struct disk_strategy)

		/* get disk-info dictionary */
#define	DIOCGDISKINFO	_IOR('d', 127, struct plistref)


#define	DIOCTUR		_IOR('d', 128, int)	/* test unit ready */

struct disk_discard_params {
	long maxsize; /* in DEV_BSIZE units */
};
#define DIOCGDISCARDPARAMS _IOR('d', 129, struct disk_discard_params)

struct disk_discard_range {
	daddr_t bno;
	long size;
};
#define DIOCDISCARD	_IOW('d', 130, struct disk_discard_range)

#endif /* _SYS_DKIO_H_ */
