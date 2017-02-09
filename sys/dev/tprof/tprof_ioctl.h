/*	$NetBSD: tprof_ioctl.h,v 1.3 2011/04/14 16:23:59 yamt Exp $	*/

/*-
 * Copyright (c)2008,2010 YAMAMOTO Takashi,
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _DEV_TPROF_TPROF_IOCTL_H_
#define _DEV_TPROF_TPROF_IOCTL_H_

/*
 * definitions for userland consumer
 */

#include <sys/ioccom.h>

#include <dev/tprof/tprof_types.h>

#define	TPROF_VERSION	3	/* kernel-userland ABI version */

#define	TPROF_IOC_GETVERSION	_IOR('T', 1, int)

struct tprof_param {
	int dummy;
};
#define	TPROF_IOC_START		_IOW('T', 2, struct tprof_param)

#define	TPROF_IOC_STOP		_IO('T', 3)

struct tprof_stat {
	uint64_t ts_sample;	/* samples successfully recorded */
	uint64_t ts_overflow;	/* samples dropped due to overflow */
	uint64_t ts_buf;	/* buffers successfully queued for read(2) */
	uint64_t ts_emptybuf;	/* empty buffers dropped */
	uint64_t ts_dropbuf;	/* buffers dropped due to the global limit */
	uint64_t ts_dropbuf_sample; /* samples dropped with ts_dropbuf */
};
#define	TPROF_IOC_GETSTAT	_IOR('T', 4, struct tprof_stat)

#endif /* _DEV_TPROF_TPROF_IOCTL_H_ */
