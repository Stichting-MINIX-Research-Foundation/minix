/*	$NetBSD: wd33c93reg.h,v 1.4 2009/02/12 06:24:45 rumble Exp $	*/

/*
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Van Jacobson of Lawrence Berkeley Laboratory.
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
 *  @(#)scsireg.h   7.3 (Berkeley) 2/5/91
 */

/*
 * Copyright (c) 2001 Wayne Knowles
 *
 * This code is derived from software contributed to Berkeley by
 * Van Jacobson of Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *  This product includes software developed by the University of
 *  California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *  @(#)scsireg.h   7.3 (Berkeley) 2/5/91
 */

/*
 * WD33C93 SCSI interface hardware description.
 *
 * Using parts of the Mach scsi driver for the 33C93
 */

#define SBIC_myid	0
#define SBIC_cdbsize	0
#define SBIC_control	1
#define SBIC_timeo	2
#define SBIC_cdb1	3
#define SBIC_tsecs	3
#define SBIC_cdb2	4
#define SBIC_theads	4
#define SBIC_cdb3	5
#define SBIC_tcyl_hi	5
#define SBIC_cdb4	6
#define SBIC_tcyl_lo	6
#define SBIC_cdb5	7
#define SBIC_addr_hi	7
#define SBIC_cdb6	8
#define SBIC_addr_2	8
#define SBIC_cdb7	9
#define SBIC_addr_3	9
#define SBIC_cdb8	10
#define SBIC_addr_lo	10
#define SBIC_cdb9	11
#define SBIC_secno	11
#define SBIC_cdb10	12
#define SBIC_headno	12
#define SBIC_cdb11	13
#define SBIC_cylno_hi	13
#define SBIC_cdb12	14
#define SBIC_cylno_lo	14
#define SBIC_tlun	15
#define SBIC_cmd_phase	16
#define SBIC_syn	17
#define SBIC_count_hi	18
#define SBIC_count_med	19
#define SBIC_count_lo	20
#define SBIC_selid	21
#define SBIC_rselid	22
#define SBIC_csr	23
#define SBIC_cmd	24
#define SBIC_data	25
#define SBIC_queue_tag	26
#define	SBIC_aux_status	27

/* wd33c93_asr is addressed directly */

/*
 *  Register defines
 */

/*
 * Auxiliary Status Register
 */

#define SBIC_ASR_INT		0x80	/* Interrupt pending */
#define SBIC_ASR_LCI		0x40	/* Last command ignored */
#define SBIC_ASR_BSY		0x20	/* Busy, only cmd/data/asr readable */
#define SBIC_ASR_CIP		0x10	/* Busy, cmd unavail also */
#define SBIC_ASR_xxx		0x0c
#define SBIC_ASR_PE		0x02	/* Parity error (even) */
#define SBIC_ASR_DBR		0x01	/* Data Buffer Ready */

/*
 * My ID register, and/or CDB Size
 */

#define SBIC_ID_FS_8_10		0x00	/* Input clock is  8-10 MHz */
					/* 11 MHz is invalid */
#define SBIC_ID_FS_12_15	0x40	/* Input clock is 12-15 MHz */
#define SBIC_ID_FS_16_20	0x80	/* Input clock is 16-20 MHz */
#define SBIC_ID_RAF		0x20	/* Enable Really Advanced Features */
#define SBIC_ID_EHP		0x10	/* Enable host parity */
#define SBIC_ID_EAF		0x08	/* Enable Advanced Features */
#define SBIC_ID_MASK		0x07
#define SBIC_ID_CBDSIZE_MASK	0x0f	/* if unk SCSI cmd group */

/*
 * Control register
 */

#define SBIC_CTL_DMA		0x80	/* Single byte dma */
#define SBIC_CTL_DBA_DMA	0x40	/* direct buffer access (bus master) */
#define SBIC_CTL_BURST_DMA	0x20	/* continuous mode (8237) */
#define SBIC_CTL_NO_DMA		0x00	/* Programmed I/O */
#define SBIC_CTL_HHP		0x10	/* Halt on host parity error */
#define SBIC_CTL_EDI		0x08	/* Ending disconnect interrupt */
#define SBIC_CTL_IDI		0x04	/* Intermediate disconnect interrupt*/
#define SBIC_CTL_HA		0x02	/* Halt on ATN */
#define SBIC_CTL_HSP		0x01	/* Halt on SCSI parity error */

