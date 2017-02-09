/*	$NetBSD: cs80busvar.h,v 1.6 2012/10/27 17:18:16 chs Exp $	*/

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Gregory McGarry.
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

#define CS80BUS_NSLAVES		8	/* number of slaves on a bus */
#define CS80BUS_NPUNITS		2	/* number of punits per slave */

struct cs80bus_attach_args {
	gpib_chipset_tag_t ca_ic;
	u_int16_t ca_id;		/* device id */
	int	ca_slave;		/* GPIB bus slave */
	int	ca_punit;		/* physical unit on slave */
};

struct cs80bus_softc {
	device_t sc_dev;		/* generic device glue */
	gpib_chipset_tag_t sc_ic;
	u_int8_t	sc_rmap[CS80BUS_NSLAVES][CS80BUS_NPUNITS];
};


/*
 * CS80/SS80 primary commands
 */
#define	CS80CMD_SCMD		0x05	/* secondary command to follow */
#define	CS80CMD_EXEC		0x0e	/* return requested data */
#define	CS80CMD_QSTAT		0x10	/* query status of device */
#define	CS80CMD_TCMD		0x12	/* transparent message */

/*
 * CS80/SS80 secondary commands
 *
 * The arguments in < > indicate the number of parameters and number of
 * bits per parameter used in the command.
 */
#define	CS80CMD_READ		0x00	/* read sector */
#define	CS80CMD_WRITE		0x02	/* write sector */
#define	CS80CMD_CLEAR		0x08	/* clear device */
#define	CS80CMD_STATUS		0x0d	/* request status */
#define	CS80CMD_SADDR		0x10	/* set block number <16,32>*/
#define	CS80CMD_SLEN		0x18	/* set block length <8> */
#define	CS80CMD_SUNIT(x)	(0x20|(x))	/* set unit */
#define	CS80CMD_NOP		0x34	/* no-op */
#define CS80CMD_DESC		0x35	/* request device description */
#define	CS80CMD_SOPT		0x38	/* set options <8> */
#define	CS80CMD_SREL		0x3b	/* set release ? <8> */
#define	CS80CMD_SSM		0x3e	/* set status mask <16,16,16,16> */
#define CS80CMD_SVOL(x)		(0x40|(x))	/* set volume */
#define	CS80CMD_SRAM		0x48	/* set description format <8> */
#define	CS80CMD_WFM		0x49	/* write end-of-file record */
#define	CS80CMD_UNLOAD		0x4a	/* unload media */

struct cs80_describecmd {		/* describe command */
	u_int8_t c_unit;
	u_int8_t c_vol;
	u_int8_t c_cmd;
} __packed;

struct	cs80_clearcmd {			/* clear device command */
	u_int8_t	c_unit;
	u_int8_t	c_cmd;
} __packed;

struct	cs80_srcmd {			/* s? release */
	u_int8_t	c_unit;
	u_int8_t	c_nop;
	u_int8_t	c_cmd;
	u_int8_t	c_param;
} __packed;

struct	cs80_statuscmd {		/* status command */
	u_int8_t	c_unit;
	u_int8_t	c_sram;
	u_int8_t	c_param;
	u_int8_t	c_cmd;
} __packed;

struct	cs80_ssmcmd {			/* status mask */
	u_int8_t	c_unit;
	u_int8_t	c_cmd;
	u_int16_t	c_refm;		/* "request error" mask */
	u_int16_t	c_fefm;		/* "fault error" mask */
	u_int16_t	c_aefm;		/* "access error" mask */
	u_int16_t	c_iefm;		/* "info error" mask */
#define	REF_MASK	0x0
#define	FEF_MASK	0x0
#define	AEF_MASK	0x0
#define	IEF_MASK	0xF970
} __packed;

struct cs80_soptcmd {			/* set options */
	u_int8_t	c_unit;
	u_int8_t	c_nop;
	u_int8_t	c_opt;
	u_int8_t	c_param;
#define C_CC		0x01		/* character count option */
#define C_SKSPAR	0x02
#define C_SPAR		0x04
#define C_IMRPT		0x08
} __packed;




/*
 * Structures returned by functions.
 */

