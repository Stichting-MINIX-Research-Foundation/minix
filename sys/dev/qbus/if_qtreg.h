/*	$NetBSD: if_qtreg.h,v 1.5 2005/12/11 12:23:29 christos Exp $	*/
/*
 * Copyright (c) 1992 Steven M. Schultz
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 *	@(#)if_qtreg.h  1.0 (GTE) 10/12/92
 */
/*
 * Modification History
 *  26 Feb 93 -- sms
 *	Add defines for number of receive and transmit ring descriptors.
 *
 *  12 Oct 92 -- Steven M. Schultz (sms)
 *	Created from the DELQA-PLUS Addendum to the DELQA User's Guide.
*/

#define QT_MAX_RCV	32
#define QT_MAX_XMT	12

/* Receive ring descriptor and bit/field definitions */

	struct	qt_rring
		{
		short	rmd0;
		short	rmd1;
		short	rmd2;
		short	rmd3;
		short	rmd4;
		short	rmd5;
#ifdef pdp11
		struct	qt_uba	*rhost0;
		short	rhost1;
#else
		short	pad1, pad2;
#endif
		};

#define	RMD0_ERR3	0x4000		/* Error summary. FRA|CRC|OFL|BUF */
#define	RMD0_FRA	0x2000		/* Framing error */
#define	RMD0_OFL	0x1000		/* Overflow error.  Oversized packet */
#define	RMD0_CRC	0x0800		/* CRC error */
#define	RMD0_BUF	0x0400		/* Internal device buffer error */
#define	RMD0_STP	0x0200		/* Start of packet */
#define	RMD0_ENP	0x0100		/* End of packet */

#define	RMD1_MCNT	0x0fff		/* Message byte count */

#define	RMD2_ERR4	0x8000		/* Error summary.  BBL|CER|MIS */
#define	RMD2_BBL	0x4000		/* Babble error on transmit */
#define	RMD2_CER	0x2000		/* Collision error on transmit */
#define	RMD2_MIS	0x1000		/* Packet lost on receive */
#define	RMD2_EOR	0x0800		/* End of receive ring */
#define	RMD2_RON	0x0020		/* Receiver on */
#define	RMD2_TON	0x0010		/* Transmitter on */

#define	RMD3_OWN	0x8000		/* Ownership field. */

#define	RMD4_LADR	0xfff8		/* Octabyte aligned low address bits */

#define	RMD5_HADR	0x003f		/* High 6 bits of buffer address */

#define	RMD0_BITS	"\010\016FRA\015OFL\014CRC\013BUF\012STP\011ENP"
#define	RMD2_BITS	"\010\017BBL\014CER\013MIS\012EOR\06RON\05TON"

/* Transmit ring descriptor and bit/field definitions */

	struct	qt_tring
		{
		short	tmd0;
		short	tmd1;
		short	tmd2;
		short	tmd3;
		short	tmd4;
		short	tmd5;
#ifdef pdp11
		struct	qt_uba	*thost0;
		short	thost1;
#else
		short	pad1, pad2;
#endif
		};

#define	TMD0_ERR1	0x4000		/* Error summary.  LCO|LCA|RTR */
#define	TMD0_MOR	0x1000		/* More than one retry on transmit */
#define	TMD0_ONE	0x0800		/* One retry on transmit */
#define	TMD0_DEF	0x0400		/* Deferral during transmit */

#define	TMD1_LCO	0x1000		/* Late collision on transmit */
#define	TMD1_LCA	0x0800		/* Loss of carrier on transmit */
#define	TMD1_RTR	0x0400		/* Retry error on transmit */
#define	TMD1_TDR	0x03ff		/* Time Domain Reflectometry value */

#define	TMD2_ERR2	0x8000		/* Error summary.  BBL|CER|MIS */
#define	TMD2_BBL	0x4000		/* Babble error on transmit */
#define	TMD2_CER	0x2000		/* Collision error on transmit */
#define	TMD2_MIS	0x1000		/* Packet lost on receive */
#define	TMD2_EOR	0x0800		/* Endof Receive ring reached */
#define	TMD2_RON	0x0020		/* Receiver on */
#define	TMD2_TON	0x0010		/* Transmitter on */

