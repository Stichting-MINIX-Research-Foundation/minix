/*	$NetBSD: mc68450reg.h,v 1.5 2008/04/28 20:23:50 martin Exp $	*/

/*-
 * Copyright (c) 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Minoura Makoto.
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
 * Motorola MC68450 DMAC register definition.
 */

#define DMAC_NCHAN	4	/* Number of channels */
#define DMAC_CHAN_SIZE	0x40	/* I/O area size per channes */

/* register location per channel */
#define DMAC_REG_CSR	0x00	/* Channel Status Register  */
#define DMAC_REG_CER	0x01	/* Channel Error Register */
#define DMAC_REG_DCR	0x04	/* Device Control Register */
#define DMAC_REG_OCR	0x05	/* Operation Control Register */
#define DMAC_REG_SCR	0x06	/* Sequence Control Register */
#define DMAC_REG_CCR	0x07	/* Channel Control Register */
#define DMAC_REG_MTCR	0x0a	/* Memory Transfer Count Register */
#define DMAC_REG_MAR	0x0c	/* Memory Address Register */
#define DMAC_REG_DAR	0x14	/* Device Address Register */
#define DMAC_REG_BTCR	0x1a	/* Base Transfer Count Register */
#define DMAC_REG_BAR	0x1c	/* Base Address Register */
#define DMAC_REG_NIVR	0x25	/* Normal Interrupt Vector Register */
#define DMAC_REG_EIVR	0x27	/* Error Interrupt Vector Register */
#define DMAC_REG_MFCR	0x29	/* Memory Function Code Register */
#define DMAC_REG_CPR	0x2d	/* Channel Priority Register */
#define DMAC_REG_DFCR	0x31	/* Device Function Code Register */
#define DMAC_REG_BFCR	0x39	/* Base Function Code Register */
#define DMAC_REG_GCR	0x3f	/* General Control Register */

/* CSR bits */
#define DMAC_CSR_COC	0x80	/* Channel Operation Complete */
#define DMAC_CSR_BTC	0x40	/* Block Transfer Complete */
#define DMAC_CSR_NDT	0x20	/* Normal Device Termination */
#define DMAC_CSR_ERR	0x10	/* Error */
#define DMAC_CSR_ACT	0x08	/* Channel Active */
#define DMAC_CSR_PCT	0x02	/* PCL Transition */
#define DMAC_CSR_PCS	0x01	/* PCL Level */

/* CER meanings */
/*
 * 0x00: No error
 * 0x01: Configuration error
 * 0x02: Operation timing error
 * 0x05: Address error in memory transfer
 * 0x06: Address error in device transfer
 * 0x07: Address error in base address reading
 * 0x09: Bus error in memory transfer
 * 0x0a: Bus error in device transfer
 * 0x0b: Bus error in base address reading
 * 0x0d: Count error in memory transfer count
 * 0x0e: Count error in device transfer count
 * 0x0f: Count error in base address
 * 0x10: External abort
 * 0x11: Software abort
 */

/* DCR bits */
#define DMAC_DCR_XRM_MASK	0xc0
#define DMAC_DCR_XRM_BURST	0x00 /* Burst mode */
#define DMAC_DCR_XRM_CSWOH	0x80 /* Cycle steal w/o hold */
#define DMAC_DCR_XRM_CSWH	0xc0 /* Cycle steal w/ hold */
#define DMAC_DCR_OTYP_MASK	0x30
#define DMAC_DCR_OTYP_EASYNC	0x00 /* Explicit M68000 */
#define DMAC_DCR_OTYP_ESYNC	0x10 /* Explicit M6800 */
#define DMAC_DCR_OTYP_IA	0x20 /* Implicit with ack */
#define DMAC_DCR_OTYP_IAR	0x30 /* Implicit with ack and rdy */
#define DMAC_DCR_OPS_MASK	0x08
#define DMAC_DCR_OPS_8BIT	0x00 /* 8bit */
#define DMAC_DCR_OPS_16BIT	0x08 /* 16bit */
#define DMAC_DCR_PCL_MASK	0x03
#define DMAC_DCR_PCL_STATUS	0x00
#define DMAC_DCR_PCL_INTERRUPT	0x01
#define DMAC_DCR_PCL_STARTPLS	0x02
#define DMAC_DCR_PCL_ABORT	0x03

