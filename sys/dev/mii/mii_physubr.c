/*	$NetBSD: mii_physubr.c,v 1.80 2013/06/20 13:56:29 roy Exp $	*/

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
 * Subroutines common to all PHYs.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mii_physubr.c,v 1.80 2013/06/20 13:56:29 roy Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/module.h>
#include <sys/proc.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/route.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

const char *(*mii_get_descr)(int, int) = mii_get_descr_stub;

int mii_verbose_loaded = 0;

const char *mii_get_descr_stub(int oui, int model)
{
	mii_load_verbose();
	if (mii_verbose_loaded)
		return mii_get_descr(oui, model);
	else
		return NULL;
}

/*    
 * Routine to load the miiverbose kernel module as needed
 */
void mii_load_verbose(void)
{
	if (mii_verbose_loaded == 0)
		module_autoload("miiverbose", MODULE_CLASS_MISC);
}  

static void mii_phy_statusmsg(struct mii_softc *);

/*
 * Media to register setting conversion table.  Order matters.
 */
static const struct mii_media mii_media_table[MII_NMEDIA] = {
	/* None */
	{ BMCR_ISO,		ANAR_CSMA,
	  0, },

	/* 10baseT */
	{ BMCR_S10,		ANAR_CSMA|ANAR_10,
	  0, },

	/* 10baseT-FDX */
	{ BMCR_S10|BMCR_FDX,	ANAR_CSMA|ANAR_10_FD,
	  0, },

	/* 100baseT4 */
	{ BMCR_S100,		ANAR_CSMA|ANAR_T4,
	  0, },

	/* 100baseTX */
	{ BMCR_S100,		ANAR_CSMA|ANAR_TX,
	  0, },

	/* 100baseTX-FDX */
	{ BMCR_S100|BMCR_FDX,	ANAR_CSMA|ANAR_TX_FD,
	  0, },

	/* 1000baseX */
	{ BMCR_S1000,		ANAR_CSMA,
	  0, },

	/* 1000baseX-FDX */
	{ BMCR_S1000|BMCR_FDX,	ANAR_CSMA,
	  0, },

	/* 1000baseT */
	{ BMCR_S1000,		ANAR_CSMA,
	  GTCR_ADV_1000THDX },

	/* 1000baseT-FDX */
	{ BMCR_S1000,		ANAR_CSMA,
	  GTCR_ADV_1000TFDX },
};

static void	mii_phy_auto_timeout(void *);

void
mii_phy_setmedia(struct mii_softc *sc)
{
	struct mii_data *mii = sc->mii_pdata;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int bmcr, anar, gtcr;

	if (IFM_SUBTYPE(ife->ifm_media) == IFM_AUTO) {
		/*
		 * Force renegotiation if MIIF_DOPAUSE.
		 *
		 * XXX This is only necessary because many NICs don't
		 * XXX advertise PAUSE capabilities at boot time.  Maybe
		 * XXX we should force this only once?
		 */
		if ((PHY_READ(sc, MII_BMCR) & BMCR_AUTOEN) == 0 ||
		    (sc->mii_flags & (MIIF_FORCEANEG|MIIF_DOPAUSE)))
			(void) mii_phy_auto(sc, 1);
		return;
	}

	/*
	 * Table index is stored in the media entry.
	 */

#ifdef DIAGNOSTIC
	if (/* ife->ifm_data < 0 || */ ife->ifm_data >= MII_NMEDIA)
		panic("mii_phy_setmedia");
#endif

	anar = mii_media_table[ife->ifm_data].mm_anar;
	bmcr = mii_media_table[ife->ifm_data].mm_bmcr;
	gtcr = mii_media_table[ife->ifm_data].mm_gtcr;

	if (mii->mii_media.ifm_media & IFM_ETH_MASTER) {
		switch (IFM_SUBTYPE(ife->ifm_media)) {
		case IFM_1000_T:
			gtcr |= GTCR_MAN_MS|GTCR_ADV_MS;
			break;

		default:
			panic("mii_phy_setmedia: MASTER on wrong media");
		}
	}

	if (mii->mii_media.ifm_media & IFM_FLOW) {
		if (sc->mii_flags & MIIF_IS_1000X)
			anar |= ANAR_X_PAUSE_SYM | ANAR_X_PAUSE_ASYM;
		else {
			anar |= ANAR_FC;
			/* XXX Only 1000BASE-T has PAUSE_ASYM? */
			if ((sc->mii_flags & MIIF_HAVE_GTCR) &&
			    (sc->mii_extcapabilities &
			     (EXTSR_1000THDX | EXTSR_1000TFDX)))
				anar |= ANAR_PAUSE_ASYM;
		}
	}

	if (ife->ifm_media & IFM_LOOP)
		bmcr |= BMCR_LOOP;

	PHY_WRITE(sc, MII_ANAR, anar);
	if (sc->mii_flags & MIIF_HAVE_GTCR)
		PHY_WRITE(sc, MII_100T2CR, gtcr);
	if (IFM_SUBTYPE(ife->ifm_media) == IFM_1000_T) {
		mii_phy_auto(sc, 0);
	} else {
		PHY_WRITE(sc, MII_BMCR, bmcr);
	}
}

