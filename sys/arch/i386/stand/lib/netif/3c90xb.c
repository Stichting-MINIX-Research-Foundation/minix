/* $NetBSD: 3c90xb.c,v 1.14 2008/12/14 18:46:33 christos Exp $ */

/*
 * Copyright (c) 1999
 * 	Matthias Drochner.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer.
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

struct mbuf; /* XXX */
typedef int bus_dmamap_t; /* XXX */
#include <dev/ic/elink3reg.h>
#include <dev/ic/elinkxlreg.h>

#include <lib/libsa/stand.h>

#include <libi386.h>
#include <pcivar.h>

#if defined(_STANDALONE) && !defined(SUPPORT_NO_NETBSD)
#include <lib/libkern/libkern.h>
#include <bootinfo.h>
#endif

#include "etherdrv.h"

#define RECVBUF_SIZE 1600 /* struct ex_upd + packet */

#ifdef _STANDALONE

static pcihdl_t mytag;
static char recvbuf[RECVBUF_SIZE];
#define RECVBUF_PHYS vtophys(recvbuf)
#define RECVBUF_VIRT ((void *)recvbuf)
static struct ex_dpd sndbuf;
#define SNDBUF_PHYS vtophys(&sndbuf)
#define SNDBUF_VIRT ((void *)&sndbuf)

#else /* !standalone, userspace testing environment */

#define	PCI_MODE1_ENABLE	0x80000000UL
#define PCIBUSNO 1
#define PCIDEVNO 4
static pcihdl_t mytag = PCI_MODE1_ENABLE | (PCIBUSNO << 16) | (PCIDEVNO << 11);

extern void *mapmem(int, int);
void *dmamem; /* virtual */
#define DMABASE 0x3ffd800
#define DMASIZE 10240
#define RECVBUF_PHYS DMABASE
#define RECVBUF_VIRT dmamem
#define SNDBUF_PHYS (DMABASE + RECVBUF_SIZE)
#define SNDBUF_VIRT ((void *)(((char *)dmamem) + RECVBUF_SIZE))

#endif /* _STANDALONE */


#define CSR_READ_1(reg) inb(iobase + (reg))
#define CSR_READ_2(reg) inw(iobase + (reg))
#define CSR_READ_4(reg) inl(iobase + (reg))
#define CSR_WRITE_1(reg, val) outb(iobase + (reg), val)
#define CSR_WRITE_2(reg, val) outw(iobase + (reg), val)
#define CSR_WRITE_4(reg, val) outl(iobase + (reg), val)

#undef GO_WINDOW
#define GO_WINDOW(x) CSR_WRITE_2(ELINK_COMMAND, WINDOW_SELECT | x)

static int iobase;
static u_char myethaddr[6];
unsigned ether_medium;

static struct {
	int did;
	int mii;
} excards[] = {
	{0x9005, 0}, /* 3c900b Combo */
	{0x9055, 1}, /* 3c905b TP */
	{0x9058, 0}, /* 3c905b Combo */
	{-1}
}, *excard;

static struct mtabentry {
	int address_cfg; /* configured connector */
	int config_bit; /* connector present */
	char *name;
} mediatab[] = { /* indexed by media type - etherdrv.h */
	{ELINKMEDIA_10BASE_2, ELINK_PCI_BNC, "BNC"},
	{ELINKMEDIA_10BASE_T, ELINK_PCI_10BASE_T, "UTP"},
	{ELINKMEDIA_AUI, ELINK_PCI_AUI, "AUI"},
	{ELINKMEDIA_MII, ELINK_PCI_100BASE_MII, "MII"},
	{ELINKMEDIA_100BASE_TX, ELINK_PCI_100BASE_TX, "100TX"},
};

#if defined(_STANDALONE) && !defined(SUPPORT_NO_NETBSD)
static struct btinfo_netif bi_netif;
#endif

#define ex_waitcmd() \
	do { \
		while (CSR_READ_2(ELINK_STATUS) & COMMAND_IN_PROGRESS) \
			continue; \
	} while (0)

void ex_reset(void);
uint16_t ex_read_eeprom(int);
static int ex_eeprom_busy(void);
void ex_init(void);
void ex_set_media(void);

void
ex_reset(void)
{
	CSR_WRITE_2(ELINK_COMMAND, GLOBAL_RESET);
	delay(100000);
	ex_waitcmd();
}

/*
 * Read EEPROM data.
 * XXX what to do if EEPROM doesn't unbusy?
 */
uint16_t
ex_read_eeprom(int offset)
{
	uint16_t data = 0;

	GO_WINDOW(0);
	if (ex_eeprom_busy())
		goto out;
	CSR_WRITE_1(ELINK_W0_EEPROM_COMMAND, READ_EEPROM | (offset & 0x3f));
	if (ex_eeprom_busy())
		goto out;
	data = CSR_READ_2(ELINK_W0_EEPROM_DATA);
out:
	return data;
}

