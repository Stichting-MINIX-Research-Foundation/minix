/*	$NetBSD: sqphy.c,v 1.50 2014/06/16 16:48:16 msaitoh Exp $	*/

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
 * driver for Seeq 80220/80221, 80223, and 80225 10/100 ethernet PHYs
 * datasheet from www.seeq.com
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sqphy.c,v 1.50 2014/06/16 16:48:16 msaitoh Exp $");

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

#include <dev/mii/sqphyreg.h>

static int	sqphymatch(device_t, cfdata_t, void *);
static void	sqphyattach(device_t, device_t, void *);

CFATTACH_DECL_NEW(sqphy, sizeof(struct mii_softc),
    sqphymatch, sqphyattach, mii_phy_detach, mii_phy_activate);

static int	sqphy_service(struct mii_softc *, struct mii_data *, int);
static void	sqphy_status(struct mii_softc *);
static void	sqphy_84220_reset(struct mii_softc *);

static const struct mii_phy_funcs sqphy_funcs = {
	sqphy_service, sqphy_status, mii_phy_reset,
};

static const struct mii_phy_funcs sqphy_84220_funcs = {
	sqphy_service, sqphy_status, sqphy_84220_reset,
};

static const struct mii_phydesc sqphys[] = {
	{ MII_OUI_SEEQ,			MII_MODEL_SEEQ_80220,
	  MII_STR_SEEQ_80220 },

	{ MII_OUI_SEEQ,			MII_MODEL_SEEQ_80225,
	  MII_STR_SEEQ_80225 },

	{ MII_OUI_SEEQ,			MII_MODEL_SEEQ_84220,
	  MII_STR_SEEQ_84220 },

	{ 0,				0,
	  NULL },
};

static int
sqphymatch(device_t parent, cfdata_t match, void *aux)
{
	struct mii_attach_args *ma = aux;

	if (mii_phy_match(ma, sqphys) != NULL)
		return (10);

	return (0);
}

static void
sqphyattach(device_t parent, device_t self, void *aux)
{
	struct mii_softc *sc = device_private(self);
	struct mii_attach_args *ma = aux;
	struct mii_data *mii = ma->mii_data;
	const struct mii_phydesc *mpd;

	mpd = mii_phy_match(ma, sqphys);
	aprint_naive(": Media interface\n");
	aprint_normal(": %s, rev. %d\n", mpd->mpd_name, MII_REV(ma->mii_id2));

	sc->mii_dev = self;
	sc->mii_inst = mii->mii_instance;
	sc->mii_phy = ma->mii_phyno;
	sc->mii_pdata = mii;
	sc->mii_flags = ma->mii_flags;
	sc->mii_anegticks = MII_ANEGTICKS;

	switch (MII_MODEL(ma->mii_id2)) {
	case MII_MODEL_SEEQ_84220:
		sc->mii_funcs = &sqphy_84220_funcs;
		aprint_normal_dev(self, "using Seeq 84220 isolate/reset hack\n");
		break;

	default:
		sc->mii_funcs = &sqphy_funcs;
	}

	PHY_RESET(sc);

	sc->mii_capabilities =
	    PHY_READ(sc, MII_BMSR) & ma->mii_capmask;
	aprint_normal_dev(self, "");
	if ((sc->mii_capabilities & BMSR_MEDIAMASK) == 0)
		aprint_error("no media present");
	else
		mii_phy_add_media(sc);
	aprint_normal("\n");
}

static int
sqphy_service(struct mii_softc *sc, struct mii_data *mii, int cmd)
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
sqphy_status(struct mii_softc *sc)
{
	struct mii_data *mii = sc->mii_pdata;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int bmsr, bmcr, status;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	bmsr = PHY_READ(sc, MII_BMSR) |
	    PHY_READ(sc, MII_BMSR);
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
		if ((bmsr & BMSR_ACOMP) == 0) {
			/* Erg, still trying, I guess... */
			mii->mii_media_active |= IFM_NONE;
			return;
		}
		/*
		 * Note: don't get fancy here -- the 80225 only
		 * supports the SPD_DET and DPLX_DET bits in
		 * the STATUS register.
		 */
		status = PHY_READ(sc, MII_SQPHY_STATUS);
		if (status & STATUS_SPD_DET)
			mii->mii_media_active |= IFM_100_TX;
		else
			mii->mii_media_active |= IFM_10_T;
		if (status & STATUS_DPLX_DET)
			mii->mii_media_active |= IFM_FDX;
		else
			mii->mii_media_active |= IFM_HDX;
	} else
		mii->mii_media_active = ife->ifm_media;
}

static void
sqphy_84220_reset(struct mii_softc *sc)
{
	int reg;

	mii_phy_reset(sc);

	/*
	 * This PHY sometimes insists on coming out of reset isolated,
	 * even when the MDA[0-3] pins are pulled high (to indicate
	 * PHY address 0), contrary to the device's datasheet.
	 *
	 * Morever, simply clearing BMCR_ISO here isn't enough; the
	 * change won't stick until about 30mS *after* the PHY has
	 * been reset.
	 *
	 * This sucks.
	 */
	while ((sc->mii_inst == 0 || (sc->mii_flags & MIIF_NOISOLATE)) &&
	    ((reg = PHY_READ(sc, MII_BMCR)) & BMCR_ISO) != 0) {

		delay(35000);
		PHY_WRITE(sc, MII_BMCR, reg & ~BMCR_ISO);
		delay(35000);
	}
}