int
mii_phy_auto(struct mii_softc *sc, int waitfor)
{
	int i;
	struct mii_data *mii = sc->mii_pdata;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;

	sc->mii_ticks = 0;
	if ((sc->mii_flags & MIIF_DOINGAUTO) == 0) {
		/*
		 * Check for 1000BASE-X.  Autonegotiation is a bit
		 * different on such devices.
		 */
		if (sc->mii_flags & MIIF_IS_1000X) {
			uint16_t anar = 0;

			if (sc->mii_extcapabilities & EXTSR_1000XFDX)
				anar |= ANAR_X_FD;
			if (sc->mii_extcapabilities & EXTSR_1000XHDX)
				anar |= ANAR_X_HD;

			if (sc->mii_flags & MIIF_DOPAUSE) {
				/* XXX Asymmetric vs. symmetric? */
				anar |= ANLPAR_X_PAUSE_TOWARDS;
			}

			PHY_WRITE(sc, MII_ANAR, anar);
		} else {
			uint16_t anar;

			anar = BMSR_MEDIA_TO_ANAR(sc->mii_capabilities) |
			    ANAR_CSMA;
			if (sc->mii_flags & MIIF_DOPAUSE) {
				anar |= ANAR_FC;
				/* XXX Only 1000BASE-T has PAUSE_ASYM? */
				if ((sc->mii_flags & MIIF_HAVE_GTCR) &&
				    (sc->mii_extcapabilities &
				     (EXTSR_1000THDX | EXTSR_1000TFDX)))
					anar |= ANAR_PAUSE_ASYM;
			}

			/*
			 *for 1000-base-T, autonegotiation mus be enabled, but 
			 *if we're not set to auto, only advertise
			 *1000-base-T with the link partner.
			 */
			if (IFM_SUBTYPE(ife->ifm_media) == IFM_1000_T) {
				anar &= ~(ANAR_T4|ANAR_TX_FD|ANAR_TX|ANAR_10_FD|ANAR_10);
			}
				
			PHY_WRITE(sc, MII_ANAR, anar);
			if (sc->mii_flags & MIIF_HAVE_GTCR) {
				uint16_t gtcr = 0;

				if (sc->mii_extcapabilities & EXTSR_1000TFDX)
					gtcr |= GTCR_ADV_1000TFDX;
				if (sc->mii_extcapabilities & EXTSR_1000THDX)
					gtcr |= GTCR_ADV_1000THDX;

				PHY_WRITE(sc, MII_100T2CR, gtcr);
			}
		}
		PHY_WRITE(sc, MII_BMCR, BMCR_AUTOEN | BMCR_STARTNEG);
	}

	if (waitfor) {
		/* Wait 500ms for it to complete. */
		for (i = 0; i < 500; i++) {
			if (PHY_READ(sc, MII_BMSR) & BMSR_ACOMP)
				return (0);
			delay(1000);
		}

		/*
		 * Don't need to worry about clearing MIIF_DOINGAUTO.
		 * If that's set, a timeout is pending, and it will
		 * clear the flag.
		 */
		return (EIO);
	}

	/*
	 * Just let it finish asynchronously.  This is for the benefit of
	 * the tick handler driving autonegotiation.  Don't want 500ms
	 * delays all the time while the system is running!
	 */
	if (sc->mii_flags & MIIF_AUTOTSLEEP) {
		sc->mii_flags |= MIIF_DOINGAUTO;
		tsleep(&sc->mii_flags, PZERO, "miiaut", hz >> 1);
		mii_phy_auto_timeout(sc);
	} else if ((sc->mii_flags & MIIF_DOINGAUTO) == 0) {
		sc->mii_flags |= MIIF_DOINGAUTO;
		callout_reset(&sc->mii_nway_ch, hz >> 1,
		    mii_phy_auto_timeout, sc);
	}
	return (EJUSTRETURN);
}

