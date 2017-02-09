/*	$NetBSD: if_axe.c,v 1.69 2015/04/13 16:33:25 riastradh Exp $	*/
/*	$OpenBSD: if_axe.c,v 1.96 2010/01/09 05:33:08 jsg Exp $ */

/*
 * Copyright (c) 2005, 2006, 2007 Jonathan Gray <jsg@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Copyright (c) 1997, 1998, 1999, 2000-2003
 *	Bill Paul <wpaul@windriver.com>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * ASIX Electronics AX88172 USB 2.0 ethernet driver. Used in the
 * LinkSys USB200M and various other adapters.
 *
 * Manuals available from:
 * http://www.asix.com.tw/datasheet/mac/Ax88172.PDF
 * Note: you need the manual for the AX88170 chip (USB 1.x ethernet
 * controller) to find the definitions for the RX control register.
 * http://www.asix.com.tw/datasheet/mac/Ax88170.PDF
 *
 * Written by Bill Paul <wpaul@windriver.com>
 * Senior Engineer
 * Wind River Systems
 */

/*
 * The AX88172 provides USB ethernet supports at 10 and 100Mbps.
 * It uses an external PHY (reference designs use a RealTek chip),
 * and has a 64-bit multicast hash filter. There is some information
 * missing from the manual which one needs to know in order to make
 * the chip function:
 *
 * - You must set bit 7 in the RX control register, otherwise the
 *   chip won't receive any packets.
 * - You must initialize all 3 IPG registers, or you won't be able
 *   to send any packets.
 *
 * Note that this device appears to only support loading the station
 * address via autoload from the EEPROM (i.e. there's no way to manaully
 * set it).
 *
 * (Adam Weinberger wanted me to name this driver if_gir.c.)
 */

/*
 * Ported to OpenBSD 3/28/2004 by Greg Taleck <taleck@oz.net>
 * with bits and pieces from the aue and url drivers.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_axe.c,v 1.69 2015/04/13 16:33:25 riastradh Exp $");

#ifdef _KERNEL_OPT
#include "opt_inet.h"
#endif

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/systm.h>

#include <sys/rndsource.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#include <net/bpf.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usbdevs.h>

#include <dev/usb/if_axereg.h>

#ifdef	AXE_DEBUG
#define DPRINTF(x)	do { if (axedebug) printf x; } while (0)
#define DPRINTFN(n,x)	do { if (axedebug >= (n)) printf x; } while (0)
int	axedebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

/*
 * Various supported device vendors/products.
 */
static const struct axe_type axe_devs[] = {
	{ { USB_VENDOR_ABOCOM,		USB_PRODUCT_ABOCOM_UFE2000}, 0 },
	{ { USB_VENDOR_ACERCM,		USB_PRODUCT_ACERCM_EP1427X2}, 0 },
	{ { USB_VENDOR_APPLE,		USB_PRODUCT_APPLE_ETHERNET }, AX772 },
	{ { USB_VENDOR_ASIX,		USB_PRODUCT_ASIX_AX88172}, 0 },
	{ { USB_VENDOR_ASIX,		USB_PRODUCT_ASIX_AX88772}, AX772 },
	{ { USB_VENDOR_ASIX,		USB_PRODUCT_ASIX_AX88772A}, AX772 },
	{ { USB_VENDOR_ASIX,		USB_PRODUCT_ASIX_AX88772B}, AX772 | AX772B },
	{ { USB_VENDOR_ASIX,		USB_PRODUCT_ASIX_AX88772B_1}, AX772 | AX772B },
	{ { USB_VENDOR_ASIX,		USB_PRODUCT_ASIX_AX88178}, AX178 },
	{ { USB_VENDOR_ATEN,		USB_PRODUCT_ATEN_UC210T}, 0 },
	{ { USB_VENDOR_BELKIN,		USB_PRODUCT_BELKIN_F5D5055 }, AX178 },
	{ { USB_VENDOR_BILLIONTON,	USB_PRODUCT_BILLIONTON_USB2AR}, 0},
	{ { USB_VENDOR_CISCOLINKSYS,	USB_PRODUCT_CISCOLINKSYS_USB200MV2}, AX772 },
	{ { USB_VENDOR_COREGA,		USB_PRODUCT_COREGA_FETHER_USB2_TX }, 0},
	{ { USB_VENDOR_DLINK,		USB_PRODUCT_DLINK_DUBE100}, 0 },
	{ { USB_VENDOR_DLINK,		USB_PRODUCT_DLINK_DUBE100B1 }, AX772 },
	{ { USB_VENDOR_DLINK,		USB_PRODUCT_DLINK_DUBE100C1 }, AX772 | AX772B },
	{ { USB_VENDOR_GOODWAY,		USB_PRODUCT_GOODWAY_GWUSB2E}, 0 },
	{ { USB_VENDOR_IODATA,		USB_PRODUCT_IODATA_ETGUS2 }, AX178 },
	{ { USB_VENDOR_JVC,		USB_PRODUCT_JVC_MP_PRX1}, 0 },
	{ { USB_VENDOR_LENOVO,		USB_PRODUCT_LENOVO_ETHERNET }, AX772 | AX772B },
	{ { USB_VENDOR_LINKSYS2,	USB_PRODUCT_LINKSYS2_USB200M}, 0 },
	{ { USB_VENDOR_LINKSYS4,	USB_PRODUCT_LINKSYS4_USB1000 }, AX178 },
	{ { USB_VENDOR_LOGITEC,		USB_PRODUCT_LOGITEC_LAN_GTJU2}, AX178 },
	{ { USB_VENDOR_MELCO,		USB_PRODUCT_MELCO_LUAU2GT}, AX178 },
	{ { USB_VENDOR_MELCO,		USB_PRODUCT_MELCO_LUAU2KTX}, 0 },
	{ { USB_VENDOR_MSI,		USB_PRODUCT_MSI_AX88772A}, AX772 },
	{ { USB_VENDOR_NETGEAR,		USB_PRODUCT_NETGEAR_FA120}, 0 },
	{ { USB_VENDOR_OQO,		USB_PRODUCT_OQO_ETHER01PLUS }, AX772 },
	{ { USB_VENDOR_PLANEX3,		USB_PRODUCT_PLANEX3_GU1000T }, AX178 },
	{ { USB_VENDOR_SYSTEMTALKS,	USB_PRODUCT_SYSTEMTALKS_SGCX2UL}, 0 },
	{ { USB_VENDOR_SITECOM,		USB_PRODUCT_SITECOM_LN029}, 0 },
	{ { USB_VENDOR_SITECOMEU,	USB_PRODUCT_SITECOMEU_LN028 }, AX178 }
};
#define axe_lookup(v, p) ((const struct axe_type *)usb_lookup(axe_devs, v, p))

int	axe_match(device_t, cfdata_t, void *);
void	axe_attach(device_t, device_t, void *);
int	axe_detach(device_t, int);
int	axe_activate(device_t, devact_t);

