/*	$NetBSD: dp8390.c,v 1.6 2008/12/14 18:46:33 christos Exp $	*/

/*
 * Polling driver for National Semiconductor DS8390/WD83C690 based
 * ethernet adapters.
 *
 * Copyright (c) 1998 Matthias Drochner.  All rights reserved.
 *
 * Copyright (c) 1994, 1995 Charles M. Hannum.  All rights reserved.
 *
 * Copyright (C) 1993, David Greenman.  This software may be used, modified,
 * copied, distributed, and sold, in both source and binary form provided that
 * the above copyright and these terms are retained.  Under no circumstances is
 * the author responsible for the proper functioning of this software, nor does
 * the author assume any responsibility for damages incurred with its use.
 */

#include <sys/types.h>
#include <machine/pio.h>

#include <lib/libsa/stand.h>
#include <libi386.h>

#include <dev/ic/dp8390reg.h>
#include "dp8390.h"
#ifdef SUPPORT_NE2000
#include "ne.h"
#endif

#include "etherdrv.h"

int dp8390_iobase, dp8390_membase, dp8390_memsize;
#if defined(SUPPORT_WD80X3) && defined(SUPPORT_SMC_ULTRA)
int dp8390_is790;
#endif
uint8_t dp8390_cr_proto;
uint8_t dp8390_dcr_reg;

#define WE_IOBASE dp8390_iobase

static u_short rec_page_start;
static u_short rec_page_stop;
static u_short next_packet;

extern u_char eth_myaddr[6];

#ifndef _STANDALONE
static void *vmembase;
extern void *mapmem(int, int);
extern void unmapmem(void *, int);
extern int mapio(void);

static void
bbcopy(void *src, void *dst, int len)
{
	char *s = (char *)src;
	char *d = (char *)dst;

	while (len--)
		*d++ = *s++;
}
#endif

static void dp8390_read(int, char *, u_short);

#define NIC_GET(reg) inb(WE_IOBASE + reg)
#define NIC_PUT(reg, val) outb(WE_IOBASE + reg, val)

static void
dp8390_init(void)
{
	int i;

	/*
	 * Initialize the NIC in the exact order outlined in the NS manual.
	 * This init procedure is "mandatory"...don't change what or when
	 * things happen.
	 */

	/* Set interface for page 0, remote DMA complete, stopped. */
	NIC_PUT(ED_P0_CR, dp8390_cr_proto | ED_CR_PAGE_0 | ED_CR_STP);

	if (dp8390_dcr_reg & ED_DCR_LS) {
		NIC_PUT(ED_P0_DCR, dp8390_dcr_reg);
	} else {
		/*
		 * Set FIFO threshold to 8, No auto-init Remote DMA, byte
		 * order=80x86, byte-wide DMA xfers,
		 */
		NIC_PUT(ED_P0_DCR, ED_DCR_FT1 | ED_DCR_LS);
	}

	/* Clear remote byte count registers. */
	NIC_PUT(ED_P0_RBCR0, 0);
	NIC_PUT(ED_P0_RBCR1, 0);

	/* Tell RCR to do nothing for now. */
	NIC_PUT(ED_P0_RCR, ED_RCR_MON);

	/* Place NIC in internal loopback mode. */
	NIC_PUT(ED_P0_TCR, ED_TCR_LB0);

	/* Set lower bits of byte addressable framing to 0. */
	if (dp8390_is790)
		NIC_PUT(0x09, 0);

	/* Initialize receive buffer ring. */
	NIC_PUT(ED_P0_BNRY, rec_page_start);
	NIC_PUT(ED_P0_PSTART, rec_page_start);
	NIC_PUT(ED_P0_PSTOP, rec_page_stop);

	/*
	 * Clear all interrupts.  A '1' in each bit position clears the
	 * corresponding flag.
	 */
	NIC_PUT(ED_P0_ISR, 0xff);

	/*
	 * Disable all interrupts.
	 */
	NIC_PUT(ED_P0_IMR, 0);

	/* Program command register for page 1. */
	NIC_PUT(ED_P0_CR, dp8390_cr_proto | ED_CR_PAGE_1 | ED_CR_STP);

	/* Copy out our station address. */
	for (i = 0; i < 6; ++i)
		NIC_PUT(ED_P1_PAR0 + i, eth_myaddr[i]);

	/*
	 * Set current page pointer to one page after the boundary pointer, as
	 * recommended in the National manual.
	 */
	next_packet = rec_page_start + 1;
	NIC_PUT(ED_P1_CURR, next_packet);

	/* Program command register for page 0. */
	NIC_PUT(ED_P1_CR, dp8390_cr_proto | ED_CR_PAGE_0 | ED_CR_STP);

	/* directed and broadcast */
	NIC_PUT(ED_P0_RCR, ED_RCR_AB);

	/* Take interface out of loopback. */
	NIC_PUT(ED_P0_TCR, 0);

	/* Fire up the interface. */
	NIC_PUT(ED_P0_CR, dp8390_cr_proto | ED_CR_PAGE_0 | ED_CR_STA);
}

