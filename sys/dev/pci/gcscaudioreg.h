/*	$NetBSD: gcscaudioreg.h,v 1.1 2008/12/28 15:16:25 jmcneill Exp $	*/

/*-
 * Copyright (c) 2008 SHIMIZU Ryo <ryo@nerv.org>
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

#ifndef _I386_PCI_GCSCAUDIOREG_H_
#define _I386_PCI_GCSCAUDIOREG_H_

/*
 * Reference:
 *  - AMD Geode CS5536 Companion Device Data Book
 *    http://www.amd.com/files/connectivitysolutions/geode/geode_lx/33238G_cs5536_db.pdf
 */

#define ACC_GLD_MSR_CAP			0x51500000	/* GeodeLinkDevice Capabilities */

/*
 * AC97 Audio Codec Controller (ACC) Registers
 */
#define ACC_GPIO_STATUS			0x00		/* Codec GPIO Status Register */
# define ACC_GPIO_STATUS_GPIO_EN	0x80000000	/* GPIO Enable */
# define ACC_GPIO_STATUS_INT_EN		0x40000000	/* Codec GPIO Interrupt Enable */
# define ACC_GPIO_STATUS_WU_INT_EN	0x20000000	/* Codec GPIO Wakeup Interrupt Enable */
# define ACC_GPIO_STATUS_INT_FLAG	0x00200000	/* Codec GPIO Interrupt Flag (Read to Clear) */
# define ACC_GPIO_STATUS_WU_INT_FLAG	0x00100000	/* Codec GPIO Wakeup Interrupt Flag (Read to Clear) */
# define ACC_GPIO_STATUS_PIN_STS_MASK	0x000fffff	/* Codec GPIO Pin Status (Read Only) */

#define ACC_GPIO_CNTL			0x04		/* Codec GPIO Control Register */
# define ACC_GPIO_CNTL_PIN_DATA_MASK	0x000fffff	/* Codec GPIO Pin Data */

#define ACC_CODEC_REG2ADDR(reg)	(((reg) & 0x7f) << 24)
#define ACC_CODEC_ADDR2REG(adr)	(((adr) >> 24) & 0x7f)

#define ACC_CODEC_STATUS		0x08		/* Codec Status Register */
# define ACC_CODEC_STATUS_STS_ADD_MASK	0xff000000	/* Codec Status Address (Read Only) */
# define ACC_CODEC_STATUS_PRM_RDY_STS	0x00800000	/* Primary Codec Ready (Read Only) */
# define ACC_CODEC_STATUS_SEC_RDY_STS	0x00400000	/* Secondary Codec Ready (Read Only) */
# define ACC_CODEC_STATUS_SDATAIN2_EN	0x00200000	/* Enable Second Serial Data Input (AC_S_IN2) */
# define ACC_CODEC_STATUS_BM5_SEL	0x00100000	/* Audio Bus Master 5 AC97 Slot Select */
# define ACC_CODEC_STATUS_BM4_SEL	0x00080000	/* Audio Bus Master 4 AC97 Slot Select */
# define ACC_CODEC_STATUS_STS_NEW	0x00020000	/* Codec Status New (Read to Clear) */
# define ACC_CODEC_STATUS_STS_DATA_MASK	0x0000ffff	/* Codec Status Data (Read Only) */

#define ACC_CODEC_CNTL			0x0c		/* Codec Control Register */
# define ACC_CODEC_CNTL_RW_CMD		0x80000000	/* Codec Read/Write Command */
# define ACC_CODEC_CNTL_READ_CMD	0x80000000	/* Codec Read Command */
# define ACC_CODEC_CNTL_WRITE_CMD	0x00000000	/* Codec Write Command */
# define ACC_CODEC_CNTL_ADD_MASK	0x7f000000	/* CMD_ADD Codec Command Address */
# define ACC_CODEC_CNTL_COMM_SEL_MASK	0x00c00000	/* COMM_SEL Audio Codec Communication */
# define ACC_CODEC_CNTL_PD_PRIM		0x00200000	/* Power-down Semaphore for Primary Codec */
# define ACC_CODEC_CNTL_PD_SEC		0x00100000	/* Power-down Semaphore for Secondary Codec */
# define ACC_CODEC_CNTL_LNK_SHTDWN	0x00040000	/* AC Link Shutdown */
# define ACC_CODEC_CNTL_LNK_WRM_RST	0x00020000	/* AC Link Warm Reset */
# define ACC_CODEC_CNTL_CMD_NEW		0x00010000	/* Codec Command New */
# define ACC_CODEC_CNTL_CMD_DATA_MASK	0x0000ffff	/* Codec Command Data */

#define ACC_IRQ_STATUS			0x12		/* Second Level Audio IRQ Status Register */
# define ACC_IRQ_STATUS_BM7_IRQ_STS	0x0200		/* Audio Bus Master 7 IRQ Status */
# define ACC_IRQ_STATUS_BM6_IRQ_STS	0x0100		/* Audio Bus Master 6 IRQ Status */
# define ACC_IRQ_STATUS_BM5_IRQ_STS	0x0080		/* Audio Bus Master 5 IRQ Status */
# define ACC_IRQ_STATUS_BM4_IRQ_STS	0x0040		/* Audio Bus Master 4 IRQ Status */
# define ACC_IRQ_STATUS_BM3_IRQ_STS	0x0020		/* Audio Bus Master 3 IRQ Status */
# define ACC_IRQ_STATUS_BM2_IRQ_STS	0x0010		/* Audio Bus Master 2 IRQ Status */
# define ACC_IRQ_STATUS_BM1_IRQ_STS	0x0008		/* Audio Bus Master 1 IRQ Status */
# define ACC_IRQ_STATUS_BM0_IRQ_STS	0x0004		/* Audio Bus Master 0 IRQ Status */
# define ACC_IRQ_STATUS_WU_IRQ_STS	0x0002		/* Codec GPIO Wakeup IRQ Status */
# define ACC_IRQ_STATUS_IRQ_STS		0x0001		/* Codec GPIO IRQ Status */

