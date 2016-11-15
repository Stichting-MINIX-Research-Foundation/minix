/* $NetBSD: siisatareg.h,v 1.7 2011/11/02 16:03:01 jakllsch Exp $ */

/*
 * Copyright (c) 2007, 2008, 2009, 2010, 2011 Jonathan A. Kollasch.
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

#ifndef _IC_SIISATAREG_H_
#define _IC_SIISATAREG_H_

/* Silicon Image SATA 2 controller register defines */

#include <sys/cdefs.h>

/* the SiI3124 has 4 ports, all others so far have less */
#define SIISATA_MAX_PORTS 4
/* all parts have a full complement of slots (so far) */
#define SIISATA_MAX_SLOTS 31

/* structures */

/* Scatter/Gather Entry */
struct siisata_sge {
#if 0
	uint32_t sge_dal; /* data address low */
	uint32_t sge_dah; /* "          " high */
#else
	uint64_t sge_da;
#endif
	uint32_t sge_dc;  /* data count (bytes) */
	uint32_t sge_flags; /* */
#define SGE_FLAG_TRM __BIT(31)
#define SGE_FLAG_LNK __BIT(30)
#define SGE_FLAG_DRD __BIT(29)
#define SGE_FLAG_XCF __BIT(28)
} __packed __aligned(8);

/* Scatter/Gather Table */
/* must be aligned to 64-bit boundary */
struct siisata_sgt {
	struct siisata_sge sgt_sge[4];
} __packed __aligned(8);

/* Port Request Block */
struct siisata_prb {
	uint16_t prb_control; /* Control Field */
#define PRB_CF_PROTOCOL_OVERRIDE __BIT(0)
#define PRB_CF_RETRANSMIT        __BIT(1)
#define PRB_CF_EXTERNAL_COMMAND  __BIT(2)
#define PRB_CF_RECEIVE           __BIT(3)
#define PRB_CF_PACKET_READ       __BIT(4)
#define PRB_CF_PACKET_WRITE      __BIT(5)
#define PRB_CF_INTERRUPT_MASK    __BIT(6)
#define PRB_CF_SOFT_RESET        __BIT(7)
	uint16_t prb_protocol_override;
#define PRB_PO_PACKET      __BIT(0)
#define PRB_PO_LCQ         __BIT(1)
#define PRB_PO_NCQ         __BIT(2)
#define PRB_PO_READ        __BIT(3)
#define PRB_PO_WRITE       __BIT(4)
#define PRB_PO_TRANSPARENT __BIT(5)
	uint32_t prb_transfer_count;
	uint8_t prb_fis[20];
	uint32_t prb_reserved_0x1C; /* "must be zero" */
/* First SGE in PRB is always reserved for ATAPI in this implementation. */
	uint8_t prb_atapi[16]; /* zero for non-ATAPI */
	struct siisata_sge prb_sge[1]; /* extended to NSGE */
} __packed __aligned(8);


#define SIISATA_NSGE ((MAXPHYS/PAGE_SIZE) + 1)
#define SIISATA_CMD_ALIGN 0x7f
#define SIISATA_CMD_SIZE \
( ( sizeof(struct siisata_prb) + (SIISATA_NSGE - 1) * sizeof(struct siisata_sge) + SIISATA_CMD_ALIGN ) & ~SIISATA_CMD_ALIGN )

/* PCI stuff */
#define SIISATA_PCI_BAR0 0x10
#define SIISATA_PCI_BAR1 0x18
#define SIISATA_PCI_BAR2 0x20

/* Cardbus stuff */
#define SIISATA_CARDBUS_BAR0 SIISATA_PCI_BAR0
#define SIISATA_CARDBUS_BAR1 SIISATA_PCI_BAR1
#define SIISATA_CARDBUS_BAR2 SIISATA_PCI_BAR2

/* BAR 0 */

