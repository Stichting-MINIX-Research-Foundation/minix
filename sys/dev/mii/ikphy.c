/*	$NetBSD: ikphy.c,v 1.10 2014/06/16 16:48:16 msaitoh Exp $	*/

/*******************************************************************************
Copyright (c) 2001-2005, Intel Corporation 
All rights reserved.

Redistribution and use in source and binary forms, with or without 
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, 
    this list of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright 
    notice, this list of conditions and the following disclaimer in the 
    documentation and/or other materials provided with the distribution.

 3. Neither the name of the Intel Corporation nor the names of its 
    contributors may be used to endorse or promote products derived from 
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/
/*
 * Copyright (c) 2006 Manuel Bouyer.  All rights reserved.
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
 * driver for Intel's i82563 ethernet 10/100/1000 PHY
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ikphy.c,v 1.10 2014/06/16 16:48:16 msaitoh Exp $");

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

#include <dev/mii/ikphyreg.h>

static int	ikphymatch(device_t, cfdata_t, void *);
static void	ikphyattach(device_t, device_t, void *);

CFATTACH_DECL_NEW(ikphy, sizeof(struct mii_softc),
    ikphymatch, ikphyattach, mii_phy_detach, mii_phy_activate);

static int	ikphy_service(struct mii_softc *, struct mii_data *, int);
static void	ikphy_status(struct mii_softc *);
static void	ikphy_setmedia(struct mii_softc *);

static const struct mii_phy_funcs ikphy_funcs = {
	ikphy_service, ikphy_status, mii_phy_reset,
};

static const struct mii_phydesc ikphys[] = {
	{ MII_OUI_xxMARVELL,		MII_MODEL_xxMARVELL_I82563,
	  MII_STR_xxMARVELL_I82563 },

	{ 0,				0,
	  NULL },
};

static int
ikphymatch(device_t parent, cfdata_t match, void *aux)
{
	struct mii_attach_args *ma = aux;

	if (mii_phy_match(ma, ikphys) != NULL)
		return (10);

	return (0);
}

static void
ikphyattach(device_t parent, device_t self, void *aux)
{
	struct mii_softc *sc = device_private(self);
	struct mii_attach_args *ma = aux;
	struct mii_data *mii = ma->mii_data;
	const struct mii_phydesc *mpd;

	mpd = mii_phy_match(ma, ikphys);
	aprint_naive(": Media interface\n");
	aprint_normal(": %s, rev. %d\n", mpd->mpd_name, MII_REV(ma->mii_id2));

	sc->mii_dev = self;
	sc->mii_inst = mii->mii_instance;
	sc->mii_phy = ma->mii_phyno;
	sc->mii_funcs = &ikphy_funcs;
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
}

static int
ikphy_service(struct mii_softc *sc, struct mii_data *mii, int cmd)
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

		ikphy_setmedia(sc);
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
ikphy_setmedia(struct mii_softc *sc)
{
	uint16_t phy_data;
	struct mii_data *mii = sc->mii_pdata;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;

	/* Enable CRS on TX for half-duplex operation. */
	phy_data = PHY_READ(sc, GG82563_PHY_MAC_SPEC_CTRL);
	phy_data |= GG82563_MSCR_ASSERT_CRS_ON_TX;
	/* Use 25MHz for both link down and 1000BASE-T for Tx clock */
	phy_data |= GG82563_MSCR_TX_CLK_1000MBPS_25MHZ;
	PHY_WRITE(sc, GG82563_PHY_MAC_SPEC_CTRL, phy_data);

	/* set mdi/mid-x options */
	phy_data = PHY_READ(sc, GG82563_PHY_SPEC_CTRL);
	phy_data &= ~GG82563_PSCR_CROSSOVER_MODE_MASK;
	if (IFM_SUBTYPE(ife->ifm_media) == IFM_AUTO)
		phy_data |= GG82563_PSCR_CROSSOVER_MODE_AUTO;
	else
		phy_data |= GG82563_PSCR_CROSSOVER_MODE_MDI;
	/* set polarity correction */
	phy_data &= ~GG82563_PSCR_POLARITY_REVERSAL_DISABLE;
	PHY_WRITE(sc, GG82563_PHY_SPEC_CTRL, phy_data);

	/* SW Reset the PHY so all changes take effect */
	PHY_RESET(sc);

	/* for the i80003 */
	phy_data = PHY_READ(sc, GG82563_PHY_SPEC_CTRL_2);
	phy_data &= ~GG82563_PSCR2_REVERSE_AUTO_NEG;
	PHY_WRITE(sc, GG82563_PHY_SPEC_CTRL_2, phy_data);

	/* Enable Electrical Idle on the PHY */
	phy_data = PHY_READ(sc, GG82563_PHY_PWR_MGMT_CTRL);
	phy_data |= GG82563_PMCR_ENABLE_ELECTRICAL_IDLE;
	PHY_WRITE(sc, GG82563_PHY_PWR_MGMT_CTRL, phy_data);

	phy_data = PHY_READ(sc, GG82563_PHY_KMRN_MODE_CTRL);
	phy_data &= ~GG82563_KMCR_PASS_FALSE_CARRIER;
	PHY_WRITE(sc, GG82563_PHY_KMRN_MODE_CTRL, phy_data);

	/*
	 * Workaround: Disable padding in Kumeran interface in the MAC
	 * and in the PHY to avoid CRC errors.
	 */
	phy_data = PHY_READ(sc, GG82563_PHY_INBAND_CTRL);
	phy_data |= GG82563_ICR_DIS_PADDING;
	PHY_WRITE(sc, GG82563_PHY_INBAND_CTRL, phy_data);

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
	phy_data = PHY_READ(sc, GG82563_PHY_MAC_SPEC_CTRL);
	phy_data &= ~GG82563_MSCR_TX_CLK_MASK;
	switch(IFM_SUBTYPE(ife->ifm_media)) {
	case IFM_10_T:
		phy_data |= GG82563_MSCR_TX_CLK_10MBPS_2_5MHZ;
		break;
	case IFM_100_TX:
		phy_data |= GG82563_MSCR_TX_CLK_100MBPS_25MHZ;
		break;
	case IFM_1000_T:
		phy_data |= GG82563_MSCR_TX_CLK_1000MBPS_25MHZ;
		break;
	}
	phy_data |= GG82563_MSCR_ASSERT_CRS_ON_TX;
	PHY_WRITE(sc, GG82563_PHY_MAC_SPEC_CTRL, phy_data);
}

