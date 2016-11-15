/*	$NetBSD: satapmpreg.h,v 1.6 2012/07/31 15:50:34 bouyer Exp $	*/

/*-
 * Copyright (c) 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Frank van der Linden of Wasabi Systems, Inc.
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

#ifndef _DEV_ATA_SATAPMPREG_H_
#define	_DEV_ATA_SATAPMPREG_H_

/*
 * Global port multiplier registers, accessed through the control port.
 */

#define PMP_GSCR_ID	0x00		/* product and vendor id */
#define 	PMP_ID_DEV(x)	((x) >> 16)
#define		PMP_ID_VEND(x)	((x) & 0xffff)
#define PMP_GSCR_REV	0x01		/* revision */
#define		PMP_REV_SPEC_10		0x02
#define		PMP_REV_SPEC_11		0x04
#define		PMP_REV_LEVEL(x)	(((x) >> 8) & 0xff)
#define PMP_GSCR_INF	0x02		/* info */
#define		PMP_INF_NPORTS(x)	((x) & 0xf)
#define PMP_GSCR_ERR	0x20		/* error bit for each port */
#define		PMP_ERR(p)		((1) << (p))
#define PMP_GSCR_ERREN	0x21		/* error bit enable for each port */
#define		PMP_ERREN(p)		((1) << (p))
#define PMP_GSCR_FEAT	0x40		/* features */
#define 	PMP_FEAT_BIST		0x01
#define 	PMP_FEAT_PMREQ		0x01
#define		PMP_FEAT_SCC		0x04
#define		PMP_FEAT_ASYNC		0x08
#define PMP_GSCR_FEATEN	0x60		/* feature enable, bits as above */

#define PMP_GSCR_VENDSTART 0x80		/* start of vendor unique registers */

#define PMP_GSCR_NREGS	256

/*
 * Port status and control registers (per port)
 */
#define PMP_PSCR_SStatus	0x00
#define PMP_PSCR_SError		0x01
#define PMP_PSCR_SControl	0x02
#define PMP_PSCR_SActive	0x03

/*
 * Control port as defined in the spec.
 */
#define PMP_PORT_CTL		0x0f

/*
 * Device commands for port multipliers
 */
#define PMPC_READ_PORT		0xe4
#define PMPC_WRITE_PORT		0xe8

/* max number of drives (last one being the PM itself */
#define PMP_MAX_DRIVES		16

#endif /* _DEV_ATA_SATAPMPREG_H_ */
