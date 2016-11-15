/* $NetBSD: cx23885reg.h,v 1.2 2011/08/09 11:26:40 jmcneill Exp $ */

/*
 * Copyright (c) 2008, 2011 Jonathan A. Kollasch
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _DEV_PCI_CX23885REG_H
#define _DEV_PCI_CX23885REG_H

#include <dev/pci/pcireg.h>

#define CX23885_MMBASE		PCI_BAR(0)

/* misc. registers */

#define DEV_CNTRL2		0x040000
#define PCI_INT_MSK		0x040010
#define PCI_INT_STAT		0x040014
#define PCI_INT_MSTAT		0x040018

#define VID_C_INT_MSK		0x040040
#define VID_C_INT_STAT		0x040044
#define VID_C_INT_MSTAT		0x040048
#define VID_C_INT_SSTAT		0x04004c

#define DMA5_PTR1		0x100010
#define DMA5_PTR2		0x100050
#define DMA5_CNT1		0x100090
#define DMA5_CNT2		0x1000d0

/* GPIO */
#define GP0_IO			0x110010
#define GPIO_ISM		0x110014
#define SOFT_RESET		0x11001c

#define CLK_DELAY		0x110048
#define PAD_CTRL		0x11004c

/* Video C Interface */
#define VID_C_GPCNT		0x130220
#define VID_C_GPCNT_CTL		0x130230
#define VBI_C_GPCNT_CTL		0x130234
#define VID_C_DMA_CTL		0x130240
#define VID_C_LNGTH		0x130250
#define VID_C_HW_SOP_CTL	0x130254
#define VID_C_GEN_CTL		0x130258
#define VID_C_BD_PKT_STATUS	0x13025c
#define VID_C_SOP_STATUS	0x130260
#define VID_C_FIFO_OVFL_STAT	0x130264
#define VID_C_VLD_MISC		0x130268
#define VID_C_TS_CLK_EN		0x13026c

/* serial controllers */
#define I2C_BASE		0x180000
#define I2C_SIZE		0x010000
#define I2C_NUM			3

/* RISC instructions */

#define CX_RISC_WRITECR		0xd0000000
#define CX_RISC_WRITECM		0xc0000000
#define CX_RISC_WRITERM		0xb0000000
#define CX_RISC_READC		0xa0000000
#define CX_RISC_READ		0x90000000
#define CX_RISC_SYNC		0x80000000
#define CX_RISC_JUMP		0x70000000
#define CX_RISC_WRITEC		0x50000000
#define CX_RISC_SKIP		0x20000000
#define CX_RISC_WRITE		0x10000000
#define CX_RISC_SOL		0x08000000
#define CX_RISC_EOL		0x04000000
#define CX_RISC_IRQ2		0x02000000
#define CX_RISC_IRQ1		0x01000000
#define CX_RISC_IMM		0x00000001
#define CX_RISC_SRP		0x00000001

#define CX_CNT_CTL_NOOP		0x0
#define CX_CNT_CTL_INCR		0x1
#define CX_CNT_CTL_ZERO		0x3
#define CX_RISC_CNT_CTL		__BITS(17,16)
#define CX_RISC_CNT_CTL_NOOP	__SHIFTIN(CX_RISC_CNT_CTL,CX_CNT_CTL_NOOP)
#define CX_RISC_CNT_CTL_INCR	__SHIFTIN(CX_RISC_CNT_CTL,CX_CNT_CTL_INCR)
#define CX_RISC_CNT_CTL_ZERO	__SHIFTIN(CX_RISC_CNT_CTL,CX_CNT_CTL_ZERO)

/* Channel Management Data Structure */
/*  offsets */
#define CMDS_O_IRPC		0x00
#define CMDS_O_CDTB		0x08
#define CMDS_O_CDTS		0x0c
#define CMDS_O_IQB		0x10
#define CMDS_O_IQS		0x14

/*  bits */
#define CMDS_IQS_ISRP		__BIT(31)

#endif /* !_DEV_PCI_CX23885REG_H */
