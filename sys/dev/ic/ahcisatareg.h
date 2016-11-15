/*	$NetBSD: ahcisatareg.h,v 1.12 2012/10/17 23:40:42 matt Exp $	*/

/*
 * Copyright (c) 2006 Manuel Bouyer.
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/* SATA AHCI v1.0 register defines */

/* misc defines */
#define AHCI_MAX_PORTS 32
#define AHCI_MAX_CMDS 32

/* in-memory structures used by the controller */
/* physical region descriptor: points to a region of data (max 4MB) */
struct ahci_dma_prd {
	uint64_t prd_dba; /* data base address */
	uint32_t prd_res; /* reserved */
	uint32_t prd_dbc; /* data byte count */
#define AHCI_PRD_DBC_MASK 0x003fffff
#define AHCI_PRD_DBC_IPC  0x80000000 /* interrupt on completion */
} __packed __aligned(8);

#define AHCI_NPRD ((MAXPHYS/PAGE_SIZE) + 1)

/* command table: describe a command to send to drive */
struct ahci_cmd_tbl {
	uint8_t cmdt_cfis[64]; /* command FIS */
	uint8_t cmdt_acmd[16]; /* ATAPI command */
	uint8_t cmdt_res[48]; /* reserved */
	struct ahci_dma_prd cmdt_prd[1]; /* extended to AHCI_NPRD */
} __packed __aligned(8);

#define AHCI_CMDTBL_ALIGN 0x7f

#define AHCI_CMDTBL_SIZE ((sizeof(struct ahci_cmd_tbl) + \
    (sizeof(struct ahci_dma_prd) * (AHCI_NPRD - 1)) + (AHCI_CMDTBL_ALIGN)) \
    & ~AHCI_CMDTBL_ALIGN)

/*
 * command header: points to a command table. The command list is an array
 * of theses.
 */
struct ahci_cmd_header {
	uint16_t cmdh_flags;
#define AHCI_CMDH_F_PMP_MASK	0xf000 /* port multiplier port */
#define AHCI_CMDH_F_PMP_SHIFT	12
#define AHCI_CMDH_F_CBSY	0x0400 /* clear BSY on R_OK */
#define AHCI_CMDH_F_BIST	0x0200 /* BIST FIS */
#define AHCI_CMDH_F_RST		0x0100 /* Reset FIS */
#define AHCI_CMDH_F_PRF		0x0080 /* prefectchable */
#define AHCI_CMDH_F_WR		0x0040 /* write */
#define AHCI_CMDH_F_A		0x0020 /* ATAPI */
#define AHCI_CMDH_F_CFL_MASK	0x001f /* command FIS length (in dw) */
#define AHCI_CMDH_F_CFL_SHIFT	0
	uint16_t cmdh_prdtl;	/* number of cmdt_prd */
	uint32_t cmdh_prdbc;	/* physical region descriptor byte count */
	uint64_t cmdh_cmdtba;	/* phys. addr. of cmd_tbl, 128bytes aligned */
	uint32_t cmdh_res[4];	/* reserved */
} __packed __aligned(8);

#define AHCI_CMDH_SIZE (sizeof(struct ahci_cmd_header) * AHCI_MAX_CMDS)

/* received FIS: where the HBA stores various type of FIS it receives */
struct ahci_r_fis {
	uint8_t rfis_dsfis[32];	/* DMA setup FIS */
	uint8_t rfis_psfis[32]; /* PIO setup FIS */
	uint8_t rfis_rfis[24];  /* D2H register FIS */
	uint8_t rfis_sdbfis[8]; /* set device bit FIS */
	uint8_t rfis_ukfis[64]; /* unknown FIS */
	uint8_t rfis_res[96];   /* reserved */
} __packed __aligned(8);

#define AHCI_RFIS_SIZE (sizeof(struct ahci_r_fis))

/* PCI registers */
/* class Mass storage, subclass SATA, interface AHCI */
#define PCI_INTERFACE_SATA_AHCI	0x01

#define AHCI_PCI_ABAR	0x24 /* native ACHI registers (memory mapped) */