/* port n slot status */
#define GR_PXSS(n) (n*4)
/* global control */
#define GR_GC		0x40
/* global interrupt status */
#define GR_GIS		0x44
/* phy config - don't touch */
#define GR_PHYC		0x48
/* BIST */
#define GR_BIST_CONTROL	0x50
#define GR_BIST_PATTERN	0x54
#define GR_BIST_STATUS	0x58
/* I2C SiI3132 */
#define GR_SII3132_IICCONTROL	0x60
#define GR_SII3132_IICSTATUS	0x64
#define GR_SII3132_IICSLAVEADDR	0x68
#define GR_SII3132_IICDATA	0x6c
/* Flash */
#define GR_FLSHADDR	0x70
#define GR_FLSHDATA	0x74
/* I2C SiI3124 */
#define GR_SII3124_IICADDR	0x78
#define GR_SII3124_IICDATA	0x7c


/* GR_GC bits */
#define GR_GC_GLBLRST		__BIT(31)
#define GR_GC_MSIACK		__BIT(30)
#define GR_GC_I2CINTEN		__BIT(29)
#define GR_GC_PERRRPTDSBL	__BIT(28)
#define GR_GC_3GBPS		__BIT(24)
#define GR_GC_REQ64		__BIT(20)
#define GR_GC_DEVSEL		__BIT(19)
#define GR_GC_STOP		__BIT(18)
#define GR_GC_TRDY		__BIT(17)
#define GR_GC_M66EN		__BIT(16)	
#define GR_GC_PXIE_MASK		__BITS(SIISATA_MAX_PORTS - 1, 0)
#define GR_GC_PXIE(n)		__SHIFTIN(__BIT(n), GR_GC_PXIE_MASK)

/* GR_GIS bits */
#define GR_GIS_I2C		__BIT(29)
#define GR_GIS_PXIS_MASK	__BITS(SIISATA_MAX_PORTS - 1, 0)
#define GR_GIS_PXIS(n)		__SHIFTIN(__BIT(n), GR_GIS_PXIS_MASK)


/* BAR 1 */

/* hmm, this could use a better name */
#define PR_PORT_SIZE	0x2000
#define PR_SLOT_SIZE	0x80
/* get the register by port number and offset */
#define PRO(p) (PR_PORT_SIZE * p)
#define PRX(p,r) (PRO(p) + r)
#define PRSX(p,s,o) (PRX(p, PR_SLOT_SIZE * s + o))

#define PRSO_RTC	0x04		/* recieved transfer count */
#define PRSO_FIS	0x08		/* base of FIS */

#define PRO_PCS		0x1000		/* (write) port control set */
#define PRO_PS		PRO_PCS		/* (read) port status */
#define PRO_PCC		0x1004		/* port control clear */
#define PRO_PIS		0x1008		/* port interrupt status */
#define PRO_PIES	0x1010		/* port interrupt enable set */
#define PRO_PIEC	0x1014		/* port interrupt enable clear */
#define PRO_32BAUA	0x101c		/* 32-bit activation upper address */
#define PRO_PCEF	0x1020		/* port command execution fifo */
#define PRO_PCE		0x1024		/* port command error */
#define PRO_PFISC	0x1028		/* port FIS config */
#define PRO_PCIRFIFOT	0x102c		/* pci request fifo threshhold */
#define PRO_P8B10BDEC	0x1040		/* port 8B/10B decode error counter */
#define PRO_PCRCEC	0x1044		/* port crc error count */
#define PRO_PHEC	0x1048		/* port handshake error count */
#define PRO_PPHYC	0x1050		/* phy config */
#define PRO_PSS		0x1800		/* port slot status */
/* technically this is a shadow of the CAR */
#define PRO_CAR		0x1c00

#define PRO_CARX(p,s)     (PRX(p, PRO_CAR) + s * sizeof(uint64_t))

