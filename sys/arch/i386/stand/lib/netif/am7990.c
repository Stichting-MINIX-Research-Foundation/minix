/*	$NetBSD: am7990.c,v 1.7 2008/12/14 18:46:33 christos Exp $	*/

/* mostly from netbsd:sys/arch/i386/netboot/ne2100.c
 memory allocation now 1 chunk, added deallocation
 receive function changed - don't use irq
 */

/*
 * source in this file came from
 * the Mach ethernet boot written by Leendert van Doorn.
 *
 * A very simple network driver for NE2100 boards that polls.
 *
 * Copyright (c) 1992 by Leendert van Doorn
 */

#include <sys/types.h>
#include <machine/pio.h>
#include <lib/libkern/libkern.h>
#include <lib/libsa/stand.h>

#include <libi386.h>

#include "etherdrv.h"
#include "lance.h"

extern u_char eth_myaddr[6];

extern int lance_rap, lance_rdp;

static void *dmamem;

#define LA(adr) vtophys(adr)

/* Lance register offsets */
#define LA_CSR          lance_rdp
#define LA_CSR1         lance_rdp
#define LA_CSR2         lance_rdp
#define LA_CSR3         lance_rdp
#define LA_RAP          lance_rap

/*
 * Some driver specific constants.
 * Take care when tuning, this program only has 32 Kb
 */
#define	LANCEBUFSIZE	1518		/* plus 4 CRC bytes */
#define	MAXLOOP		1000000L	/* arbitrary retry limit */
#define	LOG2NRCVRING	2		/* log2(NRCVRING) */
#define	NRCVRING	(1 << LOG2NRCVRING)

static int next_rmd;			/* next receive element */
static initblock_t *initblock;		/* initialization block */
static tmde_t *tmd;			/* transmit ring */
static rmde_t *rmd;			/* receive ring */
static char rbuffer[NRCVRING][LANCEBUFSIZE]; /* receive buffers */

/*
 * Stop ethernet board
 */
void
am7990_stop(void)
{
	long l;

	/* stop chip and disable DMA access */
	outw(LA_RAP, RDP_CSR0);
	outw(LA_CSR, CSR_STOP);
	for (l = 0; (inw(LA_CSR) & CSR_STOP) == 0; l++) {
		if (l >= MAXLOOP) {
			printf("Lance failed to stop\n");
			return;
		}
	}
}

/*
 * Reset ethernet board
 */
void
am7990_init(void)
{
	long l;
	u_long addr;
	int i;

	/* initblock, tmd, and rmd should be 8 byte aligned;
	   sizes of initblock_t and tmde_t are multiples of 8 */
	dmamem = alloc(sizeof(initblock_t) +
	    sizeof(tmde_t) + NRCVRING * sizeof(rmde_t) + 4);
	/* +4 is ok because alloc()'s result is 4-byte aligned! */

	initblock = (initblock_t *)(((unsigned long)dmamem + 4) & -8);
	tmd = (tmde_t *)(initblock + 1);
	rmd = (rmde_t *)(tmd + 1);

	/* stop the chip, and make sure it did */
	am7990_stop();

	/* fill lance initialization block */
	memset(initblock, 0, sizeof(initblock_t));

	/* set my ethernet address */
	for (i = 0; i < 6; i++)
		initblock->ib_padr[i] = eth_myaddr[i];

	/* receive ring pointer */
	addr = LA(rmd);
	initblock->ib_rdralow = (u_short)addr;
	initblock->ib_rdrahigh = (u_char)(addr >> 16);
	initblock->ib_rlen = LOG2NRCVRING << 5;

	/* transmit ring with one element */
	addr = LA(tmd);
	initblock->ib_tdralow = (u_short)addr;
	initblock->ib_tdrahigh = (u_char)(addr >> 16);
	initblock->ib_tlen = 0 << 5;

	/* setup the receive ring entries */
	for (next_rmd = 0, i = 0; i < NRCVRING; i++) {
		addr = LA(&rbuffer[i]);
		rmd[i].rmd_ladr = (u_short)addr;
		rmd[i].rmd_hadr = (u_char)(addr >> 16);
		rmd[i].rmd_mcnt = 0;
		rmd[i].rmd_bcnt = -LANCEBUFSIZE;
		rmd[i].rmd_flags = RMD_OWN;
	}

	/* zero transmit ring */
	memset(tmd, 0, sizeof(tmde_t));

	/* give lance the init block */
	addr = LA(initblock);
	outw(LA_RAP, RDP_CSR1);
	outw(LA_CSR1, (u_short)addr);
	outw(LA_RAP, RDP_CSR2);
	outw(LA_CSR2, (char)(addr >> 16));
	outw(LA_RAP, RDP_CSR3);
	outw(LA_CSR3, 0);

	/* and initialize it */
	outw(LA_RAP, RDP_CSR0);
	outw(LA_CSR, CSR_INIT|CSR_STRT);

	/* wait for the lance to complete initialization and fire it up */
	for (l = 0; (inw(LA_CSR) & CSR_IDON) == 0; l++) {
		if (l >= MAXLOOP) {
			printf("Lance failed to initialize\n");
			break;
		}
	}
	for (l = 0; (inw(LA_CSR)&(CSR_TXON|CSR_RXON)) != (CSR_TXON|CSR_RXON); l++) {
		if (l >= MAXLOOP) {
			printf("Lance not started\n");
			break;
		}
	}
}

