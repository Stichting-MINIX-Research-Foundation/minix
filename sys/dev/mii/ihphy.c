/*	$NetBSD: ihphy.c,v 1.8 2014/06/16 16:48:16 msaitoh Exp $	*/

/*-
 * Copyright (c) 1998, 1999, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center, and by Frank van der Linden.
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
 * Driver for Intel's 82577 (Hanksville) Ethernet 10/100/1000 PHY
 * Data Sheet: http://download.intel.com/design/network/datashts/319439.pdf
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ihphy.c,v 1.8 2014/06/16 16:48:16 msaitoh Exp $");

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

#include <dev/mii/ihphyreg.h>

static int	ihphymatch(device_t, cfdata_t, void *);
static void	ihphyattach(device_t, device_t, void *);

CFATTACH_DECL3_NEW(ihphy, sizeof(struct mii_softc),
    ihphymatch, ihphyattach, mii_phy_detach, mii_phy_activate, NULL, NULL,
    DVF_DETACH_SHUTDOWN);

static int	ihphy_service(struct mii_softc *, struct mii_data *, int);
static void	ihphy_status(struct mii_softc *);
static void	ihphy_reset(struct mii_softc *);

static const struct mii_phy_funcs ihphy_funcs = {
	ihphy_service, ihphy_status, ihphy_reset,
};

static const struct mii_phydesc ihphys[] = {
	{ MII_OUI_INTEL,		MII_MODEL_INTEL_I82577,
	  MII_STR_INTEL_I82577 },
	{ MII_OUI_INTEL,		MII_MODEL_INTEL_I82579,
	  MII_STR_INTEL_I82579 },
	{ MII_OUI_INTEL,		MII_MODEL_INTEL_I217,
	  MII_STR_INTEL_I217 },

	{ 0,				0,
	  NULL },
};

static int
ihphymatch(device_t parent, cfdata_t match, void *aux)
{
	struct mii_attach_args *ma = aux;

	if (mii_phy_match(ma, ihphys) != NULL)
		return 10;

	return 0;
}

static void
ihphyattach(device_t parent, device_t self, void *aux)
{
	struct mii_softc *sc = device_private(self);
	struct mii_attach_args *ma = aux;
	struct mii_data *mii = ma->mii_data;
	const struct mii_phydesc *mpd;
	int reg;

	mpd = mii_phy_match(ma, ihphys);
	aprint_naive(": Media interface\n");
	aprint_normal(": %s, rev. %d\n", mpd->mpd_name, MII_REV(ma->mii_id2));

	sc->mii_dev = self;
	sc->mii_inst = mii->mii_instance;
	sc->mii_phy = ma->mii_phyno;
	sc->mii_funcs = &ihphy_funcs;
	sc->mii_pdata = mii;
	sc->mii_flags = ma->mii_flags;
	sc->mii_anegticks = MII_ANEGTICKS;

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

	/*
	 * Link setup (as done by Intel's Linux driver for the 82577).
	 */
	reg = PHY_READ(sc, IHPHY_MII_CFG);
	reg |= IHPHY_CFG_TX_CRS;
	reg |= IHPHY_CFG_DOWN_SHIFT;
	PHY_WRITE(sc, IHPHY_MII_CFG, reg);
}

