/*      $NetBSD: rdcphy.c,v 1.1 2011/01/26 18:48:12 bouyer Exp $        */

/*-
 * Copyright (c) 2010, Pyun YongHyeon <yongari@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* FreeBSD: src/sys/dev/mii/rdcphy.c,v 1.1 2010/12/30 23:50:25 yongari Exp */

/*
 * Driver for the RDC Semiconductor R6040 10/100 PHY.
 */
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rdcphy.c,v 1.1 2011/01/26 18:48:12 bouyer Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/socket.h>
#include <sys/errno.h>

#include <sys/bus.h>

#include <net/if.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/miidevs.h>

#include <dev/mii/rdcphyreg.h>

struct rdcphy_softc {
	struct mii_softc sc_mii;
	int		 sc_mii_link_tick;
#define	RDCPHY_MANNEG_TICK	3
};

static int	rdcphymatch(device_t, cfdata_t, void *);
static void	rdcphyattach(device_t, device_t, void *);

CFATTACH_DECL_NEW(rdcphy, sizeof(struct rdcphy_softc),
    rdcphymatch, rdcphyattach, mii_phy_detach, mii_phy_activate);


static int	rdcphy_service(struct mii_softc *, struct mii_data *, int);
static void	rdcphy_status(struct mii_softc *);

static const struct mii_phy_funcs rdcphy_funcs = {
	rdcphy_service, rdcphy_status, mii_phy_reset,
};

static const struct mii_phydesc rdcphys[] = {
	{ MII_OUI_RDC, MII_MODEL_RDC_R6040,
	  MII_STR_RDC_R6040 },
	{ 0,                    0,
	  NULL },
};

static int
rdcphymatch(device_t parent, cfdata_t match, void *aux)
{
	struct mii_attach_args *ma = aux;

	if (mii_phy_match(ma, rdcphys) != NULL)
		return (10);

	return (0);
}

static void
rdcphyattach(device_t parent, device_t self, void *aux)
{
	struct rdcphy_softc *rsc = device_private(self);
	struct mii_softc *sc = &rsc->sc_mii;
	struct mii_attach_args *ma = aux;
	struct mii_data *mii = ma->mii_data;

	const struct mii_phydesc *mpd;

	mpd = mii_phy_match(ma, rdcphys);
	aprint_naive(": Media interface\n");
	aprint_normal(": %s, rev. %d\n", mpd->mpd_name, MII_REV(ma->mii_id2));

	sc->mii_dev = self;
	sc->mii_inst = mii->mii_instance;
	sc->mii_phy = ma->mii_phyno;
	sc->mii_funcs = &rdcphy_funcs;
	sc->mii_pdata = mii;
	sc->mii_flags = ma->mii_flags;
	sc->mii_anegticks = MII_ANEGTICKS;

	PHY_RESET(sc);

	sc->mii_capabilities = PHY_READ(sc, MII_BMSR) & ma->mii_capmask;
	if (sc->mii_capabilities & BMSR_EXTSTAT)
		sc->mii_extcapabilities = PHY_READ(sc, MII_EXTSR);
	aprint_normal_dev(self, "");
	mii_phy_add_media(sc);
	aprint_normal("\n");

	if (!pmf_device_register(self, NULL, mii_phy_resume))
		aprint_error_dev(self, "couldn't establish power handler\n");

}

static int
rdcphy_service(struct mii_softc *sc, struct mii_data *mii, int cmd)
{
	struct rdcphy_softc *rsc = (struct rdcphy_softc *)sc;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;

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
		switch (IFM_SUBTYPE(ife->ifm_media)) {
		case IFM_100_TX:
		case IFM_10_T:
			/*
			 * Report fake lost link event to parent
			 * driver.  This will stop MAC of parent
			 * driver and make it possible to reconfigure
			 * MAC after completion of link establishment.
			 * Note, the parent MAC seems to require
			 * restarting MAC when underlying any PHY
			 * configuration was changed even if the
			 * resolved speed/duplex was not changed at
			 * all.
			 */
			mii->mii_media_status = 0;
			mii->mii_media_active = IFM_ETHER | IFM_NONE;
			rsc->sc_mii_link_tick = RDCPHY_MANNEG_TICK;
			/* Immediately report link down. */
			mii_phy_update(sc, MII_MEDIACHG);
			return (0);
		default:
			break;
		}
		break;

	case MII_TICK:
		if (mii_phy_tick(sc) == EJUSTRETURN)
			return (0);
		if (IFM_SUBTYPE(ife->ifm_media) != IFM_AUTO) {
			/*
			 * It seems the PHY hardware does not correctly
			 * report link status changes when manual link
			 * configuration is in progress.  It is also
			 * possible for the PHY to complete establishing
			 * a link within one second such that mii(4)
			 * did not notice the link change.  To workaround
			 * the issue, emulate lost link event and wait
			 * for 3 seconds when manual link configuration
			 * is in progress.  3 seconds would be long
			 * enough to absorb transient link flips.
			 */
			if (rsc->sc_mii_link_tick > 0) {
				rsc->sc_mii_link_tick--;
				return (0);
			}
		}
		break;
	}

	/* Update the media status. */
	rdcphy_status(sc);

	/* Callback if something changed. */
	mii_phy_update(sc, cmd);
	return (0);
}

static void
rdcphy_status(struct mii_softc *sc)
{
	struct mii_data *mii = sc->mii_pdata;
	int bmsr, bmcr, physts;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	bmsr = PHY_READ(sc, MII_BMSR) | PHY_READ(sc, MII_BMSR);
	physts = PHY_READ(sc, MII_RDCPHY_STATUS);

	if ((physts & STATUS_LINK_UP) != 0)
		mii->mii_media_status |= IFM_ACTIVE;

	bmcr = PHY_READ(sc, MII_BMCR);
	if ((bmcr & BMCR_ISO) != 0) {
		mii->mii_media_active |= IFM_NONE;
		mii->mii_media_status = 0;
		return;
	}

	if ((bmcr & BMCR_LOOP) != 0)
		mii->mii_media_active |= IFM_LOOP;

	if ((bmcr & BMCR_AUTOEN) != 0) {
		if ((bmsr & BMSR_ACOMP) == 0) {
			/* Erg, still trying, I guess... */
			mii->mii_media_active |= IFM_NONE;
			return;
		}
	}

	switch (physts & STATUS_SPEED_MASK) {
	case STATUS_SPEED_100:
		mii->mii_media_active |= IFM_100_TX;
		break;
	case STATUS_SPEED_10:
		mii->mii_media_active |= IFM_10_T;
		break;
	default:
		mii->mii_media_active |= IFM_NONE;
		return;
	}
	if ((physts & STATUS_FULL_DUPLEX) != 0)
		mii->mii_media_active |= IFM_FDX | mii_phy_flowstatus(sc);
	else
		mii->mii_media_active |= IFM_HDX;
}
