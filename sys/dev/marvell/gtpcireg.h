/*	$NetBSD: gtpcireg.h,v 1.6 2010/06/02 06:02:20 kiyohara Exp $	*/
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

#ifndef _GTPCIREG_H_
#define _GTPCIREG_H_


/*
 * PCI Interface Registers
 */
#define GTPCI_SIZE	0x2000

#define GTPCI_NINTERFACE	2


/* PCI Slave Address Decording Registers */
							/* BAR Sizes */
#define GTPCI_CS0BARS(p)	(0x0c08 | ((p) << 7))	/*   CSn[0] */
#define GTPCI_CS1BARS(p)	(0x0d08 | ((p) << 7))	/*   CSn[1] */
#define GTPCI_CS2BARS(p)	(0x0c0c | ((p) << 7))	/*   CSn[2] */
#define GTPCI_CS3BARS(p)	(0x0d0c | ((p) << 7))	/*   CSn[3] */
#define GTPCI_DCS0BARS(p)	(0x0c10 | ((p) << 7))	/*   DevCSn[0] */
#define GTPCI_DCS1BARS(p)	(0x0d10 | ((p) << 7))	/*   DevCSn[1] */
#define GTPCI_DCS2BARS(p)	(0x0d18 | ((p) << 7))	/*   DevCSn[2] */
#define GTPCI_BCSBARS(p)	(0x0d14 | ((p) << 7))	/*   Boot CSn */
#define GTPCI_P2PM0BARS(p)	(0x0d1c	| ((p) << 7))	/*   P2P Mem0 */
#define GTPCI_P2PIOBARS(p)	(0x0d24 | ((p) << 7))	/*   P2P I/O */
#define GTPCI_EROMBARS(p)	(0x0d2c | ((p) << 7))	/*   Expansion ROM */
#define GTPCI_BARSIZE(s)		(((s) - 1) & 0xfffff000)
#define GTPCI_BARE(p)		(0x0c3c | ((p) << 7))	/* Base Addr Reg En */
#define GTPCI_BARE_ALLDISABLE		0xffffffff
#define GTPCI_BARE_CS0EN		(1 << 0)
#define GTPCI_BARE_CS1EN		(1 << 1)
#define GTPCI_BARE_CS2EN		(1 << 2)
#define GTPCI_BARE_CS3EN		(1 << 3)
#define GTPCI_BARE_DEVCS0EN		(1 << 4)
#define GTPCI_BARE_DEVCS1EN		(1 << 5)
#define GTPCI_BARE_DEVCS2EN		(1 << 6)
#define GTPCI_BARE_BOOTCSEN		(1 << 8)
#define GTPCI_BARE_INTMEMEN		(1 << 9)
#define GTPCI_BARE_INTIOEN		(1 << 10)
#define GTPCI_BARE_P2PMEM0EN		(1 << 11)
#define GTPCI_BARE_P2PIO0EN		(1 << 13)
#define GTPCI_REMAP(a)			((a) & 0xfffff000)
							/* Base Addr Remaps */
#define GTPCI_CS0BAR(p)		(0x0c48 | ((p) << 7))	/*   CSn[0] */
#define GTPCI_CS1BAR(p)		(0x0d48 | ((p) << 7))	/*   CSn[1] */
#define GTPCI_CS2BAR(p)		(0x0c4c | ((p) << 7))	/*   CSn[2] */
#define GTPCI_CS3BAR(p)		(0x0d4c | ((p) << 7))	/*   CSn[3] */
#define GTPCI_DCS0BAR(p)	(0x0c50 | ((p) << 7))	/*   DevCSn[0] */
#define GTPCI_DCS1BAR(p)	(0x0d50 | ((p) << 7))	/*   DevCSn[1] */
#define GTPCI_DCS2BAR(p)	(0x0d58 | ((p) << 7))	/*   DevCSn[2] */
#define GTPCI_BCSBAR(p)		(0x0d54 | ((p) << 7))	/*   Boot CSn */
#define GTPCI_P2PM0BARL(p)	(0x0d5c | ((p) << 7))	/*   P2P Mem0 (Low) */
#define GTPCI_P2PM0BARH(p)	(0x0d60 | ((p) << 7))	/*   P2P Mem0 (High) */
#define GTPCI_P2PIOBAR(p)	(0x0d6c | ((p) << 7))	/*   P2P I/O */
#define GTPCI_EROMBAR(p)	(0x0f38 | ((p) << 7))	/*   Expresion ROM */
#define GTPCI_DRAMBARBS(p)	(0x0c1c | ((p) << 7))	/*DRAM BAR Bank Select*/
#define GTPCI_ADC(p)		(0x0d3c | ((p) << 7))	/* Addr Decode Ctrl */
#define GTPCI_ADC_REMAPWRDIS		(1 << 0)