CFATTACH_DECL_NEW(axe, sizeof(struct axe_softc),
	axe_match, axe_attach, axe_detach, axe_activate);

static int	axe_tx_list_init(struct axe_softc *);
static int	axe_rx_list_init(struct axe_softc *);
static int	axe_encap(struct axe_softc *, struct mbuf *, int);
static void	axe_rxeof(usbd_xfer_handle, usbd_private_handle, usbd_status);
static void	axe_txeof(usbd_xfer_handle, usbd_private_handle, usbd_status);
static void	axe_tick(void *);
static void	axe_tick_task(void *);
static void	axe_start(struct ifnet *);
static int	axe_ioctl(struct ifnet *, u_long, void *);
static int	axe_init(struct ifnet *);
static void	axe_stop(struct ifnet *, int);
static void	axe_watchdog(struct ifnet *);
static int	axe_miibus_readreg_locked(device_t, int, int);
static int	axe_miibus_readreg(device_t, int, int);
static void	axe_miibus_writereg_locked(device_t, int, int, int);
static void	axe_miibus_writereg(device_t, int, int, int);
static void	axe_miibus_statchg(struct ifnet *);
static int	axe_cmd(struct axe_softc *, int, int, int, void *);
static void	axe_reset(struct axe_softc *sc);
static int	axe_ifmedia_upd(struct ifnet *);
static void	axe_ifmedia_sts(struct ifnet *, struct ifmediareq *);

static void	axe_setmulti(struct axe_softc *);
static void	axe_lock_mii(struct axe_softc *sc);
static void	axe_unlock_mii(struct axe_softc *sc);

static void	axe_ax88178_init(struct axe_softc *);
static void	axe_ax88772_init(struct axe_softc *);

/* Get exclusive access to the MII registers */
static void
axe_lock_mii(struct axe_softc *sc)
{

	sc->axe_refcnt++;
	mutex_enter(&sc->axe_mii_lock);
}

static void
axe_unlock_mii(struct axe_softc *sc)
{

	mutex_exit(&sc->axe_mii_lock);
	if (--sc->axe_refcnt < 0)
		usb_detach_wakeupold((sc->axe_dev));
}

static int
axe_cmd(struct axe_softc *sc, int cmd, int index, int val, void *buf)
{
	usb_device_request_t req;
	usbd_status err;

	KASSERT(mutex_owned(&sc->axe_mii_lock));

	if (sc->axe_dying)
		return 0;

	if (AXE_CMD_DIR(cmd))
		req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	else
		req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = AXE_CMD_CMD(cmd);
	USETW(req.wValue, val);
	USETW(req.wIndex, index);
	USETW(req.wLength, AXE_CMD_LEN(cmd));

	err = usbd_do_request(sc->axe_udev, &req, buf);

	if (err) {
		DPRINTF(("axe_cmd err: cmd %d err %d\n", cmd, err));
		return -1;
	}
	return 0;
}

static int
axe_miibus_readreg_locked(device_t dev, int phy, int reg)
{
	struct axe_softc *sc = device_private(dev);
	usbd_status err;
	uint16_t val;

	axe_cmd(sc, AXE_CMD_MII_OPMODE_SW, 0, 0, NULL);
	err = axe_cmd(sc, AXE_CMD_MII_READ_REG, reg, phy, (void *)&val);
	axe_cmd(sc, AXE_CMD_MII_OPMODE_HW, 0, 0, NULL);
	if (err) {
		aprint_error_dev(sc->axe_dev, "read PHY failed\n");
		return -1;
	}

	val = le16toh(val);
	if (sc->axe_flags & AX772 && reg == MII_BMSR) {
		/*
		 * BMSR of AX88772 indicates it supports extended
		 * capability but the extended status register is
		 * reserverd for embedded ethernet PHY. So clear the
		 * extended capability bit of BMSR.
		 */
		 val &= ~BMSR_EXTCAP;
	}

	DPRINTF(("axe_miibus_readreg: phy 0x%x reg 0x%x val 0x%x\n",
	    phy, reg, val));

	return val;
}

static int
axe_miibus_readreg(device_t dev, int phy, int reg)
{
	struct axe_softc *sc = device_private(dev);
	int val;

	if (sc->axe_dying)
		return 0;

	if (sc->axe_phyno != phy)
		return 0;

	axe_lock_mii(sc);
	val = axe_miibus_readreg_locked(dev, phy, reg);
	axe_unlock_mii(sc);

	return val;
}

static void
axe_miibus_writereg_locked(device_t dev, int phy, int reg, int aval)
{
	struct axe_softc *sc = device_private(dev);
	usbd_status err;
	uint16_t val;

	val = htole16(aval);

	axe_cmd(sc, AXE_CMD_MII_OPMODE_SW, 0, 0, NULL);
	err = axe_cmd(sc, AXE_CMD_MII_WRITE_REG, reg, phy, (void *)&val);
	axe_cmd(sc, AXE_CMD_MII_OPMODE_HW, 0, 0, NULL);

	if (err) {
		aprint_error_dev(sc->axe_dev, "write PHY failed\n");
		return;
	}
}

static void
axe_miibus_writereg(device_t dev, int phy, int reg, int aval)
{
	struct axe_softc *sc = device_private(dev);

	if (sc->axe_dying)
		return;

	if (sc->axe_phyno != phy)
		return;

	axe_lock_mii(sc);
	axe_miibus_writereg_locked(dev, phy, reg, aval);
	axe_unlock_mii(sc);
}

static void
axe_miibus_statchg(struct ifnet *ifp)
{
	struct axe_softc *sc = ifp->if_softc;
	struct mii_data *mii = &sc->axe_mii;
	int val, err;

	if ((mii->mii_media_active & IFM_GMASK) == IFM_FDX)
		val = AXE_MEDIA_FULL_DUPLEX;
	else
		val = 0;

	if (sc->axe_flags & AX178 || sc->axe_flags & AX772) {
		val |= (AXE_178_MEDIA_RX_EN | AXE_178_MEDIA_MAGIC);
		if (sc->axe_flags & AX178)
			val |= AXE_178_MEDIA_ENCK;
		switch (IFM_SUBTYPE(mii->mii_media_active)) {
		case IFM_1000_T:
			val |= AXE_178_MEDIA_GMII | AXE_178_MEDIA_ENCK;
			break;
		case IFM_100_TX:
			val |= AXE_178_MEDIA_100TX;
			break;
		case IFM_10_T:
			/* doesn't need to be handled */
			break;
		}
	}

	DPRINTF(("axe_miibus_statchg: val=0x%x\n", val));
	axe_lock_mii(sc);
	err = axe_cmd(sc, AXE_CMD_WRITE_MEDIA, 0, val, NULL);
	axe_unlock_mii(sc);
	if (err) {
		aprint_error_dev(sc->axe_dev, "media change failed\n");
		return;
	}
}

