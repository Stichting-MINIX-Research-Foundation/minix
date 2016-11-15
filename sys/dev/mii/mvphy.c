/*	$NetBSD: mvphy.c,v 1.10 2014/06/16 16:48:16 msaitoh Exp $	*/

/*-
 * Copyright (c) 2006 Sam Leffler, Errno Consulting
 * All rights reserved.
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

/*
 * Driver for Marvell 88E6060 10/100 5-port PHY switch.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mvphy.c,v 1.10 2014/06/16 16:48:16 msaitoh Exp $");

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

#include <dev/mii/mvphyreg.h>

#define	MV_PORT(sc)	((sc)->mii_phy - 16)	/* PHY # to switch port */
#define	MV_CPU_PORT	5			/* port # of CPU port */

#define	MV_READ(p, phy, r) \
	(*(p)->mii_pdata->mii_readreg)(device_parent((p)->mii_dev), \
	    phy, (r))
#define	MV_WRITE(p, phy, r, v) \
	(*(p)->mii_pdata->mii_writereg)(device_parent((p)->mii_dev), \
	    phy, (r), (v))

/* XXX sysctl'able */
#define MV_ATUCTRL_ATU_SIZE_DEFAULT	2	/* 1024 entry database */
#define MV_ATUCTRL_AGE_TIME_DEFAULT	19	/* 19 * 16 = 304 seconds */

/*
 * Register manipulation macros that expect bit field defines
 * to follow the convention that an _S suffix is appended for
 * a shift count, while the field mask has no suffix.
 */
#define	SM(_v, _f)	(((_v) << _f##_S) & _f)
#define	MS(_v, _f)	(((_v) & _f) >> _f##_S)

static int	mvphymatch(device_t, cfdata_t, void *);
static void	mvphyattach(device_t, device_t, void *);

CFATTACH_DECL_NEW(mvphy, sizeof(struct mii_softc),
    mvphymatch, mvphyattach, mii_phy_detach, mii_phy_activate);

static int	mvphy_service(struct mii_softc *, struct mii_data *, int);
static void	mvphy_status(struct mii_softc *);
static void	mvphy_reset(struct mii_softc *sc);

static const struct mii_phy_funcs mvphy_funcs = {
	mvphy_service, mvphy_status, mvphy_reset,
};

static const struct mii_phydesc mvphys[] = {
	{ MII_OUI_xxMARVELL,		MII_MODEL_xxMARVELL_E6060,
	  MII_STR_xxMARVELL_E6060 },

	{ 0,				0,
	  NULL },
};

/*
 * On AP30/AR5312 the switch is configured in one of two ways:
 * as a ROUTER or as a BRIDGE.  The ROUTER config sets up ports
 * 0-3 as LAN ports, port 4 as the WAN port, and port 5 connects
 * to the MAC in the 5312.  The BRIDGE config sets up ports
 * 0-4 as LAN ports with port 5 connected to the MAC in the 5312.
 */
