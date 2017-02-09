/*	$NetBSD: fwohci.c,v 1.137 2014/02/25 18:30:09 pooka Exp $	*/

/*-
 * Copyright (c) 2003 Hidetoshi Shimokawa
 * Copyright (c) 1998-2002 Katsushi Kobayashi and Hidetoshi Shimokawa
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the acknowledgement as bellow:
 *
 *    This product includes software developed by K. Kobayashi and H. Shimokawa
 *
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 *
 * $FreeBSD: src/sys/dev/firewire/fwohci.c,v 1.98 2009/02/13 17:44:07 sbruno Exp $
 *
 */
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: fwohci.c,v 1.137 2014/02/25 18:30:09 pooka Exp $");

#include <sys/param.h>
#include <sys/atomic.h>
#include <sys/bus.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/select.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <dev/ieee1394/firewire.h>
#include <dev/ieee1394/firewirereg.h>
#include <dev/ieee1394/fwdma.h>
#include <dev/ieee1394/fwohcireg.h>
#include <dev/ieee1394/fwohcivar.h>
#include <dev/ieee1394/firewire_phy.h>

#include "ioconf.h"

#undef OHCI_DEBUG

static int nocyclemaster = 0;
int firewire_phydma_enable = 1;
/*
 * Setup sysctl(3) MIB, hw.fwohci.*
 *
 * TBD condition CTLFLAG_PERMANENT on being a module or not
 */
SYSCTL_SETUP(sysctl_fwohci, "sysctl fwohci(4) subtree setup")
{
	int rc, fwohci_node_num;
	const struct sysctlnode *node;

	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT, CTLTYPE_NODE, "fwohci",
	    SYSCTL_DESCR("fwohci controls"),
	    NULL, 0, NULL, 0, CTL_HW, CTL_CREATE, CTL_EOL)) != 0) {
		goto err;
	}
	fwohci_node_num = node->sysctl_num;

	/* fwohci no cyclemaster flag */
	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE, CTLTYPE_INT,
	    "nocyclemaster", SYSCTL_DESCR("Do not send cycle start packets"),
	    NULL, 0, &nocyclemaster,
	    0, CTL_HW, fwohci_node_num, CTL_CREATE, CTL_EOL)) != 0) {
		goto err;
	}

	/* fwohci physical request DMA enable */
	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE, CTLTYPE_INT, "phydma_enable",
	    SYSCTL_DESCR("Allow physical request DMA from firewire"),
	    NULL, 0, &firewire_phydma_enable,
	    0, CTL_HW, fwohci_node_num, CTL_CREATE, CTL_EOL)) != 0) {
		goto err;
	}
	return;

err:
	aprint_error("%s: sysctl_createv failed (rc = %d)\n", __func__, rc);
}

static const char * const dbcode[16] = {
    "OUTM", "OUTL", "INPM", "INPL", "STOR", "LOAD", "NOP ", "STOP",
    "", "", "", "", "", "", "", ""
};

static const char * const dbkey[8] = {
    "ST0", "ST1", "ST2", "ST3", "UNDEF", "REG", "SYS", "DEV"
};

static const char * const dbcond[4] = { "NEV", "C=1", "C=0", "ALL" };
static const char * const fwohcicode[32] = {
	"No stat",	"Undef",	"long",		"miss Ack err",
	"FIFO underrun","FIFO overrun",	"desc err",	"data read err",
	"data write err","bus reset",	"timeout",	"tcode err",
	"Undef",	"Undef",	"unknown event","flushed",
	"Undef",	"ack complete",	"ack pend",	"Undef",
	"ack busy_X",	"ack busy_A",	"ack busy_B",	"Undef",
	"Undef",	"Undef",	"Undef",	"ack tardy",
	"Undef",	"ack data_err",	"ack type_err",	""
};

#define MAX_SPEED 3
extern const char *fw_linkspeed[];
static uint32_t const tagbit[4] = { 1 << 28, 1 << 29, 1 << 30, 1 << 31 };

static const struct tcode_info tinfo[] = {
/*		hdr_len block 	flag	valid_response */
/* 0 WREQQ  */ { 16,	FWTI_REQ | FWTI_TLABEL, FWTCODE_WRES },
/* 1 WREQB  */ { 16,	FWTI_REQ | FWTI_TLABEL | FWTI_BLOCK_ASY, FWTCODE_WRES },
/* 2 WRES   */ { 12,	FWTI_RES, 0xff },
/* 3 XXX    */ {  0,	0, 0xff },
/* 4 RREQQ  */ { 12,	FWTI_REQ | FWTI_TLABEL, FWTCODE_RRESQ },
/* 5 RREQB  */ { 16,	FWTI_REQ | FWTI_TLABEL, FWTCODE_RRESB },
/* 6 RRESQ  */ { 16,	FWTI_RES, 0xff },
/* 7 RRESB  */ { 16,	FWTI_RES | FWTI_BLOCK_ASY, 0xff },
/* 8 CYCS   */ {  0,	0, 0xff },
/* 9 LREQ   */ { 16,	FWTI_REQ | FWTI_TLABEL | FWTI_BLOCK_ASY, FWTCODE_LRES },
/* a STREAM */ {  4,	FWTI_REQ | FWTI_BLOCK_STR, 0xff },
/* b LRES   */ { 16,	FWTI_RES | FWTI_BLOCK_ASY, 0xff },
/* c XXX    */ {  0,	0, 0xff },
/* d XXX    */ {  0,	0, 0xff },
/* e PHY    */ { 12,	FWTI_REQ, 0xff },
/* f XXX    */ {  0,	0, 0xff }
};

#define OHCI_WRITE_SIGMASK 0xffff0000
#define OHCI_READ_SIGMASK 0xffff0000


int fwohci_print(void *, const char *);

static int fwohci_ioctl(dev_t, u_long, void *, int, struct lwp *);

static uint32_t fwohci_cyctimer(struct firewire_comm *);
static uint32_t fwohci_set_bus_manager(struct firewire_comm *, u_int);
static void fwohci_ibr(struct firewire_comm *);
static int fwohci_irx_enable(struct firewire_comm *, int);
static int fwohci_irx_disable(struct firewire_comm *, int);
static int fwohci_itxbuf_enable(struct firewire_comm *, int);
static int fwohci_itx_disable(struct firewire_comm *, int);
static void fwohci_timeout(struct firewire_comm *fc);
#if BYTE_ORDER == BIG_ENDIAN
static void fwohci_irx_post(struct firewire_comm *, uint32_t *);
#endif
static void fwohci_set_intr(struct firewire_comm *, int);

static uint32_t fwphy_rddata(struct fwohci_softc *, uint32_t);
static uint32_t fwphy_wrdata(struct fwohci_softc *, uint32_t, uint32_t);
static int fwohci_probe_phy(struct fwohci_softc *);
static void fwohci_reset(struct fwohci_softc *);
static void fwohci_execute_db(struct fwohcidb_tr *, bus_dmamap_t);
static void fwohci_start(struct fwohci_softc *, struct fwohci_dbch *);
static void fwohci_start_atq(struct firewire_comm *);
static void fwohci_start_ats(struct firewire_comm *);
static void fwohci_txd(struct fwohci_softc *, struct fwohci_dbch *);
static void fwohci_db_free(struct fwohci_softc *, struct fwohci_dbch *);
static void fwohci_db_init(struct fwohci_softc *, struct fwohci_dbch *);
static int fwohci_rx_enable(struct fwohci_softc *, struct fwohci_dbch *);
static int fwohci_tx_enable(struct fwohci_softc *, struct fwohci_dbch *);
static int fwohci_next_cycle(struct fwohci_softc *, int);
#ifdef OHCI_DEBUG
static void fwohci_dump_intr(struct fwohci_softc *, uint32_t);
#endif
static void fwohci_intr_core(struct fwohci_softc *, uint32_t);
static void fwohci_intr_dma(struct fwohci_softc *, uint32_t);
static void fwohci_task_sid(struct fwohci_softc *);
static void fwohci_task_dma(struct fwohci_softc *);
static void fwohci_tbuf_update(struct fwohci_softc *, int);
static void fwohci_rbuf_update(struct fwohci_softc *, int);
static void dump_dma(struct fwohci_softc *, uint32_t);
static void dump_db(struct fwohci_softc *, uint32_t);
static void print_db(struct fwohcidb_tr *, struct fwohcidb *, uint32_t,
		     uint32_t);
static void fwohci_txbufdb(struct fwohci_softc *, int, struct fw_bulkxfer *);
static int fwohci_add_tx_buf(struct fwohci_dbch *, struct fwohcidb_tr *, int);
static int fwohci_add_rx_buf(struct fwohci_dbch *, struct fwohcidb_tr *, int,
			     struct fwdma_alloc *);
static int fwohci_arcv_swap(struct fw_pkt *, int);
static int fwohci_get_plen(struct fwohci_softc *, struct fwohci_dbch *,
			   struct fw_pkt *);
static void fwohci_arcv_free_buf(struct fwohci_softc *, struct fwohci_dbch *,
				 struct fwohcidb_tr *, int);
static void fwohci_arcv(struct fwohci_softc *, struct fwohci_dbch *);


/*
 * memory allocated for DMA programs
 */
#define DMA_PROG_ALLOC		(8 * PAGE_SIZE)

#define NDB FWMAXQUEUE

#define	OHCI_VERSION		0x000
#define	OHCI_ATRETRY		0x008
#define	OHCI_CROMHDR		0x018
#define	OHCI_BUS_OPT		0x020
#define	OHCI_BUSIRMC		(1 << 31)
#define	OHCI_BUSCMC		(1 << 30)
#define	OHCI_BUSISC		(1 << 29)
#define	OHCI_BUSBMC		(1 << 28)
#define	OHCI_BUSPMC		(1 << 27)
#define OHCI_BUSFNC \
	(OHCI_BUSIRMC | OHCI_BUSCMC | OHCI_BUSISC | OHCI_BUSBMC | OHCI_BUSPMC)

#define	OHCI_EUID_HI		0x024
#define	OHCI_EUID_LO		0x028

#define	OHCI_CROMPTR		0x034
#define	OHCI_HCCCTL		0x050
#define	OHCI_HCCCTLCLR		0x054
#define	OHCI_AREQHI		0x100
#define	OHCI_AREQHICLR		0x104
#define	OHCI_AREQLO		0x108
#define	OHCI_AREQLOCLR		0x10c
#define	OHCI_PREQHI		0x110
#define	OHCI_PREQHICLR		0x114
#define	OHCI_PREQLO		0x118
#define	OHCI_PREQLOCLR		0x11c
#define	OHCI_PREQUPPER		0x120

#define	OHCI_SID_BUF		0x064
#define	OHCI_SID_CNT		0x068
#define OHCI_SID_ERR		(1 << 31)
#define OHCI_SID_CNT_MASK	0xffc

#define	OHCI_IT_STAT		0x090
#define	OHCI_IT_STATCLR		0x094
#define	OHCI_IT_MASK		0x098
#define	OHCI_IT_MASKCLR		0x09c

#define	OHCI_IR_STAT		0x0a0
#define	OHCI_IR_STATCLR		0x0a4
#define	OHCI_IR_MASK		0x0a8
#define	OHCI_IR_MASKCLR		0x0ac

#define	OHCI_LNKCTL		0x0e0
#define	OHCI_LNKCTLCLR		0x0e4

#define	OHCI_PHYACCESS		0x0ec
#define	OHCI_CYCLETIMER		0x0f0

#define	OHCI_DMACTL(off)	(off)
#define	OHCI_DMACTLCLR(off)	(off + 0x04)
#define	OHCI_DMACMD(off)	(off + 0x0c)
#define	OHCI_DMAMATCH(off)	(off + 0x10)

#define OHCI_ATQOFF		0x180
#define OHCI_ATQCTL		OHCI_ATQOFF
#define OHCI_ATQCTLCLR		(OHCI_ATQOFF + 0x04)
#define OHCI_ATQCMD		(OHCI_ATQOFF + 0x0c)
#define OHCI_ATQMATCH		(OHCI_ATQOFF + 0x10)

#define OHCI_ATSOFF		0x1a0
#define OHCI_ATSCTL		OHCI_ATSOFF
#define OHCI_ATSCTLCLR		(OHCI_ATSOFF + 0x04)
#define OHCI_ATSCMD		(OHCI_ATSOFF + 0x0c)
#define OHCI_ATSMATCH		(OHCI_ATSOFF + 0x10)

#define OHCI_ARQOFF		0x1c0
#define OHCI_ARQCTL		OHCI_ARQOFF
#define OHCI_ARQCTLCLR		(OHCI_ARQOFF + 0x04)
#define OHCI_ARQCMD		(OHCI_ARQOFF + 0x0c)
#define OHCI_ARQMATCH		(OHCI_ARQOFF + 0x10)

#define OHCI_ARSOFF		0x1e0
#define OHCI_ARSCTL		OHCI_ARSOFF
#define OHCI_ARSCTLCLR		(OHCI_ARSOFF + 0x04)
#define OHCI_ARSCMD		(OHCI_ARSOFF + 0x0c)
#define OHCI_ARSMATCH		(OHCI_ARSOFF + 0x10)

#define OHCI_ITOFF(CH)		(0x200 + 0x10 * (CH))
#define OHCI_ITCTL(CH)		(OHCI_ITOFF(CH))
#define OHCI_ITCTLCLR(CH)	(OHCI_ITOFF(CH) + 0x04)
#define OHCI_ITCMD(CH)		(OHCI_ITOFF(CH) + 0x0c)

#define OHCI_IROFF(CH)		(0x400 + 0x20 * (CH))
#define OHCI_IRCTL(CH)		(OHCI_IROFF(CH))
#define OHCI_IRCTLCLR(CH)	(OHCI_IROFF(CH) + 0x04)
#define OHCI_IRCMD(CH)		(OHCI_IROFF(CH) + 0x0c)
#define OHCI_IRMATCH(CH)	(OHCI_IROFF(CH) + 0x10)

#define ATRQ_CH	 0
#define ATRS_CH	 1
#define ARRQ_CH	 2
#define ARRS_CH	 3
#define ITX_CH	 4
#define IRX_CH	36


/*
 * Call fwohci_init before fwohci_attach to initialize the kernel's
 * data structures well enough that fwohci_detach won't crash, even if
 * fwohci_attach fails.
 */

