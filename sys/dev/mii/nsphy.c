/*	$NetBSD: nsphy.c,v 1.59 2014/06/16 16:48:16 msaitoh Exp $	*/

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
 * driver for National Semiconductor's DP83840A ethernet 10/100 PHY
 * Data Sheet available from www.national.com
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: nsphy.c,v 1.59 2014/06/16 16:48:16 msaitoh Exp $");

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

#include <dev/mii/nsphyreg.h>

static int	nsphymatch(device_t, cfdata_t, void *);
static void	nsphyattach(device_t, device_t, void *);

CFATTACH_DECL_NEW(nsphy, sizeof(struct mii_softc),
    nsphymatch, nsphyattach, mii_phy_detach, mii_phy_activate);

static int	nsphy_service(struct mii_softc *, struct mii_data *, int);
static void	nsphy_status(struct mii_softc *);
static void	nsphy_reset(struct mii_softc *sc);

static const struct mii_phy_funcs nsphy_funcs = {
	nsphy_service, nsphy_status, nsphy_reset,
};

static const struct mii_phydesc nsphys[] = {
	{ MII_OUI_xxNATSEMI,		MII_MODEL_xxNATSEMI_DP83840,
	  MII_STR_xxNATSEMI_DP83840 },

	{ 0,				0,
	  NULL },
};

static int
nsphymatch(device_t parent, cfdata_t match, void *aux)
{
	struct mii_attach_args *ma = aux;

	if (mii_phy_match(ma, nsphys) != NULL)
		return (10);

	return (0);
}

static void
nsphyattach(device_t parent, device_t self, void *aux)
{
	struct mii_softc *sc = device_private(self);
	struct mii_attach_args *ma = aux;
	struct mii_data *mii = ma->mii_data;
	const struct mii_phydesc *mpd;

	mpd = mii_phy_match(ma, nsphys);
	aprint_naive(": Media interface\n");
	aprint_normal(": %s, rev. %d\n", mpd->mpd_name, MII_REV(ma->mii_id2));

	sc->mii_dev = self;
	sc->mii_inst = mii->mii_instance;
	sc->mii_phy = ma->mii_phyno;
	sc->mii_funcs = &nsphy_funcs;
	sc->mii_pdata = mii;
	sc->mii_flags = ma->mii_flags;
	sc->mii_anegticks = MII_ANEGTICKS;

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
nsphy_service(struct mii_softc *sc, struct mii_data *mii, int cmd)
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

		reg = PHY_READ(sc, MII_NSPHY_PCR);

		/*
		 * Set up the PCR to use LED4 to indicate full-duplex
		 * in both 10baseT and 100baseTX modes.
		 */
		reg |= PCR_LED4MODE;

		/*
		 * Make sure Carrier Integrity Monitor function is
		 * disabled (normal for Node operation, but sometimes
		 * it's not set?!)
		 */
		reg |= PCR_CIMDIS;

		/*
		 * Make sure "force link good" is set to normal mode.
		 * It's only intended for debugging.
		 */
		reg |= PCR_FLINK100;

		/*
		 * Mystery bits which are supposedly `reserved',
		 * but we seem to need to set them when the PHY
		 * is connected to some interfaces:
		 *
		 * 0x0400 is needed for fxp
		 *        (Intel EtherExpress Pro 10+/100B, 82557 chip)
		 *        (nsphy with a DP83840 chip)
		 * 0x0100 may be needed for some other card
		 */
		reg |= 0x0100 | 0x0400;

		PHY_WRITE(sc, MII_NSPHY_PCR, reg);

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
nsphy_status(struct mii_softc *sc)
{
	struct mii_data *mii = sc->mii_pdata;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int bmsr, bmcr, par, anlpar;

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
		/*
		 * The PAR status bits are only valid if autonegotiation
		 * has completed (or it's disabled).
		 */
		if ((bmsr & BMSR_ACOMP) == 0) {
			/* Erg, still trying, I guess... */
			mii->mii_media_active |= IFM_NONE;
			return;
		}

		/*
		 * Argh.  The PAR doesn't seem to indicate duplex mode
		 * properly!  Determine media based on link partner's
		 * advertised capabilities.
		 */
		if (PHY_READ(sc, MII_ANER) & ANER_LPAN) {
			anlpar = PHY_READ(sc, MII_ANAR) &
			    PHY_READ(sc, MII_ANLPAR);
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
		 * Link partner is not capable of autonegotiation.
		 * We will never be in full-duplex mode if this is
		 * the case, so reading the PAR is OK.
		 */
		par = PHY_READ(sc, MII_NSPHY_PAR);
		if (par & PAR_10)
			mii->mii_media_active |= IFM_10_T;
		else
			mii->mii_media_active |= IFM_100_TX;
		mii->mii_media_active |= IFM_HDX;
	} else
		mii->mii_media_active = ife->ifm_media;
}

static void
nsphy_reset(struct mii_softc *sc)
{
	int reg, i;

	if (sc->mii_flags & MIIF_NOISOLATE)
		reg = BMCR_RESET;
	else
		reg = BMCR_RESET | BMCR_ISO;
	PHY_WRITE(sc, MII_BMCR, reg);

	/*
	 * Give it a little time to settle in case we just got power.
	 * The DP83840A data sheet suggests that a soft reset not happen
	 * within 500us of power being applied.  Be conservative.
	 */
	delay(1000);

	/*
	 * Wait another 2s for it to complete.
	 * This is only a little overkill as under normal circumstances
	 * the PHY can take up to 1s to complete reset.
	 * This is also a bit odd because after a reset, the BMCR will
	 * clear the reset bit and simply reports 0 even though the reset
	 * is not yet complete.
	 */
	for (i = 0; i < 1000; i++) {
		reg = PHY_READ(sc, MII_BMCR);
		if (reg && ((reg & BMCR_RESET) == 0))
			break;
		delay(2000);
	}

	if (sc->mii_inst != 0 && ((sc->mii_flags & MIIF_NOISOLATE) == 0)) {
		PHY_WRITE(sc, MII_BMCR, reg | BMCR_ISO);
	}
}