/*
 * Timeout period register
 * [val in msecs, input clk in 0.1 MHz]
 */

#define SBIC_TIMEOUT(val,clk)	((((val) * (clk)) / 800) + 1)

/*
 * CDBn registers, note that
 *  cdb11 is used for status byte in target mode (send-status-and-cc)
 *  cdb12 sez if linked command complete, and w/flag if so
 */

/*
 * Target LUN register
 * [holds target status when select-and-xfer]
 */

#define SBIC_TLUN_VALID		0x80	/* did we receive an Identify msg */
#define SBIC_TLUN_DOK		0x40	/* Disconnect OK */
#define SBIC_TLUN_xxx		0x38
#define SBIC_TLUN_MASK		0x07

/*
 * Command Phase register
 */

#define SBIC_CPH_MASK		0x7f	/* values/restarts are cmd specific */
#define SBIC_CPH(p)		((p) & SBIC_CPH_MASK)

/*
 * FIFO register
 */

#define SBIC_FIFO_93_DEPTH		5
#define SBIC_FIFO_93AB_DEPTH		12

/*
 * maximum possible size in TC registers. Since this is 24 bit, it's easy
 */
#define SBIC_TC_MAX		((1 << 24) - 1)

/*
 * Synchronous xfer register
 *
 * NB: SBIC_SYN_FSS only valid on WD33C93B with 16-20MHz clock.
 */

#define SBIC_SYN_OFF_MASK	0x0f
#define SBIC_SYN_93_MAX_OFFSET	(SBIC_FIFO_93_DEPTH - 1) /* 4 is recommended */
#define SBIC_SYN_93AB_MAX_OFFSET SBIC_FIFO_93AB_DEPTH
#define SBIC_SYN_PER_MASK	0x70
#define SBIC_SYN_MIN_PERIOD	2	/* upto 8, encoded as 0 */
#define SBIC_SYN_FSS		0x80	/* Enable Fast SCSI Transfers (10MB/s)*/

#define SBIC_SYN(o,p,f) \
    (((o) & SBIC_SYN_OFF_MASK) | (((p) << 4) & SBIC_SYN_PER_MASK) | \
     ((f) ? SBIC_SYN_FSS : 0))

/*
 * Transfer count register
 * optimal access macros depend on addressing
 */

/*
 * Destination ID (selid) register
 */

#define SBIC_SID_SCC		0x80	/* Select command chaining (tgt) */
#define SBIC_SID_DPD		0x40	/* Data phase direction (inittor) */
#define SBIC_SID_FROM_SCSI	0x40
#define SBIC_SID_TO_SCSI	0x00
#define SBIC_SID_xxx		0x38
#define SBIC_SID_IDMASK		0x07

/*
 * Source ID (rselid) register
 */

#define SBIC_RID_ER		0x80	/* Enable reselection */
#define SBIC_RID_ES		0x40	/* Enable selection */
#define SBIC_RID_DSP		0x20	/* Disable select parity */
#define SBIC_RID_SIV		0x08	/* Source ID valid */
#define SBIC_RID_MASK		0x07

/*
 * Status register
 */

#define SBIC_CSR_CAUSE		0xf0
#define SBIC_CSR_RESET		0x00	/* chip was reset */
#define SBIC_CSR_CMD_DONE	0x10	/* cmd completed */
#define SBIC_CSR_CMD_STOPPED	0x20	/* interrupted or abrted*/
#define SBIC_CSR_CMD_ERR	0x40	/* end with error */
#define SBIC_CSR_BUS_SERVICE	0x80	/* REQ pending on the bus */


