/*	$NetBSD: mvpexreg.h,v 1.2 2015/08/08 14:35:06 kiyohara Exp $	*/
/*
 * Copyright (c) 2008, 2009 KIYOHARA Takashi
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

#ifndef _MVPEXREG_H_
#define _MVPEXREG_H_


/*
 * PCI Express Interface Registers
 */
#define MVPEX_SIZE	0x2000                 


/* PCI Express BAR Control Registers */
#define MVPEX_BAR1C		0x1804	/* BAR1 Control */
#define MVPEX_BAR2C		0x1808	/* BAR2 Control */
#define MVPEX_BARC_BAREN		(1 << 0)
#define MVPEX_BARC_BARSIZE_MASK	0xffff0000
#define MVPEX_BARC_BARSIZE(s)	(((s) - 1) & MVPEX_BARC_BARSIZE_MASK)
#define MVPEX_ERBARC	0x180c	/* Expresion ROM BAR Control */
#define MVPEX_ERBARC_EXPROMEN	(1 << 0)
#define MVPEX_ERBARC_EXPROMSZ_05M	(0 << 19)
#define MVPEX_ERBARC_EXPROMSZ_1M	(1 << 19)
#define MVPEX_ERBARC_EXPROMSZ_2M	(2 << 19)
#define MVPEX_ERBARC_EXPROMSZ_4M	(3 << 19)
/* PCI Express Configuration Requests Generation Registers */
#define MVPEX_CA		0x18f8	/* Configuration Address */
#define MVPEX_CA_CONFIGEN		(1 << 31)
#define MVPEX_CD		0x18fc	/* Configuration Data */
/* PCI Express Interrupt Registers */
#define MVPEX_IC		0x1900	/* Interrupt Cause */
#define MVPEX_IM		0x1910	/* Interrupt Mask */
#define MVPEX_I_MDIS			(1 << 1)
#define MVPEX_I_ERRWRTOREG		(1 << 3)
#define MVPEX_I_HITDFLTWINERR	(1 << 4)       /* Hit Default Win Err */
#define MVPEX_I_CORERRDET		(1 << 8)    /* Correctable Err Detect */
#define MVPEX_I_NFERRDET		(1 << 9)      /* Non-Fatal Err Detect */
#define MVPEX_I_FERRDET		(1 << 10)	/* Fatal Err Detect */
#define MVPEX_I_DSTATECHANGE		(1 << 11)	/* Dstate Change */
#define MVPEX_I_BIST			(1 << 12)     /* PCI-e BIST activated */
#define MVPEX_I_RCVERRFATAL		(1 << 16)     /* Rcv ERR_FATAL msg */
#define MVPEX_I_RCVERRNONFATAL	(1 << 17)     /* Rcv ERR_NONFATAL msg */
#define MVPEX_I_RCVERRCOR		(1 << 18)	/* Rcv ERR_COR msg */
#define MVPEX_I_RCVCRS		(1 << 19)/* Rcv CRS completion status */
#define MVPEX_I_PEXSLVHOT		(1 << 20)	/* Rcv Hot Reset */
#define MVPEX_I_PEXSLVDISLINK	(1 << 21)	/* Slave Disable Link */
#define MVPEX_I_PEXSLVLB		(1 << 22)	/* Slave Loopback */
#define MVPEX_I_PEXLINKFAIL		(1 << 23)	/* Link Failure */
#define MVPEX_I_PIN(p)		(1 << (((p) - 1) + 24))
/* PCI Express Address Window Control Registers */
#define MVPEX_NWINDOW	6	/* Window 4 and 5 has Remap (High) Register */
#define MVPEX_W_OFFSET(w)	((w < 4) ? ((w) << 4) : ((w - 4) << 5) + 0x40)
#define MVPEX_WC(x)		(0x1820 + MVPEX_W_OFFSET(x))	/* Win Ctrl */
#define MVPEX_WC_WINEN		(1 << 0)
#define MVPEX_WC_BARMAP_BAR1		(0 << 1)
#define MVPEX_WC_BARMAP_BAR2		(1 << 1)
#define MVPEX_WC_TARGET(t)		(((t) & 0xf) << 4)
#define MVPEX_WC_ATTR(a)		(((a) & 0xff) << 8)
#define MVPEX_WC_SIZE(s)		(((s) - 1) & 0xffff0000)
#define MVPEX_WB(x)		(0x1824 + MVPEX_W_OFFSET(x))	/* Win Base */
#define MVPEX_WB_BASE(b)		((b) & 0xffff0000)
#define MVPEX_WR(x)		(0x182c + MVPEX_W_OFFSET(x))	/* Win Remap */
#define MVPEX_WR_REMAP_REMAPEN	(1 << 0)
#define MVPEX_WR_REMAP(a)		((a) & 0xffff0000)
#define MVPEX_DWC		0x18b0	/* Default Window Control */
#define MVPEX_EROMWC		0x18c0	/* Expresion ROM Win Control */
#define MVPEX_EROMWR		0x18c4	/* Expresion ROM Win Remap */
/* PCI Express Control and Status Registers */
#define MVPEX_CTRL		0x1a00	/* Control */
#define MVPEX_CTRL_CONFROOTCOMPLEX	(1 << 1)
#define MVPEX_CTRL_CFGMAPTOMEMEN	(1 << 2)
#define MVPEX_CTRL_CONFMSTRHOTRESET	(1 << 24)	/* Master Hot-Reset */
#define MVPEX_CTRL_CONFMSTRLB	(1 << 26)	/* Master Loopback */
#define MVPEX_CTRL_CONFMSTRDISSCRMB	(1 << 27)/* Master Disable Scrambling */
#define MVPEX_STAT		0x1a04	/* Status */
#define MVPEX_STAT_DLDOWN		(1 << 0)
#define MVPEX_STAT_PEXBUSNUM(s)	(((s) & 0x00ff00) >> 8)
#define MVPEX_STAT_PEXDEVNUM(s)	(((s) & 0x1f0000) >> 16)
#define MVPEX_STAT_PEXSLVHOTRESET	(1 << 24)     /* Slave Hot Reset (RO) */
#define MVPEX_STAT_PEXSLVDISLINK	(1 << 25)  /* Slave Disable Link (RO) */
#define MVPEX_STAT_PEXSLVLB		(1 << 26)      /* Slave Loopback (RO) */
#define MVPEX_STAT_PEXSLVDISSCRMB	(1 << 27)  /* Slv Dis Scrambling (RO) */
#define MVPEX_CT		0x1a10	/* Completion Timeout */
#define MVPEX_FC		0x1a20	/* Flow Control */
#define MVPEX_AT		0x1a40	/* Acknowledge Timers (1X) */
#define MVPEX_TLC		0x1ab0	/* TL Control */
/* PCI Express Configuration Header Registers */
/* see at dev/pci/pcireg.h from 0x00 to 0x3c. */
#define MVPEX_BAR0INTERNAL	0x0010	/* BAR0 Internal */
#define MVPEX_BAR0INTERNAL_MASK	0xfff00000
#define MVPEX_BAR0INTERNALH	0x0014	/* BAR0 Internal (High) */
#define MVPEX_BAR1		0x0018	/* BAR1 */
#define MVPEX_BAR1H		0x001c	/* BAR1 */
#define MVPEX_BAR2		0x0020	/* BAR2 */
#define MVPEX_BAR2H		0x0024	/* BAR2 */
#define MVPEX_BAR_MASK		0xffff0000
#define MVPEX_PMCH		0x0040	/* Power Management Cap Header */
#define MVPEX_PMCSH		0x0044	/*     Control and Status */
#define MVPEX_MSIMC		0x0050	/* MSI Message Control */
#define MVPEX_MSIMA		0x0054	/* MSI Message Address */
#define MVPEX_MSIMAH		0x0058	/* MSI Message Address (High) */
#define MVPEX_MSIMD		0x005c	/* MSI Message Data */
#define MVPEX_CAP		0x0060	/* Capability */
#define MVPEX_DC		0x0064	/* Device Capabilities */
#define MVPEX_DCS		0x0068	/* Device Control Status */
#define MVPEX_LC		0x006c	/* Link Capabilities */
#define MVPEX_LCS		0x0070	/* Link Control Status */
#define MVPEX_AERH		0x0100	/* Advanced Error Report Header */
#define MVPEX_UESTAT		0x0104	/* Uncorrectable Error Status */
#define MVPEX_UEM		0x0108	/* Uncorrectable Error Mask */
#define MVPEX_UESEVERITY	0x010c	/* Uncorrectable Error Serverity */
#define MVPEX_CES		0x0110	/* Correctable Error Status */
#define MVPEX_CEM		0x0114	/* Correctable Error Mask */
#define MVPEX_AECC		0x0118	/* Advanced Error Cap and Ctrl */
#define MVPEX_HLDWORD1	0x011c	/* Header Log First DWORD */
#define MVPEX_HLDWORD2	0x0120	/* Header Log Second DWORD */
#define MVPEX_HLDWORD3	0x0124	/* Header Log Third DWORD */
#define MVPEX_HLDWORD4	0x0128	/* Header Log Fourth DWORD */

#endif	/* _MVPEXREG_H_ */