void
fwohci_init(struct fwohci_softc *sc)
{
	sc->fc.arq = &sc->arrq.xferq;
	sc->fc.ars = &sc->arrs.xferq;
	sc->fc.atq = &sc->atrq.xferq;
	sc->fc.ats = &sc->atrs.xferq;

	sc->arrq.xferq.psize = roundup2(FWPMAX_S400, PAGE_SIZE);
	sc->arrs.xferq.psize = roundup2(FWPMAX_S400, PAGE_SIZE);
	sc->atrq.xferq.psize = roundup2(FWPMAX_S400, PAGE_SIZE);
	sc->atrs.xferq.psize = roundup2(FWPMAX_S400, PAGE_SIZE);

	sc->arrq.xferq.start = NULL;
	sc->arrs.xferq.start = NULL;
	sc->atrq.xferq.start = fwohci_start_atq;
	sc->atrs.xferq.start = fwohci_start_ats;

	sc->arrq.xferq.buf = NULL;
	sc->arrs.xferq.buf = NULL;
	sc->atrq.xferq.buf = NULL;
	sc->atrs.xferq.buf = NULL;

	sc->arrq.xferq.dmach = -1;
	sc->arrs.xferq.dmach = -1;
	sc->atrq.xferq.dmach = -1;
	sc->atrs.xferq.dmach = -1;

	sc->arrq.ndesc = 1;
	sc->arrs.ndesc = 1;
	sc->atrq.ndesc = 8;	/* equal to maximum of mbuf chains */
	sc->atrs.ndesc = 2;

	sc->arrq.ndb = NDB;
	sc->arrs.ndb = NDB / 2;
	sc->atrq.ndb = NDB;
	sc->atrs.ndb = NDB / 2;

	sc->arrq.off = OHCI_ARQOFF;
	sc->arrs.off = OHCI_ARSOFF;
	sc->atrq.off = OHCI_ATQOFF;
	sc->atrs.off = OHCI_ATSOFF;

	sc->fc.tcode = tinfo;

	sc->fc.cyctimer = fwohci_cyctimer;
	sc->fc.ibr = fwohci_ibr;
	sc->fc.set_bmr = fwohci_set_bus_manager;
	sc->fc.ioctl = fwohci_ioctl;
	sc->fc.irx_enable = fwohci_irx_enable;
	sc->fc.irx_disable = fwohci_irx_disable;

	sc->fc.itx_enable = fwohci_itxbuf_enable;
	sc->fc.itx_disable = fwohci_itx_disable;
	sc->fc.timeout = fwohci_timeout;
	sc->fc.set_intr = fwohci_set_intr;
#if BYTE_ORDER == BIG_ENDIAN
	sc->fc.irx_post = fwohci_irx_post;
#else
	sc->fc.irx_post = NULL;
#endif
	sc->fc.itx_post = NULL;

	sc->intmask = sc->irstat = sc->itstat = 0;

	fw_init(&sc->fc);
}

/*
 * Call fwohci_attach after fwohci_init to initialize the hardware and
 * attach children.
 */

int
fwohci_attach(struct fwohci_softc *sc)
{
	uint32_t reg;
	uint8_t ui[8];
	int i, mver;

/* OHCI version */
	reg = OREAD(sc, OHCI_VERSION);
	mver = (reg >> 16) & 0xff;
	aprint_normal_dev(sc->fc.dev, "OHCI version %x.%x (ROM=%d)\n",
	    mver, reg & 0xff, (reg >> 24) & 1);
	if (mver < 1 || mver > 9) {
		aprint_error_dev(sc->fc.dev, "invalid OHCI version\n");
		return ENXIO;
	}

/* Available Isochronous DMA channel probe */
	OWRITE(sc, OHCI_IT_MASK, 0xffffffff);
	OWRITE(sc, OHCI_IR_MASK, 0xffffffff);
	reg = OREAD(sc, OHCI_IT_MASK) & OREAD(sc, OHCI_IR_MASK);
	OWRITE(sc, OHCI_IT_MASKCLR, 0xffffffff);
	OWRITE(sc, OHCI_IR_MASKCLR, 0xffffffff);
	for (i = 0; i < 0x20; i++)
		if ((reg & (1 << i)) == 0)
			break;
	sc->fc.nisodma = i;
	aprint_normal_dev(sc->fc.dev, "No. of Isochronous channels is %d.\n",
	    i);
	if (i == 0)
		return ENXIO;

	for (i = 0; i < sc->fc.nisodma; i++) {
		sc->fc.it[i] = &sc->it[i].xferq;
		sc->fc.ir[i] = &sc->ir[i].xferq;
		sc->it[i].xferq.dmach = i;
		sc->ir[i].xferq.dmach = i;
		sc->it[i].ndb = 0;
		sc->ir[i].ndb = 0;
		sc->it[i].off = OHCI_ITOFF(i);
		sc->ir[i].off = OHCI_IROFF(i);
	}

	sc->fc.config_rom = fwdma_alloc_setup(sc->fc.dev, sc->fc.dmat,
	    CROMSIZE, &sc->crom_dma, CROMSIZE, BUS_DMA_NOWAIT);
	if (sc->fc.config_rom == NULL) {
		aprint_error_dev(sc->fc.dev, "config_rom alloc failed.\n");
		return ENOMEM;
	}

#if 0
	memset(sc->fc.config_rom, 0, CROMSIZE);
	sc->fc.config_rom[1] = 0x31333934;
	sc->fc.config_rom[2] = 0xf000a002;
	sc->fc.config_rom[3] = OREAD(sc, OHCI_EUID_HI);
	sc->fc.config_rom[4] = OREAD(sc, OHCI_EUID_LO);
	sc->fc.config_rom[5] = 0;
	sc->fc.config_rom[0] = (4 << 24) | (5 << 16);

	sc->fc.config_rom[0] |= fw_crc16(&sc->fc.config_rom[1], 5*4);
#endif

/* SID recieve buffer must align 2^11 */
#define	OHCI_SIDSIZE	(1 << 11)
	sc->sid_buf = fwdma_alloc_setup(sc->fc.dev, sc->fc.dmat, OHCI_SIDSIZE,
	    &sc->sid_dma, OHCI_SIDSIZE, BUS_DMA_NOWAIT);
	if (sc->sid_buf == NULL) {
		aprint_error_dev(sc->fc.dev, "sid_buf alloc failed.");
		return ENOMEM;
	}

	fwdma_alloc_setup(sc->fc.dev, sc->fc.dmat, sizeof(uint32_t),
	    &sc->dummy_dma, sizeof(uint32_t), BUS_DMA_NOWAIT);
	if (sc->dummy_dma.v_addr == NULL) {
		aprint_error_dev(sc->fc.dev, "dummy_dma alloc failed.");
		return ENOMEM;
	}

	fwohci_db_init(sc, &sc->arrq);
	if ((sc->arrq.flags & FWOHCI_DBCH_INIT) == 0)
		return ENOMEM;

	fwohci_db_init(sc, &sc->arrs);
	if ((sc->arrs.flags & FWOHCI_DBCH_INIT) == 0)
		return ENOMEM;

	fwohci_db_init(sc, &sc->atrq);
	if ((sc->atrq.flags & FWOHCI_DBCH_INIT) == 0)
		return ENOMEM;

	fwohci_db_init(sc, &sc->atrs);
	if ((sc->atrs.flags & FWOHCI_DBCH_INIT) == 0)
		return ENOMEM;

	sc->fc.eui.hi = OREAD(sc, FWOHCIGUID_H);
	sc->fc.eui.lo = OREAD(sc, FWOHCIGUID_L);
	for (i = 0; i < 8; i++)
		ui[i] = FW_EUI64_BYTE(&sc->fc.eui, i);
	aprint_normal_dev(sc->fc.dev,
	    "EUI64 %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
	    ui[0], ui[1], ui[2], ui[3], ui[4], ui[5], ui[6], ui[7]);

	fwohci_reset(sc);

	sc->fc.bdev =
	    config_found(sc->fc.dev, __UNCONST("ieee1394if"), fwohci_print);

	return 0;
}

int
fwohci_detach(struct fwohci_softc *sc, int flags)
{
	int i, rv;

	if (sc->fc.bdev != NULL) {
		rv = config_detach(sc->fc.bdev, flags);
		if (rv)
			return rv;
	}
	if (sc->sid_buf != NULL)
		fwdma_free(sc->sid_dma.dma_tag, sc->sid_dma.dma_map,
		    sc->sid_dma.v_addr);
	if (sc->fc.config_rom != NULL)
		fwdma_free(sc->crom_dma.dma_tag, sc->crom_dma.dma_map,
		    sc->crom_dma.v_addr);

	fwohci_db_free(sc, &sc->arrq);
	fwohci_db_free(sc, &sc->arrs);
	fwohci_db_free(sc, &sc->atrq);
	fwohci_db_free(sc, &sc->atrs);
	for (i = 0; i < sc->fc.nisodma; i++) {
		fwohci_db_free(sc, &sc->it[i]);
		fwohci_db_free(sc, &sc->ir[i]);
	}

	fw_destroy(&sc->fc);

	return 0;
}

int
fwohci_intr(void *arg)
{
	struct fwohci_softc *sc = (struct fwohci_softc *)arg;
	uint32_t stat, irstat, itstat;

	if (!device_is_active(sc->fc.dev))
		return 0;

	if (!(sc->intmask & OHCI_INT_EN))
		/* polling mode? */
		return 0;

	stat = OREAD(sc, FWOHCI_INTSTAT);
	if (stat == 0xffffffff) {
		aprint_error_dev(sc->fc.dev, "device physically ejected?\n");
		return 0;
	}
	if (stat)
		OWRITE(sc, FWOHCI_INTSTATCLR, stat & ~OHCI_INT_PHY_BUS_R);

	stat &= sc->intmask;
	if (stat == 0)
		return 0;

	atomic_swap_32(&sc->intstat, stat);
	if (stat & OHCI_INT_DMA_IR) {
		irstat = OREAD(sc, OHCI_IR_STAT);
		OWRITE(sc, OHCI_IR_STATCLR, irstat);
		atomic_swap_32(&sc->irstat, irstat);
	}
	if (stat & OHCI_INT_DMA_IT) {
		itstat = OREAD(sc, OHCI_IT_STAT);
		OWRITE(sc, OHCI_IT_STATCLR, itstat);
		atomic_swap_32(&sc->itstat, itstat);
	}

	fwohci_intr_core(sc, stat);
	return 1;
}

int
fwohci_resume(struct fwohci_softc *sc)
{
	struct fw_xferq *ir;
	struct fw_bulkxfer *chunk;
	int i;
	extern int firewire_resume(struct firewire_comm *);

	fwohci_reset(sc);
	/* XXX resume isochronous receive automatically. (how about TX?) */
	for (i = 0; i < sc->fc.nisodma; i++) {
		ir = &sc->ir[i].xferq;
		if ((ir->flag & FWXFERQ_RUNNING) != 0) {
			aprint_normal_dev(sc->fc.dev,
			    "resume iso receive ch: %d\n", i);
			ir->flag &= ~FWXFERQ_RUNNING;
			/* requeue stdma to stfree */
			while ((chunk = STAILQ_FIRST(&ir->stdma)) != NULL) {
				STAILQ_REMOVE_HEAD(&ir->stdma, link);
				STAILQ_INSERT_TAIL(&ir->stfree, chunk, link);
			}
			sc->fc.irx_enable(&sc->fc, i);
		}
	}

	firewire_resume(&sc->fc);
	sc->fc.ibr(&sc->fc);
	return 0;
}

int
fwohci_stop(struct fwohci_softc *sc)
{
	u_int i;

	fwohci_set_intr(&sc->fc, 0);

/* Now stopping all DMA channel */
	OWRITE(sc, OHCI_ARQCTLCLR, OHCI_CNTL_DMA_RUN);
	OWRITE(sc, OHCI_ARSCTLCLR, OHCI_CNTL_DMA_RUN);
	OWRITE(sc, OHCI_ATQCTLCLR, OHCI_CNTL_DMA_RUN);
	OWRITE(sc, OHCI_ATSCTLCLR, OHCI_CNTL_DMA_RUN);

	for (i = 0; i < sc->fc.nisodma; i++) {
		OWRITE(sc, OHCI_IRCTLCLR(i), OHCI_CNTL_DMA_RUN);
		OWRITE(sc, OHCI_ITCTLCLR(i), OHCI_CNTL_DMA_RUN);
	}

#if 0 /* Let dcons(4) be accessed */
/* Stop interrupt */
	OWRITE(sc, FWOHCI_INTMASKCLR,
	    OHCI_INT_EN |
	    OHCI_INT_ERR |
	    OHCI_INT_PHY_SID |
	    OHCI_INT_PHY_INT |
	    OHCI_INT_DMA_ATRQ |
	    OHCI_INT_DMA_ATRS |
	    OHCI_INT_DMA_PRRQ |
	    OHCI_INT_DMA_PRRS |
	    OHCI_INT_DMA_ARRQ |
	    OHCI_INT_DMA_ARRS |
	    OHCI_INT_PHY_BUS_R);

/* FLUSH FIFO and reset Transmitter/Reciever */
	OWRITE(sc, OHCI_HCCCTL, OHCI_HCC_RESET);
#endif

/* XXX Link down?  Bus reset? */
	return 0;
}


static int
fwohci_ioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *td)
{
	struct fwohci_softc *sc;
	struct fw_reg_req_t *reg = (struct fw_reg_req_t *)data;
	uint32_t *dmach = (uint32_t *)data;
	int err = 0;

	sc = device_lookup_private(&fwohci_cd, DEV2UNIT(dev));
	if (sc == NULL)
		return ENXIO;

	if (!data)
		return EINVAL;

	switch (cmd) {
	case FWOHCI_WRREG:
#define OHCI_MAX_REG 0x800
		if (reg->addr <= OHCI_MAX_REG) {
			OWRITE(sc, reg->addr, reg->data);
			reg->data = OREAD(sc, reg->addr);
		} else
			err = EINVAL;
		break;

	case FWOHCI_RDREG:
		if (reg->addr <= OHCI_MAX_REG)
			reg->data = OREAD(sc, reg->addr);
		else
			err = EINVAL;
		break;

/* Read DMA descriptors for debug  */
	case DUMPDMA:
		if (*dmach <= OHCI_MAX_DMA_CH) {
			dump_dma(sc, *dmach);
			dump_db(sc, *dmach);
		} else
			err = EINVAL;
		break;

/* Read/Write Phy registers */
#define OHCI_MAX_PHY_REG 0xf
	case FWOHCI_RDPHYREG:
		if (reg->addr <= OHCI_MAX_PHY_REG)
			reg->data = fwphy_rddata(sc, reg->addr);
		else
			err = EINVAL;
		break;

	case FWOHCI_WRPHYREG:
		if (reg->addr <= OHCI_MAX_PHY_REG)
			reg->data = fwphy_wrdata(sc, reg->addr, reg->data);
		else
			err = EINVAL;
		break;

	default:
		err = EINVAL;
		break;
	}
	return err;
}

int
fwohci_print(void *aux, const char *pnp)
{
	struct fw_attach_args *fwa = (struct fw_attach_args *)aux;

	if (pnp)
		aprint_normal("%s at %s", fwa->name, pnp);

	return UNCONF;
}


static uint32_t
fwohci_cyctimer(struct firewire_comm *fc)
{
	struct fwohci_softc *sc = (struct fwohci_softc *)fc;

	return OREAD(sc, OHCI_CYCLETIMER);
}

static uint32_t
fwohci_set_bus_manager(struct firewire_comm *fc, u_int node)
{
	struct fwohci_softc *sc = (struct fwohci_softc *)fc;
	uint32_t bm;
	int i;

#define OHCI_CSR_DATA	0x0c
#define OHCI_CSR_COMP	0x10
#define OHCI_CSR_CONT	0x14
#define OHCI_BUS_MANAGER_ID	0

	OWRITE(sc, OHCI_CSR_DATA, node);
	OWRITE(sc, OHCI_CSR_COMP, 0x3f);
	OWRITE(sc, OHCI_CSR_CONT, OHCI_BUS_MANAGER_ID);
 	for (i = 0; !(OREAD(sc, OHCI_CSR_CONT) & (1<<31)) && (i < 1000); i++)
		DELAY(10);
	bm = OREAD(sc, OHCI_CSR_DATA);
	if ((bm & 0x3f) == 0x3f)
		bm = node;
	if (firewire_debug)
		printf("fw_set_bus_manager: %d->%d (loop=%d)\n", bm, node, i);

	return bm;
}

