/*	$NetBSD: apple_smcreg.h,v 1.1 2014/04/01 17:47:36 riastradh Exp $	*/

/*
 * Apple System Management Controller Registers
 */

/*-
 * Copyright (c) 2013 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Taylor R. Campbell.
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

#ifndef	_DEV_IC_APPLE_SMCREG_H_
#define	_DEV_IC_APPLE_SMCREG_H_

#define	APPLE_SMC_DATA			0x00
#define	APPLE_SMC_CSR			0x04
#define APPLE_SMC_REGSIZE		0x08

#define	APPLE_SMC_STATUS_READ_READY	0x01	/* Ready for read.  */
#define	APPLE_SMC_STATUS_WRITE_PENDING	0x02	/* Write not yet accepted.  */
#define	APPLE_SMC_STATUS_WRITE_ACCEPTED	0x04	/* Write accepted.  */

#define	APPLE_SMC_CMD_READ_KEY		0x10
#define	APPLE_SMC_CMD_WRITE_KEY		0x11
#define	APPLE_SMC_CMD_NTH_KEY		0x12
#define	APPLE_SMC_CMD_KEY_DESC		0x13

#define	APPLE_SMC_NKEYS_KEY		"#KEY"

#endif  /* _DEV_IC_APPLE_SMCREG_H_ */