#define SBIC_CSR_QUALIFIER	0x0f
/* Reset State Interrupts */
#define SBIC_CSR_RESET		0x00	/* reset w/advanced features*/
#define SBIC_CSR_RESET_AM	0x01	/* reset w/advanced features*/
/* Successful Completion Interrupts */
#define SBIC_CSR_TARGET		0x10	/* reselect complete */
#define SBIC_CSR_INITIATOR	0x11	/* select complete */
#define SBIC_CSR_WO_ATN		0x13	/* tgt mode completion */
#define SBIC_CSR_W_ATN		0x14	/* ditto */
#define SBIC_CSR_XLATED		0x15	/* translate address cmd */
#define SBIC_CSR_S_XFERRED	0x16	/* initiator mode completion*/
#define SBIC_CSR_XFERRED	0x18	/* phase in low bits */
/* Paused or Aborted Interrupts */
#define SBIC_CSR_MSGIN_W_ACK	0x20	/* (I) msgin, ACK asserted*/
#define SBIC_CSR_SDP		0x21	/* (I) SDP msg received */
#define SBIC_CSR_SEL_ABRT	0x22	/* sel/resel aborted */
#define SBIC_CSR_XFR_PAUSED	0x23	/* (T) no ATN */
#define SBIC_CSR_XFR_PAUSED_ATN	0x24	/* (T) ATN is asserted */
#define SBIC_CSR_RSLT_AM	0x27	/* (I) lost selection (AM) */
#define SBIC_CSR_MIS		0x28	/* (I) xfer aborted, ph mis */
/* Terminated Interrupts */
#define SBIC_CSR_CMD_INVALID	0x40
#define SBIC_CSR_DISC		0x41	/* (I) tgt disconnected */
#define SBIC_CSR_SEL_TIMEO	0x42
#define SBIC_CSR_PE		0x43	/* parity error */
#define SBIC_CSR_PE_ATN		0x44	/* ditto, ATN is asserted */
#define SBIC_CSR_XLATE_TOOBIG	0x45
#define SBIC_CSR_RSLT_NOAM	0x46	/* (I) lost sel, no AM mode */
#define SBIC_CSR_BAD_STATUS	0x47	/* status byte was nok */
#define SBIC_CSR_MIS_1		0x48	/* ph mis, see low bits */
/* Service Required Interrupts */
#define SBIC_CSR_RSLT_NI	0x80	/* reselected, no ify msg */
#define SBIC_CSR_RSLT_IFY	0x81	/* ditto, AM mode, got ify */
#define SBIC_CSR_SLT		0x82	/* selected, no ATN */
#define SBIC_CSR_SLT_ATN	0x83	/* selected with ATN */
#define SBIC_CSR_ATN		0x84	/* (T) ATN asserted */
#define SBIC_CSR_DISC_1		0x85	/* (I) bus is free */
#define SBIC_CSR_UNK_GROUP	0x87	/* strange CDB1 */
#define SBIC_CSR_MIS_2		0x88	/* (I) ph mis, see low bits */

#define SBIC_PHASE(csr)		SCSI_PHASE(csr)

/*
 * Command register (command codes)
 */

#define SBIC_CMD_SBT		0x80	/* Single byte xfer qualifier */
#define SBIC_CMD_MASK		0x7f

		    /* Miscellaneous */
#define SBIC_CMD_RESET		0x00	/* (DTI) lev I */
#define SBIC_CMD_ABORT		0x01	/* (DTI) lev I */
#define SBIC_CMD_DISC		0x04	/* ( TI) lev I */
#define SBIC_CMD_SSCC		0x0d	/* ( TI) lev I */
#define SBIC_CMD_SET_IDI	0x0f	/* (DTI) lev I */
#define SBIC_CMD_XLATE		0x18	/* (DT ) lev II */

		    /* Initiator state */
#define SBIC_CMD_SET_ATN	0x02	/* (  I) lev I */
#define SBIC_CMD_CLR_ACK	0x03	/* (  I) lev I */
#define SBIC_CMD_XFER_PAD	0x19	/* (  I) lev II */
#define SBIC_CMD_XFER_INFO	0x20	/* (  I) lev II */

		    /* Target state */