/*
 * Set media options
 */
static int
axe_ifmedia_upd(struct ifnet *ifp)
{
	struct axe_softc *sc = ifp->if_softc;
	struct mii_data *mii = &sc->axe_mii;
	int rc;

	sc->axe_link = 0;

	if (mii->mii_instance) {
		struct mii_softc *miisc;

		LIST_FOREACH(miisc, &mii->mii_phys, mii_list)
			mii_phy_reset(miisc);
	}

	if ((rc = mii_mediachg(mii)) == ENXIO)
		return 0;
	return rc;
}

/*
 * Report current media status
 */
static void
axe_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct axe_softc	*sc = ifp->if_softc;
	struct mii_data		*mii = &sc->axe_mii;

	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
}

static void
axe_setmulti(struct axe_softc *sc)
{
	struct ifnet *ifp = &sc->sc_if;
	struct ether_multi *enm;
	struct ether_multistep step;
	uint32_t h = 0;
	uint16_t rxmode;
	uint8_t hashtbl[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

	if (sc->axe_dying)
		return;

	axe_lock_mii(sc);
	axe_cmd(sc, AXE_CMD_RXCTL_READ, 0, 0, (void *)&rxmode);
	rxmode = le16toh(rxmode);

	rxmode &= ~(AXE_RXCMD_ALLMULTI | AXE_RXCMD_PROMISC);

	/* If we want promiscuous mode, set the allframes bit */
	if (ifp->if_flags & IFF_PROMISC) {
		rxmode |= AXE_RXCMD_PROMISC;
		goto allmulti;
	}

	/* Now program new ones */
	ETHER_FIRST_MULTI(step, &sc->axe_ec, enm);
	while (enm != NULL) {
		if (memcmp(enm->enm_addrlo, enm->enm_addrhi,
		    ETHER_ADDR_LEN) != 0)
			goto allmulti;

		h = ether_crc32_be(enm->enm_addrlo, ETHER_ADDR_LEN) >> 26;
		hashtbl[h >> 3] |= 1U << (h & 7);
		ETHER_NEXT_MULTI(step, enm);
	}
	ifp->if_flags &= ~IFF_ALLMULTI;
	axe_cmd(sc, AXE_CMD_WRITE_MCAST, 0, 0, (void *)&hashtbl);
	axe_cmd(sc, AXE_CMD_RXCTL_WRITE, 0, rxmode, NULL);
	axe_unlock_mii(sc);
	return;

 allmulti:
	ifp->if_flags |= IFF_ALLMULTI;
	rxmode |= AXE_RXCMD_ALLMULTI;
	axe_cmd(sc, AXE_CMD_RXCTL_WRITE, 0, rxmode, NULL);
	axe_unlock_mii(sc);
}

static void
axe_reset(struct axe_softc *sc)
{

	if (sc->axe_dying)
		return;
	/* XXX What to reset? */

	/* Wait a little while for the chip to get its brains in order. */
	DELAY(1000);
}

static int
axe_get_phyno(struct axe_softc *sc, int sel)
{
	int phyno;

	switch (AXE_PHY_TYPE(sc->axe_phyaddrs[sel])) {
	case PHY_TYPE_100_HOME:
		/* FALLTHROUGH */
	case PHY_TYPE_GIG:
		phyno = AXE_PHY_NO(sc->axe_phyaddrs[sel]);
		break;
	case PHY_TYPE_SPECIAL:
		/* FALLTHROUGH */
	case PHY_TYPE_RSVD:
		/* FALLTHROUGH */
	case PHY_TYPE_NON_SUP:
		/* FALLTHROUGH */
	default:
		phyno = -1;
		break;
	}

	return phyno;
}

#define	AXE_GPIO_WRITE(x, y)	do {				\
	axe_cmd(sc, AXE_CMD_WRITE_GPIO, 0, (x), NULL);		\
	usbd_delay_ms(sc->axe_udev, hztoms(y));			\
} while (0)