static void
ikphy_status(struct mii_softc *sc)
{
	struct mii_data *mii = sc->mii_pdata;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int pssr, bmcr, gtsr, kmrn;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	pssr = PHY_READ(sc, GG82563_PHY_SPEC_STATUS);

	if (pssr & GG82563_PSSR_LINK)
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
		if ((pssr & GG82563_PSSR_SPEED_DUPLEX_RESOLVED) == 0) {
			/* Erg, still trying, I guess... */
			mii->mii_media_active |= IFM_NONE;
			return;
		}

		switch (pssr & GG82563_PSSR_SPEED_MASK) {
		case GG82563_PSSR_SPEED_1000MBPS:
			mii->mii_media_active |= IFM_1000_T;
			gtsr = PHY_READ(sc, MII_100T2SR);
			if (gtsr & GTSR_MS_RES)
				mii->mii_media_active |= IFM_ETH_MASTER;
			break;

		case GG82563_PSSR_SPEED_100MBPS:
			mii->mii_media_active |= IFM_100_TX;
			break;

		case GG82563_PSSR_SPEED_10MBPS:
			mii->mii_media_active |= IFM_10_T;
			break;

		default:
			mii->mii_media_active |= IFM_NONE;
			mii->mii_media_status = 0;
			return;
		}

		if (pssr & GG82563_PSSR_DUPLEX)
			mii->mii_media_active |=
			    IFM_FDX | mii_phy_flowstatus(sc);
		else
			mii->mii_media_active |= IFM_HDX;
	} else
		mii->mii_media_active = ife->ifm_media;
	kmrn = PHY_READ(sc, GG82563_PHY_KMRN_MODE_CTRL);
	if (mii->mii_media_active & IFM_FDX)
		kmrn &= ~GG82563_KMCR_PASS_FALSE_CARRIER;
	else
		kmrn |= GG82563_KMCR_PASS_FALSE_CARRIER;
	PHY_WRITE(sc, GG82563_PHY_KMRN_MODE_CTRL, kmrn);
}