static int
ex_eeprom_busy(void)
{
	int i = 100;

	while (i--) {
		if (!(CSR_READ_2(ELINK_W0_EEPROM_COMMAND) & EEPROM_BUSY))
			return 0;
		delay(100);
	}
	printf("\nex: eeprom stays busy.\n");
	return 1;
}

/*
 * Bring device up.
 */
void
ex_init(void)
{
	int i;

	ex_waitcmd();
	EtherStop();

	/*
	 * Set the station address and clear the station mask. The latter
	 * is needed for 90x cards, 0 is the default for 90xB cards.
	 */
	GO_WINDOW(2);
	for (i = 0; i < 6; i++) {
		CSR_WRITE_1(ELINK_W2_ADDR_0 + i,
		    myethaddr[i]);
		CSR_WRITE_1(ELINK_W2_RECVMASK_0 + i, 0);
	}

	GO_WINDOW(3);

	CSR_WRITE_2(ELINK_COMMAND, RX_RESET);
	ex_waitcmd();
	CSR_WRITE_2(ELINK_COMMAND, TX_RESET);
	ex_waitcmd();

	CSR_WRITE_2(ELINK_COMMAND, SET_INTR_MASK | 0); /* disable */
	CSR_WRITE_2(ELINK_COMMAND, ACK_INTR | 0xff);

	ex_set_media();

	CSR_WRITE_2(ELINK_COMMAND, SET_RX_FILTER | FIL_INDIVIDUAL | FIL_BRDCST);

	CSR_WRITE_4(ELINK_DNLISTPTR, 0);
	CSR_WRITE_2(ELINK_COMMAND, TX_ENABLE);

	CSR_WRITE_4(ELINK_UPLISTPTR, RECVBUF_PHYS);
	CSR_WRITE_2(ELINK_COMMAND, RX_ENABLE);
	CSR_WRITE_2(ELINK_COMMAND, ELINK_UPUNSTALL);

	GO_WINDOW(1);
}

void
ex_set_media(void)
{
	int config0, config1;

	CSR_WRITE_2(ELINK_W3_MAC_CONTROL, 0);

	if (ether_medium == ETHERMEDIUM_MII)
		goto setcfg;

	GO_WINDOW(4);
	CSR_WRITE_2(ELINK_W4_MEDIA_TYPE, 0);
	CSR_WRITE_2(ELINK_COMMAND, STOP_TRANSCEIVER);
	delay(800);

	switch (ether_medium) {
	case ETHERMEDIUM_UTP:
		CSR_WRITE_2(ELINK_W4_MEDIA_TYPE,
			    JABBER_GUARD_ENABLE | LINKBEAT_ENABLE);
		break;
	case ETHERMEDIUM_BNC:
		CSR_WRITE_2(ELINK_COMMAND, START_TRANSCEIVER);
		delay(800);
		break;
	case ETHERMEDIUM_AUI:
		CSR_WRITE_2(ELINK_W4_MEDIA_TYPE, SQE_ENABLE);
		delay(800);
		break;
	case ETHERMEDIUM_100TX:
		CSR_WRITE_2(ELINK_W4_MEDIA_TYPE, LINKBEAT_ENABLE);
		break;
	}

setcfg:
	GO_WINDOW(3);

	config0 = CSR_READ_2(ELINK_W3_INTERNAL_CONFIG);
	config1 = CSR_READ_2(ELINK_W3_INTERNAL_CONFIG + 2);

	config1 = config1 & ~CONFIG_MEDIAMASK;
	config1 |= (mediatab[ether_medium].address_cfg
		    << CONFIG_MEDIAMASK_SHIFT);

	CSR_WRITE_2(ELINK_W3_INTERNAL_CONFIG, config0);
	CSR_WRITE_2(ELINK_W3_INTERNAL_CONFIG + 2, config1);
}

static void
ex_probemedia(void)
{
	int i, j;
	struct mtabentry *m;

	/* test for presence of connectors */
	GO_WINDOW(3);
	i = CSR_READ_1(ELINK_W3_RESET_OPTIONS);
	j = (CSR_READ_2(ELINK_W3_INTERNAL_CONFIG + 2) & CONFIG_MEDIAMASK)
		>> CONFIG_MEDIAMASK_SHIFT;
	GO_WINDOW(0);

	for (ether_medium = 0, m = mediatab;
	     ether_medium < sizeof(mediatab) / sizeof(mediatab[0]);
	     ether_medium++, m++) {
		if (j == m->address_cfg) {
			if (!(i & m->config_bit)) {
				printf("%s not present\n", m->name);
				goto bad;
			}
			printf("using %s\n", m->name);
			return;
		}
	}
	printf("unknown connector\n");
bad:
	ether_medium = -1;
}