static void
mii_phy_auto_timeout(void *arg)
{
	struct mii_softc *sc = arg;
	int s;

	if (!device_is_active(sc->mii_dev))
		return;

	s = splnet();
	sc->mii_flags &= ~MIIF_DOINGAUTO;

	/* Update the media status. */
	(void) PHY_SERVICE(sc, sc->mii_pdata, MII_POLLSTAT);
	splx(s);
}

int
mii_phy_tick(struct mii_softc *sc)
{
	struct mii_data *mii = sc->mii_pdata;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int reg;

	/* Just bail now if the interface is down. */
	if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
		return (EJUSTRETURN);

	/*
	 * If we're not doing autonegotiation, we don't need to do
	 * any extra work here.  However, we need to check the link
	 * status so we can generate an announcement by returning
	 * with 0 if the status changes.
	 */
	if ((IFM_SUBTYPE(ife->ifm_media) != IFM_AUTO) &&
	    (IFM_SUBTYPE(ife->ifm_media) != IFM_1000_T)) {
		/*
		 * Reset autonegotiation timer to 0 just to make sure
		 * the future autonegotiation start with 0.
		 */
		sc->mii_ticks = 0;
		return (0);
	}

	/* Read the status register twice; BMSR_LINK is latch-low. */
	reg = PHY_READ(sc, MII_BMSR) | PHY_READ(sc, MII_BMSR);
	if (reg & BMSR_LINK) {
		/*
		 * Reset autonegotiation timer to 0 in case the link
		 * goes down in the next tick.
		 */
		sc->mii_ticks = 0;
		/* See above. */
		return (0);
	}

	/*
	 * mii_ticks == 0 means it's the first tick after changing the media or
	 * the link became down since the last tick (see above), so return with
	 * 0 to update the status.
	 */
	if (sc->mii_ticks++ == 0)
		return (0);

	/*
	 * Only retry autonegotiation every N seconds.
	 */
	KASSERT(sc->mii_anegticks != 0);
	if (sc->mii_ticks <= sc->mii_anegticks)
		return (EJUSTRETURN);

	PHY_RESET(sc);

	if (mii_phy_auto(sc, 0) == EJUSTRETURN)
		return (EJUSTRETURN);

	/*
	 * Might need to generate a status message if autonegotiation
	 * failed.
	 */
	return (0);
}

void
mii_phy_reset(struct mii_softc *sc)
{
	int reg, i;

	if (sc->mii_flags & MIIF_NOISOLATE)
		reg = BMCR_RESET;
	else
		reg = BMCR_RESET | BMCR_ISO;
	PHY_WRITE(sc, MII_BMCR, reg);

	/* Wait another 100ms for it to complete. */
	for (i = 0; i < 100; i++) {
		reg = PHY_READ(sc, MII_BMCR);
		if ((reg & BMCR_RESET) == 0)
			break;
		delay(1000);
	}

	if (sc->mii_inst != 0 && ((sc->mii_flags & MIIF_NOISOLATE) == 0))
		PHY_WRITE(sc, MII_BMCR, reg | BMCR_ISO);
}

void
mii_phy_down(struct mii_softc *sc)
{

	if (sc->mii_flags & MIIF_DOINGAUTO) {
		sc->mii_flags &= ~MIIF_DOINGAUTO;
		callout_stop(&sc->mii_nway_ch);
	}
}

void
mii_phy_status(struct mii_softc *sc)
{

	PHY_STATUS(sc);
}

void
mii_phy_update(struct mii_softc *sc, int cmd)
{
	struct mii_data *mii = sc->mii_pdata;

	if (sc->mii_media_active != mii->mii_media_active ||
	    sc->mii_media_status != mii->mii_media_status ||
	    cmd == MII_MEDIACHG) {
		mii_phy_statusmsg(sc);
		(*mii->mii_statchg)(mii->mii_ifp);
		sc->mii_media_active = mii->mii_media_active;
		sc->mii_media_status = mii->mii_media_status;
	}
}

static void
mii_phy_statusmsg(struct mii_softc *sc)
{
	struct mii_data *mii = sc->mii_pdata;
	struct ifnet *ifp = mii->mii_ifp;

	if (mii->mii_media_status & IFM_AVALID) {
		if (mii->mii_media_status & IFM_ACTIVE)
			if_link_state_change(ifp, LINK_STATE_UP);
		else
			if_link_state_change(ifp, LINK_STATE_DOWN);
	} else
		if_link_state_change(ifp, LINK_STATE_UNKNOWN);

	ifp->if_baudrate = ifmedia_baudrate(mii->mii_media_active);
}

/*
 * Initialize generic PHY media based on BMSR, called when a PHY is
 * attached.  We expect to be set up to print a comma-separated list
 * of media names.  Does not print a newline.
 */