static void
axe_ax88178_init(struct axe_softc *sc)
{
	int gpio0, ledmode, phymode;
	uint16_t eeprom, val;

	axe_cmd(sc, AXE_CMD_SROM_WR_ENABLE, 0, 0, NULL);
	/* XXX magic */
	axe_cmd(sc, AXE_CMD_SROM_READ, 0, 0x0017, &eeprom);
	axe_cmd(sc, AXE_CMD_SROM_WR_DISABLE, 0, 0, NULL);

	eeprom = le16toh(eeprom);

	DPRINTF((" EEPROM is 0x%x\n", eeprom));

	/* if EEPROM is invalid we have to use to GPIO0 */
	if (eeprom == 0xffff) {
		phymode = AXE_PHY_MODE_MARVELL;
		gpio0 = 1;
		ledmode = 0;
	} else {
		phymode = eeprom & 0x7f;
		gpio0 = (eeprom & 0x80) ? 0 : 1;
		ledmode = eeprom >> 8;
	}

	DPRINTF(("use gpio0: %d, phymode %d\n", gpio0, phymode));

	/* Program GPIOs depending on PHY hardware. */
	switch (phymode) {
	case AXE_PHY_MODE_MARVELL:
		if (gpio0 == 1) {
			AXE_GPIO_WRITE(AXE_GPIO_RELOAD_EEPROM | AXE_GPIO0_EN,
			    hz / 32);
			AXE_GPIO_WRITE(AXE_GPIO0_EN | AXE_GPIO2 | AXE_GPIO2_EN,
			    hz / 32);
			AXE_GPIO_WRITE(AXE_GPIO0_EN | AXE_GPIO2_EN, hz / 4);
			AXE_GPIO_WRITE(AXE_GPIO0_EN | AXE_GPIO2 | AXE_GPIO2_EN,
			    hz / 32);
		} else {
			AXE_GPIO_WRITE(AXE_GPIO_RELOAD_EEPROM | AXE_GPIO1 |
			    AXE_GPIO1_EN, hz / 3);
			if (ledmode == 1) {
				AXE_GPIO_WRITE(AXE_GPIO1_EN, hz / 3);
				AXE_GPIO_WRITE(AXE_GPIO1 | AXE_GPIO1_EN,
				    hz / 3);
			} else {
				AXE_GPIO_WRITE(AXE_GPIO1 | AXE_GPIO1_EN |
				    AXE_GPIO2 | AXE_GPIO2_EN, hz / 32);
				AXE_GPIO_WRITE(AXE_GPIO1 | AXE_GPIO1_EN |
				    AXE_GPIO2_EN, hz / 4);
				AXE_GPIO_WRITE(AXE_GPIO1 | AXE_GPIO1_EN |
				    AXE_GPIO2 | AXE_GPIO2_EN, hz / 32);
			}
		}
		break;
	case AXE_PHY_MODE_CICADA:
	case AXE_PHY_MODE_CICADA_V2:
	case AXE_PHY_MODE_CICADA_V2_ASIX:
		if (gpio0 == 1)
			AXE_GPIO_WRITE(AXE_GPIO_RELOAD_EEPROM | AXE_GPIO0 |
			    AXE_GPIO0_EN, hz / 32);
		else
			AXE_GPIO_WRITE(AXE_GPIO_RELOAD_EEPROM | AXE_GPIO1 |
			    AXE_GPIO1_EN, hz / 32);
		break;
	case AXE_PHY_MODE_AGERE:
		AXE_GPIO_WRITE(AXE_GPIO_RELOAD_EEPROM | AXE_GPIO1 |
		    AXE_GPIO1_EN, hz / 32);
		AXE_GPIO_WRITE(AXE_GPIO1 | AXE_GPIO1_EN | AXE_GPIO2 |
		    AXE_GPIO2_EN, hz / 32);
		AXE_GPIO_WRITE(AXE_GPIO1 | AXE_GPIO1_EN | AXE_GPIO2_EN, hz / 4);
		AXE_GPIO_WRITE(AXE_GPIO1 | AXE_GPIO1_EN | AXE_GPIO2 |
		    AXE_GPIO2_EN, hz / 32);
		break;
	case AXE_PHY_MODE_REALTEK_8211CL:
	case AXE_PHY_MODE_REALTEK_8211BN:
	case AXE_PHY_MODE_REALTEK_8251CL:
		val = gpio0 == 1 ? AXE_GPIO0 | AXE_GPIO0_EN :
		    AXE_GPIO1 | AXE_GPIO1_EN;
		AXE_GPIO_WRITE(val, hz / 32);
		AXE_GPIO_WRITE(val | AXE_GPIO2 | AXE_GPIO2_EN, hz / 32);
		AXE_GPIO_WRITE(val | AXE_GPIO2_EN, hz / 4);
		AXE_GPIO_WRITE(val | AXE_GPIO2 | AXE_GPIO2_EN, hz / 32);
		if (phymode == AXE_PHY_MODE_REALTEK_8211CL) {
			axe_miibus_writereg_locked(sc->axe_dev,
			    sc->axe_phyno, 0x1F, 0x0005);
			axe_miibus_writereg_locked(sc->axe_dev,
			    sc->axe_phyno, 0x0C, 0x0000);
			val = axe_miibus_readreg_locked(sc->axe_dev,
			    sc->axe_phyno, 0x0001);
			axe_miibus_writereg_locked(sc->axe_dev,
			    sc->axe_phyno, 0x01, val | 0x0080);
			axe_miibus_writereg_locked(sc->axe_dev,
			    sc->axe_phyno, 0x1F, 0x0000);
		}
		break;
	default:
		/* Unknown PHY model or no need to program GPIOs. */
		break;
	}

	/* soft reset */
	axe_cmd(sc, AXE_CMD_SW_RESET_REG, 0, AXE_SW_RESET_CLEAR, NULL);
	usbd_delay_ms(sc->axe_udev, 150);
	axe_cmd(sc, AXE_CMD_SW_RESET_REG, 0,
	    AXE_SW_RESET_PRL | AXE_178_RESET_MAGIC, NULL);
	usbd_delay_ms(sc->axe_udev, 150);
	/* Enable MII/GMII/RGMII for external PHY */
	axe_cmd(sc, AXE_CMD_SW_PHY_SELECT, 0, 0, NULL);
	usbd_delay_ms(sc->axe_udev, 10);
	axe_cmd(sc, AXE_CMD_RXCTL_WRITE, 0, 0, NULL);
}

static void
axe_ax88772_init(struct axe_softc *sc)
{

	axe_cmd(sc, AXE_CMD_WRITE_GPIO, 0, 0x00b0, NULL);
	usbd_delay_ms(sc->axe_udev, 40);

	if (sc->axe_phyno == AXE_772_PHY_NO_EPHY) {
		/* ask for the embedded PHY */
		axe_cmd(sc, AXE_CMD_SW_PHY_SELECT, 0, 0x01, NULL);
		usbd_delay_ms(sc->axe_udev, 10);

		/* power down and reset state, pin reset state */
		axe_cmd(sc, AXE_CMD_SW_RESET_REG, 0, AXE_SW_RESET_CLEAR, NULL);
		usbd_delay_ms(sc->axe_udev, 60);

		/* power down/reset state, pin operating state */
		axe_cmd(sc, AXE_CMD_SW_RESET_REG, 0,
		    AXE_SW_RESET_IPPD | AXE_SW_RESET_PRL, NULL);
		usbd_delay_ms(sc->axe_udev, 150);

		/* power up, reset */
		axe_cmd(sc, AXE_CMD_SW_RESET_REG, 0, AXE_SW_RESET_PRL, NULL);

		/* power up, operating */
		axe_cmd(sc, AXE_CMD_SW_RESET_REG, 0,
		    AXE_SW_RESET_IPRL | AXE_SW_RESET_PRL, NULL);
	} else {
		/* ask for external PHY */
		axe_cmd(sc, AXE_CMD_SW_PHY_SELECT, 0, 0x00, NULL);
		usbd_delay_ms(sc->axe_udev, 10);

		/* power down internal PHY */
		axe_cmd(sc, AXE_CMD_SW_RESET_REG, 0,
		    AXE_SW_RESET_IPPD | AXE_SW_RESET_PRL, NULL);
	}

	usbd_delay_ms(sc->axe_udev, 150);
	axe_cmd(sc, AXE_CMD_RXCTL_WRITE, 0, 0, NULL);
}

/*
 * Probe for a AX88172 chip.
 */
