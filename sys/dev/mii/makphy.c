/*	$NetBSD: makphy.c,v 1.40 2014/06/16 16:48:16 msaitoh Exp $	*/

/*-
 * Copyright (c) 1998, 1999, 2000, 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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

/*
 * Copyright (c) 1997 Manuel Bouyer.  All rights reserved.
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

/*
 * Driver for the Marvell 88E1000 ``Alaska'' 10/100/1000 PHY.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: makphy.c,v 1.40 2014/06/16 16:48:16 msaitoh Exp $");

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

#include <dev/mii/makphyreg.h>

static int	makphymatch(device_t, cfdata_t, void *);
static void	makphyattach(device_t, device_t, void *);

CFATTACH_DECL_NEW(makphy, sizeof(struct mii_softc),
    makphymatch, makphyattach, mii_phy_detach, mii_phy_activate);

static int	makphy_service(struct mii_softc *, struct mii_data *, int);
static void	makphy_status(struct mii_softc *);
static void	makphy_reset(struct mii_softc *);

static const struct mii_phy_funcs makphy_funcs = {
	makphy_service, makphy_status, makphy_reset,
};

static const struct mii_phydesc makphys[] = {
	{ MII_OUI_xxMARVELL,		MII_MODEL_xxMARVELL_E1000_3,
	  MII_STR_xxMARVELL_E1000_3 },

	{ MII_OUI_xxMARVELL,		MII_MODEL_xxMARVELL_E1000_5,
	  MII_STR_xxMARVELL_E1000_5 },

	{ MII_OUI_xxMARVELL,		MII_MODEL_xxMARVELL_E1011,
	  MII_STR_xxMARVELL_E1011 },

	/* XXX: reported not to work on eg. HP XW9400 */
	{ MII_OUI_xxMARVELL,		MII_MODEL_xxMARVELL_E1149,
	  MII_STR_xxMARVELL_E1149 },

	{ MII_OUI_xxMARVELL,		MII_MODEL_xxMARVELL_E1149R,
	  MII_STR_xxMARVELL_E1149R },

	{ MII_OUI_xxMARVELL,		MII_MODEL_xxMARVELL_E1111,
	  MII_STR_xxMARVELL_E1111 },

	{ MII_OUI_xxMARVELL,		MII_MODEL_xxMARVELL_E1116,
	  MII_STR_xxMARVELL_E1116 },

	{ MII_OUI_xxMARVELL,		MII_MODEL_xxMARVELL_E1145,
	  MII_STR_xxMARVELL_E1145 },

	{ MII_OUI_xxMARVELL,		MII_MODEL_xxMARVELL_E1116R,
	  MII_STR_xxMARVELL_E1116R },

	{ MII_OUI_xxMARVELL,		MII_MODEL_xxMARVELL_E1116R_29,
	  MII_STR_xxMARVELL_E1116R_29 },

	{ MII_OUI_xxMARVELL,		MII_MODEL_xxMARVELL_E1543,
	  MII_STR_xxMARVELL_E1543 },

	{ 0,				0,
	  NULL },
};

static int
makphymatch(device_t parent, cfdata_t match, void *aux)
{
	struct mii_attach_args *ma = aux;

	if (mii_phy_match(ma, makphys) != NULL)
		return (10);

	return (0);
}

static void
makphyattach(device_t parent, device_t self, void *aux)
{
	struct mii_softc *sc = device_private(self);
	struct mii_attach_args *ma = aux;
	struct mii_data *mii = ma->mii_data;
	const struct mii_phydesc *mpd;

	mpd = mii_phy_match(ma, makphys);
	aprint_naive(": Media interface\n");
	aprint_normal(": %s, rev. %d\n", mpd->mpd_name, MII_REV(ma->mii_id2));

	sc->mii_dev = self;
	sc->mii_mpd_model = MII_MODEL(ma->mii_id2);
	sc->mii_inst = mii->mii_instance;
	sc->mii_phy = ma->mii_phyno;
	sc->mii_funcs = &makphy_funcs;
	sc->mii_pdata = mii;
	sc->mii_flags = ma->mii_flags;
	sc->mii_anegticks = MII_ANEGTICKS;

	switch (sc->mii_mpd_model) {
	case MII_MODEL_xxMARVELL_E1011:
	case MII_MODEL_xxMARVELL_E1112:
		if (PHY_READ(sc, MII_MAKPHY_ESSR) & ESSR_FIBER_LINK)
			sc->mii_flags |= MIIF_HAVEFIBER;
                break;
	case MII_MODEL_xxMARVELL_E1149:
	case MII_MODEL_xxMARVELL_E1149R:
		/*
		 * Some 88E1149 PHY's page select is initialized to
		 * point to other bank instead of copper/fiber bank
		 * which in turn resulted in wrong registers were
		 * accessed during PHY operation. It is believed that
		 * page 0 should be used for copper PHY so reinitialize
		 * MII_MAKPHY_EADR to select default copper PHY. If parent
		 * device know the type of PHY(either copper or fiber),
		 * that information should be used to select default
		 * type of PHY.
		 */
		PHY_WRITE(sc, MII_MAKPHY_EADR, 0);
		break;
	default:
		break;
	}

	PHY_RESET(sc);

	sc->mii_capabilities =
	    PHY_READ(sc, MII_BMSR) & ma->mii_capmask;
	if (sc->mii_capabilities & BMSR_EXTSTAT)
		sc->mii_extcapabilities = PHY_READ(sc, MII_EXTSR);

	aprint_normal_dev(self, "");
	if ((sc->mii_capabilities & BMSR_MEDIAMASK) == 0 &&
	    (sc->mii_extcapabilities & EXTSR_MEDIAMASK) == 0)
		aprint_error("no media present");
	else
		mii_phy_add_media(sc);
	aprint_normal("\n");
}

