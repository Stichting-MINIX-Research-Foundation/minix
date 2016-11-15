/*	$NetBSD: gtidmacreg.h,v 1.3 2012/07/23 06:09:47 kiyohara Exp $	*/
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

#ifndef _GTIDMACREG_H_
#define _GTIDMACREG_H_

/*
 * IDMA Controller Interface Registers / XOR Engine Control Registers
 */

#define GTIDMAC_SIZE		0x1000


#define GTIDMAC_NWINDOW		8
#define GTIDMAC_NREMAP		4
#define GTIDMAC_NACCPROT	4			/* Num Access Protect */
#define GTIDMAC_NINTRRUPT	4
#define GTIDMAC_MAXXFER		(16 * 1024 * 1024 - 1)	/* 16M - 1 Byte */

#define MVXORE_NWINDOW		8
#define MVXORE_NREMAP		4
#define MVXORE_MAXXFER		(16 * 1024 * 1024 - 1)	/* 16M - 1 Byte */
#define MVXORE_NSRC		8


#define GTIDMAC_CHAN2BASE(c)	((((c) & 0x4) << 6) + (((c) & 0x3) << 2))
#define MVXORE_PORT2BASE(sc, p)	\
    (((sc)->sc_gtidmac_nchan == 0 && (p) == 0) ? -0x100 : 0x000)
#define MVXORE_CHAN2BASE(sc, c)	\
    (MVXORE_PORT2BASE(sc, (c) & 0x4) + (((c) & 0x3) << 2))


/* IDMA Descriptor Register Map */
#define GTIDMAC_CIDMABCR(c)	/* Chan IDMA Byte Count */ \
				(GTIDMAC_CHAN2BASE(c) + 0x0800)
#define GTIDMAC_CIDMABCR_BYTECNT_MASK 0x00ffffff
#define GTIDMAC_CIDMABCR_BCLEFT		(1 << 30)	/* Left Byte Count */
#define GTIDMAC_CIDMABCR_OWN		(1 << 31)	/* Ownership Bit */
#define GTIDMAC_CIDMASAR(c)	/* Chan IDMA Source Address */ \
				(GTIDMAC_CHAN2BASE(c) + 0x0810)
#define GTIDMAC_CIDMADAR(c)	/* Chan IDMA Destination Address */ \
				(GTIDMAC_CHAN2BASE(c) + 0x0820)
#define GTIDMAC_CNDPR(c)	/* Chan Next Descriptor Pointer */ \
				(GTIDMAC_CHAN2BASE(c) + 0x0830)
#define GTIDMAC_CCDPR(c)	/* Chan Current Descriptor Pointer */ \
				(GTIDMAC_CHAN2BASE(c) + 0x0870)
/* IDMA Control Register Map */
#define GTIDMAC_CCLR(c)		/* Chan Control Low */ \
				(GTIDMAC_CHAN2BASE(c) + 0x0840)