void
mii_phy_add_media(struct mii_softc *sc)
{
	struct mii_data *mii = sc->mii_pdata;
	device_t self = sc->mii_dev;
	const char *sep = "";
	int fdx = 0;

#define	ADD(m, c)	ifmedia_add(&mii->mii_media, (m), (c), NULL)
#define	PRINT(n)	aprint_normal("%s%s", sep, (n)); sep = ", "

	if ((sc->mii_flags & MIIF_NOISOLATE) == 0)
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_NONE, 0, sc->mii_inst),
		    MII_MEDIA_NONE);

	/*
	 * There are different interpretations for the bits in
	 * HomePNA PHYs.  And there is really only one media type
	 * that is supported.
	 */
	if (sc->mii_flags & MIIF_IS_HPNA) {
		if (sc->mii_capabilities & BMSR_10THDX) {
			ADD(IFM_MAKEWORD(IFM_ETHER, IFM_HPNA_1, 0,
					 sc->mii_inst),
			    MII_MEDIA_10_T);
			PRINT("HomePNA1");
		}
		goto out;
	}

	if (sc->mii_capabilities & BMSR_10THDX) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_10_T, 0, sc->mii_inst),
		    MII_MEDIA_10_T);
		PRINT("10baseT");
	}
	if (sc->mii_capabilities & BMSR_10TFDX) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_10_T, IFM_FDX, sc->mii_inst),
		    MII_MEDIA_10_T_FDX);
		PRINT("10baseT-FDX");
		fdx = 1;
	}
	if (sc->mii_capabilities & BMSR_100TXHDX) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, 0, sc->mii_inst),
		    MII_MEDIA_100_TX);
		PRINT("100baseTX");
	}
	if (sc->mii_capabilities & BMSR_100TXFDX) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, IFM_FDX, sc->mii_inst),
		    MII_MEDIA_100_TX_FDX);
		PRINT("100baseTX-FDX");
		fdx = 1;
	}
	if (sc->mii_capabilities & BMSR_100T4) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_T4, 0, sc->mii_inst),
		    MII_MEDIA_100_T4);
		PRINT("100baseT4");
	}

	if (sc->mii_extcapabilities & EXTSR_MEDIAMASK) {
		/*
		 * XXX Right now only handle 1000SX and 1000TX.  Need
		 * XXX to handle 1000LX and 1000CX some how.
		 *
		 * Note since it can take 5 seconds to auto-negotiate
		 * a gigabit link, we make anegticks 10 seconds for
		 * all the gigabit media types.
		 */
		if (sc->mii_extcapabilities & EXTSR_1000XHDX) {
			sc->mii_anegticks = MII_ANEGTICKS_GIGE;
			sc->mii_flags |= MIIF_IS_1000X;
			ADD(IFM_MAKEWORD(IFM_ETHER, IFM_1000_SX, 0,
			    sc->mii_inst), MII_MEDIA_1000_X);
			PRINT("1000baseSX");
		}
		if (sc->mii_extcapabilities & EXTSR_1000XFDX) {
			sc->mii_anegticks = MII_ANEGTICKS_GIGE;
			sc->mii_flags |= MIIF_IS_1000X;
			ADD(IFM_MAKEWORD(IFM_ETHER, IFM_1000_SX, IFM_FDX,
			    sc->mii_inst), MII_MEDIA_1000_X_FDX);
			PRINT("1000baseSX-FDX");
			fdx = 1;
		}

		/*
		 * 1000baseT media needs to be able to manipulate
		 * master/slave mode.  We set IFM_ETH_MASTER in
		 * the "don't care mask" and filter it out when
		 * the media is set.
		 *
		 * All 1000baseT PHYs have a 1000baseT control register.
		 */
		if (sc->mii_extcapabilities & EXTSR_1000THDX) {
			sc->mii_anegticks = MII_ANEGTICKS_GIGE;
			sc->mii_flags |= MIIF_HAVE_GTCR;
			mii->mii_media.ifm_mask |= IFM_ETH_MASTER;
			ADD(IFM_MAKEWORD(IFM_ETHER, IFM_1000_T, 0,
			    sc->mii_inst), MII_MEDIA_1000_T);
			PRINT("1000baseT");
		}
		if (sc->mii_extcapabilities & EXTSR_1000TFDX) {
			sc->mii_anegticks = MII_ANEGTICKS_GIGE;
			sc->mii_flags |= MIIF_HAVE_GTCR;
			mii->mii_media.ifm_mask |= IFM_ETH_MASTER;
			ADD(IFM_MAKEWORD(IFM_ETHER, IFM_1000_T, IFM_FDX,
			    sc->mii_inst), MII_MEDIA_1000_T_FDX);
			PRINT("1000baseT-FDX");
			fdx = 1;
		}
	}

	if (sc->mii_capabilities & BMSR_ANEG) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_AUTO, 0, sc->mii_inst),
		    MII_NMEDIA);	/* intentionally invalid index */
		PRINT("auto");
	}
