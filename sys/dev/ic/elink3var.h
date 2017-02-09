/*	$NetBSD: elink3var.h,v 1.39 2015/04/13 16:33:24 riastradh Exp $	*/

/*
 * Copyright (c) 1994 Herb Peyerl <hpeyerl@beer.org>
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
 *    must display the following acknowledgement:
 *      This product includes software developed by Herb Peyerl.
 * 4. The name of Herb Peyerl may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#include <sys/rndsource.h>

/*
 * Ethernet software status per interface.
 */
struct ep_softc {
	device_t sc_dev;
	void *sc_ih;

	struct ethercom sc_ethercom;	/* Ethernet common part		*/
	struct mii_data sc_mii;		/* MII/media control		*/
	struct callout sc_mii_callout;	/* MII callout handle		*/
	struct callout sc_mbuf_callout;	/* mbuf fill callout		*/
	bus_space_tag_t sc_iot;		/* bus cookie			*/
	bus_space_handle_t sc_ioh;	/* bus i/o handle		*/
	bus_space_tag_t sc_memt;	/* RoadRunner only		*/
	bus_space_handle_t sc_memh;	/* RoadRunner only		*/
	u_int32_t sc_flags;		/* misc. flags			*/
#define MAX_MBS	8			/* # of mbufs we keep around	*/
	struct mbuf *mb[MAX_MBS];	/* spare mbuf storage.		*/
	int	next_mb;		/* Which mbuf to use next. 	*/
	int	last_mb;		/* Last mbuf.			*/
	int	tx_start_thresh;	/* Current TX_start_thresh.	*/
	int	tx_succ_ok;		/* # packets sent in sequence   */
					/* w/o underrun			*/

	u_int	ep_flags;		/* capabilities flag (from EELINKROM) */
#define	ELINK_FLAGS_PNP			0x00001
#define	ELINK_FLAGS_FULLDUPLEX		0x00002
#define	ELINK_FLAGS_LARGELINKKT		0x00004	/* 4k packet support */
#define	ELINK_FLAGS_SLAVEDMA		0x00008
#define	ELINK_FLAGS_SECONDDMA		0x00010
#define	ELINK_FLAGS_FULLDMA		0x00020
#define	ELINK_FLAGS_FRAGMENTDMA		0x00040
#define	ELINK_FLAGS_CRC_PASSTHRU	0x00080
#define	ELINK_FLAGS_TXDONE		0x00100
#define	ELINK_FLAGS_NO_TXLENGTH		0x00200
#define	ELINK_FLAGS_RXRELINKEAT		0x00400
#define	ELINK_FLAGS_SNOOPING		0x00800
#define	ELINK_FLAGS_100MBIT		0x01000
#define	ELINK_FLAGS_POWERMGMT		0x02000
#define	ELINK_FLAGS_MII			0x04000
#define	ELINK_FLAGS_USEFIFOBUFFER	0x08000	/* RoadRunner only */
#define	ELINK_FLAGS_USESHAREDMEM	0x10000	/* RoadRunner only */
#define	ELINK_FLAGS_FORCENOWAIT		0x20000	/* RoadRunner only */
#define	ELINK_FLAGS_ATTACHED		0x40000	/* attach has succeeded */

	u_short ep_chipset;		/* Chipset family on this board */
#define	ELINK_CHIPSET_3C509		0x00	/* PIO: 3c509, 3c589 */
#define	ELINK_CHIPSET_VORTEX		0x01	/* 100mbit, single-pkt DMA */
#define	ELINK_CHIPSET_BOOMERANG		0x02	/* Saner DMA plus PIO */
#define	ELINK_CHIPSET_ROADRUNNER	0x03	/* like Boomerang, but for
						   PCMCIA; has shared memory
						   plus FIFO buffer */
#define	ELINK_CHIPSET_CORKSCREW		0x04	/* like Boomerang, but DMA
						   hacked to work w/ ISA */

	u_char	bustype;		/* parent bus type */
#define	ELINK_BUS_ISA	  	0x0
#define	ELINK_BUS_PCMCIA  	0x1
#define	ELINK_BUS_EISA	  	0x2
#define	ELINK_BUS_PCI	  	0x3
#define	ELINK_BUS_MCA	  	0x6

#define	ELINK_IS_BUS_32(a)	((a) & 0x2)
	int ep_pktlenshift;		/* scale factor for pkt lengths */

	krndsource_t rnd_source;

	/* power management hooks */
	int (*enable)(struct ep_softc *);
	void (*disable)(struct ep_softc *);
	int enabled;
};


u_int16_t epreadeeprom(bus_space_tag_t, bus_space_handle_t, int);
int	epconfig(struct ep_softc *, u_short, u_int8_t *);

int	epintr(void *);

int	epenable(struct ep_softc *);
void	epdisable(struct ep_softc *);

int	ep_activate(device_t, enum devact);
int	ep_detach(device_t, int);

void	ep_power(int, void *);