int
EtherInit(unsigned char *myadr)
{
	uint32_t pcicsr;
	uint16_t val;
	volatile struct ex_upd *upd;
#ifndef _STANDALONE
	uint32_t id;
#endif

	if (pcicheck()) {
		printf("pcicheck failed\n");
		return 0;
	}
#ifndef _STANDALONE
	pcicfgread(&mytag, 0, &id);
#endif
	for (excard = &excards[0]; excard->did != -1; excard++) {
#ifdef _STANDALONE
		if (pcifinddev(0x10b7, excard->did, &mytag) == 0)
			goto found;
#else
		if (id == (0x10b7 | (excard->did << 16)))
			goto found;
#endif
	}
	printf("no ex\n");
	return 0;

found:
	pcicfgread(&mytag, 0x10, &iobase);
	iobase &= ~3;

#ifndef _STANDALONE
	dmamem = mapmem(DMABASE, DMASIZE);
	if (!dmamem)
		return 0;
#endif

	/* enable bus mastering in PCI command register */
	if (pcicfgread(&mytag, 0x04, (int *)&pcicsr)
	    || pcicfgwrite(&mytag, 0x04, pcicsr | 4)) {
		printf("cannot enable DMA\n");
		return 0;
	}

	ex_reset();

	if (excard->mii)
		ether_medium = ETHERMEDIUM_MII;
	else {
		ex_probemedia();
		if (ether_medium < 0)
			return 0;
	}

	val = ex_read_eeprom(EEPROM_OEM_ADDR0);
	myethaddr[0] = val >> 8;
	myethaddr[1] = val & 0xff;
	val = ex_read_eeprom(EEPROM_OEM_ADDR1);
	myethaddr[2] = val >> 8;
	myethaddr[3] = val & 0xff;
	val = ex_read_eeprom(EEPROM_OEM_ADDR2);
	myethaddr[4] = val >> 8;
	myethaddr[5] = val & 0xff;
	memcpy(myadr, myethaddr, 6);

	upd = RECVBUF_VIRT;
	upd->upd_nextptr = RECVBUF_PHYS;
	upd->upd_pktstatus = 1500;
	upd->upd_frags[0].fr_addr = RECVBUF_PHYS + 100;
	upd->upd_frags[0].fr_len = 1500 | EX_FR_LAST;

	ex_init();

#if defined(_STANDALONE) && !defined(SUPPORT_NO_NETBSD)
	strncpy(bi_netif.ifname, "ex", sizeof(bi_netif.ifname));
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
	CSR_WRITE_2(ELINK_COMMAND, RX_DISABLE);
	CSR_WRITE_2(ELINK_COMMAND, TX_DISABLE);
        CSR_WRITE_2(ELINK_COMMAND, STOP_TRANSCEIVER);
	CSR_WRITE_2(ELINK_COMMAND, INTR_LATCH);
}

int
EtherSend(char *pkt, int len)
{
	volatile struct ex_dpd *dpd;
	int i;

	dpd = SNDBUF_VIRT;

	dpd->dpd_nextptr = 0;
	dpd->dpd_fsh = len;
#ifdef _STANDALONE
	dpd->dpd_frags[0].fr_addr = vtophys(pkt);
#else
	memcpy(SNDBUF_VIRT + 100, pkt, len);
	dpd->dpd_frags[0].fr_addr = SNDBUF_PHYS + 100;
#endif
	dpd->dpd_frags[0].fr_len = len | EX_FR_LAST;

	CSR_WRITE_4(ELINK_DNLISTPTR, SNDBUF_PHYS);
	CSR_WRITE_2(ELINK_COMMAND, ELINK_DNUNSTALL);

	i = 10000;
	while (!(dpd->dpd_fsh & 0x00010000)) {
		if (--i < 0) {
			printf("3c90xb: send timeout\n");
			return -1;
		}
		delay(1);
	}

	return len;
}

int
EtherReceive(char *pkt, int maxlen)
{
	volatile struct ex_upd *upd;
	int len;

	upd = RECVBUF_VIRT;

	if (!(upd->upd_pktstatus & ~EX_UPD_PKTLENMASK))
		return 0;

	len = upd->upd_pktstatus & EX_UPD_PKTLENMASK;
	if (len > maxlen)
		len = 0;
	else
		memcpy(pkt, RECVBUF_VIRT + 100, len);

	upd->upd_pktstatus = 1500;
	CSR_WRITE_2(ELINK_COMMAND, ELINK_UPUNSTALL);

	return len;
}
