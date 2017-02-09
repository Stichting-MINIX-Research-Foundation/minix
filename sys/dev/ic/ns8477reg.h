/*	$NetBSD: ns8477reg.h,v 1.3 2008/04/28 20:23:50 martin Exp $	 */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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
 * Register descriptions of the National Semiconductor PC8477B
 * floppy controller
 */

#define	FDC_SRA	0	/* (R)   Status Register A		*/

#define FDC_SRB	1	/* (R)   Status Register B		*/

#define FDC_DOR	2	/* (R/W) Digital Output Register	*/

# define FDC_DOR_SEL0	0x01	/* Drive Select 0	*/
# define FDC_DOR_SEL1	0x02	/* Drive Select 1	*/
# define FDC_DOR_RESET	0x04	/* Reset Controller	*/
# define FDC_DOR_DMAEN	0x08	/* Dma Enable		*/
# define FDC_DOR_MTR0	0x10	/* Motor Enable 0	*/
# define FDC_DOR_MTR1	0x20	/* Motor Enable 1	*/
# define FDC_DOR_MTR2	0x40	/* Motor Enable 2	*/
# define FDC_DOR_MTR3	0x80	/* Motor Enable 3	*/
# define FDC_DOR_MTR(a)  (1 << (n + 4))

#define FDC_TDR	3	/* (R/W) Tape Drive Register		*/
# define FDC_TDR_SEL0	0x01	/* Tape Select 0	*/
# define FDC_TDR_SEL1	0x02	/* Tape Select 1	*/

#define FDC_MSR	4	/* (R)   Main Status Register		*/
# define FDC_MSR_BUSY0	0x01	/* Drive 0 Busy		*/
# define FDC_MSR_BUSY1	0x02	/* Drive 1 Busy		*/
# define FDC_MSR_BUSY2	0x04	/* Drive 2 Busy		*/
# define FDC_MSR_BUSY3	0x08	/* Drive 3 Busy		*/
# define FDC_MSR_CMDPRG	0x10	/* Command In Progress	*/
# define FDC_MSR_NONDMA	0x20	/* Non DMA Execution	*/
# define FDC_MSR_DIO	0x40	/* Data I/O Direction	*/
# define FDC_MSR_RQM	0x80	/* Reguest for Master	*/

#define FDC_DSR	4	/* (W)   Data Rate Select Register	*/
# define FDC_DSR_DRATE0	0x01	/* Data Rate Select 0	*/
# define FDC_DSR_DRATE1	0x02	/* Data Rate Select 0	*/
/*
 * bit	MFM	FM
 *  00  500Kb/s	250Kb/s
 *  01  300Kb/s	150Kb/s
 *  10  250Kb/s	125Kb/s
 *  11	1Mb/s	illegal
 */
# define FDC_DSR_500KBPS	0x00	/* 500KBPS MFM drive transfer rate */
# define FDC_DSR_300KBPS	0x01	/* 300KBPS MFM drive transfer rate */
# define FDC_DSR_250KBPS	0x02	/* 250KBPS MFM drive transfer rate */
# define FDC_DSR_1MBPS		0x03	/* 1MBPS MFM drive transfer rate */

# define FDC_DSR_PREC0	0x04	/* Precompensation bit 0*/
# define FDC_DSR_PREC1	0x08	/* Precompensation bit 1*/
# define FDC_DSR_PREC2	0x10	/* Precompensation bit 2*/
/*
 * bit	Precomp Data Rate
 * 000	default
 * 001   41.7ns	1Mb/s
 * 010	 83.3ns
 * 011	125.0ns 500Kb/s, 300Kb/s, 250Kb/s
 * 100	168.7ns
 * 101	208.3ns
 * 110	208.3ns
 * 111	  0.0ns
 */
# define FDC_DSR_ZERO	0x20	/* Undef; should be 0	*/
# define FDC_DSR_LOWPWR	0x40	/* Low Power Mode	*/
# define FDC_DSR_SWRST	0x80	/* Software Reset	*/

#define FDC_FIFO	5	/* (R/W) Data Register (FIFO)		*/

/*
 * Commands
 */
#define FDC_CMD_MODE				(0x01)
#define	FDC_CMD_READ_TRACK(mfm)			(0x02|mfm)
#define	FDC_CMD_SPECIFY				(0x03)
#define	FDC_CMD_SENSE_DRIVE_STATUS		(0x04)
#define FDC_CMD_WRITE_DATA(mt,mfm)		(0x05|mt|mfm)
#define FDC_CMD_READ_DATA(mt,mfm,sk)		(0x06|mt|mfm|sk)
#define	FDC_CMD_RECALIBRATE			(0x07)
#define	FDC_CMD_SENSE_INTERRUPT			(0x08)
#define FDC_CMD_WRITE_DEL_DATA(mt,mfm)		(0x09|mt|mfm)
#define	FDC_CMD_READ_ID(mfm)			(0x0a|mfm)
#define FDC_CMD_READ_DEL_DATA(mt,mfm,sk)	(0x0c|mt|mfm|sk)
#define	FDC_CMD_FORMAT_TRACK(mfm)		(0x0d|mfm)
#define FDC_CMD_DUMPREG				(0x0e)
#define FDC_CMD_SEEK				(0x0f)
#define FDC_CMD_VERSION				(0x10)
#define FDC_CMD_SCAN_EQUAL(mt,mfm,sk)		(0x11|mt|mfm|sk)
#define FDC_CMD_PERPENDICULAR			(0x12)
#define FDC_CMD_CONFIGURE			(0x13)
#define FDC_CMD_LOCK(lock)			(0x14|lock)
#define FDC_CMD_VERIFY(mt,mfm,sk)		(0x16|mt|mfm|sk)
#define FDC_CMD_NSC				(0x18)
#define FDC_CMD_SCAN_LO_EQUAL(mt,mfm,sk)	(0x19|mt|mfm|sk)
#define FDC_CMD_SCAN_HI_EQUAL(mt,mfm,sk)	(0x1d|mt|mfm|sk)
#define FDC_CMD_SET_TRACK(wnr)			(0x21|wnr)
#define	FDC_CMD_REL_SEEK(dir)			(0x8f|dir)