struct mvPhyConfig {
	uint16_t switchPortAddr;/* switch port associated with PHY */
	uint16_t vlanSetting;	/* VLAN table setting  for PHY */
	uint32_t portControl;	/* switch port control setting for PHY */
};
static const struct mvPhyConfig dumbConfig[] = {
	{ 0x18, 0x2e,		/* PHY port 0 = LAN port 0 */
	  MV_PORT_CONTROL_PORT_STATE_FORWARDING },
	{ 0x19, 0x2d,		/* PHY port 1 = LAN port 1 */
	  MV_PORT_CONTROL_PORT_STATE_FORWARDING },
	{ 0x1a, 0x2b,		/* PHY port 2 = LAN port 2 */
	  MV_PORT_CONTROL_PORT_STATE_FORWARDING },
	{ 0x1b, 0x27,		/* PHY port 3 = LAN port 3 */
	  MV_PORT_CONTROL_PORT_STATE_FORWARDING },
	{ 0x1c, 0x25,		/* PHY port 4 = LAN port 4 */
	  MV_PORT_CONTROL_PORT_STATE_FORWARDING },
	{ 0x1d, 0x1f,		/* PHY port 5 = CPU port */
	  MV_PORT_CONTROL_PORT_STATE_FORWARDING }
};
static const struct mvPhyConfig routerConfig[] = {
	{ 0x18, 0x2e,		/* PHY port 0 = LAN port 0 */
	  MV_PORT_CONTROL_PORT_STATE_FORWARDING },
	{ 0x19, 0x2d,		/* PHY port 1 = LAN port 1 */
	  MV_PORT_CONTROL_PORT_STATE_FORWARDING },
	{ 0x1a, 0x2b,		/* PHY port 2 = LAN port 2 */
	  MV_PORT_CONTROL_PORT_STATE_FORWARDING },
	{ 0x1b, 0x27,		/* PHY port 3 = LAN port 3 */
	  MV_PORT_CONTROL_PORT_STATE_FORWARDING },
	{ 0x1c, 0x1020,		/* PHY port 4 = WAN port */
	  MV_PORT_CONTROL_PORT_STATE_FORWARDING },
	/* NB: 0x0f =>'s send only to LAN ports */
	{ 0x1d, 0x0f,		/* PHY port 5 = CPU port */
	  MV_PORT_CONTROL_PORT_STATE_FORWARDING
#if 0
	  | MV_PORT_CONTROL_INGRESS_TRAILER
	  | MV_PORT_CONTROL_EGRESS_MODE
#endif
	  }
};
static const struct mvPhyConfig bridgeConfig[] = {
	{ 0x18, 0x3e,		/* PHY port 0 = LAN port 0 */
	  MV_PORT_CONTROL_PORT_STATE_FORWARDING },
	{ 0x19, 0x3d,		/* PHY port 1 = LAN port 1 */
	  MV_PORT_CONTROL_PORT_STATE_FORWARDING },
	{ 0x1a, 0x3b,		/* PHY port 2 = LAN port 2 */
	  MV_PORT_CONTROL_PORT_STATE_FORWARDING },
	{ 0x1b, 0x37,		/* PHY port 3 = LAN port 3 */
	  MV_PORT_CONTROL_PORT_STATE_FORWARDING },
	{ 0x1c, 0x37,		/* PHY port 4 = LAN port 4 */
	  MV_PORT_CONTROL_PORT_STATE_FORWARDING },
	/* NB: 0x1f =>'s send to all ports */
	{ 0x1d, 0x1f,		/* PHY port 5 = CPU port */
	  MV_PORT_CONTROL_PORT_STATE_FORWARDING
#if 0
	  | MV_PORT_CONTROL_INGRESS_TRAILER
	  | MV_PORT_CONTROL_EGRESS_MODE
#endif
	}
};

static void mvphy_switchconfig(struct mii_softc *, int);
static void mvphy_flushatu(struct mii_softc *);

static int
mvphymatch(device_t parent, cfdata_t match, void *aux)
{
	struct mii_attach_args *ma = aux;

	if (mii_phy_match(ma, mvphys) != NULL)
		return (10);

	return (0);
}