#define SBIC_CMD_SND_DISC	0x0e	/* ( T ) lev II */
#define SBIC_CMD_RCV_CMD	0x10	/* ( T ) lev II */
#define SBIC_CMD_RCV_DATA	0x11	/* ( T ) lev II */
#define SBIC_CMD_RCV_MSG_OUT	0x12	/* ( T ) lev II */
#define SBIC_CMD_RCV		0x13	/* ( T ) lev II */
#define SBIC_CMD_SND_STATUS	0x14	/* ( T ) lev II */
#define SBIC_CMD_SND_DATA	0x15	/* ( T ) lev II */
#define SBIC_CMD_SND_MSG_IN	0x16	/* ( T ) lev II */
#define SBIC_CMD_SND		0x17	/* ( T ) lev II */

		    /* Disconnected state */
#define SBIC_CMD_RESELECT	0x05	/* (D  ) lev II */
#define SBIC_CMD_SEL_ATN	0x06	/* (D  ) lev II */
#define SBIC_CMD_SEL		0x07	/* (D  ) lev II */
#define SBIC_CMD_SEL_ATN_XFER	0x08	/* (D I) lev II */
#define SBIC_CMD_SEL_XFER	0x09	/* (D I) lev II */
#define SBIC_CMD_RESELECT_RECV	0x0a	/* (DT ) lev II */
#define SBIC_CMD_RESELECT_SEND	0x0b	/* (DT ) lev II */
#define SBIC_CMD_WAIT_SEL_RECV	0x0c	/* (DT ) lev II */


#define PHASE_MASK		0x07	/* mask for psns/pctl phase */
#define DATA_OUT_PHASE		0x00
#define DATA_IN_PHASE		0x01
#define CMD_PHASE		0x02
#define STATUS_PHASE		0x03
#define BUS_FREE_PHASE		0x04
#define ARB_SEL_PHASE		0x05	/* Fuji chip combines bus arb with sel. */
#define MESG_OUT_PHASE		0x06
#define MESG_IN_PHASE		0x07

#define SCSI_PHASE(reg)		((reg) & PHASE_MASK)

#define SCSI_STATUS_MASK	0x3e	/* Mask unused bits in status byte */

/* approximate, but we won't do SBT on selects */
#define wd33c93_isa_select(cmd)	   (((cmd) > 0x5) && ((cmd) < 0xa))

#define PAD(n)	char n;
#define SBIC_MACHINE_DMA_MODE	SBIC_CTL_DMA

/*
 * WD33C93 has two registers:
 *    ASR  - r : Aux Status Register, w : desired register no
 *    DATA - rw: register value
 *
 * We access them via separate handles because some people *cough*SGI*cough*
 * like to keep them apart.
 */

#define wd33c93_read_reg(sc,regno,val)					\
    do {								\
	bus_space_write_1((sc)->sc_regt,(sc)->sc_asr_regh, 0, (regno)); \
	(val) = bus_space_read_1((sc)->sc_regt,(sc)->sc_data_regh, 0);	\
    } while (0)

#define wd33c93_write_reg(sc,regno,val)					 \
    do {								 \
	bus_space_write_1((sc)->sc_regt, (sc)->sc_asr_regh, 0, (regno)); \
	bus_space_write_1((sc)->sc_regt, (sc)->sc_data_regh, 0,  (val)); \
    } while (0)