#define ACC_ENGINE_CNTL			0x14		/* Bus Master Engine Control Register */
# define ACC_ENGINE_CNTL_SSND_MODE	0x00000001	/* Surround Sound (5.1) Synchronization Mode */

#define ACC_BM0_CMD			0x20		/* Bus Master 0 Command */
#define ACC_BM0_STATUS			0x21		/* Bus Master 0 IRQ Status */
#define ACC_BM0_PRD			0x24		/* Bus Master 0 PRD Table Address */
#define ACC_BM1_CMD			0x28		/* Bus Master 1 Command */
#define ACC_BM1_STATUS			0x29		/* Bus Master 1 IRQ Status */
#define ACC_BM1_PRD			0x2c		/* Bus Master 1 PRD Table Address */
#define ACC_BM2_CMD			0x30		/* Bus Master 2 Command */
#define ACC_BM2_STATUS			0x31		/* Bus Master 2 IRQ Status */
#define ACC_BM2_PRD			0x34		/* Bus Master 2 PRD Table Address */
#define ACC_BM3_CMD			0x38		/* Bus Master 3 Command */
#define ACC_BM3_STATUS			0x39		/* Bus Master 3 IRQ Status */
#define ACC_BM3_PRD			0x3c		/* Bus Master 3 PRD Table Address */
#define ACC_BM4_CMD			0x40		/* Bus Master 4 Command */
#define ACC_BM4_STATUS			0x41		/* Bus Master 4 IRQ Status */
#define ACC_BM4_PRD			0x44		/* Bus Master 4 PRD Table Address */
#define ACC_BM5_CMD			0x48		/* Bus Master 5 Command */
#define ACC_BM5_STATUS			0x49		/* Bus Master 5 IRQ Status */
#define ACC_BM5_PRD			0x4c		/* Bus Master 5 PRD Table Address */
#define ACC_BM6_CMD			0x50		/* Bus Master 6 Command */
#define ACC_BM6_STATUS			0x51		/* Bus Master 6 IRQ Status */
#define ACC_BM6_PRD			0x54		/* Bus Master 6 PRD Table Address */
#define ACC_BM7_CMD			0x58		/* Bus Master 7 Command */
#define ACC_BM7_STATUS			0x59		/* Bus Master 7 IRQ Status */
#define ACC_BM7_PRD			0x5c		/* Bus Master 7 PRD Table Address */
# define ACC_BMx_CMD_RW_MASK		0x08
# define ACC_BMx_CMD_READ		0x08		/* Codec to Memory */
# define ACC_BMx_CMD_WRITE		0x00		/* Memory to Codec */
# define ACC_BMx_CMD_BYTE_ORD_MASK	0x04
# define ACC_BMx_CMD_BYTE_ORD_EL	0x00		/* Little Endian */
# define ACC_BMx_CMD_BYTE_ORD_EB	0x04		/* Big Endian */
# define ACC_BMx_CMD_BM_CTL_MASK	0x03
# define ACC_BMx_CMD_BM_CTL_DISABLE	0x00		/* Disable bus master */
# define ACC_BMx_CMD_BM_CTL_ENABLE	0x01		/* Enable bus master */
# define ACC_BMx_CMD_BM_CTL_PAUSE	0x03		/* Pause bus master */
# define ACC_BMx_STATUS_BM_EOP_ERR	0x02		/* Bus Master Error */
# define ACC_BMx_STATUS_EOP		0x01		/* End of Page */

/* PRD - Physical Region Descriptor Table (addressed by ACC_BMx_PRD) */
struct acc_prd {
	uint32_t address;
	uint32_t ctrlsize;
#define ACC_BMx_PRD_CTRL_EOT		0x80000000
#define ACC_BMx_PRD_CTRL_EOP		0x40000000
#define ACC_BMx_PRD_CTRL_JMP		0x20000000
#define ACC_BMx_PRD_SIZE_MASK		0x0000ffff
};

#define ACC_BM0_PNTR			0x60		/* Bus Master 0 DMA Pointer */
#define ACC_BM1_PNTR			0x64		/* Bus Master 1 DMA Pointer */
#define ACC_BM2_PNTR			0x68		/* Bus Master 2 DMA Pointer */
#define ACC_BM3_PNTR			0x6C		/* Bus Master 3 DMA Pointer */
#define ACC_BM4_PNTR			0x70		/* Bus Master 4 DMA Pointer */
#define ACC_BM5_PNTR			0x74		/* Bus Master 5 DMA Pointer */
#define ACC_BM6_PNTR			0x78		/* Bus Master 6 DMA Pointer */
#define ACC_BM7_PNTR			0x7C		/* Bus Master 7 DMA Pointer */

#endif /* _I386_PCI_GCSCAUDIOREG_H_ */
