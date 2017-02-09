/*	$NetBSD: i82801lpcreg.h,v 1.12 2014/12/26 05:09:03 msaitoh Exp $	*/

/*-
 * Copyright (c) 2004 The NetBSD Foundation, Inc.
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
 * Intel 82801 Series I/O Controller Hub (ICH) -- LPC Interface Bridge part
 *   register definitions.
 */

#ifndef _DEV_IC_I82801LPCREG_H_
#define _DEV_IC_I82801LPCREG_H_
/*
 * PCI configuration registers
 */
#define LPCIB_PCI_PMBASE	0x40
#define LPCIB_PCI_PM_SIZE	0x00000080
#define LPCIB_PCI_ACPI_CNTL	0x44
# define LPCIB_PCI_ACPI_CNTL_EN	(1 << 4)
/* GPIO config registers ICH6+ */
#define LPCIB_PCI_GPIO_BASE_ICH6	0x48
#define LPCIB_PCI_GPIO_CNTL_ICH6	0x4c
#define LPCIB_PCI_BIOS_CNTL	0x4c /* actually 0x4e */
#define LPCIB_PCI_BIOS_CNTL_BWE	(0x0001 << 16) /* write enable */
#define LPCIB_PCI_BIOS_CNTL_BLE	(0x0002 << 16) /* lock enable */
#define LPCIB_PCI_TCO_CNTL	0x54
/* GPIO config registers ICH0-ICH5 */
#define LPCIB_PCI_GPIO_BASE	0x58
#define LPCIB_PCI_GPIO_SIZE	0x00000080
#define LPCIB_PCI_GPIO_CNTL	0x5c
#define LPCIB_PCI_GPIO_CNTL_EN	(1 << 4)
#define LPCIB_PCI_PIRQA_ROUT	0x60
#define LPCIB_PCI_PIRQB_ROUT	0x61
#define LPCIB_PCI_PIRQC_ROUT	0x62
#define LPCIB_PCI_PIRQD_ROUT	0x63
#define LPCIB_PCI_SIRQ_CNTL	0x64
#define LPCIB_PCI_PIRQE_ROUT	0x68
#define LPCIB_PCI_PIRQF_ROUT	0x69
#define LPCIB_PCI_PIRQG_ROUT	0x6a
#define LPCIB_PCI_PIRQH_ROUT	0x6b
#define LPCIB_PCI_D31_ERR_CFG	0x88
#define LPCIB_PCI_D31_ERR_STS	0x8a
#define LPCIB_PCI_PCI_DMA_C	0x90
#define LPCIB_PCI_GEN_PMCON_1	0xa0
# define LPCIB_PCI_GEN_PMCON_1_SS_EN	0x08
#define LPCIB_PCI_GEN_PMCON_2	0xa2
#define LPCIB_PCI_GEN_PMCON_3	0xa4
#define LPCIB_PCI_STPCLK_DEL	0xa8
#define LPCIB_PCI_GPI_ROUT	0xb8
#define LPCIB_PCI_TRP_FWD_EN	0xc0
#define LPCIB_PCI_MON4_TRP_RNG	0xc4
#define LPCIB_PCI_MON5_TRP_RNG	0xc5
#define LPCIB_PCI_MON6_TRP_RNG	0xc6
#define LPCIB_PCI_MON7_TRP_RNG	0xc7
#define LPCIB_PCI_MON_TRP_MSK	0xcc
#define LPCIB_PCI_GEN_CNTL	0xd0
#define	LPCIB_ICH5_HPTC_EN		0x00020000
#define	LPCIB_ICH5_HPTC_WIN_MASK	0x0000c000
#define	LPCIB_ICH5_HPTC_0000		0x00000000
#define	LPCIB_ICH5_HPTC_0000_BASE	0xfed00000
#define	LPCIB_ICH5_HPTC_1000		0x00008000
#define	LPCIB_ICH5_HPTC_1000_BASE	0xfed01000
#define	LPCIB_ICH5_HPTC_2000		0x00010000
#define	LPCIB_ICH5_HPTC_2000_BASE	0xfed02000
#define	LPCIB_ICH5_HPTC_3000		0x00018000
#define	LPCIB_ICH5_HPTC_3000_BASE	0xfed03000
#define LPCIB_PCI_GEN_STA	0xd4
# define LPCIB_PCI_GEN_STA_SAFE_MODE	(1 << 2)
# define LPCIB_PCI_GEN_STA_NO_REBOOT	(1 << 1)
#define LPCIB_PCI_BACK_CNTL	0xd5
#define LPCIB_PCI_RTC_CONF	0xd8
#define LPCIB_PCI_COM_DEC	0xe0
#define LPCIB_PCI_LPCFDD_DEC	0xe1
#define LPCIB_PCI_SND_DEC	0xe2
#define LPCIB_PCI_FWH_DEC_EN1	0xe3
#define LPCIB_PCI_GEN1_DEC	0xe4
#define LPCIB_PCI_LPC_EN	0xe6
#define LPCIB_PCI_FWH_SEL1	0xe8
#define LPCIB_PCI_GEN2_DEC	0xec
#define LPCIB_PCI_FWH_SEL2	0xee
#define LPCIB_PCI_FWH_DEC_EN2	0xf0
#define LPCIB_PCI_FUNC_DIS	0xf2

