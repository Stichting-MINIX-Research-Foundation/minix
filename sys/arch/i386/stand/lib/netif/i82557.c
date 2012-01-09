/* $NetBSD: i82557.c,v 1.11 2008/12/14 18:46:33 christos Exp $ */

/*
 * Copyright (c) 1998, 1999
 * 	Matthias Drochner.  All rights reserved.
 * Copyright (c) 1995, David Greenman
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <machine/pio.h>

#include <dev/ic/i82557reg.h>

#include <lib/libsa/stand.h>

#include <libi386.h>
#include <pcivar.h>

#if defined(_STANDALONE) && !defined(SUPPORT_NO_NETBSD)
#include <lib/libkern/libkern.h>
#include <bootinfo.h>
#endif

#include "etherdrv.h"

#define RECVBUF_SIZE 1600 /* struct fxp_rfa + packet */

#ifdef _STANDALONE
static pcihdl_t mytag;
static char recvbuf[RECVBUF_SIZE];
#define RECVBUF_PHYS vtophys(recvbuf)
#define RECVBUF_VIRT ((void *)recvbuf)
static union _sndbuf {
	struct fxp_cb_config cbp;
	struct fxp_cb_ias cb_ias;
	struct fxp_cb_tx txp;
} sndbuf;
#define SNDBUF_PHYS vtophys(&sndbuf)
#define SNDBUF_VIRT ((void *)&sndbuf)
#else /* !standalone, userspace testing environment */
#define	PCI_MODE1_ENABLE	0x80000000UL
static pcihdl_t mytag = PCI_MODE1_ENABLE | (PCIDEVNO << 11);

extern void *mapmem(int, int);
void *dmamem; /* virtual */
#define RECVBUF_PHYS DMABASE
#define RECVBUF_VIRT dmamem
#define SNDBUF_PHYS (DMABASE + RECVBUF_SIZE)
#define SNDBUF_VIRT ((void *)(((char *)dmamem) + RECVBUF_SIZE))
#endif /* _STANDALONE */

static void fxp_read_eeprom(uint16_t *, int, int);
static inline void fxp_scb_wait(void);
#ifdef DEBUG
static void fxp_checkintr(char *);
#else
#define fxp_checkintr(x)
#endif
static void fxp_startreceiver(void);

/*
 * Template for default configuration parameters.
 * See struct fxp_cb_config for the bit definitions.
 */
static uint8_t fxp_cb_config_template[] = {
	0x0, 0x0,		/* cb_status */
	0x80, 0x2,		/* cb_command */
	0xff, 0xff, 0xff, 0xff,	/* link_addr */
	0x16,	/*  0 */
	0x8,	/*  1 */
	0x0,	/*  2 */
	0x0,	/*  3 */
	0x0,	/*  4 */
	0x80,	/*  5 */
	0xb2,	/*  6 */
	0x3,	/*  7 */
	0x1,	/*  8 */
	0x0,	/*  9 */
	0x26,	/* 10 */
	0x0,	/* 11 */
	0x60,	/* 12 */
	0x0,	/* 13 */
	0xf2,	/* 14 */
	0x48,	/* 15 */
	0x0,	/* 16 */
	0x40,	/* 17 */
	0xf3,	/* 18 */
	0x0,	/* 19 */
	0x3f,	/* 20 */
	0x5	/* 21 */
};

static int tx_threshold = 64; /* x8, max 192 */

#define CSR_READ_1(reg) inb(iobase + (reg))
#define CSR_READ_2(reg) inw(iobase + (reg))
#define CSR_WRITE_1(reg, val) outb(iobase + (reg), val)
#define CSR_WRITE_2(reg, val) outw(iobase + (reg), val)
#define CSR_WRITE_4(reg, val) outl(iobase + (reg), val)
#define DELAY(n) delay(n)

static int iobase;

#if defined(_STANDALONE) && !defined(SUPPORT_NO_NETBSD)
static struct btinfo_netif bi_netif;
#endif

/*
 * Wait for the previous command to be accepted (but not necessarily
 * completed).
 */
static inline void
fxp_scb_wait(void)
{
	int i = 10000;

	while (CSR_READ_1(FXP_CSR_SCB_COMMAND) && --i)
		DELAY(1);
	if (i == 0)
		printf("fxp: WARNING: SCB timed out!\n");
}

