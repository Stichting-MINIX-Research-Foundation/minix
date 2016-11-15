/*	$NetBSD: cpc700reg.h,v 1.5 2008/04/28 20:23:49 martin Exp $	*/

/*
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at Sandburst Corp.
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

/* PCI memory space */
#define CPC_PCI_MEM_BASE	0x80000000
#define CPC_PCI_MEM_END		0xf7ffffff

/* PCI IO space */
#define CPC_PCI_IO_BASE		0xf8000000
#define CPC_PCI_IO_START	0xf8800000 /* for allocation */
#define CPC_PCI_IO_END		0xfbffffff

/* PCI config space */
#define CPC_PCICFGADR		0xfec00000
#define   CPC_PCI_CONFIG_ENABLE		0x80000000
#define CPC_PCICFGDATA		0xfec00004

/* Config space regs */
#define CPC_PCI_BRDGERR		0x48
#define CPC_PCI_CLEARERR	0x0000ff00

#define CPC_BRIDGE_OPTIONS2	0x60
#define  CPC_BRIDGE_O2_ILAT_MASK	0x00f8
#define  CPC_BRIDGE_O2_ILAT_SHIFT	3
#define  CPC_BRIDGE_O2_ILAT_PRIM_ASYNC	18
#define  CPC_BRIDGE_O2_SLAT_MASK	0x0f00
#define  CPC_BRIDGE_O2_SLAT_SHIFT	8
#define  CPC_BRIDGE_O2_2LAT_PRIM_ASYNC	2

/* PCI interrupt acknowledge & special cycle */
#define CPC_INTR_ACK		0xfed00000

#define CPC_PMM0_LOCAL		0xff400000
#define CPC_PMM0_MASK_ATTR	0xff400004
#define CPC_PMM0_PCI_LOW	0xff400008
#define CPC_PMM0_PCI_HIGH	0xff40000c
#define CPC_PMM1_LOCAL		0xff400010
#define CPC_PMM1_MASK_ATTR	0xff400014
#define CPC_PMM1_PCI_LOW	0xff400018
#define CPC_PMM1_PCI_HIGH	0xff40001c
#define CPC_PMM2_LOCAL		0xff400020
#define CPC_PMM2_MASK_ATTR	0xff400024
#define CPC_PMM2_PCI_LOW	0xff400028
#define CPC_PMM2_PCI_HIGH	0xff40002c
#define CPC_PTM1_LOCAL		0xff400030
#define CPC_PTM1_MEMSIZE	0xff400034
#define CPC_PTM2_LOCAL		0xff400038
#define CPC_PTM2_MEMSIZE	0xff40003c

/* serial ports */
#define CPC_COM0		0xff600300
#define CPC_COM1		0xff600400
#define CPC_COM_SPEED(bus)	((bus) / (2 * 4))

/* processor interface registers */
#define CPC_PIF_CFGADR		0xff500000
#define  CPC_PIF_CFG_PRIFOPT1		0x00
#define  CPC_PIF_CFG_ERRDET1		0x04
#define  CPC_PIF_CFG_ERREN1		0x08
#define  CPC_PIF_CFG_CPUERAD		0x0c
#define  CPC_PIF_CFG_CPUERAT		0x10
#define  CPC_PIF_CFG_PLBMIFOPT		0x18
#define  CPC_PIF_CFG_PLBMTLSA1		0x20
#define  CPC_PIF_CFG_PLBMTLEA1		0x24
#define  CPC_PIF_CFG_PLBMTLSA2		0x28
#define  CPC_PIF_CFG_PLBMTLEA2		0x2c
#define  CPC_PIF_CFG_PLBMTLSA3		0x30
#define  CPC_PIF_CFG_PLBMTLEA3		0x34
#define  CPC_PIF_CFG_PLBSNSSA0		0x38
#define  CPC_PIF_CFG_PLBSNSEA0		0x3c
#define  CPC_PIF_CFG_BESR		0x40
#define  CPC_PIF_CFG_BESRSET		0x44
#define  CPC_PIF_CFG_BEAR		0x4c
#define  CPC_PIF_CFG_PLBSWRINT		0x80
#define CPC_PIF_CFGDATA		0xff500004

/* interrupt controller */
#define CPC_UIC_BASE		0xff500880
#define CPC_UIC_SIZE		0x00000024
#define CPC_UIC_SR		0x00000000 /* UIC status (read/clear) */
#define CPC_UIC_SRS		0x00000004 /* UIC status (set) */
#define CPC_UIC_ER		0x00000008 /* UIC enable */
#define CPC_UIC_CR		0x0000000c /* UIC critical */
#define CPC_UIC_PR		0x00000010 /* UIC polarity 0=low, 1=high*/
#define CPC_UIC_TR		0x00000014 /* UIC trigger 0=level; 1=edge */
#define CPC_UIC_MSR		0x00000018 /* UIC masked status */
#define CPC_UIC_VR		0x0000001c /* UIC vector */
#define CPC_UIC_VCR		0x00000020 /* UIC vector configuration */
#define   CPC_UIC_CVR_PRI	  0x00000001 /* 0=intr31 high, 1=intr0 high */
/*
 * if intr0 high then interrupt vector at (vcr&~3) + N*512
 * if intr31 high then interrupt vector at (vcr&~3) + (31-N)*512
 */

/* UIC interrupt bits.  Note, MSB is bit 0 */
/* Internal */
#define CPC_IB_ECC		0
#define CPC_IB_PCI_WR_RANGE	1
#define CPC_IB_PCI_WR_CMD	2
#define CPC_IB_UART_0		3
#define CPC_IB_UART_1		4
#define CPC_IB_IIC_0		5
#define CPC_IB_IIC_1		6
/* 6-16 GPT compare&capture */
/* 20-31 external */
#define CPC_IB_EXT0		20
#define CPC_IB_EXT1		21
#define CPC_IB_EXT2		22
#define CPC_IB_EXT3		23
#define CPC_IB_EXT4		24
#define CPC_IB_EXT5		25
#define CPC_IB_EXT6		26
#define CPC_IB_EXT7		27
#define CPC_IB_EXT8		28
#define CPC_IB_EXT9		29
#define CPC_IB_EXT10		30
#define CPC_IB_EXT11		31

#define CPC_INTR_MASK(irq) (0x80000000 >> (irq))


/* IIC */
#define CPC_IIC0		0xff620000
#define CPC_IIC1		0xff630000
#define CPC_IIC_SIZE		0x00000014
/* offsets from base */
#define CPC_IIC_MDBUF		0x00000000
#define CPC_IIC_SDBUF		0x00000002
#define CPC_IIC_LMADR		0x00000004
#define CPC_IIC_HNADR		0x00000005
#define CPC_IIC_CNTL		0x00000006
#define CPC_IIC_MDCNTL		0x00000007
#define CPC_IIC_STS		0x00000008
#define CPC_IIC_EXTSTS		0x00000009
#define CPC_IIC_LSADR		0x0000000a
#define CPC_IIC_HSADR		0x0000000b
#define CPC_IIC_CLKDIV		0x0000000c
#define CPC_IIC_INTRMSK		0x0000000d
#define CPC_IIC_FRCNT		0x0000000e
#define CPC_IIC_TCNTLSS		0x0000000f
#define CPC_IIC_DIRECTCNTL	0x00000010

/* timer */
#define CPC_TIMER		0xff650000
#define CPC_GPTTBC		0x00000000