/*
 * Power management I/O registers
 *  (offset from PMBASE)
 */
#define LPCIB_PM1_STS		0x00 /* ACPI PM1a_EVT_BLK fixed event status */
#define LPCIB_PM1_EN		0x02 /* ACPI PM1a_EVT_BLK fixed event enable */
#define LPCIB_PM1_CNT		0x04 /* ACPI PM1a_CNT_BLK */
#define LPCIB_PM1_TMR		0x08 /* ACPI PMTMR_BLK power mgmt timer */
#define LPCIB_PROC_CNT		0x10 /* ACPI P_BLK processor control */
#define LPCIB_LV2		0x14 /* ACPI P_BLK processor C2 control */
#define LPCIB_PM_CTRL		0x20 /* ACPI Power Management Control */
# define LPCIB_PM_SS_STATE_LOW	0x01 /* SpeedStep Low Power State */
#define LPCIB_GPE0_STS		0x28 /* ACPI GPE0_BLK GPE0 status */
#define LPCIB_GPE0_EN		0x2c /* ACPI GPE0_BLK GPE0 enable */
#define LPCIB_SMI_EN		0x30
# define LPCIB_SMI_EN_INTEL_USB2_EN	(1 << 18)
# define LPCIB_SMI_EN_LEGACY_USB2_EN	(1 << 17)
# define LPCIB_SMI_EN_PERIODIC_EN	(1 << 14)
# define LPCIB_SMI_EN_TCO_EN		(1 << 13)
# define LPCIB_SMI_EN_MCSMI_EN		(1 << 11)
# define LPCIB_SMI_EN_BIOS_RLS		(1 << 7)
# define LPCIB_SMI_EN_SWSMI_TMR_EN	(1 << 6)
# define LPCIB_SMI_EN_APMC_EN		(1 << 5)
# define LPCIB_SMI_EN_SLP_SMI_EN	(1 << 4)
# define LPCIB_SMI_EN_LEGACY_USB_EN	(1 << 3)
# define LPCIB_SMI_EN_BIOS_EN		(1 << 2)
# define LPCIB_SMI_EN_EOS		(1 << 1)
# define LPCIB_SMI_EN_GBL_SMI_EN	(1 << 0)
#define LPCIB_SMI_STS		0x34
#define LPCIB_ALT_GP_SMI_EN	0x38
#define LPCIB_ALT_GP_SMI_STS	0x3a
#define LPCIB_MON_SMI		0x40
#define LPCIB_DEVACT_STS	0x44
#define LPCIB_DEVTRAP_EN	0x48
#define LPCIB_BUS_ADDR_TRACK	0x4c
#define LPCIB_BUS_CYC_TRACK	0x4e
#define LPCIB_PM_SS_CNTL	0x50		/* SpeedStep control */
# define LPCIB_PM_SS_CNTL_ARB_DIS	0x01	/* disable arbiter */

