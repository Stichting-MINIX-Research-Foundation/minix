/*	$NetBSD: mvsatareg.h,v 1.3 2012/08/29 16:50:10 jakllsch Exp $	*/
/*
 * Copyright (c) 2008 KIYOHARA Takashi
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _MVSATAREG_H_
#define _MVSATAREG_H_

/*
 * SATAHC Arbiter Registers
 */
#define SATAHC_REGISTER_SIZE		0x10000
#define SATAHC(hc)			((hc) * SATAHC_REGISTER_SIZE)

#define SATAHC_C		0x000	/* Configuration */
#define SATAHC_C_TIMEOUT_MASK		(0xff << 0)
#define SATAHC_C_TIMEOUTEN		(1 << 16)	/* Timer Enable */
#define SATAHC_C_COALDIS(p)		(1 << ((p) + 24))/* Coalescing Disable*/
#define SATAHC_RQOP		0x004	/* Request Queue Out-Pointer */
#define SATAHC_RQIP		0x008	/* Response Queue In-Pointer */
#define SATAHC_RQP_ERPQP(p, x)	(((x) >> ((p) * 8)) & 0x7f)
#define SATAHC_ICT		0x00c	/* Interrupt Coalescing Threshold */
#define SATAHC_ICT_SAICOALT_MASK	0x000000ff
#define SATAHC_ITT		0x010	/* Interrupt Time Threshold */
#define SATAHC_ITT_SAITMTH		0x00ffffff
#define SATAHC_IC		0x014	/* Interrupt Cause */
#define SATAHC_IC_DONE(p)		(1 << (p))	/* SaCrpb/DMA Done */
#define SATAHC_IC_SAINTCOAL		(1 << 4)	/* Intr Coalescing */
#define SATAHC_IC_SADEVINTERRUPT(p)	(1 << ((p) + 8))/* Device Intr */

/*
 * Physical Registers for Generation I
 */
#define SATAHC_I_R02(p)		((p) * 0x100 + 0x108)
#define SATAHC_I_PHYCONTROL(p)	((p) * 0x100 + 0x10c)
#define SATAHC_I_LTMODE(p)	((p) * 0x100 + 0x130)
#define SATAHC_I_PHYMODE(p)	((p) * 0x100 + 0x174)


/*
 * EDMA Registers
 */
#define EDMA_REGISTERS_OFFSET		0x2000
#define EDMA_REGISTERS_SIZE		0x2000

#define EDMA_CFG		0x000	/* Configuration */
#define EDMA_CFG_RESERVED		(0x1f << 0)	/* Queue len ? */
#define EDMA_CFG_ESATANATVCMDQUE	(1 << 5)
#define EDMA_CFG_ERDBSZ			(1 << 8)
#define EDMA_CFG_EQUE			(1 << 9)
#define EDMA_CFG_ERDBSZEXT		(1 << 11)
#define EDMA_CFG_RESERVED2		(1 << 12)
#define EDMA_CFG_EWRBUFFERLEN		(1 << 13)
#define EDMA_CFG_EDEVERR		(1 << 14)
#define EDMA_CFG_EEDMAFBS		(1 << 16)
#define EDMA_CFG_ECUTTHROUGHEN		(1 << 17)
#define EDMA_CFG_EEARLYCOMPLETIONEN	(1 << 18)
#define EDMA_CFG_EEDMAQUELEN		(1 << 19)
#define EDMA_CFG_EHOSTQUEUECACHEEN	(1 << 22)
#define EDMA_CFG_EMASKRXPM		(1 << 23)
#define EDMA_CFG_RESUMEDIS		(1 << 24)
#define EDMA_CFG_EDMAFBS		(1 << 26)
#define EDMA_T			0x004	/* Timer */
#define EDMA_IEC		0x008	/* Interrupt Error Cause */
#define EDMA_IEM		0x00c	/* Interrupt Error Mask */
#define EDMA_IE_EDEVERR			(1 << 2)	/* EDMA Device Error */
#define EDMA_IE_EDEVDIS			(1 << 3)	/* EDMA Dev Disconn */
#define EDMA_IE_EDEVCON			(1 << 4)	/* EDMA Dev Conn */
#define EDMA_IE_SERRINT			(1 << 5)
#define EDMA_IE_ESELFDIS		(1 << 7)	/* EDMA Self Disable */
#define EDMA_IE_ETRANSINT		(1 << 8)	/* Transport Layer */
#define EDMA_IE_EIORDYERR		(1 << 12)	/* EDMA IORdy Error */
#   define EDMA_IE_LINKXERR_SATACRC	    (1 << 0)	/* SATA CRC error */
#   define EDMA_IE_LINKXERR_INTERNALFIFO    (1 << 1)	/* internal FIFO err */
#   define EDMA_IE_LINKXERR_LINKLAYERRESET  (1 << 2)
	/* Link Layer is reset by the reception of SYNC primitive from device */