/* PCI Control Register Map */
#define GTPCI_DLLC(p)		(0x1d20 | ((p) << 7))	/* PCI DLL Control */
#define GTPCI_MPPPC(p)		(0x1d1c | ((p) << 7))	/*PCI/MPP Pads Calibrt*/
#define GTPCI_C(p)		(0x0c00 | ((p) << 7))	/* Command */
#define GTPCI_C_MBYTESWAP		(1 << 0)	/* Master Byte Swap */
#define GTPCI_C_MWRCOM			(1 << 4) /* Master Wr Combine Enable */
#define GTPCI_C_MRDCOM			(1 << 5) /* Master Rd Combine Enable */
#define GTPCI_C_MWRTRIG			(1 << 6)	/*Master Write Trigger*/
#define GTPCI_C_MRDTRIG			(1 << 7)	/*Master Read Trigger */
#define GTPCI_C_MRDLINE			(1 << 8) /* Master Mem Rd Line Enable */
#define GTPCI_C_MRDMUL			(1 << 9) /* Master Mem Rd Mult Enable */
#define GTPCI_C_MWORDSWAP		(1 << 10)	/* Master Word Swap */
#define GTPCI_C_SWORDSWAP		(1 << 11)	/* Slave Word Swap */
#define GTPCI_C_SBYTESWAP		(1 << 16)	/* Slave Byte Swap */
#define GTPCI_C_MDACEN			(1 << 17)	/* Master DAC Enable */
#define GTPCI_C_PERRPROP		(1 << 19)/*Pari/ECC Err Propagation En*/
#define GTPCI_C_SSWAPEN			(1 << 20)	/* Slave Swap Enable */
#define GTPCI_C_MSWAPEN			(1 << 21)	/* Master Swap Enable */
#define GTPCI_C_SINTSWAP_BYTESWAP	(0 << 24)
#define GTPCI_C_SINTSWAP_NOSWAP		(1 << 24)
#define GTPCI_C_SINTSWAP_BOTH		(2 << 24)
#define GTPCI_C_SINTSWAP_WORDSWAP	(3 << 24)
#define GTPCI_C_SSBINT			(1 << 28)
#define GTPCI_C_CPU2PCIORDERING		(1 << 29)	/* PCI2CPU Ordering En*/
#define GTPCI_M(p)		(0x0d00 | ((p) << 7))	/* Mode */
#define GTPCI_R(p)		(0x0c04 | ((p) << 7))	/* Retry */
#define GTPCI_DT(p)		(0x0d04 | ((p) << 7))	/* Discard Timer */
#define GTPCI_MSITT(p)		(0x0c38 | ((p) << 7))	/* MSI Trigger Timer */
#define GTPCI_AC(p)		(0x1d00 | ((p) << 7))	/* Arviter Control */
#define GTPCI_AC_BDEN			(1 << 0) /* Broken Detection Enable */
#define GTPCI_AC_BV(v)			((v) << 3)	/* Broken Value */
#define GTPCI_AC_PD(v)			((v) << 14)	/* Parking Disable */
#define GTPCI_AC_EN			(1 << 31)	/* En Inter Arb Ope */
#define GTPCI_P2PC(p)		(0x1d14 | ((p) << 7))	/* P2P Configuration */
#define GTPCI_P2PC_BUSNUMBER(x)		(((x) >> 16) & 0xff)
#define GTPCI_P2PC_DEVNUM(x)		(((x) >> 24) & 0x1f)
#define GTPCI_NPCIAC			6
#define GTPCI_ACBL(p, N)	(0x1e00	| ((p) << 7) | ((N) << 4))
					/* Access Control Base N (Low) */
#define GTPCI_ACBL_EN			(1 << 0)
#define GTPCI_ACBL_REQ64		(1 << 1)
#define GTPCI_ACBL_SNOOP_MASK		(3 << 2)
#define GTPCI_ACBL_SNOOP_NONE		(0 << 2)
#define GTPCI_ACBL_SNOOP_WT		(1 << 2)
#define GTPCI_ACBL_SNOOP_WB		(2 << 2)
#define GTPCI_ACBL_ACCPROT		(1 << 4)
#define GTPCI_ACBL_WRPROT		(1 << 5)
#define GTPCI_ACBL_PCISWAP_MASK		(3 << 6)
#define GTPCI_ACBL_PCISWAP_BYTESWAP	(0 << 6)
#define GTPCI_ACBL_PCISWAP_NOSWAP	(1 << 6)
#define GTPCI_ACBL_PCISWAP_BOTHSWAP	(2 << 6)
#define GTPCI_ACBL_PCISWAP_WORDSWAP	(3 << 6)
#define GTPCI_ACBL_RDMBURST_MASK	(3 << 8)
#define GTPCI_ACBL_RDMBURST_32BYTE	(0 << 8)
#define GTPCI_ACBL_RDMBURST_64BYTE	(1 << 8)
#define GTPCI_ACBL_RDMBURST_128BYTE	(2 << 8)
#define GTPCI_ACBL_RDSIZE_MASK		(3 << 10)
#define GTPCI_ACBL_RDSIZE_32BYTE	(0 << 10)
#define GTPCI_ACBL_RDSIZE_64BYTE	(1 << 10)
#define GTPCI_ACBL_RDSIZE_128BYTE	(2 << 10)
#define GTPCI_ACBL_RDSIZE_256BYTE	(3 << 10)
#define GTPCI_ACBL_BASE(b)		((b) & 0xfffff000)

