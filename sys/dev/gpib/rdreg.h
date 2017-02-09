/*	$NetBSD: rdreg.h,v 1.6 2011/02/08 20:20:27 rmind Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Utah $Hdr: rdreg.h 1.2 90/10/12$
 *
 *	@(#)rdreg.h	8.1 (Berkeley) 6/10/93
 */

struct	rd_iocmd {
	u_int8_t	c_pad;		/* alignment */
	u_int8_t	c_unit;		/* punit */
	u_int8_t	c_volume;	/* CS80CMD_SVOL(0) */
	u_int8_t	c_saddr;	/* CS80CMD_SADDR */
	u_int16_t	c_hiaddr;	/* always 0 */
	u_int32_t	c_addr;		/* blkno */
	u_int8_t	c_nop2;		/* CS80CMD_NOP - 32-bit alignment */
	u_int8_t	c_slen;		/* CS80CMD_SLEN */
	u_int32_t	c_len;		/* number of sectors */
	u_int8_t	c_cmd;		/* CS80CMD_READ/CS80CMD_WRITE */
} __packed;

struct	rd_rscmd {		/* different */
	u_int8_t	c_unit;
	u_int8_t	c_sram;
	u_int8_t	c_ram;
	u_int8_t	c_cmd;
} __packed;

/* HW ids */
#define	RD7946AID	0x220	/* also 7945A */
#define	RD9134DID	0x221	/* also 9122S */
#define	RD9134LID	0x222	/* also 9122D */
#define	RD7912PID	0x209
#define RD7914CTID	0x20A
#define	RD7914PID	0x20B
#define	RD7958AID	0x22B
#define RD7957AID	0x22A
#define	RD7933HID	0x212
#define	RD7936HID	0x213	/* just guessing -- as of yet unknown */
#define	RD7937HID	0x214
#define RD7957BID	0x22C	/* another guess based on 7958B */
#define RD7958BID	0x22D
#define RD7959BID	0x22E	/* another guess based on 7958B */
#define RD2200AID	0x22F
#define RD2203AID	0x230	/* yet another guess */

/* SW ids -- indicies into rdidentinfo, order is arbitrary */
#define	RD7945A		0
#define	RD9134D		1
#define	RD9122S		2
#define	RD7912P		3
#define	RD7914P		4
#define	RD7958A		5
#define RD7957A		6
#define	RD7933H		7
#define	RD9134L		8
#define	RD7936H		9
#define	RD7937H		10
#define RD7914CT	11
#define RD7946A		12
#define RD9122D		13
#define RD7957B		14
#define RD7958B		15
#define RD7959B		16

#define	NRD7945ABPT	16
#define	NRD7945ATRK	7
#define	NRD9134DBPT	16
#define	NRD9134DTRK	6
#define	NRD9122SBPT	8
#define	NRD9122STRK	2
#define	NRD7912PBPT	32
#define	NRD7912PTRK	7
#define	NRD7914PBPT	32
#define	NRD7914PTRK	7
#define	NRD7933HBPT	46
#define	NRD7933HTRK	13
#define	NRD9134LBPT	16
#define	NRD9134LTRK	5

/*
 * Several HP drives have an odd number of 256 byte sectors per track.
 * This makes it rather difficult to break them into 512 and 1024 byte blocks.
 * So...we just do like HPUX and don't bother to respect hardware track/head
 * boundaries -- we just mold the disk so that we use the entire capacity.
 * HPUX also sometimes doesn't abide by cylinder boundaries, we attempt to
 * whenever possible.
 *
 * DISK		REAL (256 BPS)		HPUX (1024 BPS)		BSD (512 BPS)
 * 		SPT x HD x CYL		SPT x HD x CYL		SPT x HD x CYL
 * -----	---------------		---------------		--------------
 * 7936:	123 x  7 x 1396		 25 x  7 x 1716		123 x  7 x  698
 * 7937:	123 x 13 x 1396		 25 x 16 x 1395		123 x 13 x  698
 *
 * 7957A:	 63 x  5 x 1013		 11 x  7 x 1036		 22 x  7 x 1036
 * 7958A:	 63 x  8 x 1013		 21 x  6 x 1013		 36 x  7 x 1013
 *
 * 7957B:	 63 x  4 x 1269		  9 x  7 x 1269		 18 x  7 x 1269
 * 7958B:	 63 x  6 x 1572		 21 x  9 x  786		 42 x  9 x  786
 * 7959B:	 63 x 12 x 1572		 21 x  9 x 1572		 42 x  9 x 1572
 *
 * 2200A:	113 x  8 x 1449		113 x  2 x 1449		113 x  4 x 1449
 * 2203A:	113 x 16 x 1449		113 x  4 x 1449		113 x  8 x 1449
 */
#define	NRD7936HBPT	123
#define	NRD7936HTRK	7
#define	NRD7937HBPT	123
#define	NRD7937HTRK	13
#define	NRD7957ABPT	22
#define	NRD7957ATRK	7
#define	NRD7958ABPT	36
#define	NRD7958ATRK	7
#define	NRD7957BBPT	18
#define	NRD7957BTRK	7
#define	NRD7958BBPT	42
#define	NRD7958BTRK	9
#define	NRD7959BBPT	42
#define	NRD7959BTRK	9
#define	NRD2200ABPT	113
#define	NRD2200ATRK	4
#define	NRD2203ABPT	113
#define	NRD2203ATRK	8

/* convert 512 byte count into DEV_BSIZE count */
#define RDSZ(x)		((x) >> (DEV_BSHIFT-9))

/* convert block number into sector number and back */
#define	RDBTOS(x)	((x) << (DEV_BSHIFT-8))
#define RDSTOB(x)	((x) >> (DEV_BSHIFT-8))

/* extract cyl/head/sect info from three-vector address */
#define RDCYL(tva)	((u_int32_t)(tva).cu_cyhd >> 8)
#define RDHEAD(tva)	((tva).cu_cyhd & 0xFF)
#define RDSECT(tva)	((tva).cu_sect)