#   define EDMA_IE_LINKXERR_OTHERERRORS	    (1 << 3)
	/*
	 * Link state errors, coding errors, or running disparity errors occur
	 * during FIS reception.
	 */
#   define EDMA_IE_LINKTXERR_FISTXABORTED   (1 << 4)	/* FIS Tx is aborted */
#define EDMA_IE_LINKCTLRXERR(x)		((x) << 13)	/* Link Ctrl Recv Err */
#define EDMA_IE_LINKDATARXERR(x)	((x) << 17)	/* Link Data Recv Err */
#define EDMA_IE_LINKCTLTXERR(x)		((x) << 21)	/* Link Ctrl Tx Error */
#define EDMA_IE_LINKDATATXERR(x)	((x) << 26)	/* Link Data Tx Error */
#define EDMA_IE_TRANSPROTERR		(1 << 31)	/* Transport Proto E */
#define EDMA_REQQBAH		0x010	/* Request Queue Base Address High */
#define EDMA_REQQIP		0x014	/* Request Queue In-Pointer */
#define EDMA_REQQOP		0x018	/* Request Queue Out-Pointer */
#define EDMA_REQQP_ERQQP_SHIFT		5
#define EDMA_REQQP_ERQQP_MASK		0x000003e0
#define EDMA_REQQP_ERQQBAP_MASK		0x00000c00
#define EDMA_REQQP_ERQQBA_MASK		0xfffff000
#define EDMA_RESQBAH		0x01c	/* Response Queue Base Address High */
#define EDMA_RESQIP		0x020	/* Response Queue In-Pointer */
#define EDMA_RESQOP		0x024	/* Response Queue Out-Pointer */
#define EDMA_RESQP_ERPQP_SHIFT		3
#define EDMA_RESQP_ERPQP_MASK		0x000000f8
#define EDMA_RESQP_ERPQBAP_MASK		0x00000300
#define EDMA_RESQP_ERPQBA_MASK		0xfffffc00
#define EDMA_CMD		0x028	/* Command */
#define EDMA_CMD_EENEDMA		(1 << 0)	/* Enable EDMA */
#define EDMA_CMD_EDSEDMA		(1 << 1)	/* Disable EDMA */
#define EDMA_CMD_EATARST		(1 << 2)	/* ATA Device Reset */
#define EDMA_CMD_EEDMAFRZ		(1 << 4)	/* EDMA Freeze */
#define EDMA_TC			0x02c	/* Test Control */
#define EDMA_S			0x030	/* Status */
#define EDMA_S_EDEVQUETAG(s)		((s) & 0x0000001f)
#define EDMA_S_EDEVDIR_WRITE		(0 << 5)
#define EDMA_S_EDEVDIR_READ		(1 << 5)
#define EDMA_S_ECACHEEMPTY		(1 << 6)
#define EDMA_S_EDMAIDLE			(1 << 7)
#define EDMA_S_ESTATE(s)		(((s) & 0x0000ff00) >> 8)
#define EDMA_S_EIOID(s)			(((s) & 0x003f0000) >> 16)
#define EDMA_IORT		0x034	/* IORdy Timeout */
#define EDMA_CDT		0x040	/* Command Delay Threshold */
#define EDMA_HC			0x060	/* Halt Condition */
#define EDMA_CQDCQOS(x)		(0x090 + ((x) << 2)
					/* NCQ Done/TCQ Outstanding Status */