#define GTIDMAC_CCLR_DBL_MASK		(7 << 0)	/* DstBurstLimit */
#define GTIDMAC_CCLR_DBL_8B		(0 << 0)
#define GTIDMAC_CCLR_DBL_16B		(1 << 0)
#define GTIDMAC_CCLR_DBL_32B		(3 << 0)
#define GTIDMAC_CCLR_DBL_64B		(7 << 0)
#define GTIDMAC_CCLR_DBL_128B		(4 << 0)
#define GTIDMAC_CCLR_SRCHOLD		(1 << 3)	/* Source Hold */
#define GTIDMAC_CCLR_DESTHOLD		(1 << 5)	/* Destination Hold */
#define GTIDMAC_CCLR_SBL_MASK		(7 << 6)	/* SrcBurstLimit */
#define GTIDMAC_CCLR_SBL_8B		(0 << 6)
#define GTIDMAC_CCLR_SBL_16B		(1 << 6)
#define GTIDMAC_CCLR_SBL_32B		(3 << 6)
#define GTIDMAC_CCLR_SBL_64B		(7 << 6)
#define GTIDMAC_CCLR_SBL_128B		(4 << 6)
#define GTIDMAC_CCLR_CHAINMODE_C	(0 << 9)	/* Chained Mode */
#define GTIDMAC_CCLR_CHAINMODE_NC	(1 << 9)	/* Non-Chained Mode */
#define GTIDMAC_CCLR_INTMODE		(1 << 10)	/* Interrupt Mode */
#define GTIDMAC_CCLR_INTMODE_NULL	(1 << 10)	/*   Next Desc NULL */
#define GTIDMAC_CCLR_TRANSFERMODE_D	(0 << 11)	/* Transfer Mode */
#define GTIDMAC_CCLR_TRANSFERMODE_B	(1 << 11)	/*   Demand/Block */
#define GTIDMAC_CCLR_CHANEN		(1 << 12)	/* Channel Enable */
#define GTIDMAC_CCLR_FETCHND		(1 << 13)	/* Fetch Next Desc */
#define GTIDMAC_CCLR_CHANACT		(1 << 14)	/* IDMA Chan Active */
#define GTIDMAC_CCLR_CDEN		(1 << 17)	/* Close Desc Enable */
#define GTIDMAC_CCLR_ABR		(1 << 20)	/* Channel Abort */
#define GTIDMAC_CCLR_SADDROVR_MASK	(3 << 21)	/* Override Src Addr */
#define GTIDMAC_CCLR_SADDROVR_NO	(0 << 21)
#define GTIDMAC_CCLR_SADDROVR_BAR1	(1 << 21)
#define GTIDMAC_CCLR_SADDROVR_BAR2	(2 << 21)
#define GTIDMAC_CCLR_SADDROVR_BAR3	(3 << 21)
#define GTIDMAC_CCLR_NADDROVR_MASK	(3 << 21)	/* Override Next Addr */
#define GTIDMAC_CCLR_NADDROVR_NO	(0 << 21)
#define GTIDMAC_CCLR_NADDROVR_BAR1	(1 << 21)
#define GTIDMAC_CCLR_NADDROVR_BAR2	(2 << 21)
#define GTIDMAC_CCLR_NADDROVR_BAR3	(3 << 21)
#define GTIDMAC_CCLR_DESCMODE_64K	(0 << 31)
#define GTIDMAC_CCLR_DESCMODE_16M	(1 << 31)
#define GTIDMAC_CCHR(c)		/* Chan Control High */ \
				(GTIDMAC_CHAN2BASE(c) + 0x0880)
#define GTIDMAC_CCHR_ENDIAN_BE		(0 << 0)	/* big endian */
#define GTIDMAC_CCHR_ENDIAN_LE		(1 << 0)	/* little endian */
#define GTIDMAC_CCHR_DESCBYTESWAP	(1 << 1)	/* Desc Byte Swap */
#define GTIDMAC_ARBR(c)		(0x0860 + (((c) & 0x04) << 6)) /* Arbitrate ??*/
#define GTIDMAC_XTOR(c)		(0x08d0 + (((c) & 0x04) << 6)) /* x-bar t/o?? */
/* IDMA Interrupt Register Map */
#define GTIDMAC_ICR(c)		(0x08c0 + (((c) & 0x04) << 6)) /* Intr Cause */
#define GTIDMAC_IMR(c)		(0x08c4 + (((c) & 0x04) << 6)) /* Intr Mask */
#define GTIDMAC_I_BITS			8
#define GTIDMAC_I(c, b)			((b) << (GTIDMAC_I_BITS * ((c) & 0x3)))
#define GTIDMAC_I_COMP			(1 << 0)	/* Completion */
#define GTIDMAC_I_ADDRMISS		(1 << 1)	/* Address Miss */
#define GTIDMAC_I_ACCPROT		(1 << 2)	/* Acc Prot Violation */
#define GTIDMAC_I_WRPROT		(1 << 3)	/* Write Protect */
#define GTIDMAC_I_OWN			(1 << 4)	/* Ownership Violation*/
#define GTIDMAC_EAR(c)		(0x08c8 + (((c) & 0x04) << 6)) /* Err Address */
#define GTIDMAC_ESR(c)		(0x08cc + (((c) & 0x04) << 6)) /* Err Select */
#define GTIDMAC_ESR_SEL			0x1f