/*
 * Stop ethernet board and free ressources
 */
void
EtherStop(void)
{
	am7990_stop();

	dealloc(dmamem, sizeof(initblock_t) +
	    sizeof(tmde_t) + NRCVRING * sizeof(rmde_t) + 4);
}

/*
 * Send an ethernet packet
 */
int
EtherSend(char *pkt, int len)
{
	long l;
	u_long addr;
	u_short csr;
	int savlen = len;

	if (len < 60)
		len = 60;
	if (len > LANCEBUFSIZE) {
		printf("packet too long\n");
		return -1;
	}

	/* set up transmit ring element */
	if (tmd->tmd_flags & TMD_OWN) {
		printf("lesend: td busy, status=%x\n", tmd->tmd_flags);
		return -1;
	}
	addr = LA(pkt);
	if (addr & 1) {
		printf("unaligned data\n");
		return -1;
	}
	tmd->tmd_ladr = (u_short)addr;
	tmd->tmd_hadr = (u_char)(addr >> 16);
	tmd->tmd_bcnt = -len;
	tmd->tmd_err = 0;
	tmd->tmd_flags = TMD_OWN|TMD_STP|TMD_ENP;

	/* start transmission */
	outw(LA_CSR, CSR_TDMD);

	/* wait for interrupt and acknowledge it */
	for (l = 0; l < MAXLOOP; l++) {
		if ((csr = inw(LA_CSR)) & CSR_TINT) {
			outw(LA_CSR, CSR_TINT);
#ifdef LEDEBUG
			if (tmd->tmd_flags & (TMD_ONE|TMD_MORE|TMD_ERR|TMD_DEF))
				printf("lesend: status=%x\n", tmd->tmd_flags);
#endif
			break;
		}
		delay(10); /* don't poll too much on PCI, seems
			      to disturb DMA on poor hardware */
	}
	return savlen;
}

/*
 * Poll the LANCE just see if there's an Ethernet packet
 * available. If there is, its contents is returned.
 */
int
EtherReceive(char *pkt, int maxlen)
{
	rmde_t *rp;
	u_short csr;
	int len = 0;

	csr = inw(LA_CSR);
	outw(LA_CSR, csr & (CSR_BABL | CSR_MISS | CSR_MERR | CSR_RINT));

	if ((next_rmd < 0) || (next_rmd >= NRCVRING)) {
		printf("next_rmd bad\n");
		return 0;
	}
	rp = &rmd[next_rmd];

	if (rp->rmd_flags & RMD_OWN)
		return 0;

	if (csr & (CSR_BABL | CSR_CERR | CSR_MISS | CSR_MERR))
		printf("le: csr %x\n", csr);

	if (rp->rmd_flags & (RMD_FRAM | RMD_OFLO | RMD_CRC | RMD_BUFF)) {
		printf("le: rmd_flags %x\n", rp->rmd_flags);
		goto cleanup;
	}

	if (rp->rmd_flags != (RMD_STP|RMD_ENP)) {
		printf("le: rmd_flags %x\n", rp->rmd_flags);
		return -1;
	}

	len = rp->rmd_mcnt - 4;

	if ((len < 0) || (len >= LANCEBUFSIZE)) {
		printf("bad pkt len\n");
		return -1;
	}

	if (len <= maxlen)
		memcpy(pkt, rbuffer[next_rmd], len);
	else
		len = 0;

 cleanup:
	/* give packet back to the lance */
	rp->rmd_bcnt = -LANCEBUFSIZE;
	rp->rmd_mcnt = 0;
	rp->rmd_flags = RMD_OWN;
	next_rmd = (next_rmd + 1) & (NRCVRING - 1);

	return len;
}