int
axe_match(device_t parent, cfdata_t match, void *aux)
{
	struct usb_attach_arg *uaa = aux;

	return axe_lookup(uaa->vendor, uaa->product) != NULL ?
	    UMATCH_VENDOR_PRODUCT : UMATCH_NONE;
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
void
axe_attach(device_t parent, device_t self, void *aux)
{
	struct axe_softc *sc = device_private(self);
	struct usb_attach_arg *uaa = aux;
	usbd_device_handle dev = uaa->device;
	usbd_status err;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	struct mii_data	*mii;
	uint8_t eaddr[ETHER_ADDR_LEN];
	char *devinfop;
	const char *devname = device_xname(self);
	struct ifnet *ifp;
	int i, s;

	aprint_naive("\n");
	aprint_normal("\n");

	sc->axe_dev = self;
	sc->axe_udev = dev;

	devinfop = usbd_devinfo_alloc(dev, 0);
	aprint_normal_dev(self, "%s\n", devinfop);
	usbd_devinfo_free(devinfop);

	err = usbd_set_config_no(dev, AXE_CONFIG_NO, 1);
	if (err) {
		aprint_error_dev(self, "failed to set configuration"
		    ", err=%s\n", usbd_errstr(err));
		return;
	}

	sc->axe_flags = axe_lookup(uaa->vendor, uaa->product)->axe_flags;

	mutex_init(&sc->axe_mii_lock, MUTEX_DEFAULT, IPL_NONE);
	usb_init_task(&sc->axe_tick_task, axe_tick_task, sc, 0);

	err = usbd_device2interface_handle(dev, AXE_IFACE_IDX, &sc->axe_iface);
	if (err) {
		aprint_error_dev(self, "getting interface handle failed\n");
		return;
	}

	sc->axe_product = uaa->product;
	sc->axe_vendor = uaa->vendor;

	id = usbd_get_interface_descriptor(sc->axe_iface);

	/* decide on what our bufsize will be */
	if (sc->axe_flags & AX178 || sc->axe_flags & AX772)
		sc->axe_bufsz = (sc->axe_udev->speed == USB_SPEED_HIGH) ?
		    AXE_178_MAX_BUFSZ : AXE_178_MIN_BUFSZ;
	else
		sc->axe_bufsz = AXE_172_BUFSZ;

	/* Find endpoints. */
	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->axe_iface, i);
		if (ed == NULL) {
			aprint_error_dev(self, "couldn't get ep %d\n", i);
			return;
		}
		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->axe_ed[AXE_ENDPT_RX] = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
			   UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->axe_ed[AXE_ENDPT_TX] = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
			   UE_GET_XFERTYPE(ed->bmAttributes) == UE_INTERRUPT) {
			sc->axe_ed[AXE_ENDPT_INTR] = ed->bEndpointAddress;
		}
	}

	s = splnet();

	/* We need the PHYID for init dance in some cases */
	axe_lock_mii(sc);
	axe_cmd(sc, AXE_CMD_READ_PHYID, 0, 0, (void *)&sc->axe_phyaddrs);

	DPRINTF((" phyaddrs[0]: %x phyaddrs[1]: %x\n",
	    sc->axe_phyaddrs[0], sc->axe_phyaddrs[1]));
	sc->axe_phyno = axe_get_phyno(sc, AXE_PHY_SEL_PRI);
	if (sc->axe_phyno == -1)
		sc->axe_phyno = axe_get_phyno(sc, AXE_PHY_SEL_SEC);
	if (sc->axe_phyno == -1) {
		DPRINTF((" no valid PHY address found, assuming PHY address 0\n"));
		sc->axe_phyno = 0;
	}

	if (sc->axe_flags & AX178)
		axe_ax88178_init(sc);
	else if (sc->axe_flags & AX772)
		axe_ax88772_init(sc);

	/*
	 * Get station address.
	 */
	if (sc->axe_flags & AX178 || sc->axe_flags & AX772)
		axe_cmd(sc, AXE_178_CMD_READ_NODEID, 0, 0, &eaddr);
	else
		axe_cmd(sc, AXE_172_CMD_READ_NODEID, 0, 0, &eaddr);

	/*
	 * Load IPG values
	 */
	axe_cmd(sc, AXE_CMD_READ_IPG012, 0, 0, (void *)&sc->axe_ipgs);
	axe_unlock_mii(sc);

	/*
	 * An ASIX chip was detected. Inform the world.
	 */
	aprint_normal_dev(self, "Ethernet address %s\n", ether_sprintf(eaddr));

	/* Initialize interface info.*/
	ifp = &sc->sc_if;
	ifp->if_softc = sc;
	strncpy(ifp->if_xname, devname, IFNAMSIZ);
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = axe_ioctl;
	ifp->if_start = axe_start;
	ifp->if_init = axe_init;
	ifp->if_stop = axe_stop;
	ifp->if_watchdog = axe_watchdog;

	IFQ_SET_READY(&ifp->if_snd);

	sc->axe_ec.ec_capabilities = ETHERCAP_VLAN_MTU;

	/* Initialize MII/media info. */
	mii = &sc->axe_mii;
	mii->mii_ifp = ifp;
	mii->mii_readreg = axe_miibus_readreg;
	mii->mii_writereg = axe_miibus_writereg;
	mii->mii_statchg = axe_miibus_statchg;
	mii->mii_flags = MIIF_AUTOTSLEEP;

	sc->axe_ec.ec_mii = mii;
	if (sc->axe_flags & AXE_MII)
		ifmedia_init(&mii->mii_media, 0, axe_ifmedia_upd,
		    axe_ifmedia_sts);
	else
		ifmedia_init(&mii->mii_media, 0, ether_mediachange,
		    ether_mediastatus);

	mii_attach(sc->axe_dev, mii, 0xffffffff, MII_PHY_ANY, MII_OFFSET_ANY,
	    0);

	if (LIST_EMPTY(&mii->mii_phys)) {
		ifmedia_add(&mii->mii_media, IFM_ETHER | IFM_NONE, 0, NULL);
		ifmedia_set(&mii->mii_media, IFM_ETHER | IFM_NONE);
	} else
		ifmedia_set(&mii->mii_media, IFM_ETHER | IFM_AUTO);

	/* Attach the interface. */
	if_attach(ifp);
	ether_ifattach(ifp, eaddr);
	rnd_attach_source(&sc->rnd_source, device_xname(sc->axe_dev),
	    RND_TYPE_NET, RND_FLAG_DEFAULT);

	callout_init(&sc->axe_stat_ch, 0);
	callout_setfunc(&sc->axe_stat_ch, axe_tick, sc);

	sc->axe_attached = true;
	splx(s);

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->axe_udev, sc->axe_dev);

	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");
}

int
axe_detach(device_t self, int flags)
{
	struct axe_softc *sc = device_private(self);
	int s;
	struct ifnet *ifp = &sc->sc_if;

	DPRINTFN(2,("%s: %s: enter\n", device_xname(sc->axe_dev), __func__));

	/* Detached before attached finished, so just bail out. */
	if (!sc->axe_attached)
		return 0;

	pmf_device_deregister(self);

	sc->axe_dying = true;

	/*
	 * Remove any pending tasks.  They cannot be executing because they run
	 * in the same thread as detach.
	 */
	usb_rem_task(sc->axe_udev, &sc->axe_tick_task);

	s = splusb();

	if (ifp->if_flags & IFF_RUNNING)
		axe_stop(ifp, 1);

	callout_destroy(&sc->axe_stat_ch);
	mutex_destroy(&sc->axe_mii_lock);
	rnd_detach_source(&sc->rnd_source);
	mii_detach(&sc->axe_mii, MII_PHY_ANY, MII_OFFSET_ANY);
	ifmedia_delete_instance(&sc->axe_mii.mii_media, IFM_INST_ANY);
	ether_ifdetach(ifp);
	if_detach(ifp);

#ifdef DIAGNOSTIC
	if (sc->axe_ep[AXE_ENDPT_TX] != NULL ||
	    sc->axe_ep[AXE_ENDPT_RX] != NULL ||
	    sc->axe_ep[AXE_ENDPT_INTR] != NULL)
		aprint_debug_dev(self, "detach has active endpoints\n");
#endif

	sc->axe_attached = false;

	if (--sc->axe_refcnt >= 0) {
		/* Wait for processes to go away. */
		usb_detach_waitold(sc->axe_dev);
	}
	splx(s);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->axe_udev, sc->axe_dev);

	return 0;
}

