/*	$NetBSD: mb86950var.h,v 1.9 2015/04/14 20:32:35 riastradh Exp $	*/

/*
 * Copyright (c) 1995 Mika Kortelainen
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
 *      This product includes software developed by  Mika Kortelainen
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 * Adapted from if_qnreg.h for the amiga port of NetBSD by Dave J. Barnes, 2004.
 */

/*
 * EStar_softc: per line info and status
 */
struct mb86950_softc {
	device_t sc_dev;
	struct ethercom sc_ec;		/* ethernet common */
	struct ifmedia sc_media;	/* supported media information */

	bus_space_tag_t sc_bst;		/* bus space */
	bus_space_handle_t sc_bsh;

	/* Transmission buffer management. */
	int	txb_sched;	/* number of scheduled packets */
#if 0
    	/* XXX not used */
	int	txb_free;	/* free TX buffers */
	int	txb_size;	/* total size of TX buffer */
	int	txb_count;	/* number of TX buffers in use */
	int	rxb_size;   /* size of receive buffer */
#endif
	int txb_num_pkt;    /* max number of outstanding transmit packets allowed */
	int rxb_num_pkt;    /* max number of packets that could be in receive buffer */

	u_int8_t sc_enaddr[ETHER_ADDR_LEN];

	krndsource_t rnd_source;

	u_int32_t sc_stat;	/* driver status */

#define ESTAR_STAT_ENABLED	0x0001	/* power enabled on interface */
#define ESTAR_STAT_ATTACHED	0x0002	/* attach has succeeded */

	int	(*sc_enable)(struct mb86950_softc *);
	void	(*sc_disable)(struct mb86950_softc *);

	int	(*sc_mediachange)(struct mb86950_softc *);
	void	(*sc_mediastatus)(struct mb86950_softc *,
		    struct ifmediareq *);

};

/* Size (in bytes) of a "packet length" word in transmission buffer.  */
#define ESTAR_TXLEN_SIZE 2

#define GOOD_PKT 0x20

void    mb86950_attach(struct mb86950_softc *, u_int8_t *);
void    mb86950_config(struct mb86950_softc *, int *, int, int);
int     mb86950_intr(void *);
int     mb86950_enable(struct mb86950_softc *);
void    mb86950_disable(struct mb86950_softc *);
int     mb86950_activate(device_t, enum devact);
int     mb86950_detach(struct mb86950_softc *);
