/*	$NetBSD: ihavar.h,v 1.13 2008/05/14 13:29:28 tsutsui Exp $ */

/*-
 * Copyright (c) 2001, 2002 Izumi Tsutsui
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
 */

/*-
 * Device driver for the INI-9XXXU/UW or INIC-940/950 PCI SCSI Controller.
 *
 *  Written for 386bsd and FreeBSD by
 *	Winston Hung		<winstonh@initio.com>
 *
 * Copyright (c) 1997-1999 Initio Corp.
 * Copyright (c) 2000 Ken Westerback
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Ported to NetBSD by Izumi Tsutsui <tsutsui@NetBSD.org> from OpenBSD:
 * $OpenBSD: iha.h,v 1.2 2001/02/08 17:35:05 krw Exp $
 */

#define IHA_MAX_SG_ENTRIES	(MAXPHYS / PAGE_SIZE + 1)
#define IHA_MAX_TARGETS		16
#define IHA_MAX_SCB		32
#define IHA_MAX_EXTENDED_MSG	 4 /* SDTR(3) and WDTR(4) only */
#define IHA_MAX_OFFSET		15

#define SCSI_CONDITION_MET    0x04 /* SCSI Status codes not defined */
#define SCSI_INTERM_COND_MET  0x14 /*     in scsi_all.h             */

/*
 *   Scatter-Gather Element Structure
 */
struct iha_sg_element {
	uint32_t sg_addr;	/* Data Pointer */
	uint32_t sg_len;	/* Data Length  */
};

#define IHA_SG_SIZE (sizeof(struct iha_sg_element) * IHA_MAX_SG_ENTRIES)

/*
 * iha_scb - SCSI Request structure used by the
 *		    Tulip (aka inic-940/950).
 */

struct iha_scb {
	TAILQ_ENTRY(iha_scb) chain;

	bus_dmamap_t dmap;		/* maps xs->buf xfer buffer	*/

	int status;			/* Current status of the SCB	*/
#define  STATUS_QUEUED	0		/*  SCB one of Free/Done/Pend	*/
#define  STATUS_RENT	1		/*  SCB allocated, not queued	*/
#define  STATUS_SELECT	2		/*  SCB being selected		*/
#define  STATUS_BUSY	3		/*  SCB I/O is active		*/
	int nextstat;			/* Next state function to apply	*/
	int sg_index;			/* Scatter/Gather Index		*/
	int sg_max;			/* Scatter/Gather # valid entries */
	int flags;			/* SCB Flags			*/
#define  FLAG_DATAIN	0x00000001	/*  Data In			*/
#define  FLAG_DATAOUT	0x00000002	/*  Data Out			*/
#define  FLAG_RSENS	0x00000004	/*  Request Sense sent		*/
#define  FLAG_SG	0x00000008	/*  Scatter/Gather used		*/
	int target;			/* Target Id			*/
	int lun;			/* Lun				*/

	uint32_t bufaddr;		/* Data Buffer Physical Addr	*/
	uint32_t buflen;		/* Data Allocation Length	*/
	int ha_stat;			/* Status of Host Adapter	*/
#define  HOST_OK	0x00		/*  OK - operation a success	*/
#define  HOST_TIMED_OUT	0x01		/*  Request timed out		*/
#define  HOST_SPERR	0x10		/*  SCSI parity error		*/
#define  HOST_SEL_TOUT	0x11		/*  Selection Timeout		*/
#define  HOST_DO_DU	0x12		/*  Data Over/Underrun		*/
#define  HOST_BAD_PHAS	0x14		/*  Unexpected SCSI bus phase	*/
#define  HOST_SCSI_RST	0x1B		/*  SCSI bus was reset		*/
#define  HOST_DEV_RST	0x1C		/*  Device was reset		*/
	int ta_stat;			/* SCSI Status Byte		*/

	struct scsipi_xfer *xs;		/* xs this SCB is executing	*/
	struct tcs *tcs;		/* tcs for SCB_Target	   	*/
	struct iha_sg_element *sglist;
	bus_size_t sgoffset;		/* xfer buf offset              */