static void
mvphyattach(device_t parent, device_t self, void *aux)
{
	struct mii_softc *sc = device_private(self);
	struct mii_attach_args *ma = aux;
	struct mii_data *mii = ma->mii_data;
	const struct mii_phydesc *mpd;

	mpd = mii_phy_match(ma, mvphys);
	aprint_naive(": Media interface\n");
	aprint_normal(": %s, rev. %d\n", mpd->mpd_name, MII_REV(ma->mii_id2));

	sc->mii_dev = self;
	sc->mii_inst = mii->mii_instance;
	sc->mii_phy = ma->mii_phyno;
	sc->mii_funcs = &mvphy_funcs;
	sc->mii_pdata = mii;
	sc->mii_flags = ma->mii_flags;
	sc->mii_anegticks = MII_ANEGTICKS;

	if (MV_PORT(sc) == 0) {		/* NB: only when attaching first PHY */
		/*
		 * Set the global switch settings and configure the
		 * CPU port since it does not probe as a visible PHY.
		 */
		MV_WRITE(sc, MII_MV_SWITCH_GLOBAL_ADDR, MV_ATU_CONTROL,
		      SM(MV_ATUCTRL_AGE_TIME_DEFAULT, MV_ATUCTRL_AGE_TIME)
		    | SM(MV_ATUCTRL_ATU_SIZE_DEFAULT, MV_ATUCTRL_ATU_SIZE));
		mvphy_switchconfig(sc, MV_CPU_PORT);
	}
	PHY_RESET(sc);

	sc->mii_capabilities = PHY_READ(sc, MII_BMSR) & ma->mii_capmask;
	aprint_normal_dev(self, "");
	if ((sc->mii_capabilities & BMSR_MEDIAMASK) == 0)
		aprint_error("no media present");
	else
		mii_phy_add_media(sc);
	aprint_normal("\n");
}

static int
mvphy_service(struct mii_softc *sc, struct mii_data *mii, int cmd)
{
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;

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
			/* XXX? */
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
mvphy_status(struct mii_softc *sc)
{
	struct mii_data *mii = sc->mii_pdata;
	int hwstatus;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	hwstatus = PHY_READ(sc, MII_MV_PHY_SPECIFIC_STATUS);
	if (hwstatus & MV_STATUS_REAL_TIME_LINK_UP) {
		mii->mii_media_status |= IFM_ACTIVE;
		if (hwstatus & MV_STATUS_RESOLVED_SPEED_100)
			mii->mii_media_active |= IFM_100_TX;
		else
			mii->mii_media_active |= IFM_10_T;
		if (hwstatus & MV_STATUS_RESOLVED_DUPLEX_FULL)
			mii->mii_media_active |= IFM_FDX;
		else
			mii->mii_media_active |= IFM_HDX;
	} else {
		mii->mii_media_active |= IFM_NONE;
		/* XXX flush ATU only on link down transition */
		mvphy_flushatu(sc);
	}
}

static void
mvphy_reset(struct mii_softc *sc)
{

	/* XXX handle fixed media config */
	PHY_WRITE(sc, MII_BMCR, BMCR_RESET | BMCR_AUTOEN);
	mvphy_switchconfig(sc, MV_PORT(sc));
}

/*
 * Configure switch for the specified port.
 */
static void
mvphy_switchconfig(struct mii_softc *sc, int port)
{
	/* XXX router vs bridge */
	/*const struct mvPhyConfig *conf = &routerConfig[port];*/
	/*const struct mvPhyConfig *conf = &bridgeConfig[port];*/
	const struct mvPhyConfig *conf = &dumbConfig[port];

	MV_WRITE(sc, conf->switchPortAddr, MV_PORT_BASED_VLAN_MAP,
	    conf->vlanSetting);
	/* XXX administrative control of port enable? */
	MV_WRITE(sc, conf->switchPortAddr, MV_PORT_CONTROL, conf->portControl);
	MV_WRITE(sc, conf->switchPortAddr, MV_PORT_ASSOCIATION_VECTOR, 1<<port);
}

/*
 * Flush the Address Translation Unit (ATU).
 */
static void
mvphy_flushatu(struct mii_softc *sc)
{
	uint16_t status;
	int i;

	/* wait for any previous request to complete */
	/* XXX if busy defer to tick */
	/* XXX timeout */
	for (i = 0; i < 1000; i++) {
		status = MV_READ(sc, MII_MV_SWITCH_GLOBAL_ADDR,
				MV_ATU_OPERATION);
		if (MV_ATU_IS_BUSY(status))
			break;
	}
	if (i != 1000) {
		MV_WRITE(sc, MII_MV_SWITCH_GLOBAL_ADDR, MV_ATU_OPERATION,
		    MV_ATU_OP_FLUSH_ALL | MV_ATU_BUSY);
	} /*else
		aprint_error_dev(sc->mii_dev, "timeout waiting for ATU flush\n");*/
}
