/*	$NetBSD: dm9000var.h,v 1.2 2012/01/28 08:29:55 nisimura Exp $	*/

/*
 * Copyright (c) 2009 Paul Fleischer
 * All rights reserved.
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* Based on sys/dev/ic/cs89x0var.h */
/*
 * Copyright 1997
 * Digital Equipment Corporation. All rights reserved.
 *
 * This software is furnished under license and may be used and
 * copied only in accordance with the following terms and conditions.
 * Subject to these conditions, you may download, copy, install,
 * use, modify and distribute this software in source and/or binary
 * form. No title or ownership is transferred hereby.
 *
 * 1) Any source code used, modified or distributed must reproduce
 *    and retain this copyright notice and list of conditions as
 *    they appear in the source file.
 *
 * 2) No right is granted to use any trade name, trademark, or logo of
 *    Digital Equipment Corporation. Neither the "Digital Equipment
 *    Corporation" name nor any trademark or logo of Digital Equipment
 *    Corporation may be used to endorse or promote products derived
 *    from this software without the prior written permission of
 *    Digital Equipment Corporation.
 *
 * 3) This software is provided "AS-IS" and any express or implied
 *    warranties, including but not limited to, any implied warranties
 *    of merchantability, fitness for a particular purpose, or
 *    non-infringement are disclaimed. In no event shall DIGITAL be
 *    liable for any damages whatsoever, and in particular, DIGITAL
 *    shall not be liable for special, indirect, consequential, or
 *    incidental damages or damages for lost profits, loss of
 *    revenue or loss of use, whether such damages arise in contract,
 *    negligence, tort, under statute, in equity, at law or otherwise,
 *    even if advised of the possibility of such damage.
 */

#ifndef _DEV_IC_DM9000VAR_H_
#define _DEV_IC_DM9000VAR_H_

#include <sys/callout.h>

#define DM9000_MODE_8BIT 2
#define DM9000_MODE_16BIT 0
#define DM9000_MODE_32BIT 1

struct dme_softc {
	device_t	sc_dev;		/* Generic Base Device */

	struct ethercom sc_ethercom;	/* Ethernet common data */
	struct ifmedia	sc_media;	/* Media control structures */

	uint		sc_media_active;
	uint		sc_media_status;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	void		*sc_ih;

	uint		dme_io;
	uint		dme_data;

	uint16_t	sc_vendor_id;
	uint16_t	sc_product_id;

	uint8_t		sc_data_width;

	uint8_t		sc_enaddr[ETHER_ADDR_LEN];

	int		txbusy;		/* A packet is being transmitted. */
	int		txready;	/* A packet has been sent to the DM9000
					   for transmission. */
	uint16_t	txready_length;

	int (*sc_pkt_write)(struct dme_softc*, struct mbuf *);
	int (*sc_pkt_read)(struct dme_softc*, struct ifnet *, struct mbuf **);

	callout_t	sc_link_callout;

	bool		sc_phy_initialized;

#ifdef DIAGNOSTIC
	bool		sc_inside_interrupt;
#endif
};

/* Function declarations */
int	dme_attach(struct dme_softc *, const uint8_t *);
int	dme_detach(struct dme_softc *);
int	dme_intr(void *);

/* Helper method used by sc_pkt_read */
struct mbuf* dme_alloc_receive_buffer(struct ifnet *, unsigned int);

/* Inline memory access methods */
static inline uint8_t
dme_read(struct dme_softc *sc, int reg)
{
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, sc->dme_io, reg);
	return (bus_space_read_1(sc->sc_iot, sc->sc_ioh, sc->dme_data));
}

static inline void
dme_write(struct dme_softc *sc, int reg, uint8_t value)
{
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, sc->dme_io, reg);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, sc->dme_data, value);
}

static inline void
dme_write2(struct dme_softc *sc, int reg, uint16_t value)
{
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, sc->dme_io, reg);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, sc->dme_data, value);
}

static inline void
dme_write_c(struct dme_softc *sc, int reg, uint8_t value[], uint count)
{
	for(int i=0; i<count; i++) {
		dme_write(sc, reg+i, value[i]);
	}
}

static inline void
dme_read_c(struct dme_softc *sc, int reg, uint8_t *value, uint count)
{
	for(int i=0; i<count; i++) {
		value[i] = dme_read(sc, reg+i);
	}
}

#endif /* _DEV_IC_DM9000VAR_H_ */

