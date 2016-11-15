/*	$NetBSD: tlphy.c,v 1.62 2014/06/16 16:48:16 msaitoh Exp $	*/

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
 * Driver for Texas Instruments's ThunderLAN PHYs
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: tlphy.c,v 1.62 2014/06/16 16:48:16 msaitoh Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/socket.h>
#include <sys/errno.h>

#include <sys/bus.h>

#include <net/if.h>
#include <net/if_media.h>

#include <net/if_ether.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/miidevs.h>

#include <dev/mii/tlphyreg.h>
#include <dev/mii/tlphyvar.h>

/* ThunderLAN PHY can only be on a ThunderLAN */
#include <dev/pci/if_tlvar.h>

struct tlphy_softc {
	struct mii_softc sc_mii;		/* generic PHY */
	int sc_tlphycap;
	int sc_need_acomp;
};

static int	tlphymatch(device_t, cfdata_t, void *);
static void	tlphyattach(device_t, device_t, void *);

CFATTACH_DECL_NEW(tlphy, sizeof(struct tlphy_softc),
    tlphymatch, tlphyattach, mii_phy_detach, mii_phy_activate);

static int	tlphy_service(struct mii_softc *, struct mii_data *, int);
static int	tlphy_auto(struct tlphy_softc *, int);
static void	tlphy_acomp(struct tlphy_softc *);
static void	tlphy_status(struct mii_softc *);

static const struct mii_phy_funcs tlphy_funcs = {
	tlphy_service, tlphy_status, mii_phy_reset,
};

static const struct mii_phydesc tlphys[] = {
	{ MII_OUI_TI,		MII_MODEL_TI_TLAN10T,
	  MII_STR_TI_TLAN10T },

	{ 0,			0,
	  NULL },
};

static int
tlphymatch(device_t parent, cfdata_t match, void *aux)
{
	struct mii_attach_args *ma = aux;

	if (mii_phy_match(ma, tlphys) != NULL)
		return (10);

	return (0);
}

static void
tlphyattach(device_t parent, device_t self, void *aux)
{
	struct tlphy_softc *tsc = device_private(self);
	struct mii_softc *sc = &tsc->sc_mii;
	struct tl_softc *tlsc = device_private(device_parent(self));
	struct mii_attach_args *ma = aux;
	struct mii_data *mii = ma->mii_data;
	const struct mii_phydesc *mpd;
	const char *sep = "";

	mpd = mii_phy_match(ma, tlphys);
	aprint_naive(": Media interface\n");
	aprint_normal(": %s, rev. %d\n", mpd->mpd_name, MII_REV(ma->mii_id2));

	sc->mii_dev = self;
	sc->mii_inst = mii->mii_instance;
	sc->mii_phy = ma->mii_phyno;
	sc->mii_funcs = &tlphy_funcs;
	sc->mii_pdata = mii;
	sc->mii_flags = ma->mii_flags;
	sc->mii_anegticks = MII_ANEGTICKS;

	PHY_RESET(sc);

	/*
	 * Note that if we're on a device that also supports 100baseTX,
	 * we are not going to want to use the built-in 10baseT port,
	 * since there will be another PHY on the MII wired up to the
	 * UTP connector.  The parent indicates this to us by specifying
	 * the TLPHY_MEDIA_NO_10_T bit.
	 */
	tsc->sc_tlphycap = tlsc->tl_product->tp_tlphymedia;
	if ((tsc->sc_tlphycap & TLPHY_MEDIA_NO_10_T) == 0)
		sc->mii_capabilities =
		    PHY_READ(sc, MII_BMSR) & ma->mii_capmask;
	else
		sc->mii_capabilities = 0;


#define	ADD(m, c)	ifmedia_add(&mii->mii_media, (m), (c), NULL)
#define	PRINT(str)	aprint_normal("%s%s", sep, str); sep = ", "

	aprint_normal_dev(self, "");
	if (tsc->sc_tlphycap) {
		if (tsc->sc_tlphycap & TLPHY_MEDIA_10_2) {
			ADD(IFM_MAKEWORD(IFM_ETHER, IFM_10_2, 0,
			    sc->mii_inst), 0);
			PRINT("10base2");
		} else if (tsc->sc_tlphycap & TLPHY_MEDIA_10_5) {
			ADD(IFM_MAKEWORD(IFM_ETHER, IFM_10_5, 0,
			    sc->mii_inst), 0);
			PRINT("10base5");
		}
	}
	if (sc->mii_capabilities & BMSR_MEDIAMASK) {
		aprint_normal("%s", sep);
		mii_phy_add_media(sc);
	} else if ((tsc->sc_tlphycap &
		    (TLPHY_MEDIA_10_2 | TLPHY_MEDIA_10_5)) == 0)
		aprint_error("no media present");
	else if (!pmf_device_register(self, NULL, mii_phy_resume)) {
		aprint_normal("\n");
		aprint_error_dev(self, "couldn't establish power handler");
	}
	aprint_normal("\n");
#undef ADD
#undef PRINT
}