/* XOR Engine Control Registers */
#define MVXORE_XECHAR(sc, p)	/* Channel Arbiter */ \
				(MVXORE_PORT2BASE((sc), (p)) + 0x0900)
#define MVXORE_XECHAR_SLICEOWN(s, c)	((c) << (s))
#define MVXORE_XEXCR(sc, x)	/* Configuration */ \
				(MVXORE_CHAN2BASE((sc), (x)) + 0x0910)
#define MVXORE_XEXCR_OM_MASK		(7 << 0)	/* Operation Mode */
#define MVXORE_XEXCR_OM_XOR		(0 << 0)
#define MVXORE_XEXCR_OM_CRC32		(1 << 0)
#define MVXORE_XEXCR_OM_DMA		(2 << 0)
#define MVXORE_XEXCR_OM_ECC		(3 << 0)	/* ECC cleanup ope */
#define MVXORE_XEXCR_OM_MEMINIT		(4 << 0)
#define MVXORE_XEXCR_SBL_MASK		(7 << 4)	/* SrcBurstLimit */
#define MVXORE_XEXCR_SBL_32B		(2 << 4)
#define MVXORE_XEXCR_SBL_64B		(3 << 4)
#define MVXORE_XEXCR_SBL_128B		(4 << 4)
#define MVXORE_XEXCR_DBL_MASK		(7 << 8)	/* SrcBurstLimit */
#define MVXORE_XEXCR_DBL_32B		(2 << 8)
#define MVXORE_XEXCR_DBL_64B		(3 << 8)
#define MVXORE_XEXCR_DBL_128B		(4 << 8)
#define MVXORE_XEXCR_DRDRESSWP		(1 << 12)	/* Endianess Swap */
#define MVXORE_XEXCR_DWRREQSWP		(1 << 13)	/*  ReadReq/WriteRes */
#define MVXORE_XEXCR_DESSWP		(1 << 14)	/*  Desc read/write */
#define MVXORE_XEXCR_REGACCPROTECT	(1 << 15)	/* Reg Access protect */
#define MVXORE_XEXACTR(sc, x)	/* Activation */ \
				(MVXORE_CHAN2BASE((sc), (x)) + 0x0920)
#define MVXORE_XEXACTR_XESTART		(1 << 0)
#define MVXORE_XEXACTR_XESTOP		(1 << 1)
#define MVXORE_XEXACTR_XEPAUSE		(1 << 2)
#define MVXORE_XEXACTR_XERESTART	(1 << 3)
#define MVXORE_XEXACTR_XESTATUS_MASK	(3 << 4)
#define MVXORE_XEXACTR_XESTATUS_NA	(0 << 4)	/* not active */
#define MVXORE_XEXACTR_XESTATUS_ACT	(1 << 4)	/* active */
#define MVXORE_XEXACTR_XESTATUS_P	(2 << 4)	/* paused */
/* XOR Engine Interrupt Registers */
#define MVXORE_XEICR(sc, p)	/* Interrupt Cause */ \
				(MVXORE_PORT2BASE((sc), (p)) + 0x0930)
#define MVXORE_XEIMR(sc, p)	/* Interrupt Mask */ \
				(MVXORE_PORT2BASE((sc), (p)) + 0x0940)
#define MVXORE_I_BITS			16
#define MVXORE_I(c, b)			((b) << (MVXORE_I_BITS * (c)))
#define MVXORE_I_EOD			(1 << 0)	/* End of Descriptor */
#define MVXORE_I_EOC			(1 << 1)	/* End of Chain */
#define MVXORE_I_STOPPED		(1 << 2)
#define MVXORE_I_PAUSED			(1 << 3)
#define MVXORE_I_ADDRDECODE		(1 << 4)
#define MVXORE_I_ACCPROT		(1 << 5)	/* Access Protect */
#define MVXORE_I_WRPROT			(1 << 6)	/* Write Protect */
#define MVXORE_I_OWN			(1 << 7)	/* Ownership */
#define MVXORE_I_INTPARITY		(1 << 8)	/* Parity error */
#define MVXORE_I_XBAR			(1 << 9)	/* Crossbar Parity E */
#define MVXORE_XEECR(sc, p)	/* Error Cause */ \
				(MVXORE_PORT2BASE((sc), (p)) + 0x0950)
