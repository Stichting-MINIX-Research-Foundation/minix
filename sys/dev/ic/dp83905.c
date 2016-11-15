/*	$NetBSD: dp83905.c,v 1.4 2007/10/19 11:59:50 ad Exp $	*/

/*-
 * Copyright (c) 2001 Ben Harris
 * Copyright (c) 1998 Mike Pumford
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>

__KERNEL_RCSID(0, "$NetBSD: dp83905.c,v 1.4 2007/10/19 11:59:50 ad Exp $");

#include <sys/device.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#include <sys/bus.h>

#include <dev/ic/dp8390reg.h>
#include <dev/ic/dp8390var.h>

#include <dev/ic/dp83905reg.h>
#include <dev/ic/dp83905var.h>

int
dp83905_mediachange(struct dp8390_softc *sc)
{

	/* Media already set up.  Reset the interface to make it stick. */
	dp8390_reset(sc);
	return (0);
}

void
dp83905_mediastatus(struct dp8390_softc *sc, struct ifmediareq *ifmr)
{
	bus_space_tag_t nict = sc->sc_regt;
	bus_space_handle_t nich = sc->sc_regh;
	u_int8_t mcrb;

	bus_space_write_1(nict, nich, ED_P0_CR,
	    ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_STA);
	mcrb = bus_space_read_1(nict, nich, DP83905_MCRB);
	/* Random op so next RBCR1 write doesn't go to MCRB. */
	bus_space_write_1(nict, nich, ED_P0_CR,
	    ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_STA);
	switch (mcrb & DP83905_MCRB_PHY_MASK) {
	case DP83905_MCRB_PHY_10_T:
	case DP83905_MCRB_PHY_TPI_NONSPEC: /* XXX */
		ifmr->ifm_active = IFM_ETHER | IFM_10_T;
		ifmr->ifm_status = IFM_AVALID;
		if (mcrb & DP83905_MCRB_GDLNK)
			ifmr->ifm_status |= IFM_ACTIVE;
		break;
	case DP83905_MCRB_PHY_10_2:
		ifmr->ifm_active = IFM_ETHER | IFM_10_2;
		break;
	case DP83905_MCRB_PHY_AUI:
		ifmr->ifm_active = IFM_ETHER | IFM_10_5;
		break;
	}
}

void dp83905_init_card(struct dp8390_softc *sc)
{
	struct ifmedia *ifm = &sc->sc_media;
	bus_space_tag_t nict = sc->sc_regt;
	bus_space_handle_t nich = sc->sc_regh;
	u_int8_t mcrb;

	/* Set basic media type. */
	switch (IFM_SUBTYPE(ifm->ifm_cur->ifm_media)) {
	case IFM_AUTO:
		/* software auto detect the media */
		mcrb = bus_space_read_1(nict, nich, DP83905_MCRB);
		mcrb &= ~(DP83905_MCRB_PHY_MASK | DP83905_MCRB_GDLNK |
		    DP83905_MCRB_BE);
		mcrb |= DP83905_MCRB_PHY_10_T;
		bus_space_write_1(nict, nich, DP83905_MCRB, mcrb);
		mcrb = bus_space_read_1(nict, nich, DP83905_MCRB);
		if (mcrb & DP83905_MCRB_GDLNK)
			break;
		/* No UTP use BNC */
		/* FALLTHROUGH */

	case IFM_10_2:
		mcrb = bus_space_read_1(nict, nich, DP83905_MCRB);
		mcrb &= ~(DP83905_MCRB_PHY_MASK | DP83905_MCRB_GDLNK |
		    DP83905_MCRB_BE);
		mcrb |= DP83905_MCRB_PHY_10_2;
		bus_space_write_1(nict, nich, DP83905_MCRB, mcrb);
		/*
		 * seems that re-reading config B here is required to
	         * prevent the interface hanging when manually selecting.
		 */
		bus_space_read_1(nict, nich, DP83905_MCRB);
		break;

	case IFM_10_T:
		mcrb = bus_space_read_1(nict, nich, DP83905_MCRB);
		mcrb &= ~(DP83905_MCRB_PHY_MASK | DP83905_MCRB_GDLNK |
		    DP83905_MCRB_BE);
		mcrb |= DP83905_MCRB_PHY_10_T;
		bus_space_write_1(nict, nich, DP83905_MCRB, mcrb);
		/*
		 * seems that re-reading config B here is required to
	         * prevent the interface hanging when manually selecting.
		 */
		bus_space_read_1(nict, nich, DP83905_MCRB);
		break;

	case IFM_10_5:
		mcrb = bus_space_read_1(nict, nich, DP83905_MCRB);
		mcrb &= ~(DP83905_MCRB_PHY_MASK | DP83905_MCRB_GDLNK |
		    DP83905_MCRB_BE);
		mcrb |= DP83905_MCRB_PHY_AUI;
		bus_space_write_1(nict, nich, DP83905_MCRB, mcrb);
		/*
		 * seems that re-reading config B here is required to
	         * prevent the interface hanging when manually selecting.
		 */
		bus_space_read_1(nict, nich, DP83905_MCRB);
		break;
	}
}