/*
 * General Purpose I/O Registers
 *  (offset from GPIO_BASE)
 */
#define LPCIB_GPIO_GPIO_USE_SEL		0x00
#define LPCIB_GPIO_GP_IO_SEL		0x04
#define LPCIB_GPIO_GP_LVL		0x0c
#define LPCIB_GPIO_GPO_TTL		0x14
#define LPCIB_GPIO_GPO_BLINK		0x18
#define LPCIB_GPIO_GPI_INV		0x2c
#define LPCIB_GPIO_GPIO_USE_SEL2	0x30
#define LPCIB_GPIO_GP_IO_SEL2		0x34
#define LPCIB_GPIO_GP_LVL2		0x38

/*
 * SMBus controller registers.
 */

/* PCI configuration registers */
#define LPCIB_SMB_BASE	0x20		/* SMBus base address */
#define LPCIB_SMB_HOSTC	0x40		/* host configuration */
#define LPCIB_SMB_HOSTC_HSTEN	(1 << 0)	/* enable host controller */
#define LPCIB_SMB_HOSTC_SMIEN	(1 << 1)	/* generate SMI */
#define LPCIB_SMB_HOSTC_I2CEN	(1 << 2)	/* enable I2C commands */

/* SMBus I/O registers */
#define LPCIB_SMB_HS	0x00		/* host status */
#define LPCIB_SMB_HS_BUSY		(1 << 0)	/* running a command */
#define LPCIB_SMB_HS_INTR		(1 << 1)	/* command completed */
#define LPCIB_SMB_HS_DEVERR	(1 << 2)	/* command error */
#define LPCIB_SMB_HS_BUSERR	(1 << 3)	/* transaction collision */
#define LPCIB_SMB_HS_FAILED	(1 << 4)	/* failed bus transaction */
#define LPCIB_SMB_HS_SMBAL	(1 << 5)	/* SMBALERT# asserted */
#define LPCIB_SMB_HS_INUSE	(1 << 6)	/* bus semaphore */
#define LPCIB_SMB_HS_BDONE	(1 << 7)	/* byte received/transmitted */
#define LPCIB_SMB_HS_BITS		"\020\001BUSY\002INTR\003DEVERR\004BUSERR\005FAILED\006SMBAL\007INUSE\010BDONE"
#define LPCIB_SMB_HC	0x02		/* host control */
#define LPCIB_SMB_HC_INTREN	(1 << 0)	/* enable interrupts */
#define LPCIB_SMB_HC_KILL		(1 << 1)	/* kill current transaction */
#define LPCIB_SMB_HC_CMD_QUICK	(0 << 2)	/* QUICK command */
#define LPCIB_SMB_HC_CMD_BYTE	(1 << 2)	/* BYTE command */
#define LPCIB_SMB_HC_CMD_BDATA	(2 << 2)	/* BYTE DATA command */
#define LPCIB_SMB_HC_CMD_WDATA	(3 << 2)	/* WORD DATA command */
#define LPCIB_SMB_HC_CMD_PCALL	(4 << 2)	/* PROCESS CALL command */
#define LPCIB_SMB_HC_CMD_BLOCK	(5 << 2)	/* BLOCK command */
#define LPCIB_SMB_HC_CMD_I2CREAD	(6 << 2)	/* I2C READ command */
#define LPCIB_SMB_HC_CMD_BLOCKP	(7 << 2)	/* BLOCK PROCESS command */
#define LPCIB_SMB_HC_LASTB	(1 << 5)	/* last byte in block */
#define LPCIB_SMB_HC_START	(1 << 6)	/* start transaction */
#define LPCIB_SMB_HC_PECEN	(1 << 7)	/* enable PEC */
#define LPCIB_SMB_HCMD	0x03		/* host command */
#define LPCIB_SMB_TXSLVA	0x04		/* transmit slave address */
#define LPCIB_SMB_TXSLVA_READ	(1 << 0)	/* read direction */
#define LPCIB_SMB_TXSLVA_ADDR(x)	(((x) & 0x7f) << 1) /* 7-bit address */
#define LPCIB_SMB_HD0	0x05		/* host data 0 */
#define LPCIB_SMB_HD1	0x06		/* host data 1 */
#define LPCIB_SMB_HBDB	0x07		/* host block data byte */
#define LPCIB_SMB_PEC	0x08		/* PEC data */
#define LPCIB_SMB_RXSLVA	0x09		/* receive slave address */
#define LPCIB_SMB_SD	0x0a		/* receive slave data */
#define LPCIB_SMB_SD_MSG0(x)	((x) & 0xff)	/* data message byte 0 */
#define LPCIB_SMB_SD_MSG1(x)	((x) >> 8)	/* data message byte 1 */
#define LPCIB_SMB_AS	0x0c		/* auxiliary status */
#define LPCIB_SMB_AS_CRCE		(1 << 0)	/* CRC error */
#define LPCIB_SMB_AS_TCO		(1 << 1)	/* advanced TCO mode */
#define LPCIB_SMB_AC	0x0d		/* auxiliary control */
#define LPCIB_SMB_AC_AAC		(1 << 0)	/* automatically append CRC */
#define LPCIB_SMB_AC_E32B		(1 << 1)	/* enable 32-byte buffer */
#define LPCIB_SMB_SMLPC	0x0e		/* SMLink pin control */
#define LPCIB_SMB_SMLPC_LINK0	(1 << 0)	/* SMLINK0 pin state */
#define LPCIB_SMB_SMLPC_LINK1	(1 << 1)	/* SMLINK1 pin state */
#define LPCIB_SMB_SMLPC_CLKC	(1 << 2)	/* SMLINK0 pin is untouched */
#define LPCIB_SMB_SMBPC	0x0f		/* SMBus pin control */
#define LPCIB_SMB_SMBPC_CLK	(1 << 0)	/* SMBCLK pin state */
#define LPCIB_SMB_SMBPC_DATA	(1 << 1)	/* SMBDATA pin state */
#define LPCIB_SMB_SMBPC_CLKC	(1 << 2)	/* SMBCLK pin is untouched */
#define LPCIB_SMB_SS	0x10		/* slave status */
#define LPCIB_SMB_SS_HN		(1 << 0)	/* Host Notify command */
#define LPCIB_SMB_SCMD	0x11		/* slave command */
#define LPCIB_SMB_SCMD_INTREN	(1 << 0)	/* enable interrupts on HN */
#define LPCIB_SMB_SCMD_WKEN	(1 << 1)	/* wake on HN */
#define LPCIB_SMB_SCMD_SMBALDS	(1 << 2)	/* disable SMBALERT# intr */
#define LPCIB_SMB_NDADDR	0x14		/* notify device address */
#define LPCIB_SMB_NDADDR_ADDR(x)	((x) >> 1)	/* 7-bit address */
#define LPCIB_SMB_NDLOW	0x16		/* notify data low byte */
#define LPCIB_SMB_NDHIGH	0x17		/* notify data high byte */