int
axe_activate(device_t self, devact_t act)
{
	struct axe_softc *sc = device_private(self);

	DPRINTFN(2,("%s: %s: enter\n", device_xname(sc->axe_dev), __func__));

	switch (act) {
	case DVACT_DEACTIVATE:
		if_deactivate(&sc->axe_ec.ec_if);
		sc->axe_dying = true;
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

static int
axe_rx_list_init(struct axe_softc *sc)
{
	struct axe_cdata *cd;
	struct axe_chain *c;
	int i;

	DPRINTF(("%s: %s: enter\n", device_xname(sc->axe_dev), __func__));

	cd = &sc->axe_cdata;
	for (i = 0; i < AXE_RX_LIST_CNT; i++) {
		c = &cd->axe_rx_chain[i];
		c->axe_sc = sc;
		c->axe_idx = i;
		if (c->axe_xfer == NULL) {
			c->axe_xfer = usbd_alloc_xfer(sc->axe_udev);
			if (c->axe_xfer == NULL)
				return ENOBUFS;
			c->axe_buf = usbd_alloc_buffer(c->axe_xfer,
			    sc->axe_bufsz);
			if (c->axe_buf == NULL) {
				usbd_free_xfer(c->axe_xfer);
				return ENOBUFS;
			}
		}
	}

	return 0;
}

static int
axe_tx_list_init(struct axe_softc *sc)
{
	struct axe_cdata *cd;
	struct axe_chain *c;
	int i;

	DPRINTF(("%s: %s: enter\n", device_xname(sc->axe_dev), __func__));

	cd = &sc->axe_cdata;
	for (i = 0; i < AXE_TX_LIST_CNT; i++) {
		c = &cd->axe_tx_chain[i];
		c->axe_sc = sc;
		c->axe_idx = i;
		if (c->axe_xfer == NULL) {
			c->axe_xfer = usbd_alloc_xfer(sc->axe_udev);
			if (c->axe_xfer == NULL)
				return ENOBUFS;
			c->axe_buf = usbd_alloc_buffer(c->axe_xfer,
			    sc->axe_bufsz);
			if (c->axe_buf == NULL) {
				usbd_free_xfer(c->axe_xfer);
				return ENOBUFS;
			}
		}
	}

	return 0;
}

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 */
static void
axe_rxeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct axe_softc *sc;
	struct axe_chain *c;
	struct ifnet *ifp;
	uint8_t *buf;
	uint32_t total_len;
	u_int rxlen, pktlen;
	struct mbuf *m;
	struct axe_sframe_hdr hdr;
	int s;

	c = (struct axe_chain *)priv;
	sc = c->axe_sc;
	buf = c->axe_buf;
	ifp = &sc->sc_if;

	DPRINTFN(10,("%s: %s: enter\n", device_xname(sc->axe_dev),__func__));

	if (sc->axe_dying)
		return;

	if ((ifp->if_flags & IFF_RUNNING) == 0)
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;
		if (usbd_ratecheck(&sc->axe_rx_notice))
			aprint_error_dev(sc->axe_dev, "usb errors on rx: %s\n",
			    usbd_errstr(status));
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->axe_ep[AXE_ENDPT_RX]);
		goto done;
	}

	usbd_get_xfer_status(xfer, NULL, NULL, &total_len, NULL);

	do {
		if (sc->axe_flags & AX178 || sc->axe_flags & AX772) {
			if (total_len < sizeof(hdr)) {
				ifp->if_ierrors++;
				goto done;
			}

			memcpy(&hdr, buf, sizeof(hdr));
			total_len -= sizeof(hdr);
			buf += sizeof(hdr);

			if (((le16toh(hdr.len) & AXE_RH1M_RXLEN_MASK) ^
			    (le16toh(hdr.ilen) & AXE_RH1M_RXLEN_MASK)) !=
			    AXE_RH1M_RXLEN_MASK) {
				ifp->if_ierrors++;
				goto done;
			}

			rxlen = le16toh(hdr.len) & AXE_RH1M_RXLEN_MASK;
			if (total_len < rxlen) {
				pktlen = total_len;
				total_len = 0;
			} else {
				pktlen = rxlen;
				rxlen = roundup2(rxlen, 2);
				total_len -= rxlen;
			}

		} else { /* AX172 */
			pktlen = rxlen = total_len;
			total_len = 0;
		}

		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == NULL) {
			ifp->if_ierrors++;
			goto done;
		}

		if (pktlen > MHLEN - ETHER_ALIGN) {
			MCLGET(m, M_DONTWAIT);
			if ((m->m_flags & M_EXT) == 0) {
				m_freem(m);
				ifp->if_ierrors++;
				goto done;
			}
		}
		m->m_data += ETHER_ALIGN;

		ifp->if_ipackets++;
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = pktlen;

		memcpy(mtod(m, uint8_t *), buf, pktlen);
		buf += rxlen;

		s = splnet();

		bpf_mtap(ifp, m);

		DPRINTFN(10,("%s: %s: deliver %d\n", device_xname(sc->axe_dev),
		    __func__, m->m_len));
		(*(ifp)->if_input)((ifp), (m));

		splx(s);

	} while (total_len > 0);

 done:

	/* Setup new transfer. */
	usbd_setup_xfer(xfer, sc->axe_ep[AXE_ENDPT_RX],
	    c, c->axe_buf, sc->axe_bufsz,
	    USBD_SHORT_XFER_OK | USBD_NO_COPY,
	    USBD_NO_TIMEOUT, axe_rxeof);
	usbd_transfer(xfer);

	DPRINTFN(10,("%s: %s: start rx\n", device_xname(sc->axe_dev), __func__));
}

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */

static void
axe_txeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct axe_softc *sc;
	struct axe_chain *c;
	struct ifnet *ifp;
	int s;

	c = priv;
	sc = c->axe_sc;
	ifp = &sc->sc_if;

	if (sc->axe_dying)
		return;

	s = splnet();

	ifp->if_timer = 0;
	ifp->if_flags &= ~IFF_OACTIVE;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED) {
			splx(s);
			return;
		}
		ifp->if_oerrors++;
		aprint_error_dev(sc->axe_dev, "usb error on tx: %s\n",
		    usbd_errstr(status));
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->axe_ep[AXE_ENDPT_TX]);
		splx(s);
		return;
	}
	ifp->if_opackets++;

	if (!IFQ_IS_EMPTY(&ifp->if_snd))
		axe_start(ifp);

	splx(s);
}

