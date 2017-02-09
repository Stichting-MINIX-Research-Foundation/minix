/*	$NetBSD: ebusreg.h,v 1.9 2011/03/16 02:34:10 mrg Exp $	*/

/*
 * Copyright (c) 1999 Matthew R. Green
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _DEV_EBUS_EBUSREG_H_
#define _DEV_EBUS_EBUSREG_H_

/*
 * SPARC `ebus'
 *
 * The `ebus' bus is designed to plug traditional PC-ISA devices into
 * an SPARC system with as few costs as possible, without sacrificing
 * to performance.  Typically, it is implemented in the PCIO IC from
 * SME, which also implements a `hme-compatible' PCI network device
 * (`network').  The ebus has 4 DMA channels, similar to the DMA seen
 * in the ESP SCSI DMA.
 *
 * Typical UltraSPARC systems have a NatSemi SuperIO IC to provide
 * serial ports for the keyboard and mouse (`se'), floppy disk
 * controller (`fdthree'), parallel port controller (`bpp') connected
 * to the ebus, and a PCI-IDE controller (connected directly to the
 * PCI bus, of course), as well as a Siemens Nixdorf SAB82532 dual
 * channel serial controller (`su' providing ttya and ttyb), an MK48T59
 * EEPROM/clock controller (also where the idprom, including the
 * ethernet address, is located), the audio system (`SUNW,CS4231', same
 * as other UltraSPARC and some SPARC systems), and other various
 * internal devices found on traditional SPARC systems such as the
 * `power', `flashprom', etc., devices.  Other machines with this
 * device include microSPARC-IIep based systems, e.g. JavaStation10.
 *
 * The ebus uses an interrupt mapping scheme similar to PCI, though
 * the actual structures are different.
 */

/*
 * EBus PROM structures.  There's no official OFW binding for EBus,
 * so ms-IIep PROMs deviate from de-facto standard used on Ultra's.
 *
 * EBus address is represented in PROM by 2 cells: bar and offset.
 * "bar" specifies the EBus BAR register used to translate the
 * "offset" into PCI address space.
 *
 * On Ultra the bar is the _offset_ of the BAR in PCI config space but
 * in (some?) ms-IIep systems (e.g. Krups) it's the _number_ of the
 * BAR - e.g. BAR1 is represented by 1 in Krups PROM, while on Ultra
 * it's 0x14.
 */

struct ebus_regs {
	uint32_t	hi;		/* high bits of physaddr */
	uint32_t	lo;
	uint32_t	size;
};

#define	EBUS_ADDR_FROM_REG(reg)		BUS_ADDR((reg)->hi, (reg)->lo)


struct ebus_ranges {
	uint32_t	child_hi;	/* child high phys addr */
	uint32_t	child_lo;	/* child low phys addr */
	uint32_t	phys_hi;	/* parent high phys addr */
	uint32_t	phys_mid;	/* parent mid phys addr */
	uint32_t	phys_lo;	/* parent low phys addr */
	uint32_t	size;
};

struct ebus_mainbus_ranges {
	u_int32_t	child_hi;
	u_int32_t	child_lo;
	u_int32_t	phys_hi;
	u_int32_t	phys_lo;
	u_int32_t	size;
};


/* NB: ms-IIep PROMs lack these interrupt-related properties */
struct ebus_interrupt_map {
	uint32_t	hi;		/* high phys addr mask */
	uint32_t	lo;		/* low phys addr mask */
	uint32_t	intr;		/* interrupt mask */
	int32_t		cnode;		/* child node */
	uint32_t	cintr;		/* child interrupt */
};

struct ebus_interrupt_map_mask {
	uint32_t	hi;		/* high phys addr */
	uint32_t	lo;		/* low phys addr */
	uint32_t	intr;		/* interrupt */
};


/*
 * DMA controller registers.
 *
 * The "next" registers are at the same locations.
 * Which one you write to depends on EN_NEXT bit in the DCSR.
 */
#define EBUS_DMAC_DCSR	0	/* control/status register */
#define EBUS_DMAC_DACR	4	/* address count register */
#define EBUS_DMAC_DNAR	4	/* next address register */
#define EBUS_DMAC_DBCR	8	/* byte count register */
#define EBUS_DMAC_DNBR	8	/* next byte register */

#define EBUS_DMAC_SIZE	12


/*
 * DCSR bits (PCIO manual, Table 7-23, pp 134-135)
 *
 * On Reset all the register bits except ID will be 0 and CYC_PENDING
 * will reflect the status of any pending requests.
 */
#define EBDMA_INT_PEND		0x00000001 /* interrupt pending */
#define EBDMA_ERR_PEND		0x00000002 /* error pending */
#define EBDMA_DRAIN		0x00000004 /* fifo's being drained to memory */
#define EBDMA_INT_EN		0x00000010 /* enable interrupts */
#define EBDMA_RESET		0x00000080 /* reset - write 0 to clear */
#define EBDMA_WRITE		0x00000100 /* 0: mem->dev, 1: dev->mem */
#define EBDMA_EN_DMA		0x00000200 /* enable DMA */
#define EBDMA_CYC_PEND		0x00000400 /* DMA cycle pending
					      - not safe to clear reset */
#define EBDMA_DIAG_RD_DONE	0x00000800 /* DIAG mode: DMA read completed */
#define EBDMA_DIAG_WR_DONE	0x00001000 /* DIAG mode: DMA write completed */
#define EBDMA_EN_CNT		0x00002000 /* enable byte counter */
#define EBDMA_TC		0x00004000 /* terminal count
					      - write 1 to clear */
#define EBDMA_DIS_CSR_DRN	0x00010000 /* disable fifo draining
					      on slave writes to CSR */
#define EBDMA_BURST_SIZE_MASK	0x000c0000 /* burst sizes: */
#define EBDMA_BURST_SIZE_4	    0x00000000 /* 00 -  4 words */
#define EBDMA_BURST_SIZE_8	    0x00040000 /* 01 -  8 words */
#define EBDMA_BURST_SIZE_1	    0x00080000 /* 10 -  1 word  */
#define EBDMA_BURST_SIZE_16	    0x000c0000 /* 11 - 16 words */
#define EBDMA_DIAG_EN		0x00100000 /* enable diag mode */
#define EBDMA_DIS_ERR_PEND	0x00400000 /* disable stop/interrupt
					      on error pedning */
#define EBDMA_TCI_DIS		0x00800000 /* disable interrupt on TC */
#define EBDMA_EN_NEXT		0x01000000 /* enable next address autoload
					      (must set EN_CNT too) */
#define EBDMA_DMA_ON		0x02000000 /* DMA is able to respond */
#define EBDMA_A_LOADED		0x04000000 /* DACR loaded
					      (directly or from DNAR) */
#define EBDMA_NA_LOADED		0x08000000 /* DNAR loaded */
#define EBDMA_ID_MASK		0xf0000000 /* Device ID = 0xC */

#define EBUS_DCSR_BITS \
    "\20\34NA_LOADED\33A_LOADED\32DMA_ON\31EN_NEXT\30TCI_DIS\27DIS_ERR_PEND" \
    "\25DIAG_EN\21DIS_CSR_DRN\17TC\16EN_CNT\15DIAG_WR_DONE\14DIAG_RD_DONE"   \
    "\13CYC_PEND\12EN_DMA\11WRITE\10RESET\6INT_EN\3DRAIN\2ERR_PEND\1INT_PEND"

#endif /* _DEV_EBUS_EBUSREG_H_ */