/*  ABAR registers */
/* Global registers */
#define AHCI_CAP	0x00 /* HBA capabilities */
#define		AHCI_CAP_NPMASK	0x0000001f /* Number of ports */
#define		AHCI_CAP_XS	0x00000020 /* External SATA */
#define		AHCI_CAP_EM	0x00000040 /* Enclosure Management */
#define		AHCI_CAP_CCC	0x00000080 /* command completion coalescing */
#define		AHCI_CAP_NCS	0x00001f00 /* number of command slots */
#define		AHCI_CAP_PS	0x00002000 /* Partial State */
#define		AHCI_CAP_SS	0x00004000 /* Slumber State */
#define		AHCI_CAP_PMD	0x00008000 /* PIO multiple DRQ blocks */
#define		AHCI_CAP_FBS	0x00010000 /* FIS-Based switching */
#define		AHCI_CAP_SPM	0x00020000 /* Port multipliers */
#define		AHCI_CAP_SAM	0x00040000 /* AHCI-only */
#define		AHCI_CAP_NZO	0x00080000 /* Non-zero DMA offset (reserved) */
#define		AHCI_CAP_IS	0x00f00000 /* Interface speed */
#define		AHCI_CAP_IS_GEN1	0x00100000 /* 1.5 Gb/s */
#define		AHCI_CAP_IS_GEN2	0x00200000 /* 3.0 Gb/s */
#define		AHCI_CAP_IS_GEN3	0x00300000 /* 6.0 Gb/s */
#define		AHCI_CAP_CLO	0x01000000 /* Command list override */
#define		AHCI_CAP_AL	0x02000000 /* Single Activitly LED */
#define		AHCI_CAP_ALP	0x04000000 /* Agressive link power management */
#define		AHCI_CAP_SSU	0x08000000 /* Staggered spin-up */
#define		AHCI_CAP_MPS	0x10000000 /* Mechanical swicth */
#define		AHCI_CAP_NTF	0x20000000 /* Snotification */
#define		AHCI_CAP_NCQ	0x40000000 /* Native command queuing */
#define		AHCI_CAP_64BIT	0x80000000 /* 64bit addresses */

#define AHCI_GHC	0x04 /* HBA control */
#define 	AHCI_GHC_HR	 0x00000001 /* HBA reset */
#define 	AHCI_GHC_IE	 0x00000002 /* Interrupt enable */
#define 	AHCI_GHC_MRSM	 0x00000004 /* MSI revert to single message */
#define 	AHCI_GHC_AE	 0x80000000 /* AHCI enable */

#define AHCI_IS		0x08 /* Interrupt status register: one bit per port */

#define AHCI_PI		0x0c /* Port implemented: one bit per port */

#define AHCI_VS		0x10 /* AHCI version */
#define		AHCI_VS_095	0x00000905 /* AHCI spec 0.95 */
#define		AHCI_VS_100	0x00010000 /* AHCI spec 1.0 */
#define		AHCI_VS_110	0x00010100 /* AHCI spec 1.1 */
#define		AHCI_VS_120	0x00010200 /* AHCI spec 1.2 */
#define		AHCI_VS_130	0x00010300 /* AHCI spec 1.3 */
#define AHCI_VS_MJR(v) ((unsigned int)__SHIFTOUT(v, __BITS(31, 16)))
#define AHCI_VS_MNR(v) ((unsigned int)__SHIFTOUT(v, __BITS(15, 8)) * 10 + (unsigned int)__SHIFTOUT(v, __BITS(7, 0) * 1))

#define AHCI_CC_CTL	0x14 /* command completion coalescing control */
#define 	AHCI_CC_TV_MASK	0xffff0000 /* timeout value */
#define 	AHCI_CC_TV_SHIFT 16
#define 	AHCI_CC_CC_MASK	0x0000ff00 /* command completion */
#define 	AHCI_CC_CC_SHIFT 8
#define 	AHCI_CC_INT_MASK 0x000000f8 /* interrupt */
#define 	AHCI_CC_INT_SHIFT 3
#define 	AHCI_CC_EN	0x000000001 /* enable */

#define AHCI_CC_PORTS	0x18 /* command completion coalescing ports (1b/port */