	int sg_size;			/* # of valid entries in sg_list */
	uint32_t sg_addr;		/* SGList Physical Address	*/

	int cmdlen;			/* CDB Length			*/
	uint8_t cmd[12];		/* SCSI Command			*/

	uint8_t scb_id;			/* Identity Message		*/
	uint8_t scb_tagmsg;		/* Tag Message			*/
	uint8_t scb_tagid;		/* Queue Tag			*/
};

/*
 *   Target Device Control Structure
 */
struct tcs {
	int flags;
#define		      FLAG_SCSI_RATE	 0x0007 /* Index into iha_rate_tbl[] */
#define		      FLAG_EN_DISC	 0x0008 /* Enable disconnect	     */
#define		      FLAG_NO_SYNC	 0x0010 /* No sync data transfer     */
#define		      FLAG_NO_WIDE	 0x0020 /* No wide data transfer     */
#define		      FLAG_1GIGA	 0x0040 /* 255 hd/63 sec (64/32)     */
#define		      FLAG_SPINUP	 0x0080 /* Start disk drive	     */
#define		      FLAG_WIDE_DONE	 0x0100 /* WDTR msg has been sent    */
#define		      FLAG_SYNC_DONE	 0x0200 /* SDTR msg has been sent    */
#define		      FLAG_NO_NEG_SYNC   (FLAG_NO_SYNC | FLAG_SYNC_DONE)
#define		      FLAG_NO_NEG_WIDE   (FLAG_NO_WIDE | FLAG_WIDE_DONE)
	int period;
	int offset;
	int tagcnt;

	struct iha_scb *ntagscb;

	uint8_t syncm;
	uint8_t sconfig0;
};

struct iha_softc {
	device_t sc_dev;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;

	bus_dma_tag_t sc_dmat;
	bus_dmamap_t sc_dmamap;

	struct scsipi_adapter sc_adapter;
	struct scsipi_channel sc_channel;

	int sc_id;
	void *sc_ih;

	/*
	 *   Initio specific fields
	 */
	int sc_flags;
#define		      FLAG_EXPECT_DISC	     0x01
#define		      FLAG_EXPECT_SELECT     0x02
#define		      FLAG_EXPECT_RESET	     0x10
#define		      FLAG_EXPECT_DONE_DISC  0x20
	int sc_semaph;
#define		      SEMAPH_IN_MAIN	     0x00   /* Already in tulip_main */
	int sc_phase;			  	    /* MSG  C/D	 I/O	     */
#define		      PHASE_DATA_OUT	     0x00   /*	0    0	  0	     */
#define		      PHASE_DATA_IN	     0x01   /*	0    0	  1	     */
#define		      PHASE_CMD_OUT	     0x02   /*	0    1	  0	     */
#define		      PHASE_STATUS_IN	     0x03   /*	0    1	  1	     */
#define		      PHASE_MSG_OUT	     0x06   /*	1    1	  0	     */
#define		      PHASE_MSG_IN	     0x07   /*	1    1	  1	     */

	struct iha_scb *sc_scb;			    /* SCB array	     */
	struct iha_scb *sc_actscb;		    /* SCB using SCSI bus    */
	struct iha_sg_element *sc_sglist;

	TAILQ_HEAD(, iha_scb) sc_freescb,
			      sc_pendscb,
			      sc_donescb;

	struct tcs sc_tcs[IHA_MAX_TARGETS];

	uint8_t sc_msg[IHA_MAX_EXTENDED_MSG];	    /* [0] len, [1] Msg Code */
	uint8_t sc_sistat;
	uint8_t sc_status0;
	uint8_t sc_status1;
	uint8_t sc_sconf1;
};

/*
 *   EEPROM for one SCSI Channel
 */

#define EEPROM_SIZE	32
#define EEP_LBYTE(x)	((x) & 0xff)
#define EEP_HBYTE(x)	((x) >> 8)
#define EEP_WORD(l, h)	(((h) & 0xff) << 8 | ((l) && 0xff))
#define EEP_WAIT()	DELAY(5)