static void
axe_tick(void *xsc)
{
	struct axe_softc *sc = xsc;

	if (sc == NULL)
		return;

	DPRINTFN(0xff, ("%s: %s: enter\n", device_xname(sc->axe_dev), __func__));

	if (sc->axe_dying)
		return;

	/* Perform periodic stuff in process context */
	usb_add_task(sc->axe_udev, &sc->axe_tick_task, USB_TASKQ_DRIVER);
}

static void
axe_tick_task(void *xsc)
{
	int s;
	struct axe_softc *sc;
	struct ifnet *ifp;
	struct mii_data *mii;

	sc = xsc;

	if (sc == NULL)
		return;

	if (sc->axe_dying)
		return;

	ifp = &sc->sc_if;
	mii = &sc->axe_mii;

	if (mii == NULL)
		return;

	s = splnet();

	mii_tick(mii);
	if (sc->axe_link == 0 &&
	    (mii->mii_media_status & IFM_ACTIVE) != 0 &&
	    IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE) {
		DPRINTF(("%s: %s: got link\n", device_xname(sc->axe_dev),
		    __func__));
		sc->axe_link++;
		if (!IFQ_IS_EMPTY(&ifp->if_snd))
			axe_start(ifp);
	}

	callout_schedule(&sc->axe_stat_ch, hz);

	splx(s);
}

static int
axe_encap(struct axe_softc *sc, struct mbuf *m, int idx)
{
	struct ifnet *ifp = &sc->sc_if;
	struct axe_chain *c;
	usbd_status err;
	struct axe_sframe_hdr hdr;
	int length, boundary;

	c = &sc->axe_cdata.axe_tx_chain[idx];

	/*
	 * Copy the mbuf data into a contiguous buffer, leaving two
	 * bytes at the beginning to hold the frame length.
	 */
	if (sc->axe_flags & AX178 || sc->axe_flags & AX772) {
		boundary = (sc->axe_udev->speed == USB_SPEED_HIGH) ? 512 : 64;

		hdr.len = htole16(m->m_pkthdr.len);
		hdr.ilen = ~hdr.len;

		memcpy(c->axe_buf, &hdr, sizeof(hdr));
		length = sizeof(hdr);

		m_copydata(m, 0, m->m_pkthdr.len, c->axe_buf + length);
		length += m->m_pkthdr.len;

		if ((length % boundary) == 0) {
			hdr.len = 0x0000;
			hdr.ilen = 0xffff;
			memcpy(c->axe_buf + length, &hdr, sizeof(hdr));
			length += sizeof(hdr);
		}
	} else {
		m_copydata(m, 0, m->m_pkthdr.len, c->axe_buf);
		length = m->m_pkthdr.len;
	}

	usbd_setup_xfer(c->axe_xfer, sc->axe_ep[AXE_ENDPT_TX],
	    c, c->axe_buf, length, USBD_FORCE_SHORT_XFER | USBD_NO_COPY, 10000,
	    axe_txeof);

	/* Transmit */
	err = usbd_transfer(c->axe_xfer);
	if (err != USBD_IN_PROGRESS) {
		axe_stop(ifp, 0);
		return EIO;
	}

	sc->axe_cdata.axe_tx_cnt++;

	return 0;
}

static void
axe_start(struct ifnet *ifp)
{
	struct axe_softc *sc;
	struct mbuf *m;

	sc = ifp->if_softc;

	if ((sc->axe_flags & AXE_MII) != 0 && sc->axe_link == 0)
		return;

	if ((ifp->if_flags & (IFF_OACTIVE|IFF_RUNNING)) != IFF_RUNNING)
		return;

	IFQ_POLL(&ifp->if_snd, m);
	if (m == NULL) {
		return;
	}

	if (axe_encap(sc, m, 0)) {
		ifp->if_flags |= IFF_OACTIVE;
		return;
	}
	IFQ_DEQUEUE(&ifp->if_snd, m);

	/*
	 * If there's a BPF listener, bounce a copy of this frame
	 * to him.
	 */
	bpf_mtap(ifp, m);
	m_freem(m);

	ifp->if_flags |= IFF_OACTIVE;

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;

	return;
}

static int
axe_init(struct ifnet *ifp)
{
	struct axe_softc *sc = ifp->if_softc;
	struct axe_chain *c;
	usbd_status err;
	int rxmode;
	int i, s;
	uint8_t eaddr[ETHER_ADDR_LEN];

	s = splnet();

	if (ifp->if_flags & IFF_RUNNING)
		axe_stop(ifp, 0);

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	axe_reset(sc);

	/* Set MAC address */
	if (sc->axe_flags & AX178 || sc->axe_flags & AX772) {
		memcpy(eaddr, CLLADDR(ifp->if_sadl), sizeof(eaddr));
		axe_lock_mii(sc);
		axe_cmd(sc, AXE_178_CMD_WRITE_NODEID, 0, 0, eaddr);
		axe_unlock_mii(sc);
	}

	/* Enable RX logic. */

	/* Init RX ring. */
	if (axe_rx_list_init(sc) == ENOBUFS) {
		aprint_error_dev(sc->axe_dev, "rx list init failed\n");
		splx(s);
		return ENOBUFS;
	}

	/* Init TX ring. */
	if (axe_tx_list_init(sc) == ENOBUFS) {
		aprint_error_dev(sc->axe_dev, "tx list init failed\n");
		splx(s);
		return ENOBUFS;
	}

	/* Set transmitter IPG values */
	axe_lock_mii(sc);
	if (sc->axe_flags & AX178 || sc->axe_flags & AX772)
		axe_cmd(sc, AXE_178_CMD_WRITE_IPG012, sc->axe_ipgs[2],
		    (sc->axe_ipgs[1] << 8) | (sc->axe_ipgs[0]), NULL);
	else {
		axe_cmd(sc, AXE_172_CMD_WRITE_IPG0, 0, sc->axe_ipgs[0], NULL);
		axe_cmd(sc, AXE_172_CMD_WRITE_IPG1, 0, sc->axe_ipgs[1], NULL);
		axe_cmd(sc, AXE_172_CMD_WRITE_IPG2, 0, sc->axe_ipgs[2], NULL);
	}

	/* Enable receiver, set RX mode */
	rxmode = AXE_RXCMD_BROADCAST | AXE_RXCMD_MULTICAST | AXE_RXCMD_ENABLE;
	if (sc->axe_flags & AX772B)
		rxmode |= AXE_772B_RXCMD_RH1M;
	else if (sc->axe_flags & AX178 || sc->axe_flags & AX772) {
		if (sc->axe_udev->speed == USB_SPEED_HIGH) {
			/* Largest possible USB buffer size for AX88178 */
			rxmode |= AXE_178_RXCMD_MFB;
		}
	} else
		rxmode |= AXE_172_RXCMD_UNICAST;

	/* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC)
		rxmode |= AXE_RXCMD_PROMISC;

	if (ifp->if_flags & IFF_BROADCAST)
		rxmode |= AXE_RXCMD_BROADCAST;

	axe_cmd(sc, AXE_CMD_RXCTL_WRITE, 0, rxmode, NULL);
	axe_unlock_mii(sc);

	/* Load the multicast filter. */
	axe_setmulti(sc);

	/* Open RX and TX pipes. */
	err = usbd_open_pipe(sc->axe_iface, sc->axe_ed[AXE_ENDPT_RX],
	    USBD_EXCLUSIVE_USE, &sc->axe_ep[AXE_ENDPT_RX]);
	if (err) {
		aprint_error_dev(sc->axe_dev, "open rx pipe failed: %s\n",
		    usbd_errstr(err));
		splx(s);
		return EIO;
	}

	err = usbd_open_pipe(sc->axe_iface, sc->axe_ed[AXE_ENDPT_TX],
	    USBD_EXCLUSIVE_USE, &sc->axe_ep[AXE_ENDPT_TX]);
	if (err) {
		aprint_error_dev(sc->axe_dev, "open tx pipe failed: %s\n",
		    usbd_errstr(err));
		splx(s);
		return EIO;
	}

	/* Start up the receive pipe. */
	for (i = 0; i < AXE_RX_LIST_CNT; i++) {
		c = &sc->axe_cdata.axe_rx_chain[i];
		usbd_setup_xfer(c->axe_xfer, sc->axe_ep[AXE_ENDPT_RX],
		    c, c->axe_buf, sc->axe_bufsz,
		    USBD_SHORT_XFER_OK | USBD_NO_COPY, USBD_NO_TIMEOUT,
		    axe_rxeof);
		usbd_transfer(c->axe_xfer);
	}

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	splx(s);

	callout_schedule(&sc->axe_stat_ch, hz);
	return 0;
}

