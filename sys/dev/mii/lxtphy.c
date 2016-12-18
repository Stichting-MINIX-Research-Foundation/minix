/*	$NetBSD: lxtphy.c,v 1.49 2014/06/16 16:48:16 msaitoh Exp $	*/

/*-
 * Copyright (c) 1998, 1999, 2000 The NetBSD Foundation, Inc.
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
 * driver for Level One's LXT-970 ethernet 10/100 PHY
 * datasheet from www.level1.com
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: lxtphy.c,v 1.49 2014/06/16 16:48:16 msaitoh Exp $");

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

#include <dev/mii/lxtphyreg.h>

static int	lxtphymatch(device_t, cfdata_t, void *);
static void	lxtphyattach(device_t, device_t, void *);

CFATTACH_DECL_NEW(lxtphy, sizeof(struct mii_softc),
    lxtphymatch, lxtphyattach, mii_phy_detach, mii_phy_activate);

static int	lxtphy_service(struct mii_softc *, struct mii_data *, int);
static void	lxtphy_status(struct mii_softc *);
static void	lxtphy_reset(struct mii_softc *);

static void lxtphy_set_tp(struct mii_softc *);
static void lxtphy_set_fx(struct mii_softc *);

static const struct mii_phy_funcs lxtphy_funcs = {
	lxtphy_service, lxtphy_status, lxtphy_reset,
};

static const struct mii_phy_funcs lxtphy971_funcs = {
	lxtphy_service, ukphy_status, lxtphy_reset,
};

static const struct mii_phydesc lxtphys[] = {
	{ MII_OUI_xxLEVEL1,		MII_MODEL_xxLEVEL1_LXT970,
	  MII_STR_xxLEVEL1_LXT970 },

	{ MII_OUI_LEVEL1,		MII_MODEL_LEVEL1_LXT971,
	  MII_STR_LEVEL1_LXT971 },

	{ 0,				0,
	  NULL },
};

static int
lxtphymatch(device_t parent, cfdata_t match, void *aux)
{
	struct mii_attach_args *ma = aux;

	if (mii_phy_match(ma, lxtphys) != NULL)
		return (10);

	return (0);
}

static void
lxtphyattach(device_t parent, device_t self, void *aux)
{
	struct mii_softc *sc = device_private(self);
	struct mii_attach_args *ma = aux;
	struct mii_data *mii = ma->mii_data;
	const struct mii_phydesc *mpd;

	mpd = mii_phy_match(ma, lxtphys);
	aprint_naive(": Media interface\n");
	aprint_normal(": %s, rev. %d\n", mpd->mpd_name, MII_REV(ma->mii_id2));

	sc->mii_dev = self;
	sc->mii_inst = mii->mii_instance;
	sc->mii_phy = ma->mii_phyno;
	if (mpd->mpd_model == MII_MODEL_LEVEL1_LXT971)
		sc->mii_funcs = &lxtphy971_funcs;
	else
		sc->mii_funcs = &lxtphy_funcs;
	sc->mii_pdata = mii;
	sc->mii_flags = ma->mii_flags;
	sc->mii_anegticks = MII_ANEGTICKS;

	PHY_RESET(sc);

	sc->mii_capabilities =
	    PHY_READ(sc, MII_BMSR) & ma->mii_capmask;
	aprint_normal_dev(self, "");

	if (sc->mii_flags & MIIF_HAVEFIBER) {
#define	ADD(m, c)	ifmedia_add(&mii->mii_media, (m), (c), NULL)
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_FX, 0, sc->mii_inst),
		    MII_MEDIA_100_TX);
		aprint_normal("100baseFX, ");
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_FX, IFM_FDX, sc->mii_inst),
		    MII_MEDIA_100_TX_FDX);
		aprint_normal("100baseFX-FDX, ");
#undef ADD
	}

	if ((sc->mii_capabilities & BMSR_MEDIAMASK) == 0)
		aprint_error("no media present");
	else
		mii_phy_add_media(sc);
	aprint_normal("\n");
}

static int
lxtphy_service(struct mii_softc *sc, struct mii_data *mii, int cmd)
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

		if (IFM_SUBTYPE(ife->ifm_media) == IFM_100_FX)
			lxtphy_set_fx(sc);
		else
			lxtphy_set_tp(sc);

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
lxtphy_status(struct mii_softc *sc)
{
	struct mii_data *mii = sc->mii_pdata;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int bmcr, bmsr, csr;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	/*
	 * Get link status from the CSR; we need to read the CSR
	 * for media type anyhow, and the link status in the CSR
	 * doens't latch, so fewer register reads are required.
	 */
	csr = PHY_READ(sc, MII_LXTPHY_CSR);
	if (csr & CSR_LINK)
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
		bmsr = PHY_READ(sc, MII_BMSR) | PHY_READ(sc, MII_BMSR);
		if ((bmsr & BMSR_ACOMP) == 0) {
			/* Erg, still trying, I guess... */
			mii->mii_media_active |= IFM_NONE;
			return;
		}
		if (csr & CSR_SPEED)
			mii->mii_media_active |= IFM_100_TX;
		else
			mii->mii_media_active |= IFM_10_T;

		if (csr & CSR_DUPLEX)
			mii->mii_media_active |= IFM_FDX;
		else
			mii->mii_media_active |= IFM_HDX;
	} else
		mii->mii_media_active = ife->ifm_media;
}

static void
lxtphy_reset(struct mii_softc *sc)
{

	mii_phy_reset(sc);
	PHY_WRITE(sc, MII_LXTPHY_IER,
	    PHY_READ(sc, MII_LXTPHY_IER) & ~IER_INTEN);
}

static void
lxtphy_set_tp(struct mii_softc *sc)
{
	int cfg;

	cfg = PHY_READ(sc, MII_LXTPHY_CONFIG);
	cfg &= ~CONFIG_100BASEFX;
	PHY_WRITE(sc, MII_LXTPHY_CONFIG, cfg);
}

static void
lxtphy_set_fx(struct mii_softc *sc)
{
	int cfg;

	cfg = PHY_READ(sc, MII_LXTPHY_CONFIG);
	cfg |= CONFIG_100BASEFX;
	PHY_WRITE(sc, MII_LXTPHY_CONFIG, cfg);
}