#define MVXORE_XEECR_ERRORTYPE_MASK	0x0000001f
#define MVXORE_XEEAR(sc, p)	/* Error Address */ \
				(MVXORE_PORT2BASE((sc), (p)) + 0x0960)

/* IDMA Address Decoding Registers Map */
#define GTIDMAC_BARX(r)		(0x0a00 + ((r) << 3))	/* Base Address x */
#define GTIDMAC_BARX_TARGET(t)		((t) & 0xf)
#define GTIDMAC_BARX_ATTR(a)		(((a) & 0xff) << 8)
#define GTIDMAC_BARX_BASE(b)		((b) & 0xffff0000)
#define GTIDMAC_SRX(r)		(0x0a04 + ((r) << 3))	/* Size x */
#define GTIDMAC_SRX_SIZE(s)		(((s) - 1) & 0xffff0000)
#define GTIDMAC_HARXR(x)	(0x0a60 + ((x) << 2))	/* High Addr Remap x */
#define GTIDMAC_BAER		0x0a80			/* Base Addr Enable */
#define GTIDMAC_BAER_EN(w)		(1 << (w))
#define GTIDMAC_CXAPR(x)	(0x0a70 + ((x) << 2))	/* Chan x Acs Protect */
#define GTIDMAC_CXAPR_WINACC(w, ac)	((ac) << ((w) << 1))
#define GTIDMAC_CXAPR_WINACC_NOAA	0x0		/* No access allowed */
#define GTIDMAC_CXAPR_WINACC_RO		0x1		/* Read Only */
#define GTIDMAC_CXAPR_WINACC_RESV	0x2		/* Reserved */
#define GTIDMAC_CXAPR_WINACC_FA		0x3		/* Full access */

/* XOR Engine Descriptor Registers */
#define MVXORE_XEXNDPR(sc, x)	/* Next Desc Pointer */ \
				(MVXORE_CHAN2BASE((sc), (x)) + 0x0b00)
#define MVXORE_XEXCDPR(sc, x)	/* Current Desc Ptr */ \
				(MVXORE_CHAN2BASE((sc), (x)) + 0x0b10)
#define MVXORE_XEXBCR(sc, x)	/* Byte Count */ \
				(MVXORE_CHAN2BASE((sc), (x)) + 0x0b20)
/* XOR Engine Address Decording Registers */
#define MVXORE_XEXWCR(sc, x)	/* Window Control */ \
				(MVXORE_CHAN2BASE((sc), (x)) + 0x0b40)
#define MVXORE_XEXWCR_WINEN(w)		(1 << (w))
#define MVXORE_XEXWCR_WINACC(w, ac)	((ac) << (((w) << 1) + 16))
#define MVXORE_XEXWCR_WINACC_NOAA	0x0		/* No access allowed */
#define MVXORE_XEXWCR_WINACC_RO		0x1		/* Read Only */
#define MVXORE_XEXWCR_WINACC_RESV	0x2		/* Reserved */
#define MVXORE_XEXWCR_WINACC_FA		0x3		/* Full access */
#define MVXORE_XEBARX(sc, p, w)	/* Base Address */ \
			(MVXORE_PORT2BASE((sc), (p)) + 0x0b50 + ((w) << 2))
#define MVXORE_XEBARX_TARGET(t)		((t) & 0xf)
#define MVXORE_XEBARX_ATTR(a)		(((a) & 0xff) << 8)
#define MVXORE_XEBARX_BASE(b)		((b) & 0xffff0000)
#define MVXORE_XESMRX(sc, p, w)	/* Size Mask */ \
			(MVXORE_PORT2BASE((sc), (p)) + 0x0b70 + ((w) << 2))
