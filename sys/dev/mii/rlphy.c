/*	$NetBSD: rlphy.c,v 1.29 2014/06/16 16:48:16 msaitoh Exp $	*/
/*	$OpenBSD: rlphy.c,v 1.20 2005/07/31 05:27:30 pvalchev Exp $	*/

/*
 * Copyright (c) 1998, 1999 Jason L. Wright (jason@thought.net)
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
 */

/*
 * Driver for the internal PHY found on RTL8139 based nics, based
 * on drivers for the 'exphy' (Internal 3Com phys) and 'nsphy'
 * (National Semiconductor DP83840).
 */

/*
 * Ported to NetBSD by Juan Romero Pardines <xtraeme@NetBSD.org>
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rlphy.c,v 1.29 2014/06/16 16:48:16 msaitoh Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/socket.h>
#include <sys/errno.h>

#include <net/if.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/miidevs.h>
#include <sys/bus.h>
#include <dev/ic/rtl81x9reg.h>

struct rlphy_softc {
	struct mii_softc sc_mii;
	int sc_rtl8201l;
};

int	rlphymatch(device_t, cfdata_t, void *);
void	rlphyattach(device_t, device_t, void *);

CFATTACH_DECL_NEW(rlphy, sizeof(struct rlphy_softc),
    rlphymatch, rlphyattach, mii_phy_detach, mii_phy_activate);

int	rlphy_service(struct mii_softc *, struct mii_data *, int);
void	rlphy_status(struct mii_softc *);

static void rlphy_reset(struct mii_softc *);

const struct mii_phy_funcs rlphy_funcs = {
	rlphy_service, rlphy_status, rlphy_reset,
};

static const struct mii_phydesc rlphys[] = {
	{ MII_OUI_yyREALTEK,		MII_MODEL_yyREALTEK_RTL8201L,
	  MII_STR_yyREALTEK_RTL8201L },
	{ MII_OUI_ICPLUS,		MII_MODEL_ICPLUS_IP101,
	  MII_STR_ICPLUS_IP101 },

	{ 0,				0,
	  NULL },
};

int
rlphymatch(device_t parent, cfdata_t match, void *aux)
{
	struct mii_attach_args *ma = aux;
	struct mii_data *mii = ma->mii_data;

	if (mii->mii_instance != 0)
		return 0;

	if (mii_phy_match(ma, rlphys) != NULL)
		return (10);

	if (MII_OUI(ma->mii_id1, ma->mii_id2) != 0 ||
	    MII_MODEL(ma->mii_id2) != 0)
		return 0;

	if (!device_is_a(parent, "rtk") && !device_is_a(parent, "re"))
		return 0;

	/*
	 * A "real" phy should get preference, but on the 8139 there
	 * is no phyid register.
	 */
	return 5;
}

void
rlphyattach(device_t parent, device_t self, void *aux)
{
	struct rlphy_softc *rsc = device_private(self);
	struct mii_softc *sc = &rsc->sc_mii;
	struct mii_attach_args *ma = aux;
	struct mii_data *mii = ma->mii_data;

	aprint_naive("\n");
	if (MII_MODEL(ma->mii_id2) == MII_MODEL_yyREALTEK_RTL8201L) {
		rsc->sc_rtl8201l = 1;
		aprint_normal(": %s, rev. %d\n", MII_STR_yyREALTEK_RTL8201L,
		    MII_REV(ma->mii_id2));
	} else
		aprint_normal(": Realtek internal PHY\n");

	sc->mii_dev = self;
	sc->mii_inst = mii->mii_instance;
	sc->mii_phy = ma->mii_phyno;
	sc->mii_funcs = &rlphy_funcs;
	sc->mii_pdata = mii;
	sc->mii_flags = ma->mii_flags;

	sc->mii_flags |= MIIF_NOISOLATE;

	PHY_RESET(sc);

	aprint_normal_dev(self, "");
	sc->mii_capabilities =
	    PHY_READ(sc, MII_BMSR) & ma->mii_capmask;
	if (sc->mii_capabilities & BMSR_MEDIAMASK)
		mii_phy_add_media(sc);
	aprint_normal("\n");
}