#define AHCI_EM_LOC	0x1c /* enclosure managemement location */
#define		AHCI_EML_OFF_MASK 0xffff0000 /* offset in ABAR */
#define		AHCI_EML_OFF_SHIFT 16
#define		AHCI_EML_SZ_MASK  0x0000ffff /* offset in ABAR */
#define		AHCI_EML_SZ_SHIFT  0

#define AHCI_EM_CTL	0x20 /* enclosure management control */
#define		AHCI_EMC_PM	0x08000000 /* port multiplier support */
#define		AHCI_EMC_ALHD	0x04000000 /* activity LED hardware driven */
#define		AHCI_EMC_XMIT	0x02000000 /* tramsit messages only */
#define		AHCI_EMC_SMB	0x01000000 /* single message buffer */
#define		AHCI_EMC_SGPIO	0x00080000 /* enclosure management messages */
#define		AHCI_EMC_SES2	0x00040000 /* SeS-2 messages */
#define		AHCI_EMC_SAF	0x00020000 /* SAF_TE messages */
#define		AHCI_EMC_LED	0x00010000 /* LED messages */
#define		AHCI_EMC_RST	0x00000200 /* Reset */
#define		AHCI_EMC_TM	0x00000100 /* Transmit message */
#define		AHCI_EMC_MR	0x00000001 /* Message received */

#define AHCI_CAP2	0x24 /* HBA Capabilities Extended */
#define		AHCI_CAP2_APST	0x00000004
#define		AHCI_CAP2_NVMP	0x00000002
#define		AHCI_CAP2_BOH	0x00000001

#define AHCI_BOHC	0x28 /* BIOS/OS Handoff Control and Status */
#define		AHCI_BOHC_BB	0x00000010
#define		AHCI_BOHC_OOC	0x00000008
#define		AHCI_BOHC_SOOE	0x00000004
#define		AHCI_BOHC_OOS	0x00000002
#define		AHCI_BOHC_BOS	0x00000001

/* Per-port registers */
#define AHCI_P_OFFSET(port) (0x80 * (port))

#define AHCI_P_CLB(p)	(0x100 + AHCI_P_OFFSET(p)) /* command list addr */
#define AHCI_P_CLBU(p)	(0x104 + AHCI_P_OFFSET(p)) /* command list addr */
#define AHCI_P_FB(p)	(0x108 + AHCI_P_OFFSET(p)) /* FIS addr */
#define AHCI_P_FBU(p)	(0x10c + AHCI_P_OFFSET(p)) /* FIS addr */
#define AHCI_P_IS(p)	(0x110 + AHCI_P_OFFSET(p)) /* Interrupt status */
#define AHCI_P_IE(p)	(0x114 + AHCI_P_OFFSET(p)) /* Interrupt enable */
#define		AHCI_P_IX_CPDS	0x80000000 /* Cold port detect */
#define		AHCI_P_IX_TFES	0x40000000 /* Task file error */
#define		AHCI_P_IX_HBFS	0x20000000 /* Host bus fatal error */
#define		AHCI_P_IX_HBDS	0x10000000 /* Host bus data error */
#define		AHCI_P_IX_IFS	0x08000000 /* Interface fatal error */
#define		AHCI_P_IX_INFS	0x04000000 /* Interface non-fatal error */
#define		AHCI_P_IX_OFS	0x01000000 /* Overflow */
#define		AHCI_P_IX_IPMS	0x00800000 /* Incorrect port multiplier */
#define		AHCI_P_IX_PRCS	0x00400000 /* Phy Ready change */
#define		AHCI_P_IX_DMPS	0x00000080 /* Device Mechanical Presence */
#define		AHCI_P_IX_PCS	0x00000040 /* port Connect change */
#define		AHCI_P_IX_DPS	0x00000020 /* dexcriptor processed */
#define		AHCI_P_IX_UFS	0x00000010 /* Unknown FIS */
#define		AHCI_P_IX_SDBS	0x00000008 /* Set device bit */
#define		AHCI_P_IX_DSS	0x00000004 /* DMA setup FIS */
#define		AHCI_P_IX_PSS	0x00000002 /* PIO setup FIS */
#define		AHCI_P_IX_DHRS	0x00000001 /* Device to Host FIS */