struct eeprom_adapter {
	uint16_t config1;		/* 0x00 Channel Adapter SCSI Id  */
#define  CFG_ID_MASK	0x000f
#define  CFG_ID(cfg)	((cfg) & CFG_ID_MASK)
#define  CFG_SCSI_RESET	0x0100		/*     Reset bus at power up     */
#define  CFG_EN_PAR	0x0200		/*     SCSI parity enable        */
#define  CFG_ACT_TERM1	0x0400		/*     Enable active term 1      */
#define  CFG_ACT_TERM2	0x0800		/*     Enable active term 2      */
#define  CFG_AUTO_TERM	0x1000		/*     Enable auto terminator    */
#define  CFG_EN_PWR	0x8000		/*     Enable power mgmt         */
#define  CFG_DEFAULT	(CFG_SCSI_RESET | CFG_AUTO_TERM | CFG_EN_PAR)
	uint16_t config2;
#define  CFG_CFG2(x)	EEP_LBYTE(x)	/* 0x02 Unused Channel Cfg byte 2*/
#define  CFG_TARGET(x)	EEP_HBYTE(x)	/* 0x03 Number of SCSI targets   */
					/* 0x04 Lower bytes of targ flags*/
	uint16_t tflags[IHA_MAX_TARGETS / sizeof(uint16_t)];
#define  FLAG_DEFAULT	(FLAG_NO_WIDE | FLAG_1GIGA | FLAG_EN_DISC)
};

/*
 * Tulip (aka ini-940/950) Serial EEPROM Layout
 */
struct iha_eeprom {
	/* ---------- Header ------------------------------------------------*/
	uint16_t signature;		       /* 0x00 NVRAM Signature	     */
#define EEP_SIGNATURE	0xC925
	uint16_t revision;
#define EEP_SIZE(x)	EEP_LBYTE(x)	       /* 0x02 Size of data structure*/
#define EEP_REV(x)	EEP_HBYTE(x)	       /* 0x03 Rev. of data structure*/
	/* ---------- Host Adapter Structure --------------------------------*/
	uint16_t model;			       /* 0x04 Model number          */
	uint16_t modelinfo;
#define EEP_INFO(x)	EEP_LBYTE(x)	       /* 0x06 Model information     */
#define EEP_CHAN(x)	EEP_HBYTE(x)	       /* 0x07 Number of SCSI channel*/
	uint16_t bioscfg;		       /* 0x08 BIOS configuration 1  */
#define EEP_BIOSCFG_ENABLE	0x0001	       /*      BIOS enable	     */
#define EEP_BIOSCFG_8DRIVE	0x0002	       /*      Support > 2 drives    */
#define EEP_BIOSCFG_REMOVABLE	0x0004	       /*      Support removable drv */
#define EEP_BIOSCFG_INT19	0x0008	       /*      Intercept int 19h     */
#define EEP_BIOSCFG_BIOSSCAN	0x0010	       /*      Dynamic BIOS scan     */
#define EEP_BIOSCFG_LUNSUPPORT	0x0040	       /*      Support LUN	     */
#define EEP_BIOSCFG_DEFAULT	EEP_BIOSCFG_ENABLE
	uint16_t hacfg;			       /* 0x0a Host adapter config 1 */
#define EEP_HACFG_BOOTIDMASK	0x000F	       /*      Boot ID number	     */
#define EEP_HACFG_LUNMASK	0x0070	       /*      Boot LUN number	     */
#define EEP_HACFG_CHANMASK	0x0080	       /*      Boot Channel number   */
	struct eeprom_adapter adapter[2];      /* 0x0c		             */
	uint16_t reserved[5];		       /* 0x34			     */

	/* --------- CheckSum -----------------------------------------------*/
	uint16_t checksum;		       /* 0x3E Checksum of NVRam     */
};

/* Functions used by higher SCSI layers, the kernel, or iha.c and iha_pci.c  */

int iha_intr(void *);
void iha_attach(struct iha_softc *);
