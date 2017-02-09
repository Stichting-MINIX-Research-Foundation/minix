/*	$NetBSD: ncr53c400reg.h,v 1.2 2008/04/28 20:23:50 martin Exp $	*/

/*-
 * Copyright (c)  1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by John M. Ruschmeyer.
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
 * Definitions for 53C400 SCSI-controller chip.
 *
 * Derived from Linux NCR-5380 generic driver sources (by Drew Eckhardt).
 *
 * Copyright (C) 1994 Serge Vakulenko (vak@cronyx.ru)
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
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPERS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE DEVELOPERS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * NCR5380 registers
 */
#define C80_CSDR	0	/* ro - Current SCSI Data Reg. */
#define C80_ODR		0	/* wo - Output Data Reg. */
#define C80_ICR		1	/* rw - Initiator Command Reg. */
#define C80_MR		2	/* rw - Mode Reg. */
#define C80_TCR		3	/* rw - Target Command Reg. */
#define C80_CSBR	4	/* ro - Current SCSI Bus Status Reg. */
#define C80_SER		4	/* wo - Select Enable Reg. */
#define C80_BSR		5	/* ro - Bus and Status Reg. */
#define C80_SDSR	5	/* wo - Start DMA Send Reg. */
#define C80_IDR		6	/* ro - Input Data Reg. */
#define C80_SDTR	6	/* wo - Start DMA Target Receive Reg. */
#define C80_RPIR	7	/* ro - Reset Parity/Interrupt Reg. */
#define C80_SDIR	7	/* wo - Start DMA Initiator Receive Reg. */


#define C400_CSR		0	/* rw - Control and Status Reg. */
# define C400_CSR_5380_ENABLE		0x80
# define C400_CSR_TRANSFER_DIRECTION	0x40
# define C400_CSR_TRANSFER_READY_INTR	0x20
# define C400_CSR_5380_INTR		0x10
# define C400_CSR_SHARED_INTR		0x08
# define C400_CSR_HOST_BUF_NOT_READY	0x04 /* read only */
# define C400_CSR_SCSI_BUF_READY	0x02 /* read only */
# define C400_CSR_5380_GATED_IRQ	0x01 /* read only */
# define C400_CSR_BITS "\20\1irq\2sbrdy\3hbrdy\4shintr\5intr\6tintr\7tdir\10enable"

#define C400_CCR		1	/* rw - Clock Counter Reg. */

#define C400_HBR		4	/* rw - Host Buffer Reg. */

#define C400_5380_REG_OFFSET	8	/* Offset of 5380 registers. */