static int
axe_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct axe_softc *sc = ifp->if_softc;
	int s;
	int error = 0;

	s = splnet();

	switch(cmd) {
	case SIOCSIFFLAGS:
		if ((error = ifioctl_common(ifp, cmd, data)) != 0)
			break;

		switch (ifp->if_flags & (IFF_UP | IFF_RUNNING)) {
		case IFF_RUNNING:
			axe_stop(ifp, 1);
			break;
		case IFF_UP:
			axe_init(ifp);
			break;
		case IFF_UP | IFF_RUNNING:
			if ((ifp->if_flags ^ sc->axe_if_flags) == IFF_PROMISC)
				axe_setmulti(sc);
			else
				axe_init(ifp);
			break;
		}
		sc->axe_if_flags = ifp->if_flags;
		break;

	default:
		if ((error = ether_ioctl(ifp, cmd, data)) != ENETRESET)
			break;

		error = 0;

		if (cmd == SIOCADDMULTI || cmd == SIOCDELMULTI)
			axe_setmulti(sc);

	}
	splx(s);

	return error;
}

static void
axe_watchdog(struct ifnet *ifp)
{
	struct axe_softc *sc;
	struct axe_chain *c;
	usbd_status stat;
	int s;

	sc = ifp->if_softc;

	ifp->if_oerrors++;
	aprint_error_dev(sc->axe_dev, "watchdog timeout\n");

	s = splusb();
	c = &sc->axe_cdata.axe_tx_chain[0];
	usbd_get_xfer_status(c->axe_xfer, NULL, NULL, NULL, &stat);
	axe_txeof(c->axe_xfer, c, stat);

	if (!IFQ_IS_EMPTY(&ifp->if_snd))
		axe_start(ifp);
	splx(s);
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
static void
axe_stop(struct ifnet *ifp, int disable)
{
	struct axe_softc *sc = ifp->if_softc;
	usbd_status err;
	int i;

	axe_reset(sc);

	ifp->if_timer = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	callout_stop(&sc->axe_stat_ch);

	/* Stop transfers. */
	if (sc->axe_ep[AXE_ENDPT_RX] != NULL) {
		err = usbd_abort_pipe(sc->axe_ep[AXE_ENDPT_RX]);
		if (err) {
			aprint_error_dev(sc->axe_dev,
			    "abort rx pipe failed: %s\n", usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->axe_ep[AXE_ENDPT_RX]);
		if (err) {
			aprint_error_dev(sc->axe_dev,
			    "close rx pipe failed: %s\n", usbd_errstr(err));
		}
		sc->axe_ep[AXE_ENDPT_RX] = NULL;
	}

	if (sc->axe_ep[AXE_ENDPT_TX] != NULL) {
		err = usbd_abort_pipe(sc->axe_ep[AXE_ENDPT_TX]);
		if (err) {
			aprint_error_dev(sc->axe_dev,
			    "abort tx pipe failed: %s\n", usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->axe_ep[AXE_ENDPT_TX]);
		if (err) {
			aprint_error_dev(sc->axe_dev,
			    "close tx pipe failed: %s\n", usbd_errstr(err));
		}
		sc->axe_ep[AXE_ENDPT_TX] = NULL;
	}

	if (sc->axe_ep[AXE_ENDPT_INTR] != NULL) {
		err = usbd_abort_pipe(sc->axe_ep[AXE_ENDPT_INTR]);
		if (err) {
			aprint_error_dev(sc->axe_dev,
			    "abort intr pipe failed: %s\n", usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->axe_ep[AXE_ENDPT_INTR]);
		if (err) {
			aprint_error_dev(sc->axe_dev,
			    "close intr pipe failed: %s\n", usbd_errstr(err));
		}
		sc->axe_ep[AXE_ENDPT_INTR] = NULL;
	}

	/* Free RX resources. */
	for (i = 0; i < AXE_RX_LIST_CNT; i++) {
		if (sc->axe_cdata.axe_rx_chain[i].axe_xfer != NULL) {
			usbd_free_xfer(sc->axe_cdata.axe_rx_chain[i].axe_xfer);
			sc->axe_cdata.axe_rx_chain[i].axe_xfer = NULL;
		}
	}

	/* Free TX resources. */
	for (i = 0; i < AXE_TX_LIST_CNT; i++) {
		if (sc->axe_cdata.axe_tx_chain[i].axe_xfer != NULL) {
			usbd_free_xfer(sc->axe_cdata.axe_tx_chain[i].axe_xfer);
			sc->axe_cdata.axe_tx_chain[i].axe_xfer = NULL;
		}
	}

	sc->axe_link = 0;
}

MODULE(MODULE_CLASS_DRIVER, if_axe, "bpf");

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
if_axe_modcmd(modcmd_t cmd, void *aux)
{
	int error = 0;

	switch (cmd) {
	case MODULE_CMD_INIT:
#ifdef _MODULE
		error = config_init_component(cfdriver_ioconf_axe,
		    cfattach_ioconf_axe, cfdata_ioconf_axe);
#endif
		return error;
	case MODULE_CMD_FINI:
#ifdef _MODULE
		error = config_fini_component(cfdriver_ioconf_axe,
		    cfattach_ioconf_axe, cfdata_ioconf_axe);
#endif
		return error;
	default:
		return ENOTTY;
	}
}