#undef ADD
#undef PRINT
	if (fdx != 0 && (sc->mii_flags & MIIF_DOPAUSE))
		mii->mii_media.ifm_mask |= IFM_ETH_FMASK;
out:
	if (!pmf_device_register(self, NULL, mii_phy_resume)) {
		aprint_normal("\n");
		aprint_error_dev(self, "couldn't establish power handler");
	}
}

void
mii_phy_delete_media(struct mii_softc *sc)
{
	struct mii_data *mii = sc->mii_pdata;

	ifmedia_delete_instance(&mii->mii_media, sc->mii_inst);
}

int
mii_phy_activate(device_t self, enum devact act)
{
	switch (act) {
	case DVACT_DEACTIVATE:
		/* XXX Invalidate parent's media setting? */
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

/* ARGSUSED1 */
int
mii_phy_detach(device_t self, int flags)
{
	struct mii_softc *sc = device_private(self);

	/* XXX Invalidate parent's media setting? */

	if (sc->mii_flags & MIIF_DOINGAUTO)
		callout_halt(&sc->mii_nway_ch, NULL);

	callout_destroy(&sc->mii_nway_ch);

	mii_phy_delete_media(sc);
	LIST_REMOVE(sc, mii_list);

	return (0);
}

const struct mii_phydesc *
mii_phy_match(const struct mii_attach_args *ma, const struct mii_phydesc *mpd)
{

	for (; mpd->mpd_name != NULL; mpd++) {
		if (MII_OUI(ma->mii_id1, ma->mii_id2) == mpd->mpd_oui &&
		    MII_MODEL(ma->mii_id2) == mpd->mpd_model)
			return (mpd);
	}
	return (NULL);
}

/*
 * Return the flow control status flag from MII_ANAR & MII_ANLPAR.
 */
u_int
mii_phy_flowstatus(struct mii_softc *sc)
{
	u_int anar, anlpar;

	if ((sc->mii_flags & MIIF_DOPAUSE) == 0)
		return (0);

	anar = PHY_READ(sc, MII_ANAR);
	anlpar = PHY_READ(sc, MII_ANLPAR);

	/* For 1000baseX, the bits are in a different location. */
	if (sc->mii_flags & MIIF_IS_1000X) {
		anar <<= 3;
		anlpar <<= 3;
	}

	if ((anar & ANAR_PAUSE_SYM) & (anlpar & ANLPAR_PAUSE_SYM))
		return (IFM_FLOW|IFM_ETH_TXPAUSE|IFM_ETH_RXPAUSE);

	if ((anar & ANAR_PAUSE_SYM) == 0) {
		if ((anar & ANAR_PAUSE_ASYM) &&
		    ((anlpar & ANLPAR_PAUSE_TOWARDS) == ANLPAR_PAUSE_TOWARDS))
			return (IFM_FLOW|IFM_ETH_TXPAUSE);
		else
			return (0);
	}

	if ((anar & ANAR_PAUSE_ASYM) == 0) {
		if (anlpar & ANLPAR_PAUSE_SYM)
			return (IFM_FLOW|IFM_ETH_TXPAUSE|IFM_ETH_RXPAUSE);
		else
			return (0);
	}

	switch ((anlpar & ANLPAR_PAUSE_TOWARDS)) {
	case ANLPAR_PAUSE_NONE:
		return (0);

	case ANLPAR_PAUSE_ASYM:
		return (IFM_FLOW|IFM_ETH_RXPAUSE);

	default:
		return (IFM_FLOW|IFM_ETH_RXPAUSE|IFM_ETH_TXPAUSE);
	}
	/* NOTREACHED */
}

bool
mii_phy_resume(device_t dv, const pmf_qual_t *qual)
{
	struct mii_softc *sc = device_private(dv);

	PHY_RESET(sc);
	return PHY_SERVICE(sc, sc->mii_pdata, MII_MEDIACHG) == 0;
}


/*
 * Given an ifmedia word, return the corresponding ANAR value.
 */
int
mii_anar(int media)
{
	int rv;

#ifdef DIAGNOSTIC
	if (/* media < 0 || */ media >= MII_NMEDIA)
		panic("mii_anar");
#endif

	rv = mii_media_table[media].mm_anar;

	return rv;
}