#define PRO_PCR		0x1e04		/* port context register */
#define PRO_SCONTROL	0x1f00		/* SControl */
#define PRO_SSTATUS	0x1f04		/* SStatus */
#define PRO_SERROR	0x1f08		/* SError */
#define PRO_SACTIVE	0x1f0c		/* SActive */


/* Port Command Error */
#define PR_PCE_DEVICEERROR		1
#define PR_PCE_SDBERROR			2
#define PR_PCE_DATAFISERROR		3
#define PR_PCE_SENDFISERROR		4
#define PR_PCE_INCONSISTENTSTATE	5
#define PR_PCE_DIRECTIONERROR		6
#define PR_PCE_UNDERRUNERROR		7
#define PR_PCE_OVERRUNERROR		8
#define PR_PCE_LINKFIFOOVERRUN		9
#define PR_PCE_PACKETPROTOCOLERROR	11
#define PR_PCE_PLDSGTERRORBOUNDARY	16
#define PR_PCE_PLDSGTERRORTARGETABORT	17
#define PR_PCE_PLDSGTERRORMASTERABORT	18
#define PR_PCE_PLDSGTERRORPCIPERR	19
#define PR_PCE_PLDCMDERRORBOUNDARY	24
#define PR_PCE_PLDCMDERRORTARGETABORT	25
#define PR_PCE_PLDCMDERRORMASTERABORT	26
#define PR_PCE_PLDCMDERRORPCIPERR	27
#define PR_PCE_PSDERRORTARGETABORT	33
#define PR_PCE_PSDERRORMASTERABORT	34
#define PR_PCE_PSDERRORPCIPERR		35
#define PR_PCE_SENDSERVICEERROROR	36


#define PR_PIS_UNMASKED_SHIFT	16
#define PR_PIS_CMDCMPL		__BIT(0)	/* command completion */
#define PR_PIS_CMDERRR		__BIT(1)	/* command error */
#define PR_PIS_PRTRDY		__BIT(2)  /* port ready */
#define PR_PIS_PMCHNG		__BIT(3)  /* power management state change */
#define PR_PIS_PHYRDYCHG	__BIT(4)
#define PR_PIS_COMWAKE		__BIT(5)
#define PR_PIS_UNRECFIS		__BIT(6)
#define PR_PIS_DEVEXCHG		__BIT(7)
#define PR_PIS_8B10BDET		__BIT(8)
#define PR_PIS_CRCET		__BIT(9)
#define PR_PIS_HET		__BIT(10)
#define PR_PIS_SDBN		__BIT(11)

#define PR_PC_PORT_RESET	__BIT(0)
#define PR_PC_DEVICE_RESET	__BIT(1)
#define PR_PC_PORT_INITIALIZE	__BIT(2)
#define PR_PC_INCOR		__BIT(3)
#define PR_PC_LED_DISABLE	__BIT(4)
#define PR_PC_PACKET_LENGTH	__BIT(5)
#define PR_PC_RESUME		__BIT(6)
#define PR_PC_TXBIST		__BIT(7)
#define PR_PC_CONT_DISABLE	__BIT(8)
#define PR_PC_SCRAMBLER_DISABLE	__BIT(9)
#define PR_PC_32BA		__BIT(10)
#define PR_PC_INTERLOCK_REJECT	__BIT(11)
#define PR_PC_INTERLOCK_ACCEPT	__BIT(12)
#define PR_PC_PMP_ENABLE	__BIT(13)
#define PR_PC_AIA		__BIT(14)
#define PR_PC_LED_ON		__BIT(15)
#define PR_PC_OOB_BYPASS	__BIT(25)
#define PR_PS_PORT_READY	__BIT(31)

#define PR_PSS_ATTENTION	__BIT(31)
#define PR_PSS_SLOT_MASK	__BITS(30, 0)
#define PR_PXSS(n)		__SHIFTIN(__BIT(n), PR_PSS_SLOT_MASK)

#endif /* !_IC_SIISATAREG_H_ */
