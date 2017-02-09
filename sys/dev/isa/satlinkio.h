/*	$NetBSD: satlinkio.h,v 1.4 2015/09/06 06:01:00 dholland Exp $	*/
/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Canada Connect Corp.
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

#ifndef _DEV_ISA_SATLINKIO_H_
#define	_DEV_ISA_SATLINKIO_H_

/*
 * ioctl definitions for SatLink interface.
 */

#include <sys/ioccom.h>

struct satlink_id {
	u_int16_t	sid_mfrid;	/* manufacturer ID */
	u_int8_t	sid_grpid;	/* group ID */
	u_int16_t	sid_userid;	/* user ID */
	u_int32_t	sid_serial;	/* serial number */
};

#define	SATIORESET	_IO('s', 0)	/* reset interface */
#define	SATIOGID	_IOR('s', 1, struct satlink_id) /* read ID */
#define	SATIOSBUFSIZE	_IOW('s', 2, int) /* set buffer size */

#endif /* _DEV_ISA_SATLINKIO_H_ */