#define	TMD3_OWN	0x8000		/* Ownership field */
#define	TMD3_FOT	0x4000		/* First of two flag */
#define	TMD3_BCT	0x0fff		/* Byte count */

#define	TMD4_LADR	0xfff8		/* Octabyte aligned low address bits */

#define	TMD5_HADR	0x003f		/* High 6 bits of buffer address */

#define	TMD1_BITS	"\010\015LCO\014LCA\013RTR"
#define	TMD2_BITS	"\010\017BBL\016CER\015MIS\014EOR\06RON\05TON"

/* DELQA-YM CSR layout */

#ifdef notdef
	struct	qtcsr0
		{
		short	Ibal;
		short	Ibah;
		short	Icr;
		short	pad0;
		short	Srqr;
		short	pad1;
		};

	struct	qtdevice
		{
		union	{
			u_char	Sarom[12];
			struct	qtcsr0	csr0;
			} qt_un0;
		short	srr;
		short	arqr;
		};

#define	ibal	qt_un0.csr0.Ibal
#define	ibah	qt_un0.csr0.Ibah
#define	srqr	qt_un0.csr0.Srqr
#define	icr	qt_un0.csr0.Icr
#define	sarom	qt_un0.Sarom
#endif

#define	CSR_IBAL	0
#define	CSR_IBAH	2
#define	CSR_ICR		4
#define	CSR_SRQR	8
#define	CSR_SRR		12
#define	CSR_ARQR	14


/* SRR definitions */

#define	SRR_FES		0x8000
#define	SRR_CHN		0x4000
#define	SRR_NXM		0x1000
#define	SRR_PER		0x0800
#define	SRR_IME		0x0400
#define	SRR_TBL		0x0200
#define	SRR_RESP	0x0003
#define	SRR_BITS	"\010\017CHN\015NXM\014PER\013IME\012TBL"

/* SRQR definitions */

#define	SRQR_REQ	0x0003

/* ARQR definitions */

#define	ARQR_TRQ	0x8000
#define	ARQR_RRQ	0x0080
#define	ARQR_SR		0x0002

/* define ICR definitions */

#define	ICR_CMD		0x0001

/* DELQA registers used to shift into -T mode */

#ifdef notdef
#define	xcr0	qt_un0.csr0.Ibal
#define	xcr1	qt_un0.csr0.Ibah
#endif
#define	CSR_XCR0	CSR_IBAL
#define	CSR_XCR1	CSR_IBAH

/* INIT block structure and definitions */

	struct	qt_init
		{
		short	mode;
		u_char	paddr[6];	/* 48 bit physical address */
		u_char	laddr[8];	/* 64 bit logical address filter */
		u_short	rx_lo;		/* low 16 bits of receive ring addr */
		u_short	rx_hi;		/* high 6 bits of receive ring addr */
		u_short	tx_lo;		/* low 16 bits of transmit ring addr */
		u_short	tx_hi;		/* high 6 bits of transmit ring addr */
		u_short	options;
		u_short	vector;
		u_short	hit;
		char	passwd[6];
		char	pad[4];		/* even on 40 byte for alignment */
		};

#define	INIT_MODE_PRO	0x8000		/* Promiscuous mode */
#define	INIT_MODE_INT	0x0040		/* Internal Loopback */
#define	INIT_MODE_DRT	0x0020		/* Disable Retry */
#define	INIT_MODE_DTC	0x0008		/* Disable Transmit CRC */
#define	INIT_MODE_LOP	0x0004		/* Loopback */

#define	INIT_OPTIONS_HIT 0x0002		/* Host Inactivity Timeout Flag */
#define	INIT_OPTIONS_INT 0x0001		/* Interrupt Enable Flag */