static void
fwohci_ibr(struct firewire_comm *fc)
{
	struct fwohci_softc *sc = (struct fwohci_softc *)fc;
	uint32_t fun;

	aprint_normal_dev(fc->dev, "Initiate bus reset\n");

	/*
	 * Make sure our cached values from the config rom are
	 * initialised.
	 */
	OWRITE(sc, OHCI_CROMHDR, ntohl(sc->fc.config_rom[0]));
	OWRITE(sc, OHCI_BUS_OPT, ntohl(sc->fc.config_rom[2]));

	/*
	 * Set root hold-off bit so that non cyclemaster capable node
	 * shouldn't became the root node.
	 */
#if 1
	fun = fwphy_rddata(sc, FW_PHY_IBR_REG);
	fun |= FW_PHY_IBR | FW_PHY_RHB;
	fun = fwphy_wrdata(sc, FW_PHY_IBR_REG, fun);
#else	/* Short bus reset */
	fun = fwphy_rddata(sc, FW_PHY_ISBR_REG);
	fun |= FW_PHY_ISBR | FW_PHY_RHB;
	fun = fwphy_wrdata(sc, FW_PHY_ISBR_REG, fun);
#endif
}

static int
fwohci_irx_enable(struct firewire_comm *fc, int dmach)
{
	struct fwohci_softc *sc = (struct fwohci_softc *)fc;
	struct fwohci_dbch *dbch;
	struct fwohcidb_tr *db_tr;
	struct fw_bulkxfer *first, *prev, *chunk, *txfer;
	struct fw_xferq *ir;
	uint32_t stat;
	unsigned short tag, ich;
	int err = 0, ldesc;

	dbch = &sc->ir[dmach];
	ir = &dbch->xferq;

	if ((ir->flag & FWXFERQ_RUNNING) == 0) {
		tag = (ir->flag >> 6) & 3;
		ich = ir->flag & 0x3f;
		OWRITE(sc, OHCI_IRMATCH(dmach), tagbit[tag] | ich);

		ir->queued = 0;
		dbch->ndb = ir->bnpacket * ir->bnchunk;
		dbch->ndesc = 2;
		fwohci_db_init(sc, dbch);
		if ((dbch->flags & FWOHCI_DBCH_INIT) == 0)
			return ENOMEM;
		err = fwohci_rx_enable(sc, dbch);
		if (err)
			return err;
	}

	first = STAILQ_FIRST(&ir->stfree);
	if (first == NULL) {
		aprint_error_dev(fc->dev, "IR DMA no free chunk\n");
		return 0;
	}

	ldesc = dbch->ndesc - 1;
	prev = NULL;
	STAILQ_FOREACH(txfer, &ir->stdma, link)
		prev = txfer;
	while ((chunk = STAILQ_FIRST(&ir->stfree)) != NULL) {
		struct fwohcidb *db;

		if (chunk->mbuf != NULL) {
			db_tr = (struct fwohcidb_tr *)(chunk->start);
			db_tr->dbcnt = 1;
			err = bus_dmamap_load_mbuf(fc->dmat, db_tr->dma_map,
			    chunk->mbuf, BUS_DMA_NOWAIT);
			if (err == 0)
				fwohci_execute_db(db_tr, db_tr->dma_map);
			else
				aprint_error_dev(fc->dev,
				    "mbuf load failed: %d\n", err);
 			FWOHCI_DMA_SET(db_tr->db[1].db.desc.cmd,
			    OHCI_UPDATE |
			    OHCI_INPUT_LAST |
			    OHCI_INTERRUPT_ALWAYS |
			    OHCI_BRANCH_ALWAYS);
		}
		db = ((struct fwohcidb_tr *)(chunk->end))->db;
		FWOHCI_DMA_WRITE(db[ldesc].db.desc.res, 0);
		FWOHCI_DMA_CLEAR(db[ldesc].db.desc.depend, 0xf);
		if (prev != NULL) {
			db = ((struct fwohcidb_tr *)(prev->end))->db;
			FWOHCI_DMA_SET(db[ldesc].db.desc.depend, dbch->ndesc);
		}
		STAILQ_REMOVE_HEAD(&ir->stfree, link);
		STAILQ_INSERT_TAIL(&ir->stdma, chunk, link);
		prev = chunk;
	}
	fwdma_sync_multiseg_all(dbch->am,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	stat = OREAD(sc, OHCI_IRCTL(dmach));
	if (stat & OHCI_CNTL_DMA_ACTIVE)
		return 0;
	if (stat & OHCI_CNTL_DMA_RUN) {
		OWRITE(sc, OHCI_IRCTLCLR(dmach), OHCI_CNTL_DMA_RUN);
		aprint_error_dev(fc->dev, "IR DMA overrun (0x%08x)\n", stat);
	}

	if (firewire_debug)
		printf("start IR DMA 0x%x\n", stat);
	OWRITE(sc, OHCI_IR_MASKCLR, 1 << dmach);
	OWRITE(sc, OHCI_IR_STATCLR, 1 << dmach);
	OWRITE(sc, OHCI_IR_MASK, 1 << dmach);
	OWRITE(sc, OHCI_IRCTLCLR(dmach), 0xf0000000);
	OWRITE(sc, OHCI_IRCTL(dmach), OHCI_CNTL_ISOHDR);
	OWRITE(sc, OHCI_IRCMD(dmach),
	    ((struct fwohcidb_tr *)(first->start))->bus_addr | dbch->ndesc);
	OWRITE(sc, OHCI_IRCTL(dmach), OHCI_CNTL_DMA_RUN);
	OWRITE(sc, FWOHCI_INTMASK, OHCI_INT_DMA_IR);
#if 0
	dump_db(sc, IRX_CH + dmach);
#endif
	return err;
}

static int
fwohci_irx_disable(struct firewire_comm *fc, int dmach)
{
	struct fwohci_softc *sc = (struct fwohci_softc *)fc;

	OWRITE(sc, OHCI_IRCTLCLR(dmach), OHCI_CNTL_DMA_RUN);
	OWRITE(sc, OHCI_IR_MASKCLR, 1 << dmach);
	OWRITE(sc, OHCI_IR_STATCLR, 1 << dmach);
	/* XXX we cannot free buffers until the DMA really stops */
	kpause("fwirxd", true, hz, NULL);
	fwohci_db_free(sc, &sc->ir[dmach]);
	sc->ir[dmach].xferq.flag &= ~FWXFERQ_RUNNING;
	return 0;
}


static int
fwohci_itxbuf_enable(struct firewire_comm *fc, int dmach)
{
	struct fwohci_softc *sc = (struct fwohci_softc *)fc;
	struct fwohci_dbch *dbch;
	struct fw_bulkxfer *first, *chunk, *prev, *txfer;
	struct fw_xferq *it;
	uint32_t stat;
	int cycle_match, cycle_now, ldesc, err = 0;

	dbch = &sc->it[dmach];
	it = &dbch->xferq;

	if ((dbch->flags & FWOHCI_DBCH_INIT) == 0) {
		dbch->ndb = it->bnpacket * it->bnchunk;
		dbch->ndesc = 3;
		fwohci_db_init(sc, dbch);
		if ((dbch->flags & FWOHCI_DBCH_INIT) == 0)
			return ENOMEM;

		err = fwohci_tx_enable(sc, dbch);
		if (err)
			return err;
	}

	ldesc = dbch->ndesc - 1;
	prev = NULL;
	STAILQ_FOREACH(txfer, &it->stdma, link)
		prev = txfer;
	while ((chunk = STAILQ_FIRST(&it->stvalid)) != NULL) {
		struct fwohcidb *db;

		fwdma_sync_multiseg(it->buf, chunk->poffset, it->bnpacket,
		    BUS_DMASYNC_PREWRITE);
		fwohci_txbufdb(sc, dmach, chunk);
		if (prev != NULL) {
			db = ((struct fwohcidb_tr *)(prev->end))->db;
#if 0 /* XXX necessary? */
			FWOHCI_DMA_SET(db[ldesc].db.desc.cmd,
			    OHCI_BRANCH_ALWAYS);
#endif
#if 0 /* if bulkxfer->npacket changes */
			db[ldesc].db.desc.depend = db[0].db.desc.depend =
			    ((struct fwohcidb_tr *)(chunk->start))->bus_addr |
								dbch->ndesc;
#else
			FWOHCI_DMA_SET(db[0].db.desc.depend, dbch->ndesc);
			FWOHCI_DMA_SET(db[ldesc].db.desc.depend, dbch->ndesc);
#endif
		}
		STAILQ_REMOVE_HEAD(&it->stvalid, link);
		STAILQ_INSERT_TAIL(&it->stdma, chunk, link);
		prev = chunk;
	}
	fwdma_sync_multiseg_all(dbch->am,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	stat = OREAD(sc, OHCI_ITCTL(dmach));
	if (firewire_debug && (stat & OHCI_CNTL_CYCMATCH_S))
		printf("stat 0x%x\n", stat);

	if (stat & (OHCI_CNTL_DMA_ACTIVE | OHCI_CNTL_CYCMATCH_S))
		return 0;

#if 0
	OWRITE(sc, OHCI_ITCTLCLR(dmach), OHCI_CNTL_DMA_RUN);
#endif
	OWRITE(sc, OHCI_IT_MASKCLR, 1 << dmach);
	OWRITE(sc, OHCI_IT_STATCLR, 1 << dmach);
	OWRITE(sc, OHCI_IT_MASK, 1 << dmach);
	OWRITE(sc, FWOHCI_INTMASK, OHCI_INT_DMA_IT);

	first = STAILQ_FIRST(&it->stdma);
	OWRITE(sc, OHCI_ITCMD(dmach),
	    ((struct fwohcidb_tr *)(first->start))->bus_addr | dbch->ndesc);
	if (firewire_debug > 1) {
		printf("fwohci_itxbuf_enable: kick 0x%08x\n", stat);
#if 1
		dump_dma(sc, ITX_CH + dmach);
#endif
	}
	if ((stat & OHCI_CNTL_DMA_RUN) == 0) {
#if 1
		/* Don't start until all chunks are buffered */
		if (STAILQ_FIRST(&it->stfree) != NULL)
			goto out;
#endif
#if 1
		/* Clear cycle match counter bits */
		OWRITE(sc, OHCI_ITCTLCLR(dmach), 0xffff0000);

		/* 2bit second + 13bit cycle */
		cycle_now = (fc->cyctimer(fc) >> 12) & 0x7fff;
		cycle_match = fwohci_next_cycle(sc, cycle_now);

		OWRITE(sc, OHCI_ITCTL(dmach),
		    OHCI_CNTL_CYCMATCH_S | (cycle_match << 16) |
							OHCI_CNTL_DMA_RUN);
#else
		OWRITE(sc, OHCI_ITCTL(dmach), OHCI_CNTL_DMA_RUN);
#endif
		if (firewire_debug > 1) {
			printf("cycle_match: 0x%04x->0x%04x\n",
			    cycle_now, cycle_match);
			dump_dma(sc, ITX_CH + dmach);
			dump_db(sc, ITX_CH + dmach);
		}
	} else if ((stat & OHCI_CNTL_CYCMATCH_S) == 0) {
		aprint_error_dev(fc->dev, "IT DMA underrun (0x%08x)\n", stat);
		OWRITE(sc, OHCI_ITCTL(dmach), OHCI_CNTL_DMA_WAKE);
	}
out:
	return err;
}

static int
fwohci_itx_disable(struct firewire_comm *fc, int dmach)
{
	struct fwohci_softc *sc = (struct fwohci_softc *)fc;

	OWRITE(sc, OHCI_ITCTLCLR(dmach),
	    OHCI_CNTL_DMA_RUN | OHCI_CNTL_CYCMATCH_S);
	OWRITE(sc, OHCI_IT_MASKCLR, 1 << dmach);
	OWRITE(sc, OHCI_IT_STATCLR, 1 << dmach);
	/* XXX we cannot free buffers until the DMA really stops */
	kpause("fwitxd", true, hz, NULL);
	fwohci_db_free(sc, &sc->it[dmach]);
	sc->it[dmach].xferq.flag &= ~FWXFERQ_RUNNING;
	return 0;
}

static void
fwohci_timeout(struct firewire_comm *fc)
{
#if 0
	struct fwohci_softc *sc = (struct fwohci_softc *)fc;
#endif
	/* nothing? */
}

#if BYTE_ORDER == BIG_ENDIAN
static void
fwohci_irx_post (struct firewire_comm *fc, uint32_t *qld)
{

	qld[0] = FWOHCI_DMA_READ(qld[0]);
	return;
}
#endif

static void
fwohci_set_intr(struct firewire_comm *fc, int enable)
{
	struct fwohci_softc *sc = (struct fwohci_softc *)fc;

	if (firewire_debug)
		printf("fwohci_set_intr: %d\n", enable);
	if (enable) {
		sc->intmask |= OHCI_INT_EN;
		OWRITE(sc, FWOHCI_INTMASK, OHCI_INT_EN);
	} else {
		sc->intmask &= ~OHCI_INT_EN;
		OWRITE(sc, FWOHCI_INTMASKCLR, OHCI_INT_EN);
	}
}

/*
 * Communication with PHY device
 */
/* XXX need lock for phy access */
static uint32_t
fwphy_rddata(struct fwohci_softc *sc, u_int addr)
{
	uint32_t fun, stat;
	u_int i, retry = 0;

	addr &= 0xf;
#define MAX_RETRY 100
again:
	OWRITE(sc, FWOHCI_INTSTATCLR, OHCI_INT_REG_FAIL);
	fun = PHYDEV_RDCMD | (addr << PHYDEV_REGADDR);
	OWRITE(sc, OHCI_PHYACCESS, fun);
	for (i = 0; i < MAX_RETRY; i++) {
		fun = OREAD(sc, OHCI_PHYACCESS);
		if ((fun & PHYDEV_RDCMD) == 0 && (fun & PHYDEV_RDDONE) != 0)
			break;
		DELAY(100);
	}
	if (i >= MAX_RETRY) {
		if (firewire_debug)
			printf("phy read failed(1).\n");
		if (++retry < MAX_RETRY) {
			DELAY(100);
			goto again;
		}
	}
	/* Make sure that SCLK is started */
	stat = OREAD(sc, FWOHCI_INTSTAT);
	if ((stat & OHCI_INT_REG_FAIL) != 0 ||
	    ((fun >> PHYDEV_REGADDR) & 0xf) != addr) {
		if (firewire_debug)
			printf("phy read failed(2).\n");
		if (++retry < MAX_RETRY) {
			DELAY(100);
			goto again;
		}
	}
	if (firewire_debug || retry >= MAX_RETRY)
		aprint_error_dev(sc->fc.dev,
		    "fwphy_rddata: 0x%x loop=%d, retry=%d\n",
		    addr, i, retry);
#undef MAX_RETRY
	return (fun >> PHYDEV_RDDATA) & 0xff;
}

static uint32_t
fwphy_wrdata(struct fwohci_softc *sc, uint32_t addr, uint32_t data)
{
	uint32_t fun;

	addr &= 0xf;
	data &= 0xff;

	fun =
	    (PHYDEV_WRCMD | (addr << PHYDEV_REGADDR) | (data << PHYDEV_WRDATA));
	OWRITE(sc, OHCI_PHYACCESS, fun);
	DELAY(100);

	return fwphy_rddata(sc, addr);
}

static int
fwohci_probe_phy(struct fwohci_softc *sc)
{
	uint32_t reg, reg2;
	int e1394a = 1;

/*
 * probe PHY parameters
 * 0. to prove PHY version, whether compliance of 1394a.
 * 1. to probe maximum speed supported by the PHY and
 *    number of port supported by core-logic.
 *    It is not actually available port on your PC .
 */
	OWRITE(sc, OHCI_HCCCTL, OHCI_HCC_LPS);
	DELAY(500);

	reg = fwphy_rddata(sc, FW_PHY_SPD_REG);

	if ((reg >> 5) != 7) {
		sc->fc.mode &= ~FWPHYASYST;
		sc->fc.nport = reg & FW_PHY_NP;
		sc->fc.speed = reg & FW_PHY_SPD >> 6;
		if (sc->fc.speed > MAX_SPEED) {
			aprint_error_dev(sc->fc.dev,
			    "invalid speed %d (fixed to %d).\n",
			    sc->fc.speed, MAX_SPEED);
			sc->fc.speed = MAX_SPEED;
		}
		aprint_normal_dev(sc->fc.dev, "Phy 1394 only %s, %d ports.\n",
		    fw_linkspeed[sc->fc.speed], sc->fc.nport);
	} else {
		reg2 = fwphy_rddata(sc, FW_PHY_ESPD_REG);
		sc->fc.mode |= FWPHYASYST;
		sc->fc.nport = reg & FW_PHY_NP;
		sc->fc.speed = (reg2 & FW_PHY_ESPD) >> 5;
		if (sc->fc.speed > MAX_SPEED) {
			aprint_error_dev(sc->fc.dev,
			    "invalid speed %d (fixed to %d).\n",
			    sc->fc.speed, MAX_SPEED);
			sc->fc.speed = MAX_SPEED;
		}
		aprint_normal_dev(sc->fc.dev,
		    "Phy 1394a available %s, %d ports.\n",
		    fw_linkspeed[sc->fc.speed], sc->fc.nport);

		/* check programPhyEnable */
		reg2 = fwphy_rddata(sc, 5);
#if 0
		if (e1394a && (OREAD(sc, OHCI_HCCCTL) & OHCI_HCC_PRPHY)) {
#else	/* XXX force to enable 1394a */
		if (e1394a) {
#endif
			if (firewire_debug)
				printf("Enable 1394a Enhancements\n");
			/* enable EAA EMC */
			reg2 |= 0x03;
			/* set aPhyEnhanceEnable */
			OWRITE(sc, OHCI_HCCCTL, OHCI_HCC_PHYEN);
			OWRITE(sc, OHCI_HCCCTLCLR, OHCI_HCC_PRPHY);
		}
#if 0
		else {
			/* for safe */
			reg2 &= ~0x83;
		}
#endif
		reg2 = fwphy_wrdata(sc, 5, reg2);
	}

	reg = fwphy_rddata(sc, FW_PHY_SPD_REG);
	if ((reg >> 5) == 7) {
		reg = fwphy_rddata(sc, 4);
		reg |= 1 << 6;
		fwphy_wrdata(sc, 4, reg);
		reg = fwphy_rddata(sc, 4);
	}
	return 0;
}

static void
fwohci_reset(struct fwohci_softc *sc)
{
	struct fwohcidb_tr *db_tr;
	uint32_t reg, reg2;
	int i, max_rec, speed;

	/* Disable interrupts */
	OWRITE(sc, FWOHCI_INTMASKCLR, ~0);

	/* Now stopping all DMA channels */
	OWRITE(sc, OHCI_ARQCTLCLR, OHCI_CNTL_DMA_RUN);
	OWRITE(sc, OHCI_ARSCTLCLR, OHCI_CNTL_DMA_RUN);
	OWRITE(sc, OHCI_ATQCTLCLR, OHCI_CNTL_DMA_RUN);
	OWRITE(sc, OHCI_ATSCTLCLR, OHCI_CNTL_DMA_RUN);

	OWRITE(sc, OHCI_IR_MASKCLR, ~0);
	for (i = 0; i < sc->fc.nisodma; i++) {
		OWRITE(sc, OHCI_IRCTLCLR(i), OHCI_CNTL_DMA_RUN);
		OWRITE(sc, OHCI_ITCTLCLR(i), OHCI_CNTL_DMA_RUN);
	}

	/* FLUSH FIFO and reset Transmitter/Reciever */
	OWRITE(sc, OHCI_HCCCTL, OHCI_HCC_RESET);
	if (firewire_debug)
		printf("resetting OHCI...");
	i = 0;
	while (OREAD(sc, OHCI_HCCCTL) & OHCI_HCC_RESET) {
		if (i++ > 100)
			break;
		DELAY(1000);
	}
	if (firewire_debug)
		printf("done (loop=%d)\n", i);

	/* Probe phy */
	fwohci_probe_phy(sc);

	/* Probe link */
	reg = OREAD(sc, OHCI_BUS_OPT);
	reg2 = reg | OHCI_BUSFNC;
	max_rec = (reg & 0x0000f000) >> 12;
	speed = (reg & 0x00000007);
	aprint_normal_dev(sc->fc.dev, "Link %s, max_rec %d bytes.\n",
	    fw_linkspeed[speed], MAXREC(max_rec));
	/* XXX fix max_rec */
	sc->fc.maxrec = sc->fc.speed + 8;
	if (max_rec != sc->fc.maxrec) {
		reg2 = (reg2 & 0xffff0fff) | (sc->fc.maxrec << 12);
		aprint_normal_dev(sc->fc.dev, "max_rec %d -> %d\n",
		    MAXREC(max_rec), MAXREC(sc->fc.maxrec));
	}
	if (firewire_debug)
		printf("BUS_OPT 0x%x -> 0x%x\n", reg, reg2);
	OWRITE(sc, OHCI_BUS_OPT, reg2);

	/* Initialize registers */
	OWRITE(sc, OHCI_CROMHDR, sc->fc.config_rom[0]);
	OWRITE(sc, OHCI_CROMPTR, sc->crom_dma.bus_addr);
	OWRITE(sc, OHCI_HCCCTLCLR, OHCI_HCC_BIGEND);
	OWRITE(sc, OHCI_HCCCTL, OHCI_HCC_POSTWR);
	OWRITE(sc, OHCI_SID_BUF, sc->sid_dma.bus_addr);
	OWRITE(sc, OHCI_LNKCTL, OHCI_CNTL_SID);

	/* Enable link */
	OWRITE(sc, OHCI_HCCCTL, OHCI_HCC_LINKEN);

	/* Force to start async RX DMA */
	sc->arrq.xferq.flag &= ~FWXFERQ_RUNNING;
	sc->arrs.xferq.flag &= ~FWXFERQ_RUNNING;
	fwohci_rx_enable(sc, &sc->arrq);
	fwohci_rx_enable(sc, &sc->arrs);

	/* Initialize async TX */
	OWRITE(sc, OHCI_ATQCTLCLR, OHCI_CNTL_DMA_RUN | OHCI_CNTL_DMA_DEAD);
	OWRITE(sc, OHCI_ATSCTLCLR, OHCI_CNTL_DMA_RUN | OHCI_CNTL_DMA_DEAD);

	/* AT Retries */
	OWRITE(sc, FWOHCI_RETRY,
	    /* CycleLimit   PhyRespRetries ATRespRetries ATReqRetries */
	    (0xffff << 16) | (0x0f << 8) | (0x0f << 4) | 0x0f);

	sc->atrq.top = STAILQ_FIRST(&sc->atrq.db_trq);
	sc->atrs.top = STAILQ_FIRST(&sc->atrs.db_trq);
	sc->atrq.bottom = sc->atrq.top;
	sc->atrs.bottom = sc->atrs.top;

	for (i = 0, db_tr = sc->atrq.top; i < sc->atrq.ndb;
	    i++, db_tr = STAILQ_NEXT(db_tr, link))
		db_tr->xfer = NULL;
	for (i = 0, db_tr = sc->atrs.top; i < sc->atrs.ndb;
	    i++, db_tr = STAILQ_NEXT(db_tr, link))
		db_tr->xfer = NULL;


	/* Enable interrupts */
	sc->intmask =  (OHCI_INT_ERR | OHCI_INT_PHY_SID
			| OHCI_INT_DMA_ATRQ | OHCI_INT_DMA_ATRS
			| OHCI_INT_DMA_PRRQ | OHCI_INT_DMA_PRRS
			| OHCI_INT_PHY_BUS_R | OHCI_INT_PW_ERR);
	sc->intmask |= OHCI_INT_DMA_IR | OHCI_INT_DMA_IT;
	sc->intmask |= OHCI_INT_CYC_LOST | OHCI_INT_PHY_INT;
	OWRITE(sc, FWOHCI_INTMASK, sc->intmask);
	fwohci_set_intr(&sc->fc, 1);
}

#define LAST_DB(dbtr) \
	&dbtr->db[(dbtr->dbcnt > 2) ? (dbtr->dbcnt - 1) : 0];

static void
fwohci_execute_db(struct fwohcidb_tr *db_tr, bus_dmamap_t dmamap)
{
	struct fwohcidb *db;
	bus_dma_segment_t *s;
	int i;

	db = &db_tr->db[db_tr->dbcnt];
	for (i = 0; i < dmamap->dm_nsegs; i++) {
		s = &dmamap->dm_segs[i];
		FWOHCI_DMA_WRITE(db->db.desc.addr, s->ds_addr);
		FWOHCI_DMA_WRITE(db->db.desc.cmd, s->ds_len);
 		FWOHCI_DMA_WRITE(db->db.desc.res, 0);
		db++;
		db_tr->dbcnt++;
	}
}

static void
fwohci_start(struct fwohci_softc *sc, struct fwohci_dbch *dbch)
{
	struct fw_xfer *xfer;
	struct fw_pkt *fp;
	struct fwohci_txpkthdr *ohcifp;
	struct fwohcidb_tr *db_tr, *kick;
	struct fwohcidb *db;
	uint32_t *ld;
	int tcode, hdr_len, pl_off, fsegment = -1, i;
	const struct tcode_info *info;
	static int maxdesc = 0;

	KASSERT(mutex_owned(&dbch->xferq.q_mtx));

#if DIAGNOSTIC
	if (dbch->off != OHCI_ATQOFF &&
	    dbch->off != OHCI_ATSOFF)
		panic("not async tx");
#endif

	if (dbch->flags & FWOHCI_DBCH_FULL)
		return;

	db_tr = dbch->top;
	kick = db_tr;
	if (dbch->pdb_tr != NULL) {
		kick = dbch->pdb_tr;
		fwdma_sync_multiseg(dbch->am, kick->idx, kick->idx,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	}
txloop:
	xfer = STAILQ_FIRST(&dbch->xferq.q);
	if (xfer == NULL)
		goto kick;
#if 0
	if (dbch->xferq.queued == 0)
		aprint_error_dev(sc->fc.dev, "TX queue empty\n");
#endif
	STAILQ_REMOVE_HEAD(&dbch->xferq.q, link);
	db_tr->xfer = xfer;
	xfer->flag = FWXF_START;

	fp = &xfer->send.hdr;
	tcode = fp->mode.common.tcode;

	ohcifp = (struct fwohci_txpkthdr *) db_tr->db[1].db.immed;
	info = &tinfo[tcode];
	hdr_len = pl_off = info->hdr_len;

	ld = ohcifp->mode.ld;
	ld[0] = ld[1] = ld[2] = ld[3] = 0;
	for (i = 0; i < pl_off / 4; i++)
		ld[i] = fp->mode.ld[i];

	ohcifp->mode.common.spd = xfer->send.spd & 0x7;
	if (tcode == FWTCODE_STREAM) {
		hdr_len = 8;
		ohcifp->mode.stream.len = fp->mode.stream.len;
	} else if (tcode == FWTCODE_PHY) {
		hdr_len = 12;
		ld[1] = fp->mode.ld[1];
		ld[2] = fp->mode.ld[2];
		ohcifp->mode.common.spd = 0;
		ohcifp->mode.common.tcode = FWOHCITCODE_PHY;
	} else {
		ohcifp->mode.asycomm.dst = fp->mode.hdr.dst;
		ohcifp->mode.asycomm.srcbus = OHCI_ASYSRCBUS;
		ohcifp->mode.asycomm.tlrt |= FWRETRY_X;
	}
	db = db_tr->db;
 	FWOHCI_DMA_WRITE(db->db.desc.cmd,
	    OHCI_OUTPUT_MORE | OHCI_KEY_ST2 | hdr_len);
 	FWOHCI_DMA_WRITE(db->db.desc.addr, 0);
 	FWOHCI_DMA_WRITE(db->db.desc.res, 0);
/* Specify bound timer of asy. responce */
	if (dbch->off != OHCI_ATSOFF)
 		FWOHCI_DMA_WRITE(db->db.desc.res,
		     (OREAD(sc, OHCI_CYCLETIMER) >> 12) + (1 << 13));
#if BYTE_ORDER == BIG_ENDIAN
	if (tcode == FWTCODE_WREQQ || tcode == FWTCODE_RRESQ)
		hdr_len = 12;
	for (i = 0; i < hdr_len / 4; i++)
		FWOHCI_DMA_WRITE(ld[i], ld[i]);
#endif

again:
	db_tr->dbcnt = 2;
	db = &db_tr->db[db_tr->dbcnt];
	if (xfer->send.pay_len > 0) {
		int err;
		/* handle payload */
		if (xfer->mbuf == NULL)
			err = bus_dmamap_load(sc->fc.dmat, db_tr->dma_map,
			    xfer->send.payload, xfer->send.pay_len, NULL,
			    BUS_DMA_WAITOK);
		else {
			/* XXX we can handle only 6 (=8-2) mbuf chains */
			err = bus_dmamap_load_mbuf(sc->fc.dmat, db_tr->dma_map,
			    xfer->mbuf, BUS_DMA_WAITOK);
			if (err == EFBIG) {
				struct mbuf *m0;

				if (firewire_debug)
					printf("EFBIG.\n");
				m0 = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
				if (m0 != NULL) {
					m_copydata(xfer->mbuf, 0,
					    xfer->mbuf->m_pkthdr.len,
					    mtod(m0, void *));
					m0->m_len = m0->m_pkthdr.len =
					    xfer->mbuf->m_pkthdr.len;
					m_freem(xfer->mbuf);
					xfer->mbuf = m0;
					goto again;
				}
				aprint_error_dev(sc->fc.dev,
				    "m_getcl failed.\n");
			}
		}
		if (!err)
			fwohci_execute_db(db_tr, db_tr->dma_map);
		else
			aprint_error_dev(sc->fc.dev,
			    "dmamap_load: err=%d\n", err);
		bus_dmamap_sync(sc->fc.dmat, db_tr->dma_map,
		    0, db_tr->dma_map->dm_mapsize, BUS_DMASYNC_PREWRITE);
#if 0 /* OHCI_OUTPUT_MODE == 0 */
		for (i = 2; i < db_tr->dbcnt; i++)
			FWOHCI_DMA_SET(db_tr->db[i].db.desc.cmd,
			    OHCI_OUTPUT_MORE);
#endif
	}
	if (maxdesc < db_tr->dbcnt) {
		maxdesc = db_tr->dbcnt;
		if (firewire_debug)
			printf("maxdesc: %d\n", maxdesc);
	}
	/* last db */
	db = LAST_DB(db_tr);
 	FWOHCI_DMA_SET(db->db.desc.cmd,
	    OHCI_OUTPUT_LAST | OHCI_INTERRUPT_ALWAYS | OHCI_BRANCH_ALWAYS);
 	FWOHCI_DMA_WRITE(db->db.desc.depend,
	    STAILQ_NEXT(db_tr, link)->bus_addr);

	if (fsegment == -1)
		fsegment = db_tr->dbcnt;
	if (dbch->pdb_tr != NULL) {
		db = LAST_DB(dbch->pdb_tr);
 		FWOHCI_DMA_SET(db->db.desc.depend, db_tr->dbcnt);
	}
	dbch->xferq.queued++;
	dbch->pdb_tr = db_tr;
	db_tr = STAILQ_NEXT(db_tr, link);
	if (db_tr != dbch->bottom)
		goto txloop;
	else {
		aprint_error_dev(sc->fc.dev, "fwohci_start: lack of db_trq\n");
		dbch->flags |= FWOHCI_DBCH_FULL;
	}
kick:
	/* kick asy q */
	fwdma_sync_multiseg(dbch->am, kick->idx, dbch->pdb_tr->idx,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	if (dbch->xferq.flag & FWXFERQ_RUNNING)
		OWRITE(sc, OHCI_DMACTL(dbch->off), OHCI_CNTL_DMA_WAKE);
	else {
		if (firewire_debug)
			printf("start AT DMA status=%x\n",
			    OREAD(sc, OHCI_DMACTL(dbch->off)));
		OWRITE(sc, OHCI_DMACMD(dbch->off),
		    dbch->top->bus_addr | fsegment);
		OWRITE(sc, OHCI_DMACTL(dbch->off), OHCI_CNTL_DMA_RUN);
		dbch->xferq.flag |= FWXFERQ_RUNNING;
	}

	dbch->top = db_tr;
	return;
}

static void
fwohci_start_atq(struct firewire_comm *fc)
{
	struct fwohci_softc *sc = (struct fwohci_softc *)fc;
	struct fwohci_dbch *dbch = &sc->atrq;

	mutex_enter(&dbch->xferq.q_mtx);
	fwohci_start(sc, dbch);
	mutex_exit(&dbch->xferq.q_mtx);
	return;
}

static void
fwohci_start_ats(struct firewire_comm *fc)
{
	struct fwohci_softc *sc = (struct fwohci_softc *)fc;
	struct fwohci_dbch *dbch = &sc->atrs;

	mutex_enter(&dbch->xferq.q_mtx);
	fwohci_start(sc, dbch);
	mutex_exit(&dbch->xferq.q_mtx);
	return;
}

static void
fwohci_txd(struct fwohci_softc *sc, struct fwohci_dbch *dbch)
{
	struct firewire_comm *fc = &sc->fc;
	struct fwohcidb_tr *tr;
	struct fwohcidb *db;
	struct fw_xfer *xfer;
	u_int stat, status;
	int packets = 0, ch, err = 0;

#if DIAGNOSTIC
	if (dbch->off != OHCI_ATQOFF &&
	    dbch->off != OHCI_ATSOFF)
		panic("not async tx");
#endif
	if (dbch->off == OHCI_ATQOFF)
		ch = ATRQ_CH;
	else	/* OHCI_ATSOFF */
		ch = ATRS_CH;

	mutex_enter(&dbch->xferq.q_mtx);
	tr = dbch->bottom;
	while (dbch->xferq.queued > 0) {
		fwdma_sync_multiseg(dbch->am, tr->idx, tr->idx,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		db = LAST_DB(tr);
		status = FWOHCI_DMA_READ(db->db.desc.res) >> OHCI_STATUS_SHIFT;
		if (!(status & OHCI_CNTL_DMA_ACTIVE))
			if (fc->status != FWBUSINIT)
				goto out;
		if (tr->xfer->send.pay_len > 0) {
			bus_dmamap_sync(fc->dmat, tr->dma_map,
			    0, tr->dma_map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(fc->dmat, tr->dma_map);
		}
#if 1
		if (firewire_debug > 1)
			dump_db(sc, ch);
#endif
		if (status & OHCI_CNTL_DMA_DEAD) {
			/* Stop DMA */
			OWRITE(sc, OHCI_DMACTLCLR(dbch->off),
			    OHCI_CNTL_DMA_RUN);
			aprint_error_dev(fc->dev, "force reset AT FIFO\n");
			OWRITE(sc, OHCI_HCCCTLCLR, OHCI_HCC_LINKEN);
			OWRITE(sc, OHCI_HCCCTL, OHCI_HCC_LPS | OHCI_HCC_LINKEN);
			OWRITE(sc, OHCI_DMACTLCLR(dbch->off),
			    OHCI_CNTL_DMA_RUN);
		}
		stat = status & FWOHCIEV_MASK;
		switch(stat) {
		case FWOHCIEV_ACKPEND:

			/* FALLTHROUGH */

		case FWOHCIEV_ACKCOMPL:
			err = 0;
			break;

		case FWOHCIEV_ACKBSA:
		case FWOHCIEV_ACKBSB:
		case FWOHCIEV_ACKBSX:
			aprint_error_dev(fc->dev, "txd err=%2x %s\n", stat,
			    fwohcicode[stat]);
			err = EBUSY;
			break;

		case FWOHCIEV_FLUSHED:
		case FWOHCIEV_ACKTARD:
			aprint_error_dev(fc->dev, "txd err=%2x %s\n", stat,
			    fwohcicode[stat]);
			err = EAGAIN;
			break;

		case FWOHCIEV_MISSACK:
		case FWOHCIEV_UNDRRUN:
		case FWOHCIEV_OVRRUN:
		case FWOHCIEV_DESCERR:
		case FWOHCIEV_DTRDERR:
		case FWOHCIEV_TIMEOUT:
		case FWOHCIEV_TCODERR:
		case FWOHCIEV_UNKNOWN:
		case FWOHCIEV_ACKDERR:
		case FWOHCIEV_ACKTERR:
		default:
			aprint_error_dev(fc->dev, "txd err=%2x %s\n", stat,
			    fwohcicode[stat]);
			err = EINVAL;
			break;
		}
		dbch->xferq.queued--;
		dbch->bottom = STAILQ_NEXT(tr, link);
		if (tr->xfer != NULL) {
			xfer = tr->xfer;
			tr->xfer = NULL;
			mutex_exit(&dbch->xferq.q_mtx);
			if (xfer->flag & FWXF_RCVD) {
#if 0
				if (firewire_debug)
					printf("already rcvd\n");
#endif
				fw_xfer_done(xfer);
			} else {
				microtime(&xfer->tv);
				xfer->flag = FWXF_SENT;
				if (err == EBUSY) {
					xfer->flag = FWXF_BUSY;
					xfer->resp = err;
					xfer->recv.pay_len = 0;
					fw_xfer_done(xfer);
				} else if (stat != FWOHCIEV_ACKPEND) {
					if (stat != FWOHCIEV_ACKCOMPL)
						xfer->flag = FWXF_SENTERR;
					xfer->resp = err;
					xfer->recv.pay_len = 0;
					fw_xfer_done(xfer);
				}
			}
			mutex_enter(&dbch->xferq.q_mtx);
			/*
			 * The watchdog timer takes care of split
			 * transcation timeout for ACKPEND case.
			 */
		} else
			aprint_error_dev(fc->dev, "this shouldn't happen\n");
		packets++;
		if (dbch->bottom == dbch->top) {
			/* we reaches the end of context program */
			if (firewire_debug && dbch->xferq.queued > 0)
				printf("queued > 0\n");
			break;
		}
		tr = dbch->bottom;
	}
out:
	if (dbch->xferq.queued > 0 || packets > 0)
		fwdma_sync_multiseg(dbch->am, tr->idx, tr->idx,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	if ((dbch->flags & FWOHCI_DBCH_FULL) && packets > 0) {
		aprint_normal_dev(fc->dev, "make free slot\n");
		dbch->flags &= ~FWOHCI_DBCH_FULL;
		fwohci_start(sc, dbch);
	}
	mutex_exit(&dbch->xferq.q_mtx);
}

static void
fwohci_db_free(struct fwohci_softc *sc, struct fwohci_dbch *dbch)
{
	struct fwohcidb_tr *db_tr, *last;

	if ((dbch->flags & FWOHCI_DBCH_INIT) == 0)
		return;

	for (last = db_tr = STAILQ_FIRST(&dbch->db_trq); db_tr != last;
	    db_tr = STAILQ_NEXT(db_tr, link)) {
		bus_dmamap_destroy(sc->fc.dmat, db_tr->dma_map);
		if ((dbch->xferq.flag & FWXFERQ_EXTBUF) == 0 &&
		    db_tr->buf != NULL) {
			fwdma_free(sc->fc.dmat, db_tr->dma_map, db_tr->buf);
			db_tr->buf = NULL;
		}
	}
	dbch->ndb = 0;
	db_tr = STAILQ_FIRST(&dbch->db_trq);
	fwdma_free_multiseg(dbch->am);
	free(db_tr, M_FW);
	STAILQ_INIT(&dbch->db_trq);
	dbch->flags &= ~FWOHCI_DBCH_INIT;
	seldestroy(&dbch->xferq.rsel);
}

static void
fwohci_db_init(struct fwohci_softc *sc, struct fwohci_dbch *dbch)
{
	struct firewire_comm *fc = &sc->fc;
	struct fwohcidb_tr *db_tr, *lastq, *tmpq;
	int idb;
	const int db_tr_sz = sizeof(struct fwohcidb_tr) * dbch->ndb;

	if ((dbch->flags & FWOHCI_DBCH_INIT) != 0)
		goto out;

	/* allocate DB entries and attach one to each DMA channels */
	/* DB entry must start at 16 bytes bounary. */
	STAILQ_INIT(&dbch->db_trq);
	db_tr = (struct fwohcidb_tr *)malloc(db_tr_sz, M_FW, M_WAITOK | M_ZERO);
	if (db_tr == NULL) {
		aprint_error_dev(fc->dev, "malloc(1) failed\n");
		return;
	}

#define DB_SIZE(x) (sizeof(struct fwohcidb) * (x)->ndesc)
	dbch->am = fwdma_malloc_multiseg(fc, DB_SIZE(dbch), DB_SIZE(dbch),
#if 0
	    dbch->ndb, BUS_DMA_WAITOK);
#else	/* Ooops, debugging now... */
	    dbch->ndb, BUS_DMA_WAITOK |
		(dbch->off == OHCI_ARQOFF || dbch->off == OHCI_ARSOFF) ?
							BUS_DMA_COHERENT : 0);
#endif
	if (dbch->am == NULL) {
		aprint_error_dev(fc->dev, "fwdma_malloc_multiseg failed\n");
		free(db_tr, M_FW);
		return;
	}
	/* Attach DB to DMA ch. */
	for (idb = 0; idb < dbch->ndb; idb++) {
		db_tr->idx = idb;
		db_tr->dbcnt = 0;
		db_tr->db = (struct fwohcidb *)fwdma_v_addr(dbch->am, idb);
		db_tr->bus_addr = fwdma_bus_addr(dbch->am, idb);
		/* create dmamap for buffers */
#define MAX_REQCOUNT	0xffff
		if (bus_dmamap_create(fc->dmat, dbch->xferq.psize,
		    dbch->ndesc > 3 ? dbch->ndesc - 2 : 1, MAX_REQCOUNT, 0,
		    0, &db_tr->dma_map) != 0) {
			aprint_error_dev(fc->dev, "bus_dmamap_create failed\n");
			dbch->flags = FWOHCI_DBCH_INIT; /* XXX fake */
			fwohci_db_free(sc, dbch);
			return;
		}
		if (dbch->off == OHCI_ARQOFF ||
		    dbch->off == OHCI_ARSOFF) {
			db_tr->buf = fwdma_malloc(fc->dev, fc->dmat,
			    &db_tr->dma_map, dbch->xferq.psize, 1,
			    BUS_DMA_NOWAIT);
			if (db_tr->buf == NULL) {
				aprint_error_dev(fc->dev,
				    "fwdma_malloc failed\n");
				dbch->flags = FWOHCI_DBCH_INIT; /* XXX fake */
				fwohci_db_free(sc, dbch);
				return;
			}
		}
		STAILQ_INSERT_TAIL(&dbch->db_trq, db_tr, link);
		if (dbch->xferq.flag & FWXFERQ_EXTBUF) {
			struct fw_bulkxfer *bulkxfer =
			    &dbch->xferq.bulkxfer[idb / dbch->xferq.bnpacket];

			if (idb % dbch->xferq.bnpacket == 0)
				bulkxfer->start = (void *)db_tr;
			if ((idb + 1) % dbch->xferq.bnpacket == 0)
				bulkxfer->end = (void *)db_tr;
		}
		db_tr++;
	}
	lastq = NULL;
	STAILQ_FOREACH(tmpq, &dbch->db_trq, link)
		lastq = tmpq;
	lastq->link.stqe_next = STAILQ_FIRST(&dbch->db_trq);
out:
	dbch->xferq.queued = 0;
	dbch->pdb_tr = NULL;
	dbch->top = STAILQ_FIRST(&dbch->db_trq);
	dbch->bottom = dbch->top;
	dbch->flags = FWOHCI_DBCH_INIT;
	selinit(&dbch->xferq.rsel);
}

static int
fwohci_tx_enable(struct fwohci_softc *sc, struct fwohci_dbch *dbch)
{
	int err = 0;
	int idb, z, i, dmach = 0, ldesc;
	struct fwohcidb_tr *db_tr;
	struct fwohcidb *db;

	if (!(dbch->xferq.flag & FWXFERQ_EXTBUF)) {
		err = EINVAL;
		return err;
	}
	z = dbch->ndesc;
	for (dmach = 0; dmach < sc->fc.nisodma; dmach++)
		if (dbch->off == sc->it[dmach].off)
			break;
	if (dmach == sc->fc.nisodma) {
		err = EINVAL;
		return err;
	}
	if (dbch->xferq.flag & FWXFERQ_RUNNING)
		return err;
	dbch->xferq.flag |= FWXFERQ_RUNNING;
	for (i = 0, dbch->bottom = dbch->top; i < dbch->ndb - 1; i++)
		dbch->bottom = STAILQ_NEXT(dbch->bottom, link);
	db_tr = dbch->top;
	for (idb = 0; idb < dbch->ndb; idb++) {
		fwohci_add_tx_buf(dbch, db_tr, idb);
		if (STAILQ_NEXT(db_tr, link) == NULL)
			break;
		db = db_tr->db;
		ldesc = db_tr->dbcnt - 1;
		FWOHCI_DMA_WRITE(db[0].db.desc.depend,
		    STAILQ_NEXT(db_tr, link)->bus_addr | z);
		db[ldesc].db.desc.depend = db[0].db.desc.depend;
		if (dbch->xferq.flag & FWXFERQ_EXTBUF) {
			if (((idb + 1) % dbch->xferq.bnpacket) == 0) {
				FWOHCI_DMA_SET(db[ldesc].db.desc.cmd,
				    OHCI_INTERRUPT_ALWAYS);
				/* OHCI 1.1 and above */
				FWOHCI_DMA_SET(db[0].db.desc.cmd,
				    OHCI_INTERRUPT_ALWAYS);
			}
		}
		db_tr = STAILQ_NEXT(db_tr, link);
	}
	FWOHCI_DMA_CLEAR(
	    dbch->bottom->db[dbch->bottom->dbcnt - 1].db.desc.depend, 0xf);
	return err;
}

static int
fwohci_rx_enable(struct fwohci_softc *sc, struct fwohci_dbch *dbch)
{
	struct fwohcidb_tr *db_tr;
	struct fwohcidb *db;
	int idb, z, i, ldesc, err = 0;

	z = dbch->ndesc;
	if (dbch->xferq.flag & FWXFERQ_STREAM) {
		if (dbch->xferq.flag & FWXFERQ_RUNNING)
			return err;
	} else
		if (dbch->xferq.flag & FWXFERQ_RUNNING) {
			err = EBUSY;
			return err;
		}
	dbch->xferq.flag |= FWXFERQ_RUNNING;
	dbch->top = STAILQ_FIRST(&dbch->db_trq);
	for (i = 0, dbch->bottom = dbch->top; i < dbch->ndb - 1; i++)
		dbch->bottom = STAILQ_NEXT(dbch->bottom, link);
	db_tr = dbch->top;
	if (db_tr->dbcnt != 0)
		goto run;
	for (idb = 0; idb < dbch->ndb; idb++) {
		if (dbch->off == OHCI_ARQOFF ||
		    dbch->off == OHCI_ARSOFF)
			bus_dmamap_sync(sc->fc.dmat, db_tr->dma_map,
			    0, db_tr->dma_map->dm_mapsize, BUS_DMASYNC_PREREAD);
		fwohci_add_rx_buf(dbch, db_tr, idb, &sc->dummy_dma);
		if (STAILQ_NEXT(db_tr, link) == NULL)
			break;
		db = db_tr->db;
		ldesc = db_tr->dbcnt - 1;
		FWOHCI_DMA_WRITE(db[ldesc].db.desc.depend,
		    STAILQ_NEXT(db_tr, link)->bus_addr | z);
		if (dbch->xferq.flag & FWXFERQ_EXTBUF) {
			if (((idb + 1) % dbch->xferq.bnpacket) == 0) {
				FWOHCI_DMA_SET(db[ldesc].db.desc.cmd,
				    OHCI_INTERRUPT_ALWAYS);
				FWOHCI_DMA_CLEAR(db[ldesc].db.desc.depend, 0xf);
			}
		}
		db_tr = STAILQ_NEXT(db_tr, link);
	}
	FWOHCI_DMA_CLEAR(dbch->bottom->db[db_tr->dbcnt - 1].db.desc.depend,
	    0xf);
	dbch->buf_offset = 0;
run:
	fwdma_sync_multiseg_all(dbch->am,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	if (!(dbch->xferq.flag & FWXFERQ_STREAM)) {
		OWRITE(sc, OHCI_DMACMD(dbch->off), dbch->top->bus_addr | z);
		OWRITE(sc, OHCI_DMACTL(dbch->off), OHCI_CNTL_DMA_RUN);
	}
	return err;
}

static int
fwohci_next_cycle(struct fwohci_softc *sc, int cycle_now)
{
	int sec, cycle, cycle_match;

	cycle = cycle_now & 0x1fff;
	sec = cycle_now >> 13;
#define CYCLE_MOD	0x10
#if 1
#define CYCLE_DELAY	8	/* min delay to start DMA */
#else
#define CYCLE_DELAY	7000	/* min delay to start DMA */
#endif
	cycle = cycle + CYCLE_DELAY;
	if (cycle >= 8000) {
		sec++;
		cycle -= 8000;
	}
	cycle = roundup2(cycle, CYCLE_MOD);
	if (cycle >= 8000) {
		sec++;
		if (cycle == 8000)
			cycle = 0;
		else
			cycle = CYCLE_MOD;
	}
	cycle_match = ((sec << 13) | cycle) & 0x7ffff;

	return cycle_match;
}

#ifdef OHCI_DEBUG
static void
fwohci_dump_intr(struct fwohci_softc *sc, uint32_t stat)
{

	if (stat & OREAD(sc, FWOHCI_INTMASK))
		print("%s: INTERRUPT"
		    " < %s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s>"
		    " 0x%08x, 0x%08x\n",
		    device_xname(fc->dev),
		    stat & OHCI_INT_EN ? "DMA_EN ":"",
		    stat & OHCI_INT_PHY_REG ? "PHY_REG ":"",
		    stat & OHCI_INT_CYC_LONG ? "CYC_LONG ":"",
		    stat & OHCI_INT_ERR ? "INT_ERR ":"",
		    stat & OHCI_INT_CYC_ERR ? "CYC_ERR ":"",
		    stat & OHCI_INT_CYC_LOST ? "CYC_LOST ":"",
		    stat & OHCI_INT_CYC_64SECOND ? "CYC_64SECOND ":"",
		    stat & OHCI_INT_CYC_START ? "CYC_START ":"",
		    stat & OHCI_INT_PHY_INT ? "PHY_INT ":"",
		    stat & OHCI_INT_PHY_BUS_R ? "BUS_RESET ":"",
		    stat & OHCI_INT_PHY_SID ? "SID ":"",
		    stat & OHCI_INT_LR_ERR ? "DMA_LR_ERR ":"",
		    stat & OHCI_INT_PW_ERR ? "DMA_PW_ERR ":"",
		    stat & OHCI_INT_DMA_IR ? "DMA_IR ":"",
		    stat & OHCI_INT_DMA_IT ? "DMA_IT " :"",
		    stat & OHCI_INT_DMA_PRRS ? "DMA_PRRS " :"",
		    stat & OHCI_INT_DMA_PRRQ ? "DMA_PRRQ " :"",
		    stat & OHCI_INT_DMA_ARRS ? "DMA_ARRS " :"",
		    stat & OHCI_INT_DMA_ARRQ ? "DMA_ARRQ " :"",
		    stat & OHCI_INT_DMA_ATRS ? "DMA_ATRS " :"",
		    stat & OHCI_INT_DMA_ATRQ ? "DMA_ATRQ " :"",
		    stat, OREAD(sc, FWOHCI_INTMASK)
		);
}
#endif

static void
fwohci_intr_core(struct fwohci_softc *sc, uint32_t stat)
{
	struct firewire_comm *fc = &sc->fc;
	uint32_t node_id, plen;

	if ((stat & OHCI_INT_PHY_BUS_R) && (fc->status != FWBUSRESET)) {
		fc->status = FWBUSRESET;
		/* Disable bus reset interrupt until sid recv. */
		OWRITE(sc, FWOHCI_INTMASKCLR, OHCI_INT_PHY_BUS_R);

		aprint_normal_dev(fc->dev, "BUS reset\n");
		OWRITE(sc, FWOHCI_INTMASKCLR, OHCI_INT_CYC_LOST);
		OWRITE(sc, OHCI_LNKCTLCLR, OHCI_CNTL_CYCSRC);

		OWRITE(sc, OHCI_ATQCTLCLR, OHCI_CNTL_DMA_RUN);
		sc->atrq.xferq.flag &= ~FWXFERQ_RUNNING;
		OWRITE(sc, OHCI_ATSCTLCLR, OHCI_CNTL_DMA_RUN);
		sc->atrs.xferq.flag &= ~FWXFERQ_RUNNING;

		fw_busreset(&sc->fc, FWBUSRESET);
		OWRITE(sc, OHCI_CROMHDR, ntohl(sc->fc.config_rom[0]));
		OWRITE(sc, OHCI_BUS_OPT, ntohl(sc->fc.config_rom[2]));
	}
	if (stat & OHCI_INT_PHY_SID) {
		/* Enable bus reset interrupt */
		OWRITE(sc, FWOHCI_INTSTATCLR, OHCI_INT_PHY_BUS_R);
		OWRITE(sc, FWOHCI_INTMASK, OHCI_INT_PHY_BUS_R);

		/* Allow async. request to us */
		OWRITE(sc, OHCI_AREQHI, 1 << 31);
		if (firewire_phydma_enable) {
			/* allow from all nodes */
			OWRITE(sc, OHCI_PREQHI, 0x7fffffff);
			OWRITE(sc, OHCI_PREQLO, 0xffffffff);
			/* 0 to 4GB region */
			OWRITE(sc, OHCI_PREQUPPER, 0x10000);
		}
		/* Set ATRetries register */
		OWRITE(sc, OHCI_ATRETRY, 1<<(13+16) | 0xfff);

		/*
		 * Checking whether the node is root or not. If root, turn on
		 * cycle master.
		 */
		node_id = OREAD(sc, FWOHCI_NODEID);
		plen = OREAD(sc, OHCI_SID_CNT);

		fc->nodeid = node_id & 0x3f;
		aprint_normal_dev(fc->dev, "node_id=0x%08x, gen=%d, ",
		    node_id, (plen >> 16) & 0xff);
		if (!(node_id & OHCI_NODE_VALID)) {
			aprint_error_dev(fc->dev, "Bus reset failure\n");
			goto sidout;
		}

		/* cycle timer */
		sc->cycle_lost = 0;
		OWRITE(sc, FWOHCI_INTMASK, OHCI_INT_CYC_LOST);
		if ((node_id & OHCI_NODE_ROOT) && !nocyclemaster) {
			aprint_normal("CYCLEMASTER mode\n");
			OWRITE(sc, OHCI_LNKCTL,
			    OHCI_CNTL_CYCMTR | OHCI_CNTL_CYCTIMER);
		} else {
			aprint_normal("non CYCLEMASTER mode\n");
			OWRITE(sc, OHCI_LNKCTLCLR, OHCI_CNTL_CYCMTR);
			OWRITE(sc, OHCI_LNKCTL, OHCI_CNTL_CYCTIMER);
		}

		fc->status = FWBUSINIT;

		fwohci_task_sid(sc);
	}
sidout:
	if ((stat & ~(OHCI_INT_PHY_BUS_R | OHCI_INT_PHY_SID)))
		fwohci_task_dma(sc);
}

static void
fwohci_intr_dma(struct fwohci_softc *sc, uint32_t stat)
{
	struct firewire_comm *fc = &sc->fc;
	uint32_t irstat, itstat;
	u_int i;

	if (stat & OHCI_INT_DMA_IR) {
		irstat = atomic_swap_32(&sc->irstat, 0);
		for (i = 0; i < fc->nisodma; i++)
			if ((irstat & (1 << i)) != 0) {
				struct fwohci_dbch *dbch = &sc->ir[i];

				if ((dbch->xferq.flag & FWXFERQ_OPEN) == 0) {
					aprint_error_dev(fc->dev,
					    "dma(%d) not active\n", i);
					continue;
				}
				fwohci_rbuf_update(sc, i);
			}
	}
	if (stat & OHCI_INT_DMA_IT) {
		itstat = atomic_swap_32(&sc->itstat, 0);
		for (i = 0; i < fc->nisodma; i++)
			if ((itstat & (1 << i)) != 0)
				fwohci_tbuf_update(sc, i);
	}
	if (stat & OHCI_INT_DMA_PRRS) {
#if 0
		dump_dma(sc, ARRS_CH);
		dump_db(sc, ARRS_CH);
#endif
		fwohci_arcv(sc, &sc->arrs);
	}
	if (stat & OHCI_INT_DMA_PRRQ) {
#if 0
		dump_dma(sc, ARRQ_CH);
		dump_db(sc, ARRQ_CH);
#endif
		fwohci_arcv(sc, &sc->arrq);
	}
	if (stat & OHCI_INT_CYC_LOST) {
		if (sc->cycle_lost >= 0)
			sc->cycle_lost++;
		if (sc->cycle_lost > 10) {
			sc->cycle_lost = -1;
#if 0
			OWRITE(sc, OHCI_LNKCTLCLR, OHCI_CNTL_CYCTIMER);
#endif
			OWRITE(sc, FWOHCI_INTMASKCLR, OHCI_INT_CYC_LOST);
			aprint_error_dev(fc->dev, "too many cycle lost, "
			    "no cycle master present?\n");
		}
	}
	if (stat & OHCI_INT_DMA_ATRQ)
		fwohci_txd(sc, &(sc->atrq));
	if (stat & OHCI_INT_DMA_ATRS)
		fwohci_txd(sc, &(sc->atrs));
	if (stat & OHCI_INT_PW_ERR)
		aprint_error_dev(fc->dev, "posted write error\n");
	if (stat & OHCI_INT_ERR)
		aprint_error_dev(fc->dev, "unrecoverable error\n");
	if (stat & OHCI_INT_PHY_INT)
		aprint_normal_dev(fc->dev, "phy int\n");

	return;
}

static void
fwohci_task_sid(struct fwohci_softc *sc)
{
	struct firewire_comm *fc = &sc->fc;
	uint32_t *buf;
	int i, plen;

	plen = OREAD(sc, OHCI_SID_CNT);

	if (plen & OHCI_SID_ERR) {
		aprint_error_dev(fc->dev, "SID Error\n");
		return;
	}
	plen &= OHCI_SID_CNT_MASK;
	if (plen < 4 || plen > OHCI_SIDSIZE) {
		aprint_error_dev(fc->dev, "invalid SID len = %d\n", plen);
		return;
	}
	plen -= 4; /* chop control info */
	buf = (uint32_t *)malloc(OHCI_SIDSIZE, M_FW, M_NOWAIT);
	if (buf == NULL) {
		aprint_error_dev(fc->dev, "malloc failed\n");
		return;
	}
	for (i = 0; i < plen / 4; i++)
		buf[i] = FWOHCI_DMA_READ(sc->sid_buf[i + 1]);
#if 1 /* XXX needed?? */
	/* pending all pre-bus_reset packets */
	fwohci_txd(sc, &sc->atrq);
	fwohci_txd(sc, &sc->atrs);
	fwohci_arcv(sc, &sc->arrs);
	fwohci_arcv(sc, &sc->arrq);
	fw_drain_txq(fc);
#endif
	fw_sidrcv(fc, buf, plen);
	free(buf, M_FW);
}

static void
fwohci_task_dma(struct fwohci_softc *sc)
{
	uint32_t stat;

again:
	stat = atomic_swap_32(&sc->intstat, 0);
	if (stat)
		fwohci_intr_dma(sc, stat);
	else
		return;
	goto again;
}

static void
fwohci_tbuf_update(struct fwohci_softc *sc, int dmach)
{
	struct firewire_comm *fc = &sc->fc;
	struct fwohcidb *db;
	struct fw_bulkxfer *chunk;
	struct fw_xferq *it;
	uint32_t stat;
#if 0
	uint32_t count;
#endif
	int w = 0, ldesc;

	it = fc->it[dmach];
	ldesc = sc->it[dmach].ndesc - 1;
	mutex_enter(&fc->fc_mtx);
	fwdma_sync_multiseg_all(sc->it[dmach].am, BUS_DMASYNC_POSTREAD);
	if (firewire_debug)
		dump_db(sc, ITX_CH + dmach);
	while ((chunk = STAILQ_FIRST(&it->stdma)) != NULL) {
		db = ((struct fwohcidb_tr *)(chunk->end))->db;
		stat =
		    FWOHCI_DMA_READ(db[ldesc].db.desc.res) >> OHCI_STATUS_SHIFT;
		db = ((struct fwohcidb_tr *)(chunk->start))->db;
		/* timestamp */
#if 0
		count =
		    FWOHCI_DMA_READ(db[ldesc].db.desc.res) & OHCI_COUNT_MASK;
#else
		(void)FWOHCI_DMA_READ(db[ldesc].db.desc.res);
#endif
		if (stat == 0)
			break;
		STAILQ_REMOVE_HEAD(&it->stdma, link);
		switch (stat & FWOHCIEV_MASK) {
		case FWOHCIEV_ACKCOMPL:
#if 0
			printf("0x%08x\n", count);
#endif
			break;
		default:
			aprint_error_dev(fc->dev,
			    "Isochronous transmit err %02x(%s)\n",
			    stat, fwohcicode[stat & 0x1f]);
		}
		STAILQ_INSERT_TAIL(&it->stfree, chunk, link);
		w++;
	}
	mutex_exit(&fc->fc_mtx);
	if (w)
		wakeup(it);
}

static void
fwohci_rbuf_update(struct fwohci_softc *sc, int dmach)
{
	struct firewire_comm *fc = &sc->fc;
	struct fwohcidb_tr *db_tr;
	struct fw_bulkxfer *chunk;
	struct fw_xferq *ir;
	uint32_t stat;
	int w = 0, ldesc;

	ir = fc->ir[dmach];
	ldesc = sc->ir[dmach].ndesc - 1;

#if 0
	dump_db(sc, dmach);
#endif
	if ((ir->flag & FWXFERQ_HANDLER) == 0)
		mutex_enter(&fc->fc_mtx);
	fwdma_sync_multiseg_all(sc->ir[dmach].am, BUS_DMASYNC_POSTREAD);
	while ((chunk = STAILQ_FIRST(&ir->stdma)) != NULL) {
		db_tr = (struct fwohcidb_tr *)chunk->end;
		stat = FWOHCI_DMA_READ(db_tr->db[ldesc].db.desc.res) >>
		    OHCI_STATUS_SHIFT;
		if (stat == 0)
			break;

		if (chunk->mbuf != NULL) {
			bus_dmamap_sync(fc->dmat, db_tr->dma_map, 0,
			    db_tr->dma_map->dm_mapsize, BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(fc->dmat, db_tr->dma_map);
		} else if (ir->buf != NULL)
			fwdma_sync_multiseg(ir->buf, chunk->poffset,
			    ir->bnpacket, BUS_DMASYNC_POSTREAD);
		else
			/* XXX */
			aprint_error_dev(fc->dev,
			    "fwohci_rbuf_update: this shouldn't happend\n");

		STAILQ_REMOVE_HEAD(&ir->stdma, link);
		STAILQ_INSERT_TAIL(&ir->stvalid, chunk, link);
		switch (stat & FWOHCIEV_MASK) {
		case FWOHCIEV_ACKCOMPL:
			chunk->resp = 0;
			break;
		default:
			chunk->resp = EINVAL;
			aprint_error_dev(fc->dev,
			    "Isochronous receive err %02x(%s)\n",
			    stat, fwohcicode[stat & 0x1f]);
		}
		w++;
	}
	if ((ir->flag & FWXFERQ_HANDLER) == 0)
		mutex_exit(&fc->fc_mtx);
	if (w == 0)
		return;
	if (ir->flag & FWXFERQ_HANDLER)
		ir->hand(ir);
	else
		wakeup(ir);
}

static void
dump_dma(struct fwohci_softc *sc, uint32_t ch)
{
	struct fwohci_dbch *dbch;
	uint32_t cntl, stat, cmd, match;

	if (ch == ATRQ_CH)
		dbch = &sc->atrq;
	else if (ch == ATRS_CH)
		dbch = &sc->atrs;
	else if (ch == ARRQ_CH)
		dbch = &sc->arrq;
	else if (ch == ARRS_CH)
		dbch = &sc->arrs;
	else if (ch < IRX_CH)
		dbch = &sc->it[ch - ITX_CH];
	else
		dbch = &sc->ir[ch - IRX_CH];
	cntl = stat = OREAD(sc, dbch->off);
	cmd = OREAD(sc, dbch->off + 0xc);
	match = OREAD(sc, dbch->off + 0x10);

	aprint_normal_dev(sc->fc.dev,
	    "ch %1x cntl:0x%08x cmd:0x%08x match:0x%08x\n",
	    ch,
	    cntl,
	    cmd,
	    match);
	stat &= 0xffff;
	if (stat)
		aprint_normal_dev(sc->fc.dev, "dma %d ch:%s%s%s%s%s%s %s(%x)\n",
		    ch,
		    stat & OHCI_CNTL_DMA_RUN ? "RUN," : "",
		    stat & OHCI_CNTL_DMA_WAKE ? "WAKE," : "",
		    stat & OHCI_CNTL_DMA_DEAD ? "DEAD," : "",
		    stat & OHCI_CNTL_DMA_ACTIVE ? "ACTIVE," : "",
		    stat & OHCI_CNTL_DMA_BT ? "BRANCH," : "",
		    stat & OHCI_CNTL_DMA_BAD ? "BADDMA," : "",
		    fwohcicode[stat & 0x1f],
		    stat & 0x1f
		);
	else
		aprint_normal_dev(sc->fc.dev, "dma %d ch: Nostat\n", ch);
}

static void
dump_db(struct fwohci_softc *sc, uint32_t ch)
{
	struct fwohci_dbch *dbch;
	struct fwohcidb_tr *cp = NULL, *pp;
	struct fwohcidb *curr = NULL;
#if 0
	struct fwohcidb_tr *np = NULL;
	struct fwohcidb *prev, *next = NULL;
#endif
	int idb, jdb;
	uint32_t cmd;

	if (ch == ATRQ_CH)
		dbch = &sc->atrq;
	else if (ch == ATRS_CH)
		dbch = &sc->atrs;
	else if (ch == ARRQ_CH)
		dbch = &sc->arrq;
	else if (ch == ARRS_CH)
		dbch = &sc->arrs;
	else if (ch < IRX_CH)
		dbch = &sc->it[ch - ITX_CH];
	else
		dbch = &sc->ir[ch - IRX_CH];
	cmd = OREAD(sc, dbch->off + 0xc);

	if (dbch->ndb == 0) {
		aprint_error_dev(sc->fc.dev, "No DB is attached ch=%d\n", ch);
		return;
	}
	pp = dbch->top;
#if 0
	prev = pp->db;
#endif
	for (idb = 0; idb < dbch->ndb; idb++) {
		cp = STAILQ_NEXT(pp, link);
		if (cp == NULL) {
			curr = NULL;
			goto outdb;
		}
#if 0
		np = STAILQ_NEXT(cp, link);
#endif
		for (jdb = 0; jdb < dbch->ndesc; jdb++)
			if ((cmd & 0xfffffff0) == cp->bus_addr) {
				curr = cp->db;
#if 0
				if (np != NULL)
					next = np->db;
				else
					next = NULL;
#endif
				goto outdb;
			}
		pp = STAILQ_NEXT(pp, link);
		if (pp == NULL) {
			curr = NULL;
			goto outdb;
		}
#if 0
		prev = pp->db;
#endif
	}
outdb:
	if (curr != NULL) {
#if 0
		aprint_normal("Prev DB %d\n", ch);
		print_db(pp, prev, ch, dbch->ndesc);
#endif
		aprint_normal("Current DB %d\n", ch);
		print_db(cp, curr, ch, dbch->ndesc);
#if 0
		aprint_normal("Next DB %d\n", ch);
		print_db(np, next, ch, dbch->ndesc);
#endif
	} else
		aprint_error("dbdump err ch = %d cmd = 0x%08x\n", ch, cmd);
	return;
}

static void
print_db(struct fwohcidb_tr *db_tr, struct fwohcidb *db, uint32_t ch,
	 uint32_t hogemax)
{
	fwohcireg_t stat;
	int i, key;
	uint32_t cmd, res;

	if (db == NULL) {
		aprint_error("No Descriptor is found\n");
		return;
	}

	aprint_normal("ch = %d\n%8s %s %s %s %s %4s %8s %8s %4s:%4s\n",
	    ch,
	    "Current",
	    "OP  ",
	    "KEY",
	    "INT",
	    "BR ",
	    "len",
	    "Addr",
	    "Depend",
	    "Stat",
	    "Cnt");
	for (i = 0; i <= hogemax; i++) {
		cmd = FWOHCI_DMA_READ(db[i].db.desc.cmd);
		res = FWOHCI_DMA_READ(db[i].db.desc.res);
		key = cmd & OHCI_KEY_MASK;
		stat = res >> OHCI_STATUS_SHIFT;
		aprint_normal("%08jx %s %s %s %s %5d %08x %08x %04x:%04x",
		    (uintmax_t)db_tr->bus_addr,
		    dbcode[(cmd >> 28) & 0xf],
		    dbkey[(cmd >> 24) & 0x7],
		    dbcond[(cmd >> 20) & 0x3],
		    dbcond[(cmd >> 18) & 0x3],
		    cmd & OHCI_COUNT_MASK,
		    FWOHCI_DMA_READ(db[i].db.desc.addr),
		    FWOHCI_DMA_READ(db[i].db.desc.depend),
		    stat,
		    res & OHCI_COUNT_MASK);
		if (stat & 0xff00)
			aprint_normal(" %s%s%s%s%s%s %s(%x)\n",
			    stat & OHCI_CNTL_DMA_RUN ? "RUN," : "",
			    stat & OHCI_CNTL_DMA_WAKE ? "WAKE," : "",
			    stat & OHCI_CNTL_DMA_DEAD ? "DEAD," : "",
			    stat & OHCI_CNTL_DMA_ACTIVE ? "ACTIVE," : "",
			    stat & OHCI_CNTL_DMA_BT ? "BRANCH," : "",
			    stat & OHCI_CNTL_DMA_BAD ? "BADDMA," : "",
			    fwohcicode[stat & 0x1f],
			    stat & 0x1f
			);
		else
			aprint_normal(" Nostat\n");
		if (key == OHCI_KEY_ST2)
			aprint_normal("0x%08x 0x%08x 0x%08x 0x%08x\n",
			    FWOHCI_DMA_READ(db[i+1].db.immed[0]),
			    FWOHCI_DMA_READ(db[i+1].db.immed[1]),
			    FWOHCI_DMA_READ(db[i+1].db.immed[2]),
			    FWOHCI_DMA_READ(db[i+1].db.immed[3]));
		if (key == OHCI_KEY_DEVICE)
			return;
		if ((cmd & OHCI_BRANCH_MASK) == OHCI_BRANCH_ALWAYS)
			return;
		if ((cmd & OHCI_CMD_MASK) == OHCI_OUTPUT_LAST)
			return;
		if ((cmd & OHCI_CMD_MASK) == OHCI_INPUT_LAST)
			return;
		if (key == OHCI_KEY_ST2)
			i++;
	}
	return;
}

static void
fwohci_txbufdb(struct fwohci_softc *sc, int dmach, struct fw_bulkxfer *bulkxfer)
{
	struct fwohcidb_tr *db_tr /*, *fdb_tr */;
	struct fwohci_dbch *dbch;
	struct fwohcidb *db;
	struct fw_pkt *fp;
	struct fwohci_txpkthdr *ohcifp;
	unsigned short chtag;
	int idb;

	KASSERT(mutex_owner(&sc->fc.fc_mtx));

	dbch = &sc->it[dmach];
	chtag = sc->it[dmach].xferq.flag & 0xff;

	db_tr = (struct fwohcidb_tr *)(bulkxfer->start);
/*
	fdb_tr = (struct fwohcidb_tr *)(bulkxfer->end);
aprint_normal(sc->fc.dev, "DB %08x %08x %08x\n", bulkxfer, db_tr->bus_addr, fdb_tr->bus_addr);
*/
	for (idb = 0; idb < dbch->xferq.bnpacket; idb++) {
		db = db_tr->db;
		fp = (struct fw_pkt *)db_tr->buf;
		ohcifp = (struct fwohci_txpkthdr *) db[1].db.immed;
		ohcifp->mode.ld[0] = fp->mode.ld[0];
		ohcifp->mode.common.spd = 0 & 0x7;
		ohcifp->mode.stream.len = fp->mode.stream.len;
		ohcifp->mode.stream.chtag = chtag;
		ohcifp->mode.stream.tcode = 0xa;
#if BYTE_ORDER == BIG_ENDIAN
		FWOHCI_DMA_WRITE(db[1].db.immed[0], db[1].db.immed[0]);
		FWOHCI_DMA_WRITE(db[1].db.immed[1], db[1].db.immed[1]);
#endif

		FWOHCI_DMA_CLEAR(db[2].db.desc.cmd, OHCI_COUNT_MASK);
		FWOHCI_DMA_SET(db[2].db.desc.cmd, fp->mode.stream.len);
		FWOHCI_DMA_WRITE(db[2].db.desc.res, 0);
#if 0 /* if bulkxfer->npackets changes */
		db[2].db.desc.cmd =
		    OHCI_OUTPUT_LAST | OHCI_UPDATE | OHCI_BRANCH_ALWAYS;
		db[0].db.desc.depend = db[dbch->ndesc - 1].db.desc.depend =
		    STAILQ_NEXT(db_tr, link)->bus_addr | dbch->ndesc;
#else
		FWOHCI_DMA_SET(db[0].db.desc.depend, dbch->ndesc);
		FWOHCI_DMA_SET(db[dbch->ndesc - 1].db.desc.depend, dbch->ndesc);
#endif
		bulkxfer->end = (void *)db_tr;
		db_tr = STAILQ_NEXT(db_tr, link);
	}
	db = ((struct fwohcidb_tr *)bulkxfer->end)->db;
	FWOHCI_DMA_CLEAR(db[0].db.desc.depend, 0xf);
	FWOHCI_DMA_CLEAR(db[dbch->ndesc - 1].db.desc.depend, 0xf);
#if 0 /* if bulkxfer->npackets changes */
	db[dbch->ndesc - 1].db.desc.control |= OHCI_INTERRUPT_ALWAYS;
	/* OHCI 1.1 and above */
	db[0].db.desc.control |= OHCI_INTERRUPT_ALWAYS;
#endif
/*
	db_tr = (struct fwohcidb_tr *)bulkxfer->start;
	fdb_tr = (struct fwohcidb_tr *)bulkxfer->end;
aprint_normal(sc->fc.dev, "DB %08x %3d %08x %08x\n", bulkxfer, bulkxfer->npacket, db_tr->bus_addr, fdb_tr->bus_addr);
*/
	return;
}

static int
fwohci_add_tx_buf(struct fwohci_dbch *dbch, struct fwohcidb_tr *db_tr,
		  int poffset)
{
	struct fwohcidb *db = db_tr->db;
	struct fw_xferq *it;
	int err = 0;

	it = &dbch->xferq;
	if (it->buf == 0) {
		err = EINVAL;
		return err;
	}
	db_tr->buf = fwdma_v_addr(it->buf, poffset);
	db_tr->dbcnt = 3;

	FWOHCI_DMA_WRITE(db[0].db.desc.cmd,
	    OHCI_OUTPUT_MORE | OHCI_KEY_ST2 | 8);
	FWOHCI_DMA_WRITE(db[0].db.desc.addr, 0);
	memset((void *)db[1].db.immed, 0, sizeof(db[1].db.immed));
	FWOHCI_DMA_WRITE(db[2].db.desc.addr,
	    fwdma_bus_addr(it->buf, poffset) + sizeof(uint32_t));

	FWOHCI_DMA_WRITE(db[2].db.desc.cmd,
	    OHCI_OUTPUT_LAST | OHCI_UPDATE | OHCI_BRANCH_ALWAYS);
#if 1
	FWOHCI_DMA_WRITE(db[0].db.desc.res, 0);
	FWOHCI_DMA_WRITE(db[2].db.desc.res, 0);
#endif
	return 0;
}

int
fwohci_add_rx_buf(struct fwohci_dbch *dbch, struct fwohcidb_tr *db_tr,
		  int poffset, struct fwdma_alloc *dummy_dma)
{
	struct fwohcidb *db = db_tr->db;
	struct fw_xferq *rq;
	int i, ldesc;
	bus_addr_t dbuf[2];
	int dsiz[2];

	rq = &dbch->xferq;
	if (rq->buf == NULL && (dbch->xferq.flag & FWXFERQ_EXTBUF) == 0) {
		/* async */
		db_tr->dbcnt = 1;
		dsiz[0] = rq->psize;
		dbuf[0] = db_tr->dma_map->dm_segs[0].ds_addr;
	} else {
		/* isoc */
		db_tr->dbcnt = 0;
		dsiz[db_tr->dbcnt] = sizeof(uint32_t);
		dbuf[db_tr->dbcnt++] = dummy_dma->bus_addr;
		dsiz[db_tr->dbcnt] = rq->psize;
		if (rq->buf != NULL) {
			db_tr->buf = fwdma_v_addr(rq->buf, poffset);
			dbuf[db_tr->dbcnt] = fwdma_bus_addr(rq->buf, poffset);
		}
		db_tr->dbcnt++;
	}
	for (i = 0; i < db_tr->dbcnt; i++) {
		FWOHCI_DMA_WRITE(db[i].db.desc.addr, dbuf[i]);
		FWOHCI_DMA_WRITE(db[i].db.desc.cmd, OHCI_INPUT_MORE | dsiz[i]);
		if (rq->flag & FWXFERQ_STREAM)
			FWOHCI_DMA_SET(db[i].db.desc.cmd, OHCI_UPDATE);
		FWOHCI_DMA_WRITE(db[i].db.desc.res, dsiz[i]);
	}
	ldesc = db_tr->dbcnt - 1;
	if (rq->flag & FWXFERQ_STREAM)
		FWOHCI_DMA_SET(db[ldesc].db.desc.cmd, OHCI_INPUT_LAST);
	FWOHCI_DMA_SET(db[ldesc].db.desc.cmd, OHCI_BRANCH_ALWAYS);
	return 0;
}


static int
fwohci_arcv_swap(struct fw_pkt *fp, int len)
{
	struct fw_pkt *fp0;
	uint32_t ld0;
	int hlen;
#if BYTE_ORDER == BIG_ENDIAN
	int slen, i;
#endif

	ld0 = FWOHCI_DMA_READ(fp->mode.ld[0]);
#if 0
	printf("ld0: x%08x\n", ld0);
#endif
	fp0 = (struct fw_pkt *)&ld0;
	/* determine length to swap */
	switch (fp0->mode.common.tcode) {
	case FWTCODE_WRES:
	case FWTCODE_RREQQ:
	case FWTCODE_WREQQ:
	case FWTCODE_RRESQ:
	case FWOHCITCODE_PHY:
#if BYTE_ORDER == BIG_ENDIAN
		slen = 12;
#endif
		break;

	case FWTCODE_RREQB:
	case FWTCODE_WREQB:
	case FWTCODE_LREQ:
	case FWTCODE_RRESB:
	case FWTCODE_LRES:
#if BYTE_ORDER == BIG_ENDIAN
		slen = 16;
#endif
		break;

	default:
		aprint_error("Unknown tcode %d\n", fp0->mode.common.tcode);
		return 0;
	}
	hlen = tinfo[fp0->mode.common.tcode].hdr_len;
	if (hlen > len) {
		if (firewire_debug)
			printf("splitted header\n");
		return len - hlen;
	}
#if BYTE_ORDER == BIG_ENDIAN
	for (i = 0; i < slen / 4; i++)
		fp->mode.ld[i] = FWOHCI_DMA_READ(fp->mode.ld[i]);
#endif
	return hlen;
}

static int
fwohci_get_plen(struct fwohci_softc *sc, struct fwohci_dbch *dbch,
		struct fw_pkt *fp)
{
	const struct tcode_info *info;
	int r;

	info = &tinfo[fp->mode.common.tcode];
	r = info->hdr_len + sizeof(uint32_t);
	if (info->flag & FWTI_BLOCK_ASY)
		r += roundup2(fp->mode.wreqb.len, sizeof(uint32_t));

	if (r == sizeof(uint32_t)) {
		/* XXX */
		aprint_error_dev(sc->fc.dev, "Unknown tcode %d\n",
		    fp->mode.common.tcode);
		return -1;
	}

	if (r > dbch->xferq.psize) {
		aprint_error_dev(sc->fc.dev, "Invalid packet length %d\n", r);
		return -1;
		/* panic ? */
	}

	return r;
}

static void
fwohci_arcv_free_buf(struct fwohci_softc *sc, struct fwohci_dbch *dbch,
		     struct fwohcidb_tr *db_tr, int wake)
{
	struct fwohcidb *db = db_tr->db;
	struct fwohcidb_tr *bdb_tr = dbch->bottom;

	FWOHCI_DMA_CLEAR(db->db.desc.depend, 0xf);
	FWOHCI_DMA_WRITE(db->db.desc.res, dbch->xferq.psize);

	fwdma_sync_multiseg(dbch->am, bdb_tr->idx, bdb_tr->idx,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	FWOHCI_DMA_SET(bdb_tr->db[0].db.desc.depend, dbch->ndesc);

	fwdma_sync_multiseg(dbch->am, bdb_tr->idx, db_tr->idx,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	dbch->bottom = db_tr;

	if (wake)
		OWRITE(sc, OHCI_DMACTL(dbch->off), OHCI_CNTL_DMA_WAKE);
}

static void
fwohci_arcv(struct fwohci_softc *sc, struct fwohci_dbch *dbch)
{
	struct fwohcidb_tr *db_tr;
	struct fw_pkt pktbuf, *fp;
	struct iovec vec[2];
	bus_addr_t m;
	bus_size_t n;
	u_int spd;
	uint32_t stat, status, event;
	uint8_t *ld;
	int nvec, resCount, len, plen, hlen, offset;
	const int psize = dbch->xferq.psize;

#if DIAGNOSTIC
	if (dbch->off != OHCI_ARQOFF &&
	    dbch->off != OHCI_ARSOFF)
		panic("not async rx");
#endif

	mutex_enter(&dbch->xferq.q_mtx);
	db_tr = dbch->top;
	/* XXX we cannot handle a packet which lies in more than two buf */
	fwdma_sync_multiseg(dbch->am, db_tr->idx, db_tr->idx,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	status = FWOHCI_DMA_READ(db_tr->db[0].db.desc.res) >> OHCI_STATUS_SHIFT;
	resCount = FWOHCI_DMA_READ(db_tr->db[0].db.desc.res) & OHCI_COUNT_MASK;
	while (status & OHCI_CNTL_DMA_ACTIVE) {
#if 0
		if (dbch->off == OHCI_ARQOFF)
			aprint_normal_dev(sc->fc.dev,
			    "buf 0x%08x, status 0x%04x, resCount 0x%04x\n",
			    db_tr->bus_addr, status, resCount);
#endif
		n = 0;
		len = psize - resCount;
		ld = (uint8_t *)db_tr->buf;
		if (dbch->pdb_tr == NULL) {
			len -= dbch->buf_offset;
			ld += dbch->buf_offset;
			m = dbch->buf_offset;
		} else
			m = 0;
		if (len > 0)
			bus_dmamap_sync(sc->fc.dmat, db_tr->dma_map,
			    m, len, BUS_DMASYNC_POSTREAD);
		while (len > 0) {
			if (dbch->pdb_tr != NULL) {
				/* we have a fragment in previous buffer */
				int rlen = 0;
				void *buf;

				if (dbch->buf_offset < 0) {
					/* splitted in header, pull up */
					char *p;

					rlen -= dbch->buf_offset;
					buf = (char *)dbch->pdb_tr->buf +
					    psize - rlen;

					KASSERT(rlen <= sizeof(pktbuf));

					p = (char *)&pktbuf;
					memcpy(p, buf, rlen);
					p += rlen;
					/* this must be too long but harmless */
					rlen = sizeof(pktbuf) - rlen;
					memcpy(p, db_tr->buf, rlen);
					ld += rlen;
					len -= rlen;
					hlen = fwohci_arcv_swap(&pktbuf,
					    sizeof(pktbuf));
					if (hlen <= 0) {
						aprint_error_dev(sc->fc.dev,
						    "hlen should be positive.");
						goto err;
					}
					offset = sizeof(pktbuf);
					vec[0].iov_base = (char *)&pktbuf;
					vec[0].iov_len = offset;
				} else {
					/* splitted in payload */
					buf = (char *)dbch->pdb_tr->buf +
					    dbch->buf_offset;
					rlen = psize - dbch->buf_offset;
					if (firewire_debug)
						printf("rlen=%d, offset=%d\n",
						    rlen, dbch->buf_offset);
					offset = rlen;
					vec[0].iov_base = buf;
					vec[0].iov_len = rlen;
				}
				fp = (struct fw_pkt *)vec[0].iov_base;
				nvec = 1;
			} else {
				/* no fragment in previous buffer */
				fp = (struct fw_pkt *)ld;
				hlen = fwohci_arcv_swap(fp, len);
				if (hlen == 0)
					goto err;
				if (hlen < 0) {
					dbch->pdb_tr = db_tr;
					dbch->buf_offset -= psize;
					/* sanity check */
					if (resCount != 0)  {
						aprint_error_dev(sc->fc.dev,
						    "resCount=%d hlen=%d\n",
						    resCount, hlen);
						goto err;
					}
					goto out;
				}
				offset = 0;
				nvec = 0;
			}
			plen = fwohci_get_plen(sc, dbch, fp) - offset;
			if (plen < 0) {
				/*
				 * minimum header size + trailer =
				 *     sizeof(fw_pkt) so this shouldn't happens
				 */
				aprint_error_dev(sc->fc.dev,
				    "plen(%d) is negative! offset=%d\n",
				    plen, offset);
				goto err;
			}
			if (plen > 0) {
				len -= plen;
				if (len < 0) {
					dbch->pdb_tr = db_tr;
					if (firewire_debug)
						printf("splitted payload\n");
					/* sanity check */
					if (resCount != 0) {
						aprint_error_dev(sc->fc.dev,
						    "resCount=%d plen=%d"
						    " len=%d\n",
						    resCount, plen, len);
						goto err;
					}
					goto out;
				}
				vec[nvec].iov_base = ld;
				vec[nvec].iov_len = plen;
				nvec++;
				ld += plen;
			}
			if (nvec == 0)
				aprint_error_dev(sc->fc.dev, "nvec == 0\n");

/* DMA result-code will be written at the tail of packet */
			stat = FWOHCI_DMA_READ(*(uint32_t *)(ld -
						sizeof(struct fwohci_trailer)));
#if 0
			aprint_normal("plen: %d, stat %x\n", plen, stat);
#endif
			spd = (stat >> 21) & 0x3;
			event = (stat >> 16) & 0x1f;
			switch (event) {
			case FWOHCIEV_ACKPEND:
#if 0
				aprint_normal(sc->fc.dev,
				    "ack pending tcode=0x%x..\n",
				    fp->mode.common.tcode);
#endif
				/* fall through */
			case FWOHCIEV_ACKCOMPL:
			{
				struct fw_rcv_buf rb;

				vec[nvec - 1].iov_len -=
				    sizeof(struct fwohci_trailer);
				if (vec[nvec - 1].iov_len == 0)
					nvec--;
				rb.fc = &sc->fc;
				rb.vec = vec;
				rb.nvec = nvec;
				rb.spd = spd;
				fw_rcv(&rb);
				break;
			}
			case FWOHCIEV_BUSRST:
				if ((sc->fc.status != FWBUSRESET) &&
				    (sc->fc.status != FWBUSINIT))
					aprint_error_dev(sc->fc.dev,
					    "got BUSRST packet!?\n");
				break;
			default:
				aprint_error_dev(sc->fc.dev,
				    "Async DMA Receive error err=%02x %s"
				    " plen=%d offset=%d len=%d status=0x%08x"
				    " tcode=0x%x, stat=0x%08x\n",
				    event, fwohcicode[event], plen,
				    (int)(ld - (uint8_t *)db_tr->buf - plen),
				    len, OREAD(sc, OHCI_DMACTL(dbch->off)),
				    fp->mode.common.tcode, stat);
#if 1 /* XXX */
				goto err;
#endif
				break;
			}
			if (dbch->pdb_tr != NULL) {
				if (dbch->buf_offset < 0)
					bus_dmamap_sync(sc->fc.dmat,
					    dbch->pdb_tr->dma_map,
					    psize + dbch->buf_offset,
					    0 - dbch->buf_offset,
					    BUS_DMASYNC_PREREAD);
				else
					bus_dmamap_sync(sc->fc.dmat,
					    dbch->pdb_tr->dma_map,
					    dbch->buf_offset,
					    psize - dbch->buf_offset,
					    BUS_DMASYNC_PREREAD);
				fwohci_arcv_free_buf(sc, dbch, dbch->pdb_tr, 1);
				dbch->pdb_tr = NULL;
			}
			dbch->buf_offset = ld - (uint8_t *)db_tr->buf;
			n += (plen + offset);
		}
out:
		if (n > 0)
			bus_dmamap_sync(sc->fc.dmat, db_tr->dma_map, m, n,
			    BUS_DMASYNC_PREREAD);

		if (resCount != 0) {
			dbch->buf_offset = psize - resCount;
			break;
		}

		/* done on this buffer */

		if (dbch->pdb_tr == NULL) {
			fwohci_arcv_free_buf(sc, dbch, db_tr, 1);
			dbch->buf_offset = 0;
		} else
			if (dbch->pdb_tr != db_tr)
				aprint_error_dev(sc->fc.dev,
				    "pdb_tr != db_tr\n");
		dbch->top = STAILQ_NEXT(db_tr, link);

		db_tr = dbch->top;
		fwdma_sync_multiseg(dbch->am, db_tr->idx, db_tr->idx,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		status = FWOHCI_DMA_READ(db_tr->db[0].db.desc.res) >>
		    OHCI_STATUS_SHIFT;
		resCount = FWOHCI_DMA_READ(db_tr->db[0].db.desc.res)
		    & OHCI_COUNT_MASK;

		/* XXX check buffer overrun */

		/* XXX make sure DMA is not dead */
	}
	fwdma_sync_multiseg(dbch->am, db_tr->idx, db_tr->idx,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	mutex_exit(&dbch->xferq.q_mtx);
	return;

err:
	aprint_error_dev(sc->fc.dev, "AR DMA status=%x, ",
	    OREAD(sc, OHCI_DMACTL(dbch->off)));
	if (dbch->pdb_tr != NULL) {
		if (dbch->buf_offset < 0)
			bus_dmamap_sync(sc->fc.dmat, dbch->pdb_tr->dma_map,
			    psize + dbch->buf_offset, 0 - dbch->buf_offset,
			    BUS_DMASYNC_PREREAD);
		else
			bus_dmamap_sync(sc->fc.dmat, dbch->pdb_tr->dma_map,
			    dbch->buf_offset, psize - dbch->buf_offset,
			    BUS_DMASYNC_PREREAD);
		fwohci_arcv_free_buf(sc, dbch, dbch->pdb_tr, 1);
		dbch->pdb_tr = NULL;
	}
	/* skip until resCount != 0 */
	aprint_error(" skip buffer");
	while (resCount == 0) {
		aprint_error(" #");
		fwohci_arcv_free_buf(sc, dbch, db_tr, 0);
		db_tr = STAILQ_NEXT(db_tr, link);
		resCount = FWOHCI_DMA_READ(db_tr->db[0].db.desc.res)
		    & OHCI_COUNT_MASK;
	}
	aprint_error(" done\n");
	dbch->top = db_tr;
	dbch->buf_offset = psize - resCount;
	OWRITE(sc, OHCI_DMACTL(dbch->off), OHCI_CNTL_DMA_WAKE);
	fwdma_sync_multiseg(dbch->am, db_tr->idx, db_tr->idx,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(sc->fc.dmat, db_tr->dma_map,
	    0, db_tr->dma_map->dm_mapsize, BUS_DMASYNC_PREREAD);
	mutex_exit(&dbch->xferq.q_mtx);
}