#define AHCI_P_CMD(p)	(0x118 + AHCI_P_OFFSET(p)) /* Port command/status */
#define		AHCI_P_CMD_ICC_MASK 0xf0000000 /* Interface Comm. Control */
#define		AHCI_P_CMD_ICC_SL   0x60000000 /* State slumber */
#define		AHCI_P_CMD_ICC_PA   0x20000000 /* State partial */
#define		AHCI_P_CMD_ICC_AC   0x10000000 /* State active */
#define		AHCI_P_CMD_ICC_NO   0x00000000 /* State idle/NOP */
#define		AHCI_P_CMD_ASP	0x08000000 /* Agressive Slumber/Partial */
#define		AHCI_P_CMD_ALPE	0x04000000 /* Agressive link power management */
#define		AHCI_P_CMD_DLAE	0x02000000 /* drive LED on ATAPI */
#define		AHCI_P_CMD_ATAP	0x01000000 /* Device is ATAPI */
#define		AHCI_P_CMD_ESP	0x00200000 /* external SATA port */
#define		AHCI_P_CMD_CPD	0x00100000 /* Cold presence detection */
#define		AHCI_P_CMD_MPSP	0x00080000 /* Mechanical switch attached */
#define		AHCI_P_CMD_HPCP	0x00040000 /* hot-plug capable */
#define		AHCI_P_CMD_PMA	0x00020000 /* port multiplier attached */
#define		AHCI_P_CMD_CPS	0x00010000 /* cold presence state */
#define		AHCI_P_CMD_CR	0x00008000 /* command list running */
#define		AHCI_P_CMD_FR	0x00004000 /* FIS receive running */
#define		AHCI_P_CMD_MPSS	0x00002000 /* mechanical switch state */
#define		AHCI_P_CMD_CCS_MASK 0x00001f00 /* current command slot */
#define		AHCI_P_CMD_CCS_SHIFT 12
#define		AHCI_P_CMD_FRE	0x00000010 /* FIS receive enable */
#define		AHCI_P_CMD_CLO	0x00000008 /* command list override */
#define		AHCI_P_CMD_POD	0x00000004 /* power on device */
#define		AHCI_P_CMD_SUD	0x00000002 /* spin up device */
#define		AHCI_P_CMD_ST	0x00000001 /* start */

#define AHCI_P_TFD(p)	(0x120 + AHCI_P_OFFSET(p)) /* Port task file data */
#define		AHCI_P_TFD_ERR_MASK	0x0000ff00 /* error register */
#define		AHCI_P_TFD_ERR_SHIFT	8
#define		AHCI_P_TFD_ST		0x000000ff /* status register */
#define		AHCI_P_TFD_ST_SHIFT	0

#define AHCI_P_SIG(p)	(0x124 + AHCI_P_OFFSET(p)) /* device signature */
#define		AHCI_P_SIG_LBAH_MASK	0xff000000
#define		AHCI_P_SIG_LBAH_SHIFT	24
#define		AHCI_P_SIG_LBAM_MASK	0x00ff0000
#define		AHCI_P_SIG_LBAM_SHIFT	16
#define		AHCI_P_SIG_LBAL_MASK	0x0000ff00
#define		AHCI_P_SIG_LBAL_SHIFT	8
#define		AHCI_P_SIG_SC_MASK	0x000000ff
#define		AHCI_P_SIG_SC_SHIFT	0

#define AHCI_P_SSTS(p)	(0x128 + AHCI_P_OFFSET(p)) /* Serial ATA status */

#define AHCI_P_SCTL(p)	(0x12c + AHCI_P_OFFSET(p)) /* Serial ATA control */

#define AHCI_P_SERR(p)	(0x130 + AHCI_P_OFFSET(p)) /* Serial ATA error */

#define AHCI_P_SACT(p)	(0x134 + AHCI_P_OFFSET(p)) /* Serial ATA active */
	/* one bit per tag/command slot */

#define AHCI_P_CI(p)	(0x138 + AHCI_P_OFFSET(p)) /* Command issued */
	/* one bit per tag/command slot */

#define AHCI_P_FNTF(p)	(0x13c + AHCI_P_OFFSET(p)) /* SNotification */
	/* one bit per port */