#ifdef DEBUG
static void
fxp_checkintr(char *msg)
{
	uint8_t statack;
	int i = 10000;

	do {
		statack = CSR_READ_1(FXP_CSR_SCB_STATACK);
	} while ((statack == 0) && (--i > 0));

	if (statack != 0) {
		CSR_WRITE_1(FXP_CSR_SCB_STATACK, statack);
		printf("%s: ack'd irq %x, i=%d\n", msg, statack, i);
	}
}
#endif

int
EtherInit(unsigned char *myadr)
{
#ifndef _STANDALONE
	uint32_t id;
#endif
	volatile struct fxp_cb_config *cbp;
	volatile struct fxp_cb_ias *cb_ias;
	int i;

	if (pcicheck()) {
		printf("pcicheck failed\n");
		return 0;
	}
#ifdef _STANDALONE
	if (pcifinddev(0x8086, 0x1229, &mytag)) {
		printf("no fxp\n");
		return 0;
	}
#else
	pcicfgread(&mytag, 0, &id);
	if (id != 0x12298086) {
		printf("no fxp\n");
		return 0;
	}
#endif

	pcicfgread(&mytag, FXP_PCI_IOBA, &iobase);
	iobase &= ~3;

#ifndef _STANDALONE
	dmamem = mapmem(DMABASE, DMASIZE);
	if (!dmamem)
		return 0;
#endif

	fxp_read_eeprom((void *)myadr, 0, 3);

	/*
	 * Initialize base of CBL and RFA memory. Loading with zero
	 * sets it up for regular linear addressing.
	 */
	CSR_WRITE_4(FXP_CSR_SCB_GENERAL, 0);
	CSR_WRITE_1(FXP_CSR_SCB_COMMAND, FXP_SCB_COMMAND_CU_BASE);

	fxp_scb_wait();
	CSR_WRITE_1(FXP_CSR_SCB_COMMAND, FXP_SCB_COMMAND_RU_BASE);

	cbp = SNDBUF_VIRT;
	/*
	 * This memcpy is kind of disgusting, but there are a bunch of must be
	 * zero and must be one bits in this structure and this is the easiest
	 * way to initialize them all to proper values.
	 */
	memcpy((void *)cbp, fxp_cb_config_template,
	      sizeof(fxp_cb_config_template));

#define prm 0
#define phy_10Mbps_only 0
#define all_mcasts 0
	cbp->cb_status =	0;
	cbp->cb_command =	FXP_CB_COMMAND_CONFIG | FXP_CB_COMMAND_EL;
	cbp->link_addr =	-1;	/* (no) next command */
	cbp->byte_count =	22;	/* (22) bytes to config */
	cbp->rx_fifo_limit =	8;	/* rx fifo threshold (32 bytes) */
	cbp->tx_fifo_limit =	0;	/* tx fifo threshold (0 bytes) */
	cbp->adaptive_ifs =	0;	/* (no) adaptive interframe spacing */
	cbp->rx_dma_bytecount =	0;	/* (no) rx DMA max */
	cbp->tx_dma_bytecount =	0;	/* (no) tx DMA max */
	cbp->dma_mbce =		0;	/* (disable) dma max counters */
	cbp->late_scb =		0;	/* (don't) defer SCB update */
	cbp->tno_int_or_tco_en = 0;	/* (disable) tx not okay interrupt */
	cbp->ci_int =		0;	/* interrupt on CU not active */
	cbp->save_bf =		prm;	/* save bad frames */
	cbp->disc_short_rx =	!prm;	/* discard short packets */
	cbp->underrun_retry =	1;	/* retry mode (1) on DMA underrun */
	cbp->mediatype =	!phy_10Mbps_only; /* interface mode */
	cbp->nsai =		1;     /* (don't) disable source addr insert */
	cbp->preamble_length =	2;	/* (7 byte) preamble */
	cbp->loopback =		0;	/* (don't) loopback */
	cbp->linear_priority =	0;	/* (normal CSMA/CD operation) */
	cbp->linear_pri_mode =	0;	/* (wait after xmit only) */
	cbp->interfrm_spacing =	6;	/* (96 bits of) interframe spacing */
	cbp->promiscuous =	prm;	/* promiscuous mode */
	cbp->bcast_disable =	0;	/* (don't) disable broadcasts */
	cbp->crscdt =		0;	/* (CRS only) */
	cbp->stripping =	!prm;	/* truncate rx packet to byte count */
	cbp->padding =		1;	/* (do) pad short tx packets */
	cbp->rcv_crc_xfer =	0;	/* (don't) xfer CRC to host */
	cbp->force_fdx =	0;	/* (don't) force full duplex */
	cbp->fdx_pin_en =	1;	/* (enable) FDX# pin */
	cbp->multi_ia =		0;	/* (don't) accept multiple IAs */
	cbp->mc_all =		all_mcasts;/* accept all multicasts */
#undef prm
#undef phy_10Mbps_only
#undef all_mcasts

	/*
	 * Start the config command/DMA.
	 */
	fxp_scb_wait();
	CSR_WRITE_4(FXP_CSR_SCB_GENERAL, SNDBUF_PHYS);
	CSR_WRITE_1(FXP_CSR_SCB_COMMAND, FXP_SCB_COMMAND_CU_START);
	/* ...and wait for it to complete. */
	i = 10000;
	while (!(cbp->cb_status & FXP_CB_STATUS_C) && (--i > 0))
		DELAY(1);
	if (i == 0)
		printf("config timeout");

	fxp_checkintr("config");

	cb_ias = SNDBUF_VIRT;
	/*
	 * Now initialize the station address. Temporarily use the TxCB
	 * memory area like we did above for the config CB.
	 */
	cb_ias->cb_status = 0;
	cb_ias->cb_command = FXP_CB_COMMAND_IAS | FXP_CB_COMMAND_EL;
	cb_ias->link_addr = -1;
	memcpy((void *)cb_ias->macaddr, myadr, 6);

	/*
	 * Start the IAS (Individual Address Setup) command/DMA.
	 */
	fxp_scb_wait();
	/* address is still there */
	CSR_WRITE_1(FXP_CSR_SCB_COMMAND, FXP_SCB_COMMAND_CU_START);
	/* ...and wait for it to complete. */
	i = 10000;
	while (!(cb_ias->cb_status & FXP_CB_STATUS_C) && (--i > 0))
		DELAY(1);
	if (i == 0)
		printf("ias timeout");

	fxp_checkintr("ias");

	fxp_startreceiver();

#if defined(_STANDALONE) && !defined(SUPPORT_NO_NETBSD)
	strncpy(bi_netif.ifname, "fxp", sizeof(bi_netif.ifname));
	bi_netif.bus = BI_BUS_PCI;
	bi_netif.addr.tag = mytag;

	BI_ADD(&bi_netif, BTINFO_NETIF, sizeof(bi_netif));
#endif

	return 1;
}