int
dp8390_config(void)
{
#ifndef _STANDALONE
	if (mapio()) {
		printf("no IO access\n");
		return -1;
	}
	vmembase = mapmem(dp8390_membase, dp8390_memsize);
	if (!vmembase) {
		printf("no memory access\n");
		return -1;
	}
#endif

	rec_page_start = TX_PAGE_START + ED_TXBUF_SIZE;
	rec_page_stop = TX_PAGE_START + (dp8390_memsize >> ED_PAGE_SHIFT);

	dp8390_init();

	return 0;
}

void
dp8390_stop(void)
{
	int n = 5000;

	/* Stop everything on the interface, and select page 0 registers. */
	NIC_PUT(ED_P0_CR, dp8390_cr_proto | ED_CR_PAGE_0 | ED_CR_STP);

	/*
	 * Wait for interface to enter stopped state, but limit # of checks to
	 * 'n' (about 5ms).  It shouldn't even take 5us on modern DS8390's, but
	 * just in case it's an old one.
	 */
	while (((NIC_GET(ED_P0_ISR) & ED_ISR_RST) == 0) && --n)
		continue;

#ifndef _STANDALONE
	unmapmem(vmembase, dp8390_memsize);
#endif
}

int
EtherSend(char *pkt, int len)
{
#ifdef SUPPORT_NE2000
	ne2000_writemem(pkt, dp8390_membase, len);
#else
#ifdef _STANDALONE
	vpbcopy(pkt, (void *)dp8390_membase, len);
#else
	bbcopy(pkt, vmembase, len);
#endif
#endif

	/* Set TX buffer start page. */
	NIC_PUT(ED_P0_TPSR, TX_PAGE_START);

	/* Set TX length. */
	NIC_PUT(ED_P0_TBCR0, len < 60 ? 60 : len);
	NIC_PUT(ED_P0_TBCR1, len >> 8);

	/* Set page 0, remote DMA complete, transmit packet, and *start*. */
	NIC_PUT(ED_P0_CR, dp8390_cr_proto | ED_CR_PAGE_0 | ED_CR_TXP | ED_CR_STA);

	return len;
}

static void
dp8390_read(int buf, char *dest, u_short len)
{
	u_short tmp_amount;

	/* Does copy wrap to lower addr in ring buffer? */
	if (buf + len > dp8390_membase + dp8390_memsize) {
		tmp_amount = dp8390_membase + dp8390_memsize - buf;

		/* Copy amount up to end of NIC memory. */
#ifdef SUPPORT_NE2000
		ne2000_readmem(buf, dest, tmp_amount);
#else
#ifdef _STANDALONE
		pvbcopy((void *)buf, dest, tmp_amount);
#else
		bbcopy(vmembase + buf - dp8390_membase, dest, tmp_amount);
#endif
#endif

		len -= tmp_amount;
		buf = RX_BUFBASE + (rec_page_start << ED_PAGE_SHIFT);
		dest += tmp_amount;
	}
#ifdef SUPPORT_NE2000
	ne2000_readmem(buf, dest, len);
#else
#ifdef _STANDALONE
	pvbcopy((void *)buf, dest, len);
#else
	bbcopy(vmembase + buf - dp8390_membase, dest, len);
#endif
#endif
}

