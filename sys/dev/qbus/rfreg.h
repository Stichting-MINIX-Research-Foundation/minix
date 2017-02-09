/*	$NetBSD: rfreg.h,v 1.4 2005/12/11 12:23:29 christos Exp $	*/
/*
 * Copyright (c) 2002 Jochen Kunz.
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
 * 3. The name of Jochen Kunz may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JOCHEN KUNZ
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL JOCHEN KUNZ
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */



/* Registers in Uni/QBus I/O space. */
#define	RX2CS	0	/* Command and Status Register */
#define	RX2DB	2	/* Data Buffer Register */
/* RX2DB is depending on context: */
#define	RX2BA	2	/* Bus Address Register */
#define	RX2TA	2	/* Track Address Register */
#define	RX2SA	2	/* Sector Address Register */
#define	RX2WC	2	/* Word Count Register */
#define	RX2ES	2	/* Error and Status Register */


/* Bitdefinitions of CSR. */
#define	RX2CS_ERR	0x8000	/* Error				RO */
#define	RX2CS_INIT	0x4000	/* Initialize				WO */
#define	RX2CS_UAEBH	0x2000	/* Unibus address extension high bit	WO */
#define	RX2CS_UAEBI	0x1000	/* Unibus address extension low bit	WO */
#define	RX2CS_RX02	0x0800	/* RX02					RO */
/*			0x0400	   Not Used				-- */
/*			0x0200	   Not Used				-- */
#define	RX2CS_DD	0x0100	/* Double Density			R/W */
#define	RX2CS_TR	0x0080	/* Transfer Request			RO */
#define	RX2CS_IE	0x0040	/* Interrupt Enable			R/W */
#define	RX2CS_DONE	0x0020	/* Done					RO */
#define	RX2CS_US	0x0010	/* Unit Select				WO */
#define	RX2CS_FCH	0x0008	/* Function Code high bit		WO */
#define	RX2CS_FCM	0x0004	/* Function Code mid bit		WO */
#define	RX2CS_FCL	0x0002	/* Function Code low bit		WO */
#define	RX2CS_GO	0x0001	/* Go					WO */
#define	RX2CS_NU	0x0600	/* not used bits			-- */


#define	RX2CS_UAEB	( RX2CS_UAEBH | RX2CS_UAEBI )
#define	RX2CS_FC	( RX2CS_FCH | RX2CS_FCM | RX2CS_FCL )


/* Commands of the controller and parameter cont. */
#define	RX2CS_FBUF	001	/* Fill Buffer, word count and bus address */
#define	RX2CS_EBUF	003	/* Empty Buffer, word count and bus address */
#define	RX2CS_WSEC	005	/* Write Sector, sector and track */
#define	RX2CS_RSEC	007	/* Read Sector, sector and track */
#define	RX2CS_SMD	011	/* Set Media Density, ??? */
#define	RX2CS_RSTAT	013	/* Read Status, no params */
#define	RX2CS_WDDS	015	/* Write Deleted Data Sector, sector and track */
#define	RX2CS_REC	017	/* Read Error Code, bus address */


/* Track Address Register */
#define	RX2TA_MASK	0x7f


/* Sector Address Register */
#define	RX2SA_MASK	0x1f


/* Word Count Register */
#define	RX2WC_MASK	0x7f


/* Bitdefinitions of RX2ES. */
/*			<15-12> Not Used		-- */
#define	RX2ES_NEM	0x0800	/* Non-Existend Memory	RO */
#define	RX2ES_WCO	0x0400	/* Word Count Overflow	RO */
/*			0x0200	   Not Used		RO */
#define	RX2ES_US	0x0010	/* Unit Select		RO */
#define	RX2ES_RDY	0x0080	/* Ready		RO */
#define	RX2ES_DEL	0x0040	/* Deleted Data		RO */
#define	RX2ES_DD	0x0020	/* Double Density	RO */
#define	RX2ES_DE	0x0010	/* Density Error	RO */
#define	RX2ES_ACL	0x0008	/* AC Lost		RO */
#define	RX2ES_ID	0x0004	/* Initialize Done	RO */
/*			0x0002	   Not Used		-- */
#define	RX2ES_CRCE	0x0001	/* CRC Error		RO */
#define	RX2ES_NU	0xF202	/* not used bits	-- */


#define	RX2_TRACKS	77	/* number of tracks */
#define	RX2_SECTORS	26	/* number of sectors / track */
#define	RX2_BYTE_SD	128	/* number of bytes / sector in single density */
#define	RX2_BYTE_DD	256	/* number of bytes / sector in double density */
#define	RX2_HEADS	1	/* number of heads */