void
EtherStop(void)
{

	/*
	 * Issue software reset
	 */
	CSR_WRITE_4(FXP_CSR_PORT, FXP_PORT_SELECTIVE_RESET);
	DELAY(10);
}

int
EtherSend(char *pkt, int len)
{
	volatile struct fxp_cb_tx *txp;
#ifdef _STANDALONE
	static volatile struct fxp_tbd tbd;
#endif
	volatile struct fxp_tbd *tbdp;
	int i;

	txp = SNDBUF_VIRT;
#ifdef _STANDALONE
	tbdp = &tbd;
	txp->tbd_array_addr = vtophys((void *)&tbd);
	tbdp->tb_addr = vtophys(pkt);
#else
	/* XXX assuming we send at max 400 bytes */
	tbdp = (struct fxp_tbd *)(SNDBUF_VIRT + 440);
	txp->tbd_array_addr = SNDBUF_PHYS + 440;
	memcpy(SNDBUF_VIRT + 400, pkt, len);
	tbdp->tb_addr = SNDBUF_PHYS + 400;
#endif
	tbdp->tb_size = len;
	txp->tbd_number = 1;
	txp->cb_status = 0;
	txp->cb_command =
	    FXP_CB_COMMAND_XMIT | FXP_CB_COMMAND_SF | FXP_CB_COMMAND_EL;
	txp->tx_threshold = tx_threshold;

	txp->link_addr = -1;
	txp->byte_count = 0;

	fxp_scb_wait();
	CSR_WRITE_4(FXP_CSR_SCB_GENERAL, SNDBUF_PHYS);
	CSR_WRITE_1(FXP_CSR_SCB_COMMAND, FXP_SCB_COMMAND_CU_START);
	/* ...and wait for it to complete. */
	i = 10000;
	while (!(txp->cb_status & FXP_CB_STATUS_C) && (--i > 0))
		DELAY(1);
	if (i == 0)
		printf("send timeout");

	fxp_checkintr("send");

	return len;
}