#define SET_SBIC_myid(sc,val)		wd33c93_write_reg(sc,SBIC_myid,val)
#define GET_SBIC_myid(sc,val)		wd33c93_read_reg(sc,SBIC_myid,val)
#define SET_SBIC_cdbsize(sc,val)	wd33c93_write_reg(sc,SBIC_cdbsize,val)
#define GET_SBIC_cdbsize(sc,val)	wd33c93_read_reg(sc,SBIC_cdbsize,val)
#define SET_SBIC_control(sc,val)	wd33c93_write_reg(sc,SBIC_control,val)
#define GET_SBIC_control(sc,val)	wd33c93_read_reg(sc,SBIC_control,val)
#define SET_SBIC_timeo(sc,val)		wd33c93_write_reg(sc,SBIC_timeo,val)
#define GET_SBIC_timeo(sc,val)		wd33c93_read_reg(sc,SBIC_timeo,val)
#define SET_SBIC_cdb1(sc,val)		wd33c93_write_reg(sc,SBIC_cdb1,val)
#define GET_SBIC_cdb1(sc,val)		wd33c93_read_reg(sc,SBIC_cdb1,val)
#define SET_SBIC_cdb2(sc,val)		wd33c93_write_reg(sc,SBIC_cdb2,val)
#define GET_SBIC_cdb2(sc,val)		wd33c93_read_reg(sc,SBIC_cdb2,val)
#define SET_SBIC_cdb3(sc,val)		wd33c93_write_reg(sc,SBIC_cdb3,val)
#define GET_SBIC_cdb3(sc,val)		wd33c93_read_reg(sc,SBIC_cdb3,val)
#define SET_SBIC_cdb4(sc,val)		wd33c93_write_reg(sc,SBIC_cdb4,val)
#define GET_SBIC_cdb4(sc,val)		wd33c93_read_reg(sc,SBIC_cdb4,val)
#define SET_SBIC_cdb5(sc,val)		wd33c93_write_reg(sc,SBIC_cdb5,val)
#define GET_SBIC_cdb5(sc,val)		wd33c93_read_reg(sc,SBIC_cdb5,val)
#define SET_SBIC_cdb6(sc,val)		wd33c93_write_reg(sc,SBIC_cdb6,val)
#define GET_SBIC_cdb6(sc,val)		wd33c93_read_reg(sc,SBIC_cdb6,val)
#define SET_SBIC_cdb7(sc,val)		wd33c93_write_reg(sc,SBIC_cdb7,val)
#define GET_SBIC_cdb7(sc,val)		wd33c93_read_reg(sc,SBIC_cdb7,val)
#define SET_SBIC_cdb8(sc,val)		wd33c93_write_reg(sc,SBIC_cdb8,val)
#define GET_SBIC_cdb8(sc,val)		wd33c93_read_reg(sc,SBIC_cdb8,val)
#define SET_SBIC_cdb9(sc,val)		wd33c93_write_reg(sc,SBIC_cdb9,val)
#define GET_SBIC_cdb9(sc,val)		wd33c93_read_reg(sc,SBIC_cdb9,val)
#define SET_SBIC_cdb10(sc,val)		wd33c93_write_reg(sc,SBIC_cdb10,val)
#define GET_SBIC_cdb10(sc,val)		wd33c93_read_reg(sc,SBIC_cdb10,val)
#define SET_SBIC_cdb11(sc,val)		wd33c93_write_reg(sc,SBIC_cdb11,val)
#define GET_SBIC_cdb11(sc,val)		wd33c93_read_reg(sc,SBIC_cdb11,val)
#define SET_SBIC_cdb12(sc,val)		wd33c93_write_reg(sc,SBIC_cdb12,val)
#define GET_SBIC_cdb12(sc,val)		wd33c93_read_reg(sc,SBIC_cdb12,val)
#define SET_SBIC_tlun(sc,val)		wd33c93_write_reg(sc,SBIC_tlun,val)
#define GET_SBIC_tlun(sc,val)		wd33c93_read_reg(sc,SBIC_tlun,val)
#define SET_SBIC_cmd_phase(sc,val)	wd33c93_write_reg(sc,SBIC_cmd_phase,val)
#define GET_SBIC_cmd_phase(sc,val)	wd33c93_read_reg(sc,SBIC_cmd_phase,val)
#define SET_SBIC_syn(sc,val)		wd33c93_write_reg(sc,SBIC_syn,val)
#define GET_SBIC_syn(sc,val)		wd33c93_read_reg(sc,SBIC_syn,val)
#define SET_SBIC_count_hi(sc,val)	wd33c93_write_reg(sc,SBIC_count_hi,val)
#define GET_SBIC_count_hi(sc,val)	wd33c93_read_reg(sc,SBIC_count_hi,val)
#define SET_SBIC_count_med(sc,val)	wd33c93_write_reg(sc,SBIC_count_med,val)
#define GET_SBIC_count_med(sc,val)	wd33c93_read_reg(sc,SBIC_count_med,val)
#define SET_SBIC_count_lo(sc,val)	wd33c93_write_reg(sc,SBIC_count_lo,val)
#define GET_SBIC_count_lo(sc,val)	wd33c93_read_reg(sc,SBIC_count_lo,val)
#define SET_SBIC_selid(sc,val)		wd33c93_write_reg(sc,SBIC_selid,val)
#define GET_SBIC_selid(sc,val)		wd33c93_read_reg(sc,SBIC_selid,val)
#define SET_SBIC_rselid(sc,val)		wd33c93_write_reg(sc,SBIC_rselid,val)
#define GET_SBIC_rselid(sc,val)		wd33c93_read_reg(sc,SBIC_rselid,val)
#define SET_SBIC_csr(sc,val)		wd33c93_write_reg(sc,SBIC_csr,val)
#define GET_SBIC_csr(sc,val)		wd33c93_read_reg(sc,SBIC_csr,val)
#define SET_SBIC_cmd(sc,val)		wd33c93_write_reg(sc,SBIC_cmd,val)
#define GET_SBIC_cmd(sc,val)		wd33c93_read_reg(sc,SBIC_cmd,val)
#define SET_SBIC_data(sc,val)		wd33c93_write_reg(sc,SBIC_data,val)
#define GET_SBIC_data(sc,val)		wd33c93_read_reg(sc,SBIC_data,val)
#define SET_SBIC_queue_tag(sc,val)	wd33c93_write_reg(sc,SBIC_queue_tag,val)
#define GET_SBIC_queue_tag(sc,val)	wd33c93_read_reg(sc,SBIC_queue_tag,val)

