/*	$NetBSD: mii.c,v 1.51 2015/06/11 05:22:55 matt Exp $	*/

/*-
 * Copyright (c) 1998, 2000 The NetBSD Foundation, Inc.
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
 * MII bus layer, glues MII-capable network interface drivers to sharable
 * PHY drivers.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mii.c,v 1.51 2015/06/11 05:22:55 matt Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include "locators.h"

static int	mii_print(void *, const char *);

/*
 * Helper function used by network interface drivers, attaches PHYs
 * to the network interface driver parent.
 */
void
mii_attach(device_t parent, struct mii_data *mii, int capmask,
    int phyloc, int offloc, int flags)
{
	struct mii_attach_args ma;
	struct mii_softc *child;
	int bmsr, offset = 0;
	int phymin, phymax;
	int locs[MIICF_NLOCS];

	if (phyloc != MII_PHY_ANY && offloc != MII_OFFSET_ANY)
		panic("mii_attach: phyloc and offloc specified");

	if (phyloc == MII_PHY_ANY) {
		phymin = 0;
		phymax = MII_NPHY - 1;
	} else
		phymin = phymax = phyloc;

	if ((mii->mii_flags & MIIF_INITDONE) == 0) {
		LIST_INIT(&mii->mii_phys);
		mii->mii_flags |= MIIF_INITDONE;
	}

	for (ma.mii_phyno = phymin; ma.mii_phyno <= phymax; ma.mii_phyno++) {
		/*
		 * Make sure we haven't already configured a PHY at this
		 * address.  This allows mii_attach() to be called
		 * multiple times.
		 */
		LIST_FOREACH(child, &mii->mii_phys, mii_list) {
			if (child->mii_phy == ma.mii_phyno) {
				/*
				 * Yes, there is already something
				 * configured at this address.
				 */
				offset++;
				continue;
			}
		}

		/*
		 * Check to see if there is a PHY at this address.  Note,
		 * many braindead PHYs report 0/0 in their ID registers,
		 * so we test for media in the BMSR.
		 */
		bmsr = (*mii->mii_readreg)(parent, ma.mii_phyno, MII_BMSR);
		if (bmsr == 0 || bmsr == 0xffff ||
		    (bmsr & (BMSR_EXTSTAT|BMSR_MEDIAMASK)) == 0) {
			/* Assume no PHY at this address. */
			continue;
		}

		/*
		 * There is a PHY at this address.  If we were given an
		 * `offset' locator, skip this PHY if it doesn't match.
		 */
		if (offloc != MII_OFFSET_ANY && offloc != offset) {
			offset++;
			continue;
		}

		/*
		 * Extract the IDs.  Braindead PHYs will be handled by
		 * the `ukphy' driver, as we have no ID information to
		 * match on.
		 */
		ma.mii_id1 = (*mii->mii_readreg)(parent, ma.mii_phyno,
		    MII_PHYIDR1);
		ma.mii_id2 = (*mii->mii_readreg)(parent, ma.mii_phyno,
		    MII_PHYIDR2);

		ma.mii_data = mii;
		ma.mii_capmask = capmask;
		ma.mii_flags = flags | (mii->mii_flags & MIIF_INHERIT_MASK);

		locs[MIICF_PHY] = ma.mii_phyno;

		child = device_private(config_found_sm_loc(parent, "mii",
			locs, &ma, mii_print, config_stdsubmatch));
		if (child) {
			/*
			 * Link it up in the parent's MII data.
			 */
			callout_init(&child->mii_nway_ch, 0);
			LIST_INSERT_HEAD(&mii->mii_phys, child, mii_list);
			child->mii_offset = offset;
			mii->mii_instance++;
		}
		offset++;
	}
}

void
mii_detach(struct mii_data *mii, int phyloc, int offloc)
{
	struct mii_softc *child, *nchild;

	if (phyloc != MII_PHY_ANY && offloc != MII_PHY_ANY)
		panic("mii_detach: phyloc and offloc specified");

	if ((mii->mii_flags & MIIF_INITDONE) == 0)
		return;

	for (child = LIST_FIRST(&mii->mii_phys);
	     child != NULL; child = nchild) {
		nchild = LIST_NEXT(child, mii_list);
		if (phyloc != MII_PHY_ANY || offloc != MII_OFFSET_ANY) {
			if (phyloc != MII_PHY_ANY &&
			    phyloc != child->mii_phy)
				continue;
			if (offloc != MII_OFFSET_ANY &&
			    offloc != child->mii_offset)
				continue;
		}
		(void)config_detach(child->mii_dev, DETACH_FORCE);
	}
}

static int
mii_print(void *aux, const char *pnp)
{
	struct mii_attach_args *ma = aux;

	if (pnp != NULL)
		aprint_normal("OUI 0x%06x model 0x%04x rev %d at %s",
		    MII_OUI(ma->mii_id1, ma->mii_id2), MII_MODEL(ma->mii_id2),
		    MII_REV(ma->mii_id2), pnp);

	aprint_normal(" phy %d", ma->mii_phyno);
	return (UNCONF);
}

static inline int
phy_service(struct mii_softc *sc, struct mii_data *mii, int cmd)
{
	if (!device_is_active(sc->mii_dev))
		return ENXIO;
	return PHY_SERVICE(sc, mii, cmd);
}

int
mii_ifmedia_change(struct mii_data *mii)
{
	return ifmedia_change(&mii->mii_media, mii->mii_ifp);
}

/*
 * Media changed; notify all PHYs.
 */
int
mii_mediachg(struct mii_data *mii)
{
	struct mii_softc *child;
	int rv;

	mii->mii_media_status = 0;
	mii->mii_media_active = IFM_NONE;

	LIST_FOREACH(child, &mii->mii_phys, mii_list) {
		rv = phy_service(child, mii, MII_MEDIACHG);
		if (rv)
			return (rv);
	}
	return (0);
}

/*
 * Call the PHY tick routines, used during autonegotiation.
 */
void
mii_tick(struct mii_data *mii)
{
	struct mii_softc *child;

	LIST_FOREACH(child, &mii->mii_phys, mii_list)
		(void)phy_service(child, mii, MII_TICK);
}

/*
 * Get media status from PHYs.
 */
void
mii_pollstat(struct mii_data *mii)
{
	struct mii_softc *child;

	mii->mii_media_status = 0;
	mii->mii_media_active = IFM_NONE;

	LIST_FOREACH(child, &mii->mii_phys, mii_list)
		(void)phy_service(child, mii, MII_POLLSTAT);
}

/*
 * Inform the PHYs that the interface is down.
 */
void
mii_down(struct mii_data *mii)
{
	struct mii_softc *child;

	LIST_FOREACH(child, &mii->mii_phys, mii_list)
		(void)phy_service(child, mii, MII_DOWN);
}

static unsigned char
bitreverse(unsigned char x)
{
	static const unsigned char nibbletab[16] = {
		0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15
	};

	return ((nibbletab[x & 15] << 4) | nibbletab[x >> 4]);
}

u_int
mii_oui(u_int id1, u_int id2)
{
	u_int h;

	h = (id1 << 6) | (id2 >> 10);

	return ((bitreverse(h >> 16) << 16) |
		(bitreverse((h >> 8) & 255) << 8) |
		bitreverse(h & 255));
}