static void
fxp_startreceiver(void)
{
	volatile struct fxp_rfa *rfa;
	uint32_t v;

	rfa = RECVBUF_VIRT;
	rfa->size = RECVBUF_SIZE - sizeof(struct fxp_rfa);
	rfa->rfa_status = 0;
	rfa->rfa_control = FXP_RFA_CONTROL_S;
	rfa->actual_size = 0;
	v = RECVBUF_PHYS; /* close the "ring" */
	memcpy((void *)&rfa->link_addr, &v, sizeof(v));
	v = -1;
	memcpy((void *)&rfa->rbd_addr, &v, sizeof(v));

	fxp_scb_wait();
	CSR_WRITE_4(FXP_CSR_SCB_GENERAL, RECVBUF_PHYS);
	CSR_WRITE_1(FXP_CSR_SCB_COMMAND, FXP_SCB_COMMAND_RU_START);
}

int
EtherReceive(char *pkt, int maxlen)
{
	uint8_t ruscus;
	volatile struct fxp_rfa *rfa;
	int len = 0;

	ruscus = CSR_READ_1(FXP_CSR_SCB_RUSCUS);
	if (((ruscus >> 2) & 0x0f) == FXP_SCB_RUS_READY)
		return 0;
	if (((ruscus >> 2) & 0x0f) != FXP_SCB_RUS_SUSPENDED) {
		printf("rcv: ruscus=%x\n", ruscus);
		return 0;
	}

	rfa = RECVBUF_VIRT;
	if (rfa->rfa_status & FXP_RFA_STATUS_C) {
		len = rfa->actual_size & 0x7ff;
		if (len <= maxlen) {
			memcpy(pkt, (char *) rfa + RFA_SIZE, maxlen);
#if 0
			printf("rfa status=%x, len=%x\n",
			       rfa->rfa_status, len);
#endif
		} else
			len = 0;
	}

	fxp_scb_wait();
	CSR_WRITE_1(FXP_CSR_SCB_COMMAND, FXP_SCB_COMMAND_RU_RESUME);

	return len;
}

/*
 * Read from the serial EEPROM. Basically, you manually shift in
 * the read opcode (one bit at a time) and then shift in the address,
 * and then you shift out the data (all of this one bit at a time).
 * The word size is 16 bits, so you have to provide the address for
 * every 16 bits of data.
 */
static void
fxp_read_eeprom(uint16_t *data, int offset, int words)
{
	uint16_t reg;
	int i, x;

	for (i = 0; i < words; i++) {
		CSR_WRITE_2(FXP_CSR_EEPROMCONTROL, FXP_EEPROM_EECS);
		/*
		 * Shift in read opcode.
		 */
		for (x = 3; x > 0; x--) {
			if (FXP_EEPROM_OPC_READ & (1 << (x - 1))) {
				reg = FXP_EEPROM_EECS | FXP_EEPROM_EEDI;
			} else {
				reg = FXP_EEPROM_EECS;
			}
			CSR_WRITE_2(FXP_CSR_EEPROMCONTROL, reg);
			CSR_WRITE_2(FXP_CSR_EEPROMCONTROL,
			    reg | FXP_EEPROM_EESK);
			DELAY(1);
			CSR_WRITE_2(FXP_CSR_EEPROMCONTROL, reg);
			DELAY(1);
		}
		/*
		 * Shift in address.
		 */
		for (x = 6; x > 0; x--) {
			if ((i + offset) & (1 << (x - 1))) {
				reg = FXP_EEPROM_EECS | FXP_EEPROM_EEDI;
			} else {
				reg = FXP_EEPROM_EECS;
			}
			CSR_WRITE_2(FXP_CSR_EEPROMCONTROL, reg);
			CSR_WRITE_2(FXP_CSR_EEPROMCONTROL,
			    reg | FXP_EEPROM_EESK);
			DELAY(1);
			CSR_WRITE_2(FXP_CSR_EEPROMCONTROL, reg);
			DELAY(1);
		}
		reg = FXP_EEPROM_EECS;
		data[i] = 0;
		/*
		 * Shift out data.
		 */
		for (x = 16; x > 0; x--) {
			CSR_WRITE_2(FXP_CSR_EEPROMCONTROL,
			    reg | FXP_EEPROM_EESK);
			DELAY(1);
			if (CSR_READ_2(FXP_CSR_EEPROMCONTROL) &
			    FXP_EEPROM_EEDO)
				data[i] |= (1 << (x - 1));
			CSR_WRITE_2(FXP_CSR_EEPROMCONTROL, reg);
			DELAY(1);
		}
		CSR_WRITE_2(FXP_CSR_EEPROMCONTROL, 0);
		DELAY(1);
	}
}