struct cs80_description {
	u_int16_t	d_iuw;		/* ctlr: installed unit word */
	u_int16_t	d_cmaxxfr;	/* ctlr: max transfer rate (KB) */
	u_int8_t	d_ctype;	/* ctlr: controller type */
	u_int8_t	d_utype;	/* unit: unit type */
	u_int8_t	d_name[3];	/* unit: name (6 BCD digits) */
	u_int16_t	d_sectsize;	/* unit: # of bytes per block */
	u_int8_t	d_blkbuf;	/* unit: # of blocks can be buffered */
	u_int8_t	d_burstsize;	/* unit: recommended burst size */
	u_int16_t	d_blocktime;	/* unit: block time (u-sec) */
	u_int16_t	d_uavexfr;	/* unit: average transfer rate (Kb) */
	u_int16_t	d_retry;	/* unit: retry time (1/100-sec) */
	u_int16_t	d_access;	/* unit: access time (1/100-sec) */
	u_int8_t	d_maxint;	/* unit: max interleave */
	u_int8_t	d_fvbyte;	/* unit: fixed volume byte */
	u_int8_t	d_rvbyte;	/* unit: removable volume byte */
	u_int32_t	d_maxcylhead;	/* volume: max cylinder/head */
	u_int16_t	d_maxsect;	/* volume: max sector on track */
	u_int16_t	d_maxvsecth;	/* volume: max volume block (MSW) */
	u_int32_t	d_maxvsectl;	/* volume: max volume block (LSWs) */
	u_int8_t	d_interleave;	/* volume: current interleave */
} __packed;

struct	cs80_stat {		/* device status */
	u_int8_t	c_vu;	/* volume/unit */
	u_int8_t	c_pend;
	u_int16_t	c_ref;	/* reject error */
#define REF_bit3	0x0008		/* message length */
#define REF_bit5	0x0020		/* message sequence */
#define REF_bit6	0x0040		/* illegal parameter */
#define REF_bit7	0x0080		/* parameter bounds */
#define REF_bit8	0x0100		/* address bounds */
#define REF_bit9	0x0200		/* module addressing */
#define REF_bit10	0x0400		/* illegal opcode */
#define REF_bit13	0x2000		/* channel parity error */
	u_int16_t	c_fef;	/* fault error */
#define	FEF_REXMT	0x0001		/* retransmit */
#define	FEF_PF		0x0002		/* power fail */
#define FEF_IMR		0x0008		/* internal maintenance release */
#define FEF_bit4	0x0010		/* diagnostic release request */
#define FEF_bit5	0x0020		/* operator release request */
#define FEF_DR		0x0080		/* diagnostic result */
#define FEF_bit9	0x0200		/* unit fault */
#define FEF_bit12	0x1000		/* controller fault */
#define FEF_CU		0x4000		/* cross-unit */
	u_int16_t	c_aef;	/* access error */
#define	AEF_EOV		0x0008		/* end of volume */
#define	AEF_EOF		0x0010		/* end of file */
#define AEF_UD		0x0040		/* unrecoverable data */
#define AEF_bit7	0x0080		/* unrecoverable data overflow */
#define AEF_bit10	0x0400		/* no data found */
#define AEF_bit11	0x0800		/* write protect */
#define AEF_bit12	0x1000		/* not ready */
#define AEF_bit13	0x2000		/* no spares available */
#define AEF_bit14	0x4000		/* uninitialized media */
#define AEF_bit15	0x8000		/* illegal parallel operation */
	u_int16_t	c_ief;	/* info error */
#define IEF_bit2	0x0004		/* maintenance track overflow */
#define IEF_RD		0x0010		/* recoverable data */
#define IEF_MD		0x0020		/* marginal data */
#define IEF_bit6	0x0040		/* recoverable data overflow */
#define IEF_bit8	0x0100		/* auto-sparing invoked */
#define IEF_bit9	0x0800		/* latency induced */
#define IEF_bit10	0x1000		/* media wear */
#define IEF_RRMASK	0xe000		/* request release bits */
	union {
		u_int8_t cu_raw[10];
		struct {
			u_int16_t	cu_msw;
			u_int32_t	cu_lsl;
		} cu_sva;
		struct {
			u_int32_t	cu_cyhd;
			u_int16_t	cu_sect;
		} cu_tva;
	} c_pf;
#define c_raw	c_pf.cu_raw
#define c_blk	c_pf.cu_sva.cu_lsl
#define c_tva	c_pf.cu_tva
} __packed;


int cs80describe(void *, int, int, struct cs80_description *);
int cs80reset(void *, int, int);
int cs80status(void *, int, int, struct cs80_stat *);
int cs80setoptions(void *, int, int, u_int8_t);
int cs80send(void *, int, int, int, void *, int);