int
EtherReceive(char *pkt, int maxlen)
{
	struct dp8390_ring packet_hdr;
	int packet_ptr;
	u_short len;
	u_char boundary, current;
#ifdef DP8390_OLDCHIPS
	u_char nlen;
#endif

	if (!(NIC_GET(ED_P0_RSR) & ED_RSR_PRX))
		return 0; /* XXX error handling */

	/* Set NIC to page 1 registers to get 'current' pointer. */
	NIC_PUT(ED_P0_CR, dp8390_cr_proto | ED_CR_PAGE_1 | ED_CR_STA);

	/*
	 * 'sc->next_packet' is the logical beginning of the ring-buffer - i.e.
	 * it points to where new data has been buffered.  The 'CURR' (current)
	 * register points to the logical end of the ring-buffer - i.e. it
	 * points to where additional new data will be added.  We loop here
	 * until the logical beginning equals the logical end (or in other
	 * words, until the ring-buffer is empty).
	 */
	current = NIC_GET(ED_P1_CURR);

	/* Set NIC to page 0 registers to update boundary register. */
	NIC_PUT(ED_P1_CR, dp8390_cr_proto | ED_CR_PAGE_0 | ED_CR_STA);

	if (next_packet == current)
		return 0;

	/* Get pointer to this buffer's header structure. */
	packet_ptr = RX_BUFBASE + (next_packet << ED_PAGE_SHIFT);

	/*
	 * The byte count includes a 4 byte header that was added by
	 * the NIC.
	 */
#ifdef SUPPORT_NE2000
	ne2000_readmem(packet_ptr, (void *)&packet_hdr, 4);
#else
#ifdef _STANDALONE
	pvbcopy((void *)packet_ptr, &packet_hdr, 4);
#else
	bbcopy(vmembase + packet_ptr - dp8390_membase, &packet_hdr, 4);
#endif
#endif

	len = packet_hdr.count;

#ifdef DP8390_OLDCHIPS
	/*
	 * Try do deal with old, buggy chips that sometimes duplicate
	 * the low byte of the length into the high byte.  We do this
	 * by simply ignoring the high byte of the length and always
	 * recalculating it.
	 *
	 * NOTE: sc->next_packet is pointing at the current packet.
	 */
	if (packet_hdr.next_packet >= next_packet)
		nlen = (packet_hdr.next_packet - next_packet);
	else
		nlen = ((packet_hdr.next_packet - rec_page_start) +
			(rec_page_stop - next_packet));
	--nlen;
	if ((len & ED_PAGE_MASK) + sizeof(packet_hdr) > ED_PAGE_SIZE)
		--nlen;
	len = (len & ED_PAGE_MASK) | (nlen << ED_PAGE_SHIFT);
#ifdef DIAGNOSTIC
	if (len != packet_hdr.count) {
		printf(IFNAME ": length does not match next packet pointer\n");
		printf(IFNAME ": len %04x nlen %04x start %02x "
		       "first %02x curr %02x next %02x stop %02x\n",
		       packet_hdr.count, len,
		       rec_page_start, next_packet, current,
		       packet_hdr.next_packet, rec_page_stop);
	}
#endif
#endif

	if (packet_hdr.next_packet < rec_page_start ||
	    packet_hdr.next_packet >= rec_page_stop)
		panic(IFNAME ": RAM corrupt");

	len -= sizeof(struct dp8390_ring);
	if (len < maxlen) {
		/* Go get packet. */
		dp8390_read(packet_ptr + sizeof(struct dp8390_ring),
			    pkt, len);
	} else
		len = 0;

	/* Update next packet pointer. */
	next_packet = packet_hdr.next_packet;

	/*
	 * Update NIC boundary pointer - being careful to keep it one
	 * buffer behind (as recommended by NS databook).
	 */
	boundary = next_packet - 1;
	if (boundary < rec_page_start)
		boundary = rec_page_stop - 1;
	NIC_PUT(ED_P0_BNRY, boundary);

	return len;
}