static void
makphy_reset(struct mii_softc *sc)
{
	uint16_t pscr;

	/* Assert CRS on transmit */
	pscr = PHY_READ(sc, MII_MAKPHY_PSCR);
	PHY_WRITE(sc, MII_MAKPHY_PSCR, pscr | PSCR_CRS_ON_TX);

	mii_phy_reset(sc);
}

static int
makphy_service(struct mii_softc *sc, struct mii_data *mii, int cmd)
{
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int reg;

	switch (cmd) {
	case MII_POLLSTAT:
		/*
		 * If we're not polling our PHY instance, just return.
		 */
		if (IFM_INST(ife->ifm_media) != sc->mii_inst)
			return (0);
		break;

	case MII_MEDIACHG:
		/*
		 * If the media indicates a different PHY instance,
		 * isolate ourselves.
		 */
		if (IFM_INST(ife->ifm_media) != sc->mii_inst) {
			reg = PHY_READ(sc, MII_BMCR);
			PHY_WRITE(sc, MII_BMCR, reg | BMCR_ISO);
			return (0);
		}

		/*
		 * If the interface is not up, don't do anything.
		 */
		if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
			break;

		mii_phy_setmedia(sc);
		if (IFM_SUBTYPE(ife->ifm_media) != IFM_AUTO) {
			/*
			 * when not in auto mode, we need to restart nego
			 * anyway, or a switch from a fixed mode to another
			 * fixed mode may not be seen by the switch.
			 */
			PHY_WRITE(sc, MII_BMCR,
			    PHY_READ(sc, MII_BMCR) | BMCR_STARTNEG);
		}
		break;

	case MII_TICK:
		/*
		 * If we're not currently selected, just return.
		 */
		if (IFM_INST(ife->ifm_media) != sc->mii_inst)
			return (0);

		if (mii_phy_tick(sc) == EJUSTRETURN)
			return (0);
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

static void
makphy_status(struct mii_softc *sc)
{
	struct mii_data *mii = sc->mii_pdata;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int bmcr, pssr, gtsr;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	pssr = PHY_READ(sc, MII_MAKPHY_PSSR);

	if (pssr & PSSR_LINK)
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
		 * The media status bits are only valid of autonegotiation
		 * has completed (or it's disabled).
		 */
		if ((pssr & PSSR_RESOLVED) == 0) {
			/* Erg, still trying, I guess... */
			mii->mii_media_active |= IFM_NONE;
			return;
		}

		switch (PSSR_SPEED_get(pssr)) {
		case SPEED_1000:
			mii->mii_media_active |= IFM_1000_T;
			gtsr = PHY_READ(sc, MII_100T2SR);
			if (gtsr & GTSR_MS_RES)
				mii->mii_media_active |= IFM_ETH_MASTER;
			break;

		case SPEED_100:
			mii->mii_media_active |= IFM_100_TX;
			break;

		case SPEED_10:
			mii->mii_media_active |= IFM_10_T;
			break;

		default:
			mii->mii_media_active |= IFM_NONE;
			mii->mii_media_status = 0;
			return;
		}

		if (pssr & PSSR_DUPLEX)
			mii->mii_media_active |=
			    IFM_FDX | mii_phy_flowstatus(sc);
		else
			mii->mii_media_active |= IFM_HDX;
	} else
		mii->mii_media_active = ife->ifm_media;
}