/* ICH Chipset Configuration Registers (ICH6 and newer) */
#define LPCIB_RCBA		0xf0
#define LPCIB_RCBA_EN		0x00000001
#define	LPCIB_RCBA_SIZE		0x00004000
#define LPCIB_GCS_OFFSET		0x3410
#define LPCIB_GCS_NO_REBOOT		0x20
#define	LPCIB_RCBA_HPTC			0x00003404
#define	LPCIB_RCBA_HPTC_EN		0x00000080
#define	LPCIB_RCBA_HPTC_WIN_MASK	0x00000003
#define	LPCIB_RCBA_HPTC_0000		0x00000000
#define	LPCIB_RCBA_HPTC_0000_BASE	0xfed00000
#define	LPCIB_RCBA_HPTC_1000		0x00000001
#define	LPCIB_RCBA_HPTC_1000_BASE	0xfed01000
#define	LPCIB_RCBA_HPTC_2000		0x00000002
#define	LPCIB_RCBA_HPTC_2000_BASE	0xfed02000
#define	LPCIB_RCBA_HPTC_3000		0x00000003
#define	LPCIB_RCBA_HPTC_3000_BASE	0xfed03000

/*
 * System management TCO registers
 *  (offset from PMBASE)
 */
#define LPCIB_TCO_BASE		0x60
#define LPCIB_TCO_RLD		(LPCIB_TCO_BASE+0x00)
#define LPCIB_TCO_TMR		(LPCIB_TCO_BASE+0x01)
#define LPCIB_TCO_TMR2		(LPCIB_TCO_BASE+0x12) /* ICH6 and newer */
# define LPCIB_TCO_TMR_MASK 		0x3f
#define LPCIB_TCO_DAT_IN	(LPCIB_TCO_BASE+0x02)
#define LPCIB_TCO_DAT_OUT	(LPCIB_TCO_BASE+0x03)
#define LPCIB_TCO1_STS		(LPCIB_TCO_BASE+0x04)
# define LPCIB_TCO1_STS_TIMEOUT 	0x08
#define LPCIB_TCO2_STS		(LPCIB_TCO_BASE+0x06)
# define LPCIB_TCO2_STS_BOOT_STS 	0x04
# define LPCIB_TCO2_STS_SECONDS_TO_STS 	0x02
#define LPCIB_TCO1_CNT		(LPCIB_TCO_BASE+0x08)
# define LPCIB_TCO1_CNT_TCO_LOCK 	(1 << 12)
# define LPCIB_TCO1_CNT_TCO_TMR_HLT	(1 << 11)
# define LPCIB_TCO1_CNT_SEND_NOW	(1 << 10)
# define LPCIB_TCO1_CNT_NMI2SMI_EN	(1 << 9)
# define LPCIB_TCO1_CNT_NMI_NOW		(1 << 8)
#define LPCIB_TCO2_CNT		(LPCIB_TCO_BASE+0x0a)
#define LPCIB_TCO_MESSAGE1	(LPCIB_TCO_BASE+0x0c)
#define LPCIB_TCO_MESSAGE2	(LPCIB_TCO_BASE+0x0d)
#define LPCIB_TCO_WDSTATUS	(LPCIB_TCO_BASE+0x0e)
#define LPCIB_SW_IRQ_GEN	(LPCIB_TCO_BASE+0x10)

/*
 * TCO timer tick.  ICH datasheets say:
 *  - The timer is clocked at approximately 0.6 seconds
 *  - 6 bit; values of 0-3 will be ignored and should not be attempted
 */
static __inline int
lpcib_tcotimer_tick_to_second(int ltick)
{
	return ltick * 6 / 10;
}

static __inline int
lpcib_tcotimer_second_to_tick(int ltick)
{
	return ltick * 10 / 6;
}

#define LPCIB_TCOTIMER_MIN_TICK 	4
#define LPCIB_TCOTIMER2_MIN_TICK	2
#define LPCIB_TCOTIMER_MAX_TICK 	0x3f 	/* 39 seconds max */
#define LPCIB_TCOTIMER2_MAX_TICK 	0x265	/* 613 seconds max */

#endif /*  _DEV_IC_I82801LPCREG_H_ */
