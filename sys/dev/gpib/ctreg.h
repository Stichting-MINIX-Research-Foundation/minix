/*	$NetBSD: ctreg.h,v 1.4 2007/12/25 18:33:37 perry Exp $	*/

/*
 * Copyright (c) 1982, 1990, 1993
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
 *
 *	@(#)ctreg.h	8.1 (Berkeley) 6/10/93
 */

struct	ct_iocmd {
	u_int8_t	unit;		/* punit */
	u_int8_t	saddr;		/* CS80CMD_SADDR */
	u_int16_t	addr0;		/* always 0 */
	u_int32_t	addr;		/* blkno */
	u_int8_t	nop2;		/* CS80CMD_NOP */
	u_int8_t	slen;		/* CS80CMD_SLEN */
	u_int32_t	len;		/* 0 (for rewind) /resid */
	u_int8_t	nop3;		/* CS80CMD_NOP */
	u_int8_t	cmd;		/* CS80CMD_READ/CS80CMD_WRITE */
} __packed;

struct	ct_rscmd {
	u_int8_t	unit;
	u_int8_t	cmd;
} __packed;

struct	ct_ulcmd {
	u_int8_t	unit;
	u_int8_t	cmd;
} __packed;

struct	ct_wfmcmd {
	u_int8_t	unit;
	u_int8_t	cmd;
} __packed;

#define	CT7946ID	0x220
#define CT9145ID	0x268
#define	CT9144ID	0x260
#define	CT9144		0
#define	CT7912PID	0x209
#define	CT7914PID	0x20B
#define	CT88140		1
#define	CT35401ID	0x270

/* convert bytes to 1k tape block and back */
#define CTBTOK(x)	((x) >> 10)
#define CTKTOB(x)	((x) << 10)