/*
 * Shadow Register Block Registers
 */
#define SHADOW_REG_BLOCK_OFFSET	0x100
#define SHADOW_REG_BLOCK_SIZE	0x100

#define SRB_PIOD		0x000	/* PIO Data */
#define SRB_FE			0x004	/* Feature/Error */
#define SRB_SC			0x008	/* Sector Count */
#define SRB_LBAL		0x00c	/* LBA Low */
#define SRB_LBAM		0x010	/* LBA Mid */
#define SRB_LBAH		0x014	/* LBA High */
#define SRB_H			0x018	/* Head */
#define SRB_CS			0x01c	/* Command/Status */
#define SRB_CAS			0x020	/* Control/Alternate Status */

/*
 * Basic DMA Registers
 *   Does support for this registers only 88Sx6xxx?
 */
#define DMA_C			0x224	/* Basic DMA Command */
#define DMA_C_START			(1 << 0)
#define DMA_C_READ			(1 << 3)
#define DMA_C_DREGIONVALID		(1 << 8)
#define DMA_C_DREGIONLAST		(1 << 9)
#define DMA_C_CONTFROMPREV		(1 << 10)
#define DMA_C_DRBC(n)			(((n) & 0xffff) << 16)
#define DMA_S			0x228	/* Basic DMA Status */
#define DMA_DTLBA		0x22c	/* Descriptor Table Low Base Address */
#define DMA_DTLBA_MASK			0xfffffff0
#define DMA_DTHBA		0x230	/* Descriptor Table High Base Address */
#define DMA_DRLA		0x234	/* Data Region Low Address */
#define DMA_DRHA		0x238	/* Data Region High Address */

/*
 * Serial-ATA Registers
 */
#define SATA_SS			0x300	/* SStatus */
#define SATA_SE			0x304	/* SError */
#define SATA_SEIM		0x340	/* SError Interrupt Mask */
#define SATA_SC			0x308	/* SControl */
#define SATA_LTM		0x30c	/* LTMode */
#define SATA_PHYM3		0x310	/* PHY Mode 3 */
#define SATA_PHYM4		0x314	/* PHY Mode 4 */
#define SATA_PHYM1		0x32c	/* PHY Mode 1 */
#define SATA_PHYM2		0x330	/* PHY Mode 2 */
#define SATA_BISTC		0x334	/* BIST Control */
#define SATA_BISTDW1		0x338	/* BIST DW1 */
#define SATA_BISTDW2		0x33c	/* BIST DW2 */
#define SATA_SATAICFG		0x050	/* Serial-ATA Interface Configuration */
#define SATA_SATAICFG_REFCLKCNF_20MHZ	(0 << 0)
#define SATA_SATAICFG_REFCLKCNF_25MHZ	(1 << 0)
#define SATA_SATAICFG_REFCLKCNF_30MHZ	(2 << 0)
#define SATA_SATAICFG_REFCLKCNF_40MHZ	(3 << 0)
#define SATA_SATAICFG_REFCLKCNF_MASK	(3 << 0)
#define SATA_SATAICFG_REFCLKDIV_1	(0 << 2)
#define SATA_SATAICFG_REFCLKDIV_2	(1 << 2)	/* Used 20 or 25MHz */
#define SATA_SATAICFG_REFCLKDIV_4	(2 << 2)	/* Used 40MHz */
#define SATA_SATAICFG_REFCLKDIV_3	(3 << 2)	/* Used 30MHz */
#define SATA_SATAICFG_REFCLKDIV_MASK	(3 << 2)
#define SATA_SATAICFG_REFCLKFEEDDIV_50	(0 << 4)	/* or 100, when Gen2En is 1 */
#define SATA_SATAICFG_REFCLKFEEDDIV_60	(1 << 4)	/* or 120. Used 25MHz */
#define SATA_SATAICFG_REFCLKFEEDDIV_75	(2 << 4)	/* or 150. Used 20MHz */
#define SATA_SATAICFG_REFCLKFEEDDIV_90	(3 << 4)	/* or 180 */
#define SATA_SATAICFG_REFCLKFEEDDIV_MASK (3 << 4)
#define SATA_SATAICFG_PHYSSCEN		(1 << 6)
#define SATA_SATAICFG_GEN2EN		(1 << 7)
#define SATA_SATAICFG_COMMEN		(1 << 8)
#define SATA_SATAICFG_PHYSHUTDOWN	(1 << 9)
#define SATA_SATAICFG_TARGETMODE	(1 << 10)	/* 1 = Initiator */
#define SATA_SATAICFG_COMCHANNEL	(1 << 11)
#define SATA_SATAICFG_IGNOREBSY		(1 << 24)
#define SATA_SATAICFG_LINKRSTEN		(1 << 25)
#define SATA_SATAICFG_CMDRETXDS		(1 << 26)
#define SATA_SATAICTL		0x344	/* Serial-ATA Interface Control */
#define SATA_SATAITC		0x348	/* Serial-ATA Interface Test Control */
#define SATA_SATAIS		0x34c	/* Serial-ATA Interface Status */
#define SATA_VU			0x35c	/* Vendor Unique */
#define SATA_FISC		0x360	/* FIS Configuration */
#define SATA_FISC_FISWAIT4RDYEN_B0	(1 << 0) /* Device to Host FIS */
#define SATA_FISC_FISWAIT4RDYEN_B1	(1 << 1) /* SDB FIS rcv with <N>bit 0 */
#define SATA_FISC_FISWAIT4RDYEN_B2	(1 << 2) /* DMA Activate FIS */
#define SATA_FISC_FISWAIT4RDYEN_B3	(1 << 3) /* DMA Setup FIS */
#define SATA_FISC_FISWAIT4RDYEN_B4	(1 << 4) /* Data FIS first DW */
#define SATA_FISC_FISWAIT4RDYEN_B5	(1 << 5) /* Data FIS entire FIS */
#define SATA_FISC_FISWAIT4HOSTRDYEN_B0	(1 << 8)
				/* Device to Host FIS with <ERR> or <DF> */