#define SBIC_TC_PUT(sc,val)						\
    do {								\
	wd33c93_write_reg(sc,SBIC_count_hi,((val)>>16));		\
	bus_space_write_1((sc)->sc_regt, (sc)->sc_data_regh, 0,		\
			  (val)>>8); 					\
	bus_space_write_1((sc)->sc_regt, (sc)->sc_data_regh, 0,		\
			  (val)); 					\
    } while (0)

#define SBIC_TC_GET(sc,val)						\
    do {								\
	wd33c93_read_reg(sc,SBIC_count_hi,(val));			\
	(val) = ((val)<<8) | bus_space_read_1((sc)->sc_regt,		\
				(sc)->sc_data_regh, 0);			\
	(val) = ((val)<<8) | bus_space_read_1((sc)->sc_regt,		\
				(sc)->sc_data_regh, 0);			\
    } while (0)

#define SBIC_LOAD_COMMAND(sc,cmd,cmdsize)				\
    do {								\
	int   n   = (cmdsize) - 1;					\
	char *ptr = (char *)(cmd);					\
	wd33c93_write_reg(regs, SBIC_cdb1, *ptr++);			\
	while(n-- > 0)							\
		bus_space_write_1((sc)->sc_regt, (sc)->sc_data_regh,	\
			  0, *ptr++); /* XXX write_multi */		\
    } while (0)

#define GET_SBIC_asr(sc,val)						\
    do {								\
	(val) = bus_space_read_1((sc)->sc_regt,(sc)->sc_asr_regh, 0);	\
    } while (0)


#define WAIT_CIP(sc)							\
    do {								\
	while (bus_space_read_1((sc)->sc_regt,(sc)->sc_asr_regh,	\
			0) & SBIC_ASR_CIP) 				\
		/*nop*/;						\
    } while (0)

/*
 * transmit a byte in programmed I/O mode
 */
#define SEND_BYTE(sc, ch)						\
    do {								\
	WAIT_CIP(sc);							\
	SET_SBIC_cmd(sc, SBIC_CMD_SBT | SBIC_CMD_XFER_INFO);		\
	SBIC_WAIT(sc, SBIC_ASR_DBR, 0);					\
	SET_SBIC_data(sc, ch);						\
    } while (0)

/*
 * receive a byte in programmed I/O mode
 */
#define RECV_BYTE(sc, ch)						\
    do {								\
	WAIT_CIP(sc);							\
	SET_SBIC_cmd(sc, SBIC_CMD_SBT | SBIC_CMD_XFER_INFO);		\
	SBIC_WAIT(sc, SBIC_ASR_DBR, 0);					\
	GET_SBIC_data(sc, ch);						\
    } while (0)