#define GTPCI_GT64260_ACBL_BASE(b)	((b) & 0x00000fff)
#define GTPCI_GT64260_ACBL_PREFETCHEN	(1 << 12)
#define GTPCI_GT64260_ACBL_DREADEN	(1 << 13)
#define GTPCI_GT64260_ACBL_RDPREFETCH	(1 << 16)
#define GTPCI_GT64260_ACBL_RDLINEPREFETCH (1 << 17)
#define GTPCI_GT64260_ACBL_RDMULPREFETCH  (1 << 18)
#define GTPCI_GT64260_ACBL_WBURST_MASK	(3 << 20)
#define GTPCI_GT64260_ACBL_WBURST_4_QW	(0 << 20)
#define GTPCI_GT64260_ACBL_WBURST_8_QW	(1 << 20)
#define GTPCI_GT64260_ACBL_WBURST_16_QW	(2 << 20)
#define GTPCI_GT64260_ACBL_PCISWAP_BYTESWAP     (0 << 24)
#define GTPCI_GT64260_ACBL_PCISWAP_NOSWAP       (1 << 24)
#define GTPCI_GT64260_ACBL_PCISWAP_BYTEWORDSWAP (3 << 24)
#define GTPCI_GT64260_ACBL_PCISWAP_WORDSWAP     (3 << 24)
#define GTPCI_GT64260_ACBL_ACCPROT	(1 << 28)
#define GTPCI_GT64260_ACBL_WRPROT	(1 << 29)

#define GTPCI_ACBH(p, N)	(0x1e04	| ((p) << 7) | ((N) << 4))
					/* Access Control Base N (High) */
#define GTPCI_ACS(p, N)		(0x1e08	| ((p) << 7) | ((N) << 4))
					/* Access Ctrl Size N */
#define GTPCI_ACS_AGGRWM1		(1 << 4)
#define GTPCI_ACS_WRMBURST_MASK		(3 << 8)
#define GTPCI_ACS_WRMBURST_32BYTE	(0 << 8)
#define GTPCI_ACS_WRMBURST_64BYTE	(1 << 8)
#define GTPCI_ACS_WRMBURST_128BYTE	(2 << 8)
#define GTPCI_ACS_AGGR			(1 << 10)
#define GTPCI_ACS_PCIOR			(1 << 11)
#define GTPCI_ACS_SIZE(s)		(((s) - 1) & 0xfffff000)

/* PCI Configuration Access Register Map */
#define GTPCI_CA(p)		(0x0cf8 ^ ((p) << 7))	/* Configuration Addr */
#define GTPCI_CA_CONFIGEN		(1 << 31)
#define GTPCI_CD(p)		(0x0cfc ^ ((p) << 7))	/* Configuration Data */

#define GTPCI_IA(p)		(0x0c34 | ((p) << 7)	/* Intr Acknowledge */

/* PCI Error Report Register Map */
#define GTPCI_SERRM(p)		(0x0c28 | ((p) << 7)	/* SERRn Mask */
#define GTPCI_IC(p)		(0x0d58 | ((p) << 7)	/* Interrupt Cause */
#define GTPCI_IM(p)		(0x0d5c | ((p) << 7)	/* Interrupt Mask */
#define GTPCI_EAL(p)		(0x0d40 | ((p) << 7)	/* Error Addr (Low) */
#define GTPCI_EAH(p)		(0x0d44 | ((p) << 7)	/* Error Addr (High) */
#define GTPCI_EC(p)		(0x0d50 | ((p) << 7)	/* Error Command */

/* PCI Configuration, Function 0, Register Map */
/* see at dev/pci/pcireg.h from 0x00 to 0x3c. */

#define GTPCI_BARLOW_MASK		0xfffff000
#define GTPCI_BARLOW_BASE(b)		((b) & GTPCI_BARLOW_MASK)

#endif	/* _GTPCIREG_H_ */