#define  FDC_CMD_CONFIGURE_FLAGS_POLL	0x10
#define  FDC_CMD_CONFIGURE_FLAGS_FIFO	0x20
#define  FDC_CMD_CONFIGURE_FLAGS_EIS	0x40
#define  FDC_CMD_FLAGS_LOCK		0x80
#define  FDC_CMD_FLAGS_MT		0x80
#define  FDC_CMD_FLAGS_MFM		0x40
#define  FDC_CMD_FLAGS_SK		0x20
#define  FDC_CMD_FLAGS_DIR		0x40
#define  FDC_CMD_FLAGS_WNR		0x40
/*
 * Command Status
 */
/* Status register ST0 */
#define FDC_ST0BITS	"\020\010invld\007abnrml\006seek_cmplt\005drv_chck\004drive_rdy\003top_head\002ds1\001ds0"
# define FDC_ST0_DS0	0x01	/* Drive Select 0	*/
# define FDC_ST0_DS1	0x02	/* Drive Select 1	*/
# define FDC_ST0_HDS	0x04	/* Head select		*/
# define FDC_ST0_ZERO	0x08	/* Undef; should be 0	*/
# define FDC_ST0_EC	0x10	/* Equipment check	*/
# define FDC_ST0_SE	0x20	/* Seek completed	*/
# define FDC_ST0_IC0	0x40	/* Interrupt code 0	*/
# define FDC_ST0_IC1	0x80	/* Interrupt code 1	*/
# define FDC_ST0(a)	(a & ~(FDC_ST0_DS0|FDC_ST0_DS1|FDC_ST0_HDS))
#  define FDC_ST0_NRML 0x00	/* Normal Completion	*/
#  define FDC_ST0_ABNR 0x40	/* Abnormal Termination	*/
#  define FDC_ST0_INVL 0x80	/* Invalid Command	*/
#  define FDC_ST0_CHGD 0xc0	/* Drive status changed	*/

/* Status register ST1 */
#define FDC_ST1BITS	"\020\010end_of_cyl\006bad_crc\005data_overrun\003sec_not_fnd\002write_protect\001no_am"
# define FDC_ST1_MA	0x01	/* Missing address mark	*/
# define FDC_ST1_NW	0x02	/* Write Protect	*/
# define FDC_ST1_ND	0x04	/* No Data		*/
# define FDC_ST1_ZERO0	0x08	/* Undef; should be 0	*/
# define FDC_ST1_OR	0x10	/* Overrun error	*/
# define FDC_ST1_CE	0x20	/* CRC error		*/
# define FDC_ST1_ZERO1	0x40	/* Undef; should be 0	*/
# define FDC_ST1_ET	0x80	/* End of Track		*/

/* Status register ST2 */
#define FDC_ST2BITS	"\020\007ctrl_mrk\006bad_crc\005wrong_cyl\004scn_eq\003scn_not_fnd\002bad_cyl\001no_dam"
# define FDC_ST2_MD	0x01	/* Missing Address Mark */
# define FDC_ST2_BT	0x02	/* Bad Track		*/
# define FDC_ST2_SNS	0x04	/* Scan Not Satisfied	*/
# define FDC_ST2_SEH	0x08	/* Scan Equal Hit	*/
# define FDC_ST2_WT	0x10	/* Wrong Track		*/
# define FDC_ST2_CD	0x20	/* CRC Error in Data	*/
# define FDC_ST2_CM	0x40	/* Control Mark		*/
# define FDC_ST2_ZERO	0x80	/* Undef; should be 0	*/

/* Status register ST3 */
#define FDC_ST3BITS	"\020\010fault\007write_protect\006drdy\005tk0\004two_side\003side_sel\002ds1\001ds0"
# define FDC_ST3_DS0	0x01	/* Drive Select 0	*/
# define FDC_ST3_DS1	0x02	/* Drive Select 1	*/
# define FDC_ST3_HDS	0x04	/* Head Select		*/
# define FDC_ST3_ONE0	0x08	/* Undef; should be 0	*/
# define FDC_ST3_TK0	0x10	/* Track 0		*/
# define FDC_ST3_ONE1	0x20	/* Undef; should be 0	*/
# define FDC_ST3_WP	0x40	/* Write Protect	*/
# define FDC_ST3_ZERO	0x80	/* Undef; should be 0	*/

#define FDC_NONE	6	/* (X)   None (Bus TRI-STATE)	*/

#define FDC_DIR	7	/* (R)   Digital Input Register		*/
# define FDC_DIR_DSKCHG	0x80	/* Disk Changed		*/

#define FDC_CCR	7	/* (W)   Configuration Register		*/
# define FDC_CCR_DRATE0	0x01	/* Data Rate Select 0	*/
# define FDC_CCR_DRATE1	0x02	/* Data Rate Select 0	*/
# define FDC_CCR_ZERO0	0x04	/* Undef; should be 0	*/
# define FDC_CCR_ZERO1	0x08	/* Undef; should be 0	*/
# define FDC_CCR_ZERO2	0x10	/* Undef; should be 0	*/
# define FDC_CCR_ZERO3	0x20	/* Undef; should be 0	*/
# define FDC_CCR_ZERO4	0x40	/* Undef; should be 0	*/
# define FDC_CCR_ZERO5	0x80	/* Undef; should be 0	*/

#define FDC_NPORT	8