/* OCR bits */
#define DMAC_OCR_DIR_MASK	0x80
#define DMAC_OCR_DIR_MTD	0x00 /* Direction: memory to device */
#define DMAC_OCR_DIR_DTM	0x80 /* Direction: device to memory */
#define DMAC_OCR_SIZE_MASK 	0x30
#define DMAC_OCR_SIZE_BYTE	0x00 /* Size: byte */
#define DMAC_OCR_SIZE_WORD	0x10 /* Size: word */
#define DMAC_OCR_SIZE_LONGWORD	0x20 /* Size: longword */
#define DMAC_OCR_SIZE_BYTE_NOPACK 0x30 /* Size: byte, no packing */
#define DMAC_OCR_CHAIN_MASK	0x0c
#define DMAC_OCR_CHAIN_DISABLED	0x00 /* Chain mode disabled */
#define DMAC_OCR_CHAIN_ARRAY	0x08 /* Array chain mode */
#define DMAC_OCR_CHAIN_LINKARRAY 0x0c /* Linked array chain mode */
#define DMAC_OCR_REQG_MASK	0x03
#define DMAC_OCR_REQG_LIMITED_RATE 0x00	/* Internal limited rate */
#define DMAC_OCR_REQG_MAXIMUM_RATE 0x01	/* Internal maximum rate */
#define DMAC_OCR_REQG_EXTERNAL	0x02 /* External */
#define DMAC_OCR_REQG_AUTO_START 0x03 /* Auto start, external */

/* SCR bits */
#define DMAC_SCR_MAC_MASK	0x0c
#define DMAC_SCR_MAC_NO_COUNT	0x00 /* Fixed memory address */
#define DMAC_SCR_MAC_COUNT_UP	0x04 /* Memory address count up */
#define DMAC_SCR_MAC_COUNT_DOWN	0x08 /* Memory address count down */
#define DMAC_SCR_DAC_MASK	0x03
#define DMAC_SCR_DAC_NO_COUNT	0x00 /* Fixed device address */
#define DMAC_SCR_DAC_COUNT_UP	0x01 /* Device address count up */
#define DMAC_SCR_DAC_COUNT_DOWN	0x02 /* Device address count down */

/* CCR bits */
#define DMAC_CCR_STR		0x80 /* Start channel */
#define DMAC_CCR_CNT		0x40 /* Continue operation */
#define DMAC_CCR_HLT		0x20 /* Software halt */
#define DMAC_CCR_SAB		0x10 /* Software abort */
#define DMAC_CCR_INT		0x08 /* Interrupt enable */

/* GCR bits */
#define DMAC_GCR_BT_MASK	0x0c
#define DMAC_GCR_BT_16		0x00 /* 16clocks */
#define DMAC_GCR_BT_32		0x04 /* 32clocks */
#define DMAC_GCR_BT_64		0x08 /* 64clocks */
#define DMAC_GCR_BT_128		0x0c /* 128clocks */
#define DMAC_GCR_BR_MASK	0x03
#define DMAC_GCR_BR_50		0x00 /* 50% bandwidth */
#define DMAC_GCR_BR_25		0x01 /* 25% bandwidth */
#define DMAC_GCR_BR_12		0x02 /* 12.5% bandwidth */
#define DMAC_GCR_BR_6		0x03 /* 6.25% bandwidth */

/* MFC/DFC function codes */
#define DMAC_FC_USER_DATA	0x01
#define DMAC_FC_USER_PROGRAM	0x02
#define DMAC_FC_KERNEL_DATA	0x05
#define DMAC_FC_KERNEL_PROGRAM	0x06
#define DMAC_FC_CPU		0x07

/*
 * An element of the array used in DMAC scatter-gather transfer
 * (array chaining mode)
 */
struct dmac_sg_array {
	u_int32_t	da_addr;
	u_int16_t	da_count;
};