static int
ihphy_service(struct mii_softc *sc, struct mii_data *mii, int cmd)
{
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int reg;

	switch (cmd) {
	case MII_POLLSTAT:
		/*
		 * If we're not polling our PHY instance, just return.
		 */
		if (IFM_INST(ife->ifm_media) != sc->mii_inst)
			return 0;
		break;

	case MII_MEDIACHG:
		/*
		 * If the media indicates a different PHY instance,
		 * isolate ourselves.
		 */
		if (IFM_INST(ife->ifm_media) != sc->mii_inst) {
			reg = PHY_READ(sc, MII_BMCR);
			PHY_WRITE(sc, MII_BMCR, reg | BMCR_ISO);
			return 0;
		}

		/*
		 * If the interface is not up, don't do anything.
		 */
		if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
			break;

		/*
		 * If media is deselected, disable link (standby).
		 */
		reg = PHY_READ(sc, IHPHY_MII_ECR);
		if (IFM_SUBTYPE(ife->ifm_media) == IFM_NONE)
			reg &= ~IHPHY_ECR_LNK_EN;
		else
			reg |= IHPHY_ECR_LNK_EN;
		PHY_WRITE(sc, IHPHY_MII_ECR, reg);

		/*
		 * XXX Adjust MDI/MDIX configuration?  Other settings?
		 */

		mii_phy_setmedia(sc);
		break;

	case MII_TICK:
		/*
		 * If we're not currently selected, just return.
		 */
		if (IFM_INST(ife->ifm_media) != sc->mii_inst)
			return 0;

		if (mii_phy_tick(sc) == EJUSTRETURN)
			return 0;
		break;

	case MII_DOWN:
		mii_phy_down(sc);
		PHY_WRITE(sc, MII_BMCR, BMCR_PDOWN);
		return 0;
	}

	/* Update the media status. */
	mii_phy_status(sc);

	/* Callback if something changed. */
	mii_phy_update(sc, cmd);
	return 0;
}

static void
ihphy_status(struct mii_softc *sc)
{
	struct mii_data *mii = sc->mii_pdata;
	int esr, bmcr, gtsr;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	esr = PHY_READ(sc, IHPHY_MII_ESR);

	if (esr & IHPHY_ESR_LINK)
		mii->mii_media_status |= IFM_ACTIVE;

	bmcr = PHY_READ(sc, MII_BMCR);
	if (bmcr & (BMCR_ISO | BMCR_PDOWN)) {
		mii->mii_media_active |= IFM_NONE;
		mii->mii_media_status = 0;
		return;
	}

	if (bmcr & BMCR_LOOP)
		mii->mii_media_active |= IFM_LOOP;

	if (bmcr & BMCR_AUTOEN) {
		if ((esr & IHPHY_ESR_ANEG_STAT) == 0) {
			/* Erg, still trying, I guess... */
			mii->mii_media_active |= IFM_NONE;
			return;
		}
	}

	switch (esr & IHPHY_ESR_SPEED) {
	case IHPHY_SPEED_1000:
		mii->mii_media_active |= IFM_1000_T;
		gtsr = PHY_READ(sc, MII_100T2SR);
		if (gtsr & GTSR_MS_RES)
			mii->mii_media_active |= IFM_ETH_MASTER;
		break;

	case IHPHY_SPEED_100:
		/* 100BASE-T2 and 100BASE-T4 are not supported. */
		mii->mii_media_active |= IFM_100_TX;
		break;

	case IHPHY_SPEED_10:
		mii->mii_media_active |= IFM_10_T;
		break;

	default:
		mii->mii_media_active |= IFM_NONE;
		mii->mii_media_status = 0;
		return;
	}

	if (esr & IHPHY_ESR_DUPLEX)
		mii->mii_media_active |= IFM_FDX | mii_phy_flowstatus(sc);
	else
		mii->mii_media_active |= IFM_HDX;
}

static void
ihphy_reset(struct mii_softc *sc)
{
	int reg, i;

	PHY_WRITE(sc, MII_BMCR, BMCR_RESET | BMCR_ISO);

	/*
	 * Regarding reset, the data sheet specifies (page 55):
	 *
	 * "After PHY reset, a delay of 10 ms is required before
	 *  any register access using MDIO."
	 */
	delay(10000);

	/* Wait another 100ms for it to complete. */
	for (i = 0; i < 100; i++) {
		reg = PHY_READ(sc, MII_BMCR);
		if ((reg & BMCR_RESET) == 0)
			break;
		delay(1000);
	}

	if (sc->mii_inst != 0)
		PHY_WRITE(sc, MII_BMCR, reg | BMCR_ISO);
}