#define MVXORE_XESMRX_SIZE(s)		(((s) - 1) & 0xffff0000)
#define MVXORE_XEHARRX(sc, p, w)/* High Address Remap */ \
			(MVXORE_PORT2BASE((sc), (p)) + 0x0b90 + ((w) << 2))
#define MVXORE_XEXAOCR(sc, x)	/* Addr Override Ctrl */ \
				(MVXORE_CHAN2BASE((sc), (x)) + 0x0ba0)
/* XOR Engine ECC/MemInit Registers */
#define MVXORE_XEXDPR(sc, x)	/* Destination Ptr */ \
				(MVXORE_CHAN2BASE((sc), (x)) + 0x0bb0)
#define MVXORE_XEXBSR(sc, x)	/* Block Size */ \
				(MVXORE_CHAN2BASE((sc), (x)) + 0x0bc0)
#define MVXORE_XETMCR(sc, p)	/* Timer Mode Control */ \
				(MVXORE_PORT2BASE((sc), (p)) + 0x0bd0)
#define MVXORE_XETMCR_TIMEREN		(1 << 0)
#define MVXORE_XETMCR_SECTIONSIZECTRL_MASK  0x1f
#define MVXORE_XETMCR_SECTIONSIZECTRL_SHIFT 8
#define MVXORE_XETMIVR(sc, p)	/* Tmr Mode Init Val */ \
				(MVXORE_PORT2BASE((sc), (p)) + 0x0bd4)
#define MVXORE_XETMCVR(sc, p)	/* Tmr Mode Curr Val */ \
				(MVXORE_PORT2BASE((sc), (p)) + 0x0bd8)
#define MVXORE_XEIVRL(sc, p)	/* Initial Value Low */ \
				(MVXORE_PORT2BASE((sc), (p)) + 0x0be0)
#define MVXORE_XEIVRH(sc, p)	/* Initial Value High */ \
				(MVXORE_PORT2BASE((sc), (p)) + 0x0be4)


struct gtidmac_desc {
#if BYTE_ORDER == LITTLE_ENDIAN
	union {
		struct {
			uint16_t rbc;	/* Remind BC */
			uint16_t bcnt;
		} mode64k;
		struct {
			uint32_t bcnt;
		} mode16m;
	} bc;			/* Byte Count */
	uint32_t srcaddr;	/* Source Address */
	uint32_t dstaddr;	/* Destination Address */
	uint32_t nextdp;	/* Next Descriptor Pointer */
#else
	uint32_t srcaddr;	/* Source Address */
	union {
		struct {
			uint16_t rbc;	/* Remind BC */
			uint16_t bcnt;
		} mode64k;
		struct {
			uint32_t bcnt;
		} mode16m;
	} bc;			/* Byte Count */
	uint32_t nextdp;	/* Next Descriptor Pointer */
	uint32_t dstaddr;	/* Destination Address */
#endif
} __packed;

#define GTIDMAC_DESC_BYTECOUNT_MASK	0x00ffffff

struct mvxore_desc {
	uint32_t stat;				/* Status */
	uint32_t result;			/* CRC-32 Result */
	uint32_t cmd;				/* Command */
	uint32_t nextda;			/* Next Descriptor Address */
	uint32_t bcnt;				/* Byte Count */
	uint32_t dstaddr;			/* Destination Address */
	uint32_t srcaddr[MVXORE_NSRC];	/* Source Address #0-7 */
	uint32_t reserved[2];
} __packed;

#define MVXORE_DESC_STAT_SUCCESS	(1 << 30)
#define MVXORE_DESC_STAT_OWN		(1 << 31)

#define MVXORE_DESC_CMD_SRCCMD(s)	(1 << (s))
#define MVXORE_DESC_CMD_CRCLAST		(1 << 30) /* Indicate last desc CRC32 */
#define MVXORE_DESC_CMD_EODINTEN	(1 << 31) /* End of Desc Intr Enable */

#define MVXORE_DESC_BCNT_MASK	0x00ffffff

#endif	/* _GTIDMACREG_H_ */