#define SATA_FISC_FISWAIT4HOSTRDYEN_B1	(1 << 9) /* SDB FIS rcv with <N>bit */
#define SATA_FISC_FISWAIT4HOSTRDYEN_B2	(1 << 10) /* SDB FIS rcv with <ERR> */
#define SATA_FISC_FISWAIT4HOSTRDYEN_B3	(1 << 11) /* BIST Acivate FIS */
#define SATA_FISC_FISWAIT4HOSTRDYEN_B4	(1 << 12) /* PIO Setup FIS */
#define SATA_FISC_FISWAIT4HOSTRDYEN_B5	(1 << 13) /* Data FIS with Link error */
#define SATA_FISC_FISWAIT4HOSTRDYEN_B6	(1 << 14) /* Unrecognized FIS type */
#define SATA_FISC_FISWAIT4HOSTRDYEN_B7	(1 << 15) /* Any FIS */
#define SATA_FISC_FISDMAACTIVATESYNCRESP (1 << 16)
#define SATA_FISC_FISUNRECTYPECONT	(1 << 17)
#define SATA_FISIC		0x364	/* FIS Interrupt Cause */
#define SATA_FISIM		0x368	/* FIS Interrupt Mask */
#define SATA_FISDW0		0x370	/* FIS DW0 */
#define SATA_FISDW1		0x374	/* FIS DW1 */
#define SATA_FISDW2		0x378	/* FIS DW2 */
#define SATA_FISDW3		0x37c	/* FIS DW3 */
#define SATA_FISDW4		0x380	/* FIS DW4 */
#define SATA_FISDW5		0x384	/* FIS DW5 */
#define SATA_FISDW6		0x388	/* FIS DW6 */


/* EDMA Command Request Block (CRQB) Data */
struct crqb {
	uint32_t cprdbl;	/* cPRD Desriptor Table Base Low Address */
	uint32_t cprdbh;	/* cPRD Desriptor Table Base High Address */
	uint16_t ctrlflg;	/* Control Flags */
	uint16_t atacommand[11];
} __packed __aligned(8);