static int
tlphy_service(struct mii_softc *sc, struct mii_data *mii, int cmd)
{
	struct tlphy_softc *tsc = (struct tlphy_softc *)sc;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int reg;

	if ((sc->mii_flags & MIIF_DOINGAUTO) == 0 && tsc->sc_need_acomp)
		tlphy_acomp(tsc);

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

		switch (IFM_SUBTYPE(ife->ifm_media)) {
		case IFM_AUTO:
			/*
			 * The ThunderLAN PHY doesn't self-configure after
			 * an autonegotiation cycle, so there's no such
			 * thing as "already in auto mode".
			 */
			(void) tlphy_auto(tsc, 1);
			break;
		case IFM_10_2:
		case IFM_10_5:
			PHY_WRITE(sc, MII_BMCR, 0);
			PHY_WRITE(sc, MII_TLPHY_CTRL, CTRL_AUISEL);
			delay(100000);
			break;
		default:
			PHY_WRITE(sc, MII_TLPHY_CTRL, 0);
			delay(100000);
			mii_phy_setmedia(sc);
		}
		break;

	case MII_TICK:
		/*
		 * If we're not currently selected, just return.
		 */
		if (IFM_INST(ife->ifm_media) != sc->mii_inst)
			return (0);

		/*
		 * XXX WHAT ABOUT CHECKING LINK ON THE BNC/AUI?!
		 */

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
tlphy_status(struct mii_softc *sc)
{
	struct tlphy_softc *tsc = (struct tlphy_softc *)sc;
	struct mii_data *mii = sc->mii_pdata;
	int bmsr, bmcr, tlctrl;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	bmcr = PHY_READ(sc, MII_BMCR);
	if (bmcr & BMCR_ISO) {
		mii->mii_media_active |= IFM_NONE;
		mii->mii_media_status = 0;
		return;
	}

	tlctrl = PHY_READ(sc, MII_TLPHY_CTRL);
	if (tlctrl & CTRL_AUISEL) {
		if (tsc->sc_tlphycap & TLPHY_MEDIA_10_2)
			mii->mii_media_active |= IFM_10_2;
		else if (tsc->sc_tlphycap & TLPHY_MEDIA_10_5)
			mii->mii_media_active |= IFM_10_5;
		else
			printf("%s: AUI selected with no matching media !\n",
			    device_xname(sc->mii_dev));
		mii->mii_media_status |= IFM_ACTIVE;
		return;
	}

	bmsr = PHY_READ(sc, MII_BMSR) |
	    PHY_READ(sc, MII_BMSR);
	if (bmsr & BMSR_LINK)
		mii->mii_media_status |= IFM_ACTIVE;

	if (bmcr & BMCR_LOOP)
		mii->mii_media_active |= IFM_LOOP;

	/*
	 * Grr, braindead ThunderLAN PHY doesn't have any way to
	 * tell which media is actually active.  (Note it also
	 * doesn't self-configure after autonegotiation.)  We
	 * just have to report what's in the BMCR.
	 */
	if (bmcr & BMCR_FDX)
		mii->mii_media_active |= IFM_FDX;
	else
		mii->mii_media_active |= IFM_HDX;
	mii->mii_media_active |= IFM_10_T;
}

static int
tlphy_auto(struct tlphy_softc *tsc, int waitfor)
{
	struct mii_softc *sc = &tsc->sc_mii;
	int error;

	switch ((error = mii_phy_auto(sc, waitfor))) {
	case EIO:
		/*
		 * Just assume we're not in full-duplex mode.
		 * XXX Check link and try AUI/BNC?
		 */
		PHY_WRITE(sc, MII_BMCR, 0);
		break;

	case EJUSTRETURN:
		/* Flag that we need to program when it completes. */
		tsc->sc_need_acomp = 1;
		break;

	default:
		tlphy_acomp(tsc);
	}

	return (error);
}

static void
tlphy_acomp(struct tlphy_softc *tsc)
{
	struct mii_softc *sc = &tsc->sc_mii;
	int aner, anlpar;

	tsc->sc_need_acomp = 0;

	/*
	 * Grr, braindead ThunderLAN PHY doesn't self-configure
	 * after autonegotiation.  We have to do it ourselves
	 * based on the link partner status.
	 */

	aner = PHY_READ(sc, MII_ANER);
	if (aner & ANER_LPAN) {
		anlpar = PHY_READ(sc, MII_ANLPAR) &
		    PHY_READ(sc, MII_ANAR);
		if (anlpar & ANAR_10_FD) {
			PHY_WRITE(sc, MII_BMCR, BMCR_FDX);
			return;
		}
	}
	PHY_WRITE(sc, MII_BMCR, 0);
}