int
rlphy_service(struct mii_softc *sc, struct mii_data *mii, int cmd)
{
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;

	/*
	 * Can't isolate the RTL8139 phy, so it has to be the only one.
	 */
	if (IFM_INST(ife->ifm_media) != sc->mii_inst)
		panic("rlphy_service: attempt to isolate phy");

	switch (cmd) {
	case MII_POLLSTAT:
		break;

	case MII_MEDIACHG:
		/*
		 * If the interface is not up, don't do anything.
		 */
		if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
			break;

		mii_phy_setmedia(sc);
		break;

	case MII_TICK:
		/*
		 * Is the interface even up?
		 */
		if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
			return (0);

		/*
		 * Only used for autonegotiation.
		 */
		if (IFM_SUBTYPE(ife->ifm_media) != IFM_AUTO)
			break;

		/*
		 * The RealTek PHY's autonegotiation doesn't need to be
		 * kicked; it continues in the background.
		 */
		break;

	case MII_DOWN:
		mii_phy_down(sc);
		return (0);
	}

	/* Update the media status. */
	mii_phy_status(sc);

	/* Callback if something changed. */
	mii_phy_update(sc, cmd);
	return (0);
}

void
rlphy_status(struct mii_softc *sc)
{
	struct rlphy_softc *rsc = (void *)sc;
	struct mii_data *mii = sc->mii_pdata;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int bmsr, bmcr, anlpar;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	bmsr = PHY_READ(sc, MII_BMSR) | PHY_READ(sc, MII_BMSR);
	if (bmsr & BMSR_LINK)
		mii->mii_media_status |= IFM_ACTIVE;

	bmcr = PHY_READ(sc, MII_BMCR);
	if (bmcr & BMCR_ISO) {
		mii->mii_media_active |= IFM_NONE;
		mii->mii_media_status = 0;
		return;
	}

	if (bmcr & BMCR_LOOP)
		mii->mii_media_active |= IFM_LOOP;

	if (bmcr & BMCR_AUTOEN) {
		/*
		 * NWay autonegotiation takes the highest-order common
		 * bit of the ANAR and ANLPAR (i.e. best media advertised
		 * both by us and our link partner).
		 */
		if ((bmsr & BMSR_ACOMP) == 0) {
			/* Erg, still trying, I guess... */
			mii->mii_media_active |= IFM_NONE;
			return;
		}

		if ((anlpar = PHY_READ(sc, MII_ANAR) &
		    PHY_READ(sc, MII_ANLPAR))) {
			if (anlpar & ANLPAR_TX_FD)
				mii->mii_media_active |= IFM_100_TX|IFM_FDX;
			else if (anlpar & ANLPAR_T4)
				mii->mii_media_active |= IFM_100_T4|IFM_HDX;
			else if (anlpar & ANLPAR_TX)
				mii->mii_media_active |= IFM_100_TX|IFM_HDX;
			else if (anlpar & ANLPAR_10_FD)
				mii->mii_media_active |= IFM_10_T|IFM_FDX;
			else if (anlpar & ANLPAR_10)
				mii->mii_media_active |= IFM_10_T|IFM_HDX;
			else
				mii->mii_media_active |= IFM_NONE;
			return;
		}

		/*
		 * If the other side doesn't support NWAY, then the
		 * best we can do is determine if we have a 10Mbps or
		 * 100Mbps link. There's no way to know if the link
		 * is full or half duplex, so we default to half duplex
		 * and hope that the user is clever enough to manually
		 * change the media settings if we're wrong.
		 */

		/*
		 * The RealTek PHY supports non-NWAY link speed
		 * detection, however it does not report the link
		 * detection results via the ANLPAR or BMSR registers.
		 * (What? RealTek doesn't do things the way everyone
		 * else does? I'm just shocked, shocked I tell you.)
		 * To determine the link speed, we have to do one
		 * of two things:
		 *
		 * - If this is a standalone RealTek RTL8201(L) PHY,
		 *   we can determine the link speed by testing bit 0
		 *   in the magic, vendor-specific register at offset
		 *   0x19.
		 *
		 * - If this is a RealTek MAC with integrated PHY, we
		 *   can test the 'SPEED10' bit of the MAC's media status
		 *   register.
		 */
		if (rsc->sc_rtl8201l) {
			if (PHY_READ(sc, 0x0019) & 0x01)
				mii->mii_media_active |= IFM_100_TX;
			else
				mii->mii_media_active |= IFM_10_T;
		} else {
			if (PHY_READ(sc, RTK_MEDIASTAT) & RTK_MEDIASTAT_SPEED10)
				mii->mii_media_active |= IFM_10_T;
			else
				mii->mii_media_active |= IFM_100_TX;
		}
		mii->mii_media_active |= IFM_HDX;
	} else
		mii->mii_media_active = ife->ifm_media;
}

static void
rlphy_reset(struct mii_softc *sc)
{

	mii_phy_reset(sc);

	/*
	 * XXX RealTek PHY doesn't set the BMCR properly after
	 * XXX reset, which breaks autonegotiation.
	 */
	PHY_WRITE(sc, MII_BMCR, BMCR_AUTOEN);
}
