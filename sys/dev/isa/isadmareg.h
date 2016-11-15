/*	$NetBSD: isadmareg.h,v 1.8 2008/04/28 20:23:52 martin Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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

#ifndef _DEV_ISA_ISADMAREG_H_
#define	_DEV_ISA_ISADMAREG_H_

#include <dev/ic/i8237reg.h>

/*
 * By default, ISA DMA controllers can do 64k or 128k transfers, depending
 * on the width of the channel being used.  However, this may be modified
 * by our parent based on bus constraints, etc.  Clients of ISA DMA should
 * query the ISA DMA to determine the maximum transfer size allowed.
 */
#define	ISA_DMA_MAXSIZE_8BIT	(64 * 1024)
#define	ISA_DMA_MAXSIZE_16BIT	(ISA_DMA_MAXSIZE_8BIT * 2)

#define	ISA_DMA_MAXSIZE_DEFAULT(chan)					\
	(((chan) & 4) ? ISA_DMA_MAXSIZE_16BIT : ISA_DMA_MAXSIZE_8BIT)

/*
 * Register definitions for DMA controller 1 (channels 0..3):
 */
#define	DMA1_CHN(c)	(1*(2*(c)))		/* addr reg for channel c */
#define	DMA1_SR		(1*8)			/* status register */
#define	DMA1_SMSK	(1*10)			/* single mask register */
#define	DMA1_MODE	(1*11)			/* mode register */
#define	DMA1_FFC	(1*12)			/* clear first/last FF */
#define	DMA1_MASK	(1*15)			/* mask register */

#define	DMA1_IOSIZE	(1*16)

/*
 * Register definitions for DMA controller 2 (channels 4..7):
 */
#define	DMA2_CHN(c)	(2*(2*(c)))		/* addr reg for channel c */
#define	DMA2_SR		(2*8)			/* status register */
#define	DMA2_SMSK	(2*10)			/* single mask register */
#define	DMA2_MODE	(2*11)			/* mode register */
#define	DMA2_FFC	(2*12)			/* clear first/last FF */
#define	DMA2_MASK	(2*15)			/* mask register */

#define	DMA2_IOSIZE	(2*16)

#endif /* _DEV_ISA_ISADMAREG_H_ */