struct crqb_gen2e {
	uint32_t cprdbl;	/* cPRD Desriptor Table Base Low Address */
	uint32_t cprdbh;	/* cPRD Desriptor Table Base High Address */
	uint32_t ctrlflg;	/* Control Flags */
	uint32_t drbc;		/* Data Region Byte Count */
	uint32_t atacommand[4];
} __packed __aligned(8);


#define CRQB_CRQBL_EPRD_MASK	0xfffffff0
#define CRQB_CRQBL_SDR_MASK	0xfffffffe	/* Single data region mask */

/* Control Flags */
#define CRQB_CDIR_WRITE		(0 << 0)
#define CRQB_CDIR_READ		(1 << 0)
#define CRQB_CDEVICEQUETAG(x)	(((x) & 0x1f) << 1)	/* CRQB Dev Queue Tag */
#define CRQB_CHOSTQUETAG(x)	(((x) & 0x7f) << 1)	/* CRQB Host Q Tag */
#define CRQB_CPMPORT(x)		(((x) & 0xf) << 12)	/* PM Port Transmit */
#define CRQB_CPRDMODE_EPRD	(0 << 16)		/* PRD table */
#define CRQB_CPRDMODE_SDR	(1 << 16)		/* Single data region */
#define CRQB_CHOSTQUETAG_GEN2(x) (((x) & 0x7f) << 17)	/* CRQB Host Q Tag G2 */

/* Data Region Byte Count */
#define CRQB_CDRBC(x)		(((x) & 0xfffe) << 0)

#define CRQB_ATACOMMAND(reg, val) \
				((reg << 8) | (val & 0xff))
#define CRQB_ATACOMMAND_LAST	(1 << 15)
#define CRQB_ATACOMMAND_REG(reg)	(((reg) >> 2) + 0x10)
#define CRQB_ATACOMMAND_FEATURES	CRQB_ATACOMMAND_REG(SRB_FE)
#define CRQB_ATACOMMAND_SECTORCOUNT	CRQB_ATACOMMAND_REG(SRB_SC)
#define CRQB_ATACOMMAND_LBALOW		CRQB_ATACOMMAND_REG(SRB_LBAL)
#define CRQB_ATACOMMAND_LBAMID		CRQB_ATACOMMAND_REG(SRB_LBAM)
#define CRQB_ATACOMMAND_LBAHIGH		CRQB_ATACOMMAND_REG(SRB_LBAH)
#define CRQB_ATACOMMAND_DEVICE		CRQB_ATACOMMAND_REG(SRB_H)
#define CRQB_ATACOMMAND_COMMAND		CRQB_ATACOMMAND_REG(SRB_CS)


/* EDMA Phisical Region Descriptors (ePRD) Table Data Structure */
struct eprd {
	uint32_t prdbal;	/* address bits[31:1] */
	uint16_t bytecount;	/* Byte Count */
	uint16_t eot;		/* End Of Table */
	uint32_t prdbah;	/* address bits[63:32] */
	uint32_t resv;
} __packed __aligned(8);

#define EPRD_PRDBAL_MASK	0xfffffffe	/* phy memory region mask */

#define EPRD_BYTECOUNT(x)	(((x) & 0xffff) << 0)
#define EPRD_EOT		(1 << 15)	/* End Of Table */


/* EDMA Command Response Block (CRPB) Data */
struct crpb {
	uint16_t id;		/* CRPB ID */
	uint16_t rspflg;	/* CPRB Response Flags */
	uint32_t ts;		/* CPRB Time Stamp */
} __packed __aligned(8);

/* ID */
#define CRPB_CHOSTQUETAG(x)	(((x) >> 0) & 0x7f)	/* CRPB Host Q Tag */

/* Response Flags */
#define CRPB_CEDMASTS(x)	(((x) >> 0) & 0x7f)	/* CRPB EDMA Status */
#define CRPB_CDEVSTS(x)		(((x) >> 8) & 0xff)	/* CRPB Device Status */

#endif	/* _MVSATAREG_H_ */
