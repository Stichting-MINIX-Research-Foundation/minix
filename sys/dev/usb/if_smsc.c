/*	$NetBSD: if_smsc.c,v 1.24 2015/08/02 11:55:28 mlelstv Exp $	*/

/*	$OpenBSD: if_smsc.c,v 1.4 2012/09/27 12:38:11 jsg Exp $	*/
/* $FreeBSD: src/sys/dev/usb/net/if_smsc.c,v 1.1 2012/08/15 04:03:55 gonzo Exp $ */
/*-
 * Copyright (c) 2012
 *	Ben Gray <bgray@freebsd.org>.
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
 * SMSC LAN9xxx devices (http://www.smsc.com/)
 *
 * The LAN9500 & LAN9500A devices are stand-alone USB to Ethernet chips that
 * support USB 2.0 and 10/100 Mbps Ethernet.
 *
 * The LAN951x devices are an integrated USB hub and USB to Ethernet adapter.
 * The driver only covers the Ethernet part, the standard USB hub driver
 * supports the hub part.
 *
 * This driver is closely modelled on the Linux driver written and copyrighted
 * by SMSC.
 *
 * H/W TCP & UDP Checksum Offloading
 * ---------------------------------
 * The chip supports both tx and rx offloading of UDP & TCP checksums, this
 * feature can be dynamically enabled/disabled.
 *
 * RX checksuming is performed across bytes after the IPv4 header to the end of
 * the Ethernet frame, this means if the frame is padded with non-zero values
 * the H/W checksum will be incorrect, however the rx code compensates for this.
 *
 * TX checksuming is more complicated, the device requires a special header to
 * be prefixed onto the start of the frame which indicates the start and end
 * positions of the UDP or TCP frame.  This requires the driver to manually
 * go through the packet data and decode the headers prior to sending.
 * On Linux they generally provide cues to the location of the csum and the
 * area to calculate it over, on FreeBSD we seem to have to do it all ourselves,
 * hence this is not as optimal and therefore h/w TX checksum is currently not
 * implemented.
 */

#ifdef _KERNEL_OPT
#include "opt_usb.h"
#include "opt_inet.h"
#endif

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/socket.h>

#include <sys/device.h>

#include <sys/rndsource.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_ether.h>

#include <net/bpf.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_inarp.h>
#endif

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usbdevs.h>

#include <dev/usb/if_smscreg.h>
#include <dev/usb/if_smscvar.h>

#include "ioconf.h"

#ifdef USB_DEBUG
int smsc_debug = 0;
#endif

#define ETHER_ALIGN 2
/*
 * Various supported device vendors/products.
 */
static const struct usb_devno smsc_devs[] = {
	{ USB_VENDOR_SMSC,	USB_PRODUCT_SMSC_LAN89530 },
	{ USB_VENDOR_SMSC,	USB_PRODUCT_SMSC_LAN9530 },
	{ USB_VENDOR_SMSC,	USB_PRODUCT_SMSC_LAN9730 },
	{ USB_VENDOR_SMSC,	USB_PRODUCT_SMSC_SMSC9500 },
	{ USB_VENDOR_SMSC,	USB_PRODUCT_SMSC_SMSC9500A },
	{ USB_VENDOR_SMSC,	USB_PRODUCT_SMSC_SMSC9500A_ALT },
	{ USB_VENDOR_SMSC,	USB_PRODUCT_SMSC_SMSC9500A_HAL },
	{ USB_VENDOR_SMSC,	USB_PRODUCT_SMSC_SMSC9500A_SAL10 },
	{ USB_VENDOR_SMSC,	USB_PRODUCT_SMSC_SMSC9500_ALT },
	{ USB_VENDOR_SMSC,	USB_PRODUCT_SMSC_SMSC9500_SAL10 },
	{ USB_VENDOR_SMSC,	USB_PRODUCT_SMSC_SMSC9505 },
	{ USB_VENDOR_SMSC,	USB_PRODUCT_SMSC_SMSC9505A },
	{ USB_VENDOR_SMSC,	USB_PRODUCT_SMSC_SMSC9505A_HAL },
	{ USB_VENDOR_SMSC,	USB_PRODUCT_SMSC_SMSC9505A_SAL10 },
	{ USB_VENDOR_SMSC,	USB_PRODUCT_SMSC_SMSC9505_SAL10 },
	{ USB_VENDOR_SMSC,	USB_PRODUCT_SMSC_SMSC9512_14 },
	{ USB_VENDOR_SMSC,	USB_PRODUCT_SMSC_SMSC9512_14_ALT },
	{ USB_VENDOR_SMSC,	USB_PRODUCT_SMSC_SMSC9512_14_SAL10 }
};

#ifdef USB_DEBUG
#define smsc_dbg_printf(sc, fmt, args...) \
	do { \
		if (smsc_debug > 0) \
			printf("debug: " fmt, ##args); \
	} while(0)
#else
#define smsc_dbg_printf(sc, fmt, args...)
#endif

#define smsc_warn_printf(sc, fmt, args...) \
	printf("%s: warning: " fmt, device_xname((sc)->sc_dev), ##args)

#define smsc_err_printf(sc, fmt, args...) \
	printf("%s: error: " fmt, device_xname((sc)->sc_dev), ##args)

/* Function declarations */
int		 smsc_chip_init(struct smsc_softc *);
void		 smsc_setmulti(struct smsc_softc *);
int		 smsc_setmacaddress(struct smsc_softc *, const uint8_t *);

int		 smsc_match(device_t, cfdata_t, void *);
void		 smsc_attach(device_t, device_t, void *);
int		 smsc_detach(device_t, int);
int		 smsc_activate(device_t, enum devact);

int		 smsc_init(struct ifnet *);
void		 smsc_start(struct ifnet *);
int		 smsc_ioctl(struct ifnet *, u_long, void *);
void		 smsc_stop(struct ifnet *, int);

void		 smsc_reset(struct smsc_softc *);
struct mbuf	*smsc_newbuf(void);

void		 smsc_tick(void *);
void		 smsc_tick_task(void *);
void		 smsc_miibus_statchg(struct ifnet *);
int		 smsc_miibus_readreg(device_t, int, int);
void		 smsc_miibus_writereg(device_t, int, int, int);
int		 smsc_ifmedia_upd(struct ifnet *);
void		 smsc_ifmedia_sts(struct ifnet *, struct ifmediareq *);
void		 smsc_lock_mii(struct smsc_softc *);
void		 smsc_unlock_mii(struct smsc_softc *);

int		 smsc_tx_list_init(struct smsc_softc *);
int		 smsc_rx_list_init(struct smsc_softc *);
int		 smsc_encap(struct smsc_softc *, struct mbuf *, int);
void		 smsc_rxeof(usbd_xfer_handle, usbd_private_handle, usbd_status);
void		 smsc_txeof(usbd_xfer_handle, usbd_private_handle, usbd_status);

int		 smsc_read_reg(struct smsc_softc *, uint32_t, uint32_t *);
int		 smsc_write_reg(struct smsc_softc *, uint32_t, uint32_t);
int		 smsc_wait_for_bits(struct smsc_softc *, uint32_t, uint32_t);
int		 smsc_sethwcsum(struct smsc_softc *);

CFATTACH_DECL_NEW(usmsc, sizeof(struct smsc_softc), smsc_match, smsc_attach,
    smsc_detach, smsc_activate);

int
smsc_read_reg(struct smsc_softc *sc, uint32_t off, uint32_t *data)
{
	usb_device_request_t req;
	uint32_t buf;
	usbd_status err;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = SMSC_UR_READ_REG;
	USETW(req.wValue, 0);
	USETW(req.wIndex, off);
	USETW(req.wLength, 4);

	err = usbd_do_request(sc->sc_udev, &req, &buf);
	if (err != 0)
		smsc_warn_printf(sc, "Failed to read register 0x%0x\n", off);

	*data = le32toh(buf);

	return (err);
}

int
smsc_write_reg(struct smsc_softc *sc, uint32_t off, uint32_t data)
{
	usb_device_request_t req;
	uint32_t buf;
	usbd_status err;

	buf = htole32(data);

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = SMSC_UR_WRITE_REG;
	USETW(req.wValue, 0);
	USETW(req.wIndex, off);
	USETW(req.wLength, 4);

	err = usbd_do_request(sc->sc_udev, &req, &buf);
	if (err != 0)
		smsc_warn_printf(sc, "Failed to write register 0x%0x\n", off);

	return (err);
}

int
smsc_wait_for_bits(struct smsc_softc *sc, uint32_t reg, uint32_t bits)
{
	uint32_t val;
	int err, i;

	for (i = 0; i < 100; i++) {
		if ((err = smsc_read_reg(sc, reg, &val)) != 0)
			return (err);
		if (!(val & bits))
			return (0);
		DELAY(5);
	}

	return (1);
}

int
smsc_miibus_readreg(device_t dev, int phy, int reg)
{
	struct smsc_softc *sc = device_private(dev);
	uint32_t addr;
	uint32_t val = 0;

	smsc_lock_mii(sc);
	if (smsc_wait_for_bits(sc, SMSC_MII_ADDR, SMSC_MII_BUSY) != 0) {
		smsc_warn_printf(sc, "MII is busy\n");
		goto done;
	}

	addr = (phy << 11) | (reg << 6) | SMSC_MII_READ;
	smsc_write_reg(sc, SMSC_MII_ADDR, addr);

	if (smsc_wait_for_bits(sc, SMSC_MII_ADDR, SMSC_MII_BUSY) != 0)
		smsc_warn_printf(sc, "MII read timeout\n");

	smsc_read_reg(sc, SMSC_MII_DATA, &val);

done:
	smsc_unlock_mii(sc);

	return (val & 0xFFFF);
}

void
smsc_miibus_writereg(device_t dev, int phy, int reg, int val)
{
	struct smsc_softc *sc = device_private(dev);
	uint32_t addr;

	if (sc->sc_phyno != phy)
		return;

	smsc_lock_mii(sc);
	if (smsc_wait_for_bits(sc, SMSC_MII_ADDR, SMSC_MII_BUSY) != 0) {
		smsc_warn_printf(sc, "MII is busy\n");
		smsc_unlock_mii(sc);
		return;
	}

	smsc_write_reg(sc, SMSC_MII_DATA, val);

	addr = (phy << 11) | (reg << 6) | SMSC_MII_WRITE;
	smsc_write_reg(sc, SMSC_MII_ADDR, addr);
	smsc_unlock_mii(sc);

	if (smsc_wait_for_bits(sc, SMSC_MII_ADDR, SMSC_MII_BUSY) != 0)
		smsc_warn_printf(sc, "MII write timeout\n");
}

void
smsc_miibus_statchg(struct ifnet *ifp)
{
	struct smsc_softc *sc = ifp->if_softc;
	struct mii_data *mii = &sc->sc_mii;
	int err;
	uint32_t flow;
	uint32_t afc_cfg;

	if (mii == NULL || ifp == NULL ||
	    (ifp->if_flags & IFF_RUNNING) == 0)
		return;

	/* Use the MII status to determine link status */
	sc->sc_flags &= ~SMSC_FLAG_LINK;
	if ((mii->mii_media_status & (IFM_ACTIVE | IFM_AVALID)) ==
	    (IFM_ACTIVE | IFM_AVALID)) {
		switch (IFM_SUBTYPE(mii->mii_media_active)) {
			case IFM_10_T:
			case IFM_100_TX:
				sc->sc_flags |= SMSC_FLAG_LINK;
				break;
			case IFM_1000_T:
				/* Gigabit ethernet not supported by chipset */
				break;
			default:
				break;
		}
	}

	/* Lost link, do nothing. */
	if ((sc->sc_flags & SMSC_FLAG_LINK) == 0) {
		smsc_dbg_printf(sc, "link flag not set\n");
		return;
	}

	err = smsc_read_reg(sc, SMSC_AFC_CFG, &afc_cfg);
	if (err) {
		smsc_warn_printf(sc, "failed to read initial AFC_CFG, "
		    "error %d\n", err);
		return;
	}

	/* Enable/disable full duplex operation and TX/RX pause */
	if ((IFM_OPTIONS(mii->mii_media_active) & IFM_FDX) != 0) {
		smsc_dbg_printf(sc, "full duplex operation\n");
		sc->sc_mac_csr &= ~SMSC_MAC_CSR_RCVOWN;
		sc->sc_mac_csr |= SMSC_MAC_CSR_FDPX;

		if ((IFM_OPTIONS(mii->mii_media_active) & IFM_ETH_RXPAUSE) != 0)
			flow = 0xffff0002;
		else
			flow = 0;

		if ((IFM_OPTIONS(mii->mii_media_active) & IFM_ETH_TXPAUSE) != 0)
			afc_cfg |= 0xf;
		else
			afc_cfg &= ~0xf;

	} else {
		smsc_dbg_printf(sc, "half duplex operation\n");
		sc->sc_mac_csr &= ~SMSC_MAC_CSR_FDPX;
		sc->sc_mac_csr |= SMSC_MAC_CSR_RCVOWN;

		flow = 0;
		afc_cfg |= 0xf;
	}

	err = smsc_write_reg(sc, SMSC_MAC_CSR, sc->sc_mac_csr);
	err += smsc_write_reg(sc, SMSC_FLOW, flow);
	err += smsc_write_reg(sc, SMSC_AFC_CFG, afc_cfg);
	if (err)
		smsc_warn_printf(sc, "media change failed, error %d\n", err);
}

int
smsc_ifmedia_upd(struct ifnet *ifp)
{
	struct smsc_softc *sc = ifp->if_softc;
	struct mii_data *mii = &sc->sc_mii;
	int err;

	if (mii->mii_instance) {
		struct mii_softc *miisc;

		LIST_FOREACH(miisc, &mii->mii_phys, mii_list)
			mii_phy_reset(miisc);
	}
	err = mii_mediachg(mii);
	return (err);
}

void
smsc_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct smsc_softc *sc = ifp->if_softc;
	struct mii_data *mii = &sc->sc_mii;

	mii_pollstat(mii);

	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
}

static inline uint32_t
smsc_hash(uint8_t addr[ETHER_ADDR_LEN])
{
	return (ether_crc32_be(addr, ETHER_ADDR_LEN) >> 26) & 0x3f;
}

void
smsc_setmulti(struct smsc_softc *sc)
{
	struct ifnet		*ifp = &sc->sc_ec.ec_if;
	struct ether_multi	*enm;
	struct ether_multistep	 step;
	uint32_t		 hashtbl[2] = { 0, 0 };
	uint32_t		 hash;

	if (sc->sc_dying)
		return;

	if (ifp->if_flags & (IFF_ALLMULTI | IFF_PROMISC)) {
allmulti:
		smsc_dbg_printf(sc, "receive all multicast enabled\n");
		sc->sc_mac_csr |= SMSC_MAC_CSR_MCPAS;
		sc->sc_mac_csr &= ~SMSC_MAC_CSR_HPFILT;
		smsc_write_reg(sc, SMSC_MAC_CSR, sc->sc_mac_csr);
		return;
	} else {
		sc->sc_mac_csr |= SMSC_MAC_CSR_HPFILT;
		sc->sc_mac_csr &= ~(SMSC_MAC_CSR_PRMS | SMSC_MAC_CSR_MCPAS);
	}

	ETHER_FIRST_MULTI(step, &sc->sc_ec, enm);
	while (enm != NULL) {
		if (memcmp(enm->enm_addrlo, enm->enm_addrhi,
		    ETHER_ADDR_LEN) != 0)
			goto allmulti;

		hash = smsc_hash(enm->enm_addrlo);
		hashtbl[hash >> 5] |= 1 << (hash & 0x1F);
		ETHER_NEXT_MULTI(step, enm);
	}

	/* Debug */
	if (sc->sc_mac_csr & SMSC_MAC_CSR_HPFILT) {
		smsc_dbg_printf(sc, "receive select group of macs\n");
	} else {
		smsc_dbg_printf(sc, "receive own packets only\n");
	}

	/* Write the hash table and mac control registers */
	ifp->if_flags &= ~IFF_ALLMULTI;
	smsc_write_reg(sc, SMSC_HASHH, hashtbl[1]);
	smsc_write_reg(sc, SMSC_HASHL, hashtbl[0]);
	smsc_write_reg(sc, SMSC_MAC_CSR, sc->sc_mac_csr);
}

int
smsc_sethwcsum(struct smsc_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	uint32_t val;
	int err;

	if (!ifp)
		return EIO;

	err = smsc_read_reg(sc, SMSC_COE_CTRL, &val);
	if (err != 0) {
		smsc_warn_printf(sc, "failed to read SMSC_COE_CTRL (err=%d)\n",
		    err);
		return (err);
	}

	/* Enable/disable the Rx checksum */
	if (ifp->if_capenable & (IFCAP_CSUM_TCPv4_Rx|IFCAP_CSUM_UDPv4_Rx))
		val |= (SMSC_COE_CTRL_RX_EN | SMSC_COE_CTRL_RX_MODE);
	else
		val &= ~(SMSC_COE_CTRL_RX_EN | SMSC_COE_CTRL_RX_MODE);

	/* Enable/disable the Tx checksum (currently not supported) */
	if (ifp->if_capenable & (IFCAP_CSUM_TCPv4_Tx|IFCAP_CSUM_UDPv4_Tx))
		val |= SMSC_COE_CTRL_TX_EN;
	else
		val &= ~SMSC_COE_CTRL_TX_EN;

	sc->sc_coe_ctrl = val;

	err = smsc_write_reg(sc, SMSC_COE_CTRL, val);
	if (err != 0) {
		smsc_warn_printf(sc, "failed to write SMSC_COE_CTRL (err=%d)\n",
		    err);
		return (err);
	}

	return (0);
}

int
smsc_setmacaddress(struct smsc_softc *sc, const uint8_t *addr)
{
	int err;
	uint32_t val;

	smsc_dbg_printf(sc, "setting mac address to "
	    "%02x:%02x:%02x:%02x:%02x:%02x\n",
	    addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

	val = (addr[3] << 24) | (addr[2] << 16) | (addr[1] << 8) | addr[0];
	if ((err = smsc_write_reg(sc, SMSC_MAC_ADDRL, val)) != 0)
		goto done;

	val = (addr[5] << 8) | addr[4];
	err = smsc_write_reg(sc, SMSC_MAC_ADDRH, val);

done:
	return (err);
}

void
smsc_reset(struct smsc_softc *sc)
{
	if (sc->sc_dying)
		return;

	/* Wait a little while for the chip to get its brains in order. */
	DELAY(1000);

	/* Reinitialize controller to achieve full reset. */
	smsc_chip_init(sc);
}

int
smsc_init(struct ifnet *ifp)
{
	struct smsc_softc	*sc = ifp->if_softc;
	struct smsc_chain	*c;
	usbd_status		 err;
	int			 s, i;

	if (sc->sc_dying)
		return EIO;

	s = splnet();

	/* Cancel pending I/O */
	if (ifp->if_flags & IFF_RUNNING)
		smsc_stop(ifp, 1);

	/* Reset the ethernet interface. */
	smsc_reset(sc);

	/* Init RX ring. */
	if (smsc_rx_list_init(sc) == ENOBUFS) {
		aprint_error_dev(sc->sc_dev, "rx list init failed\n");
		splx(s);
		return EIO;
	}

	/* Init TX ring. */
	if (smsc_tx_list_init(sc) == ENOBUFS) {
		aprint_error_dev(sc->sc_dev, "tx list init failed\n");
		splx(s);
		return EIO;
	}

	/* Load the multicast filter. */
	smsc_setmulti(sc);

	/* TCP/UDP checksum offload engines. */
	smsc_sethwcsum(sc);

	/* Open RX and TX pipes. */
	err = usbd_open_pipe(sc->sc_iface, sc->sc_ed[SMSC_ENDPT_RX],
	    USBD_EXCLUSIVE_USE, &sc->sc_ep[SMSC_ENDPT_RX]);
	if (err) {
		printf("%s: open rx pipe failed: %s\n",
		    device_xname(sc->sc_dev), usbd_errstr(err));
		splx(s);
		return EIO;
	}

	err = usbd_open_pipe(sc->sc_iface, sc->sc_ed[SMSC_ENDPT_TX],
	    USBD_EXCLUSIVE_USE, &sc->sc_ep[SMSC_ENDPT_TX]);
	if (err) {
		printf("%s: open tx pipe failed: %s\n",
		    device_xname(sc->sc_dev), usbd_errstr(err));
		splx(s);
		return EIO;
	}

	/* Start up the receive pipe. */
	for (i = 0; i < SMSC_RX_LIST_CNT; i++) {
		c = &sc->sc_cdata.rx_chain[i];
		usbd_setup_xfer(c->sc_xfer, sc->sc_ep[SMSC_ENDPT_RX],
		    c, c->sc_buf, sc->sc_bufsz,
		    USBD_SHORT_XFER_OK | USBD_NO_COPY,
		    USBD_NO_TIMEOUT, smsc_rxeof);
		usbd_transfer(c->sc_xfer);
	}

	/* Indicate we are up and running. */
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	splx(s);

	callout_reset(&sc->sc_stat_ch, hz, smsc_tick, sc);

	return 0;
}

void
smsc_start(struct ifnet *ifp)
{
	struct smsc_softc	*sc = ifp->if_softc;
	struct mbuf		*m_head = NULL;

	/* Don't send anything if there is no link or controller is busy. */
	if ((sc->sc_flags & SMSC_FLAG_LINK) == 0) {
		return;
	}

	if ((ifp->if_flags & (IFF_OACTIVE|IFF_RUNNING)) != IFF_RUNNING)
		return;

	IFQ_POLL(&ifp->if_snd, m_head);
	if (m_head == NULL)
		return;

	if (smsc_encap(sc, m_head, 0)) {
		ifp->if_flags |= IFF_OACTIVE;
		return;
	}
	IFQ_DEQUEUE(&ifp->if_snd, m_head);

	bpf_mtap(ifp, m_head);

	ifp->if_flags |= IFF_OACTIVE;

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;
}

void
smsc_tick(void *xsc)
{
	struct smsc_softc *sc = xsc;

	if (sc == NULL)
		return;

	if (sc->sc_dying)
		return;

	usb_add_task(sc->sc_udev, &sc->sc_tick_task, USB_TASKQ_DRIVER);
}

void
smsc_stop(struct ifnet *ifp, int disable)
{
	usbd_status		err;
	struct smsc_softc	*sc = ifp->if_softc;
	int			i;

	smsc_reset(sc);

	ifp = &sc->sc_ec.ec_if;
	ifp->if_timer = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	callout_stop(&sc->sc_stat_ch);

	/* Stop transfers. */
	if (sc->sc_ep[SMSC_ENDPT_RX] != NULL) {
		err = usbd_abort_pipe(sc->sc_ep[SMSC_ENDPT_RX]);
		if (err) {
			printf("%s: abort rx pipe failed: %s\n",
			    device_xname(sc->sc_dev), usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->sc_ep[SMSC_ENDPT_RX]);
		if (err) {
			printf("%s: close rx pipe failed: %s\n",
			    device_xname(sc->sc_dev), usbd_errstr(err));
		}
		sc->sc_ep[SMSC_ENDPT_RX] = NULL;
	}

	if (sc->sc_ep[SMSC_ENDPT_TX] != NULL) {
		err = usbd_abort_pipe(sc->sc_ep[SMSC_ENDPT_TX]);
		if (err) {
			printf("%s: abort tx pipe failed: %s\n",
			    device_xname(sc->sc_dev), usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->sc_ep[SMSC_ENDPT_TX]);
		if (err) {
			printf("%s: close tx pipe failed: %s\n",
			    device_xname(sc->sc_dev), usbd_errstr(err));
		}
		sc->sc_ep[SMSC_ENDPT_TX] = NULL;
	}

	if (sc->sc_ep[SMSC_ENDPT_INTR] != NULL) {
		err = usbd_abort_pipe(sc->sc_ep[SMSC_ENDPT_INTR]);
		if (err) {
			printf("%s: abort intr pipe failed: %s\n",
			    device_xname(sc->sc_dev), usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->sc_ep[SMSC_ENDPT_INTR]);
		if (err) {
			printf("%s: close intr pipe failed: %s\n",
			    device_xname(sc->sc_dev), usbd_errstr(err));
		}
		sc->sc_ep[SMSC_ENDPT_INTR] = NULL;
	}

	/* Free RX resources. */
	for (i = 0; i < SMSC_RX_LIST_CNT; i++) {
		if (sc->sc_cdata.rx_chain[i].sc_mbuf != NULL) {
			m_freem(sc->sc_cdata.rx_chain[i].sc_mbuf);
			sc->sc_cdata.rx_chain[i].sc_mbuf = NULL;
		}
		if (sc->sc_cdata.rx_chain[i].sc_xfer != NULL) {
			usbd_free_xfer(sc->sc_cdata.rx_chain[i].sc_xfer);
			sc->sc_cdata.rx_chain[i].sc_xfer = NULL;
		}
	}

	/* Free TX resources. */
	for (i = 0; i < SMSC_TX_LIST_CNT; i++) {
		if (sc->sc_cdata.tx_chain[i].sc_mbuf != NULL) {
			m_freem(sc->sc_cdata.tx_chain[i].sc_mbuf);
			sc->sc_cdata.tx_chain[i].sc_mbuf = NULL;
		}
		if (sc->sc_cdata.tx_chain[i].sc_xfer != NULL) {
			usbd_free_xfer(sc->sc_cdata.tx_chain[i].sc_xfer);
			sc->sc_cdata.tx_chain[i].sc_xfer = NULL;
		}
	}
}

int
smsc_chip_init(struct smsc_softc *sc)
{
	int err;
	uint32_t reg_val;
	int burst_cap;

	/* Enter H/W config mode */
	smsc_write_reg(sc, SMSC_HW_CFG, SMSC_HW_CFG_LRST);

	if ((err = smsc_wait_for_bits(sc, SMSC_HW_CFG,
	    SMSC_HW_CFG_LRST)) != 0) {
		smsc_warn_printf(sc, "timed-out waiting for reset to "
		    "complete\n");
		goto init_failed;
	}

	/* Reset the PHY */
	smsc_write_reg(sc, SMSC_PM_CTRL, SMSC_PM_CTRL_PHY_RST);

	if ((err = smsc_wait_for_bits(sc, SMSC_PM_CTRL,
	    SMSC_PM_CTRL_PHY_RST) != 0)) {
		smsc_warn_printf(sc, "timed-out waiting for phy reset to "
		    "complete\n");
		goto init_failed;
	}
	usbd_delay_ms(sc->sc_udev, 40);

	/* Set the mac address */
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	const char *eaddr = CLLADDR(ifp->if_sadl);
	if ((err = smsc_setmacaddress(sc, eaddr)) != 0) {
		smsc_warn_printf(sc, "failed to set the MAC address\n");
		goto init_failed;
	}

	/*
	 * Don't know what the HW_CFG_BIR bit is, but following the reset
	 * sequence as used in the Linux driver.
	 */
	if ((err = smsc_read_reg(sc, SMSC_HW_CFG, &reg_val)) != 0) {
		smsc_warn_printf(sc, "failed to read HW_CFG: %d\n", err);
		goto init_failed;
	}
	reg_val |= SMSC_HW_CFG_BIR;
	smsc_write_reg(sc, SMSC_HW_CFG, reg_val);

	/*
	 * There is a so called 'turbo mode' that the linux driver supports, it
	 * seems to allow you to jam multiple frames per Rx transaction.
	 * By default this driver supports that and therefore allows multiple
	 * frames per USB transfer.
	 *
	 * The xfer buffer size needs to reflect this as well, therefore based
	 * on the calculations in the Linux driver the RX bufsize is set to
	 * 18944,
	 *     bufsz = (16 * 1024 + 5 * 512)
	 *
	 * Burst capability is the number of URBs that can be in a burst of
	 * data/ethernet frames.
	 */

	if (sc->sc_udev->speed == USB_SPEED_HIGH)
		burst_cap = 37;
	else
		burst_cap = 128;

	smsc_write_reg(sc, SMSC_BURST_CAP, burst_cap);

	/* Set the default bulk in delay (magic value from Linux driver) */
	smsc_write_reg(sc, SMSC_BULK_IN_DLY, 0x00002000);

	/*
	 * Initialise the RX interface
	 */
	if ((err = smsc_read_reg(sc, SMSC_HW_CFG, &reg_val)) < 0) {
		smsc_warn_printf(sc, "failed to read HW_CFG: (err = %d)\n",
		    err);
		goto init_failed;
	}

	/*
	 * The following settings are used for 'turbo mode', a.k.a multiple
	 * frames per Rx transaction (again info taken form Linux driver).
	 */
	reg_val |= (SMSC_HW_CFG_MEF | SMSC_HW_CFG_BCE);

	/*
	 * set Rx data offset to ETHER_ALIGN which will make the IP header
	 * align on a word boundary.
	 */
	reg_val |= ETHER_ALIGN << SMSC_HW_CFG_RXDOFF_SHIFT;

	smsc_write_reg(sc, SMSC_HW_CFG, reg_val);

	/* Clear the status register ? */
	smsc_write_reg(sc, SMSC_INTR_STATUS, 0xffffffff);

	/* Read and display the revision register */
	if ((err = smsc_read_reg(sc, SMSC_ID_REV, &sc->sc_rev_id)) < 0) {
		smsc_warn_printf(sc, "failed to read ID_REV (err = %d)\n", err);
		goto init_failed;
	}

	/* GPIO/LED setup */
	reg_val = SMSC_LED_GPIO_CFG_SPD_LED | SMSC_LED_GPIO_CFG_LNK_LED |
	    SMSC_LED_GPIO_CFG_FDX_LED;
	smsc_write_reg(sc, SMSC_LED_GPIO_CFG, reg_val);

	/*
	 * Initialise the TX interface
	 */
	smsc_write_reg(sc, SMSC_FLOW, 0);

	smsc_write_reg(sc, SMSC_AFC_CFG, AFC_CFG_DEFAULT);

	/* Read the current MAC configuration */
	if ((err = smsc_read_reg(sc, SMSC_MAC_CSR, &sc->sc_mac_csr)) < 0) {
		smsc_warn_printf(sc, "failed to read MAC_CSR (err=%d)\n", err);
		goto init_failed;
	}

	/* disable pad stripping, collides with checksum offload */
	sc->sc_mac_csr &= ~SMSC_MAC_CSR_PADSTR;

	/* Vlan */
	smsc_write_reg(sc, SMSC_VLAN1, (uint32_t)ETHERTYPE_VLAN);

	/*
	 * Start TX
	 */
	sc->sc_mac_csr |= SMSC_MAC_CSR_TXEN;
	smsc_write_reg(sc, SMSC_MAC_CSR, sc->sc_mac_csr);
	smsc_write_reg(sc, SMSC_TX_CFG, SMSC_TX_CFG_ON);

	/*
	 * Start RX
	 */
	sc->sc_mac_csr |= SMSC_MAC_CSR_RXEN;
	smsc_write_reg(sc, SMSC_MAC_CSR, sc->sc_mac_csr);

	return (0);

init_failed:
	smsc_err_printf(sc, "smsc_chip_init failed (err=%d)\n", err);
	return (err);
}

int
smsc_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct smsc_softc	*sc = ifp->if_softc;
	struct ifreq /*const*/	*ifr = data;
	int			s, error = 0;

	if (sc->sc_dying)
		return EIO;

	s = splnet();

	switch(cmd) {
	case SIOCSIFFLAGS:
		if ((error = ifioctl_common(ifp, cmd, data)) != 0)
			break;

		switch (ifp->if_flags & (IFF_UP | IFF_RUNNING)) {
		case IFF_RUNNING:
			smsc_stop(ifp, 1);
			break;
		case IFF_UP:
			smsc_init(ifp);
			break;
		case IFF_UP | IFF_RUNNING:
			if (ifp->if_flags & IFF_PROMISC &&
			    !(sc->sc_if_flags & IFF_PROMISC)) {
				sc->sc_mac_csr |= SMSC_MAC_CSR_PRMS;
				smsc_write_reg(sc, SMSC_MAC_CSR,
				    sc->sc_mac_csr);
				smsc_setmulti(sc);
			} else if (!(ifp->if_flags & IFF_PROMISC) &&
			    sc->sc_if_flags & IFF_PROMISC) {
				sc->sc_mac_csr &= ~SMSC_MAC_CSR_PRMS;
				smsc_write_reg(sc, SMSC_MAC_CSR,
				    sc->sc_mac_csr);
				smsc_setmulti(sc);
			} else {
				smsc_init(ifp);
			}
			break;
		}
		sc->sc_if_flags = ifp->if_flags;
		break;

	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_mii.mii_media, cmd);
		break;

	default:
		if ((error = ether_ioctl(ifp, cmd, data)) != ENETRESET)
			break;

		error = 0;

		if (cmd == SIOCADDMULTI || cmd == SIOCDELMULTI)
			smsc_setmulti(sc);

	}
	splx(s);

	return error;
}

int
smsc_match(device_t parent, cfdata_t match, void *aux)
{
	struct usb_attach_arg *uaa = aux;

	return (usb_lookup(smsc_devs, uaa->vendor, uaa->product) != NULL) ?
	    UMATCH_VENDOR_PRODUCT : UMATCH_NONE;
}

void
smsc_attach(device_t parent, device_t self, void *aux)
{
	struct smsc_softc *sc = device_private(self);
	struct usb_attach_arg *uaa = aux;
	usbd_device_handle dev = uaa->device;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	char *devinfop;
	struct mii_data *mii;
	struct ifnet *ifp;
	int err, s, i;
	uint32_t mac_h, mac_l;

	sc->sc_dev = self;
	sc->sc_udev = dev;

	aprint_naive("\n");
	aprint_normal("\n");

	devinfop = usbd_devinfo_alloc(sc->sc_udev, 0);
	aprint_normal_dev(self, "%s\n", devinfop);
	usbd_devinfo_free(devinfop);

	err = usbd_set_config_no(dev, SMSC_CONFIG_INDEX, 1);
	if (err) {
		aprint_error_dev(self, "failed to set configuration"
		    ", err=%s\n", usbd_errstr(err));
		return;
	}
	/* Setup the endpoints for the SMSC LAN95xx device(s) */
	usb_init_task(&sc->sc_tick_task, smsc_tick_task, sc, 0);
	usb_init_task(&sc->sc_stop_task, (void (*)(void *))smsc_stop, sc, 0);
	mutex_init(&sc->sc_mii_lock, MUTEX_DEFAULT, IPL_NONE);

	err = usbd_device2interface_handle(dev, SMSC_IFACE_IDX, &sc->sc_iface);
	if (err) {
		aprint_error_dev(self, "getting interface handle failed\n");
		return;
	}

	id = usbd_get_interface_descriptor(sc->sc_iface);

	if (sc->sc_udev->speed >= USB_SPEED_HIGH)
		sc->sc_bufsz = SMSC_MAX_BUFSZ;
	else
		sc->sc_bufsz = SMSC_MIN_BUFSZ;

	/* Find endpoints. */
	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->sc_iface, i);
		if (!ed) {
			aprint_error_dev(self, "couldn't get ep %d\n", i);
			return;
		}
		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->sc_ed[SMSC_ENDPT_RX] = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
			   UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->sc_ed[SMSC_ENDPT_TX] = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
			   UE_GET_XFERTYPE(ed->bmAttributes) == UE_INTERRUPT) {
			sc->sc_ed[SMSC_ENDPT_INTR] = ed->bEndpointAddress;
		}
	}

	s = splnet();

	ifp = &sc->sc_ec.ec_if;
	ifp->if_softc = sc;
	strlcpy(ifp->if_xname, device_xname(sc->sc_dev), IFNAMSIZ);
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_init = smsc_init;
	ifp->if_ioctl = smsc_ioctl;
	ifp->if_start = smsc_start;
	ifp->if_stop = smsc_stop;

#ifdef notyet
	/*
	 * We can do TCPv4, and UDPv4 checksums in hardware.
	 */
	ifp->if_capabilities |=
	    /*IFCAP_CSUM_TCPv4_Tx |*/ IFCAP_CSUM_TCPv4_Rx |
	    /*IFCAP_CSUM_UDPv4_Tx |*/ IFCAP_CSUM_UDPv4_Rx;
#endif

	sc->sc_ec.ec_capabilities = ETHERCAP_VLAN_MTU;

	/* Setup some of the basics */
	sc->sc_phyno = 1;

	/*
	 * Attempt to get the mac address, if an EEPROM is not attached this
	 * will just return FF:FF:FF:FF:FF:FF, so in such cases we invent a MAC
	 * address based on urandom.
	 */
	memset(sc->sc_enaddr, 0xff, ETHER_ADDR_LEN);

	prop_dictionary_t dict = device_properties(self);
	prop_data_t eaprop = prop_dictionary_get(dict, "mac-address");

	if (eaprop != NULL) {
		KASSERT(prop_object_type(eaprop) == PROP_TYPE_DATA);
		KASSERT(prop_data_size(eaprop) == ETHER_ADDR_LEN);
		memcpy(sc->sc_enaddr, prop_data_data_nocopy(eaprop),
		    ETHER_ADDR_LEN);
	} else
	/* Check if there is already a MAC address in the register */
	if ((smsc_read_reg(sc, SMSC_MAC_ADDRL, &mac_l) == 0) &&
	    (smsc_read_reg(sc, SMSC_MAC_ADDRH, &mac_h) == 0)) {
		sc->sc_enaddr[5] = (uint8_t)((mac_h >> 8) & 0xff);
		sc->sc_enaddr[4] = (uint8_t)((mac_h) & 0xff);
		sc->sc_enaddr[3] = (uint8_t)((mac_l >> 24) & 0xff);
		sc->sc_enaddr[2] = (uint8_t)((mac_l >> 16) & 0xff);
		sc->sc_enaddr[1] = (uint8_t)((mac_l >> 8) & 0xff);
		sc->sc_enaddr[0] = (uint8_t)((mac_l) & 0xff);
	}

	aprint_normal_dev(self, "Ethernet address %s\n", ether_sprintf(sc->sc_enaddr));

	IFQ_SET_READY(&ifp->if_snd);

	/* Initialize MII/media info. */
	mii = &sc->sc_mii;
	mii->mii_ifp = ifp;
	mii->mii_readreg = smsc_miibus_readreg;
	mii->mii_writereg = smsc_miibus_writereg;
	mii->mii_statchg = smsc_miibus_statchg;
	mii->mii_flags = MIIF_AUTOTSLEEP;
	sc->sc_ec.ec_mii = mii;
	ifmedia_init(&mii->mii_media, 0, smsc_ifmedia_upd, smsc_ifmedia_sts);
	mii_attach(self, mii, 0xffffffff, MII_PHY_ANY, MII_OFFSET_ANY, 0);

	if (LIST_FIRST(&mii->mii_phys) == NULL) {
		ifmedia_add(&mii->mii_media, IFM_ETHER | IFM_NONE, 0, NULL);
		ifmedia_set(&mii->mii_media, IFM_ETHER | IFM_NONE);
	} else
		ifmedia_set(&mii->mii_media, IFM_ETHER | IFM_AUTO);

	if_attach(ifp);
	ether_ifattach(ifp, sc->sc_enaddr);

	rnd_attach_source(&sc->sc_rnd_source, device_xname(sc->sc_dev),
	    RND_TYPE_NET, RND_FLAG_DEFAULT);

	callout_init(&sc->sc_stat_ch, 0);

	splx(s);

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev, sc->sc_dev);
}

int
smsc_detach(device_t self, int flags)
{
	struct smsc_softc *sc = device_private(self);
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	int s;

	callout_stop(&sc->sc_stat_ch);

	if (sc->sc_ep[SMSC_ENDPT_TX] != NULL)
		usbd_abort_pipe(sc->sc_ep[SMSC_ENDPT_TX]);
	if (sc->sc_ep[SMSC_ENDPT_RX] != NULL)
		usbd_abort_pipe(sc->sc_ep[SMSC_ENDPT_RX]);
	if (sc->sc_ep[SMSC_ENDPT_INTR] != NULL)
		usbd_abort_pipe(sc->sc_ep[SMSC_ENDPT_INTR]);

	/*
	 * Remove any pending tasks.  They cannot be executing because they run
	 * in the same thread as detach.
	 */
	usb_rem_task(sc->sc_udev, &sc->sc_tick_task);
	usb_rem_task(sc->sc_udev, &sc->sc_stop_task);

	s = splusb();

	if (--sc->sc_refcnt >= 0) {
		/* Wait for processes to go away */
		usb_detach_waitold(sc->sc_dev);
	}

	if (ifp->if_flags & IFF_RUNNING)
		smsc_stop(ifp ,1);

	rnd_detach_source(&sc->sc_rnd_source);
	mii_detach(&sc->sc_mii, MII_PHY_ANY, MII_OFFSET_ANY);
	ifmedia_delete_instance(&sc->sc_mii.mii_media, IFM_INST_ANY);
	if (ifp->if_softc != NULL) {
		ether_ifdetach(ifp);
		if_detach(ifp);
	}

#ifdef DIAGNOSTIC
	if (sc->sc_ep[SMSC_ENDPT_TX] != NULL ||
	    sc->sc_ep[SMSC_ENDPT_RX] != NULL ||
	    sc->sc_ep[SMSC_ENDPT_INTR] != NULL)
		printf("%s: detach has active endpoints\n",
		    device_xname(sc->sc_dev));
#endif

	if (--sc->sc_refcnt >= 0) {
		/* Wait for processes to go away. */
		usb_detach_waitold(sc->sc_dev);
	}
	splx(s);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev, sc->sc_dev);

	mutex_destroy(&sc->sc_mii_lock);

	return (0);
}

void
smsc_tick_task(void *xsc)
{
	int			 s;
	struct smsc_softc	*sc = xsc;
	struct ifnet		*ifp;
	struct mii_data		*mii;

	if (sc == NULL)
		return;

	if (sc->sc_dying)
		return;
	ifp = &sc->sc_ec.ec_if;
	mii = &sc->sc_mii;
	if (mii == NULL)
		return;

	s = splnet();

	mii_tick(mii);
	if ((sc->sc_flags & SMSC_FLAG_LINK) == 0)
		smsc_miibus_statchg(ifp);
	callout_reset(&sc->sc_stat_ch, hz, smsc_tick, sc);

	splx(s);
}

int
smsc_activate(device_t self, enum devact act)
{
	struct smsc_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		if_deactivate(&sc->sc_ec.ec_if);
		sc->sc_dying = 1;
		return 0;
	default:
		return EOPNOTSUPP;
	}
	return (0);
}

void
smsc_lock_mii(struct smsc_softc *sc)
{
	sc->sc_refcnt++;
	mutex_enter(&sc->sc_mii_lock);
}

void
smsc_unlock_mii(struct smsc_softc *sc)
{
	mutex_exit(&sc->sc_mii_lock);
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeupold(sc->sc_dev);
}

void
smsc_rxeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct smsc_chain	*c = (struct smsc_chain *)priv;
	struct smsc_softc	*sc = c->sc_sc;
	struct ifnet		*ifp = &sc->sc_ec.ec_if;
	u_char			*buf = c->sc_buf;
	uint32_t		total_len;
	uint32_t		rxhdr;
	uint16_t		pktlen;
	struct mbuf		*m;
	int			s;

	if (sc->sc_dying)
		return;

	if (!(ifp->if_flags & IFF_RUNNING))
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;
		if (usbd_ratecheck(&sc->sc_rx_notice)) {
			printf("%s: usb errors on rx: %s\n",
			    device_xname(sc->sc_dev), usbd_errstr(status));
		}
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->sc_ep[SMSC_ENDPT_RX]);
		goto done;
	}

	usbd_get_xfer_status(xfer, NULL, NULL, &total_len, NULL);
	smsc_dbg_printf(sc, "xfer status total_len %d\n", total_len);

	while (total_len != 0) {
		if (total_len < sizeof(rxhdr)) {
			smsc_dbg_printf(sc, "total_len %d < sizeof(rxhdr) %zu\n",
			    total_len, sizeof(rxhdr));
			ifp->if_ierrors++;
			goto done;
		}

		memcpy(&rxhdr, buf, sizeof(rxhdr));
		rxhdr = le32toh(rxhdr);
		buf += sizeof(rxhdr);
		total_len -= sizeof(rxhdr);

		if (rxhdr & SMSC_RX_STAT_COLLISION)
			ifp->if_collisions++;

		if (rxhdr & (SMSC_RX_STAT_ERROR
		           | SMSC_RX_STAT_LENGTH_ERROR
		           | SMSC_RX_STAT_MII_ERROR)) {
			smsc_dbg_printf(sc, "rx error (hdr 0x%08x)\n", rxhdr);
			ifp->if_ierrors++;
			goto done;
		}

		pktlen = (uint16_t)SMSC_RX_STAT_FRM_LENGTH(rxhdr);
		smsc_dbg_printf(sc, "rxeof total_len %d pktlen %d rxhdr "
		    "0x%08x\n", total_len, pktlen, rxhdr);

		if (pktlen < ETHER_HDR_LEN) {
			smsc_dbg_printf(sc, "pktlen %d < ETHER_HDR_LEN %d\n",
			    pktlen, ETHER_HDR_LEN);
			ifp->if_ierrors++;
			goto done;
		}

		pktlen += ETHER_ALIGN;

		if (pktlen > MCLBYTES) {
			smsc_dbg_printf(sc, "pktlen %d > MCLBYTES %d\n",
			    pktlen, MCLBYTES);
			ifp->if_ierrors++;
			goto done;
		}

		if (pktlen > total_len) {
			smsc_dbg_printf(sc, "pktlen %d > total_len %d\n",
			    pktlen, total_len);
			ifp->if_ierrors++;
			goto done;
		}

		m = smsc_newbuf();
		if (m == NULL) {
			smsc_dbg_printf(sc, "smc_newbuf returned NULL\n");
			ifp->if_ierrors++;
			goto done;
		}

		ifp->if_ipackets++;
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = pktlen;
		m->m_flags |= M_HASFCS;
		m_adj(m, ETHER_ALIGN);

		KASSERT(m->m_len < MCLBYTES);
		memcpy(mtod(m, char *), buf + ETHER_ALIGN, m->m_len);

		/* Check if RX TCP/UDP checksumming is being offloaded */
		if (sc->sc_coe_ctrl & SMSC_COE_CTRL_RX_EN) {
			smsc_dbg_printf(sc,"RX checksum offload checking\n");
			struct ether_header *eh;

			eh = mtod(m, struct ether_header *);

			/* Remove the extra 2 bytes of the csum */
			m_adj(m, -2);

			/*
			 * The checksum appears to be simplistically calculated
			 * over the udp/tcp header and data up to the end of the
			 * eth frame.  Which means if the eth frame is padded
			 * the csum calculation is incorrectly performed over
			 * the padding bytes as well. Therefore to be safe we
			 * ignore the H/W csum on frames less than or equal to
			 * 64 bytes.
			 *
			 * Ignore H/W csum for non-IPv4 packets.
			 */
			smsc_dbg_printf(sc,"Ethertype %02x pktlen %02x\n",
			    be16toh(eh->ether_type), pktlen);
			if (be16toh(eh->ether_type) == ETHERTYPE_IP &&
			    pktlen > ETHER_MIN_LEN) {

				m->m_pkthdr.csum_flags |=
				    (M_CSUM_TCPv4 | M_CSUM_UDPv4 | M_CSUM_DATA);

				/*
				 * Copy the TCP/UDP checksum from the last 2
				 * bytes of the transfer and put in the
				 * csum_data field.
				 */
				memcpy(&m->m_pkthdr.csum_data,
				    buf + pktlen - 2, 2);
				/*
				 * The data is copied in network order, but the
				 * csum algorithm in the kernel expects it to be
				 * in host network order.
				 */
				m->m_pkthdr.csum_data =
				    ntohs(m->m_pkthdr.csum_data);
				smsc_dbg_printf(sc,
				    "RX checksum offloaded (0x%04x)\n",
				    m->m_pkthdr.csum_data);
			}
		}

		/* round up to next longword */
		pktlen = (pktlen + 3) & ~0x3;

		/* total_len does not include the padding */
		if (pktlen > total_len)
			pktlen = total_len;

		buf += pktlen;
		total_len -= pktlen;

		/* push the packet up */
		s = splnet();
		bpf_mtap(ifp, m);
		ifp->if_input(ifp, m);
		splx(s);
	}

done:
	/* Setup new transfer. */
	usbd_setup_xfer(xfer, sc->sc_ep[SMSC_ENDPT_RX],
	    c, c->sc_buf, sc->sc_bufsz,
	    USBD_SHORT_XFER_OK | USBD_NO_COPY,
	    USBD_NO_TIMEOUT, smsc_rxeof);
	usbd_transfer(xfer);

	return;
}

void
smsc_txeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct smsc_softc	*sc;
	struct smsc_chain	*c;
	struct ifnet		*ifp;
	int			s;

	c = priv;
	sc = c->sc_sc;
	ifp = &sc->sc_ec.ec_if;

	if (sc->sc_dying)
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
		printf("%s: usb error on tx: %s\n", device_xname(sc->sc_dev),
		    usbd_errstr(status));
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->sc_ep[SMSC_ENDPT_TX]);
		splx(s);
		return;
	}
	ifp->if_opackets++;

	m_freem(c->sc_mbuf);
	c->sc_mbuf = NULL;

	if (IFQ_IS_EMPTY(&ifp->if_snd) == 0)
		smsc_start(ifp);

	splx(s);
}

int
smsc_tx_list_init(struct smsc_softc *sc)
{
	struct smsc_cdata *cd;
	struct smsc_chain *c;
	int i;

	cd = &sc->sc_cdata;
	for (i = 0; i < SMSC_TX_LIST_CNT; i++) {
		c = &cd->tx_chain[i];
		c->sc_sc = sc;
		c->sc_idx = i;
		c->sc_mbuf = NULL;
		if (c->sc_xfer == NULL) {
			c->sc_xfer = usbd_alloc_xfer(sc->sc_udev);
			if (c->sc_xfer == NULL)
				return (ENOBUFS);
			c->sc_buf = usbd_alloc_buffer(c->sc_xfer,
			    sc->sc_bufsz);
			if (c->sc_buf == NULL) {
				usbd_free_xfer(c->sc_xfer);
				return (ENOBUFS);
			}
		}
	}

	return (0);
}

int
smsc_rx_list_init(struct smsc_softc *sc)
{
	struct smsc_cdata *cd;
	struct smsc_chain *c;
	int i;

	cd = &sc->sc_cdata;
	for (i = 0; i < SMSC_RX_LIST_CNT; i++) {
		c = &cd->rx_chain[i];
		c->sc_sc = sc;
		c->sc_idx = i;
		c->sc_mbuf = NULL;
		if (c->sc_xfer == NULL) {
			c->sc_xfer = usbd_alloc_xfer(sc->sc_udev);
			if (c->sc_xfer == NULL)
				return (ENOBUFS);
			c->sc_buf = usbd_alloc_buffer(c->sc_xfer,
			    sc->sc_bufsz);
			if (c->sc_buf == NULL) {
				usbd_free_xfer(c->sc_xfer);
				return (ENOBUFS);
			}
		}
	}

	return (0);
}

struct mbuf *
smsc_newbuf(void)
{
	struct mbuf	*m;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (NULL);

	MCLGET(m, M_DONTWAIT);
	if (!(m->m_flags & M_EXT)) {
		m_freem(m);
		return (NULL);
	}

	return (m);
}

int
smsc_encap(struct smsc_softc *sc, struct mbuf *m, int idx)
{
	struct ifnet		*ifp = &sc->sc_ec.ec_if;
	struct smsc_chain	*c;
	usbd_status		 err;
	uint32_t		 txhdr;
	uint32_t		 frm_len = 0;

	c = &sc->sc_cdata.tx_chain[idx];

	/*
	 * Each frame is prefixed with two 32-bit values describing the
	 * length of the packet and buffer.
	 */
	txhdr = SMSC_TX_CTRL_0_BUF_SIZE(m->m_pkthdr.len) |
			SMSC_TX_CTRL_0_FIRST_SEG | SMSC_TX_CTRL_0_LAST_SEG;
	txhdr = htole32(txhdr);
	memcpy(c->sc_buf, &txhdr, sizeof(txhdr));

	txhdr = SMSC_TX_CTRL_1_PKT_LENGTH(m->m_pkthdr.len);
	txhdr = htole32(txhdr);
	memcpy(c->sc_buf + 4, &txhdr, sizeof(txhdr));

	frm_len += 8;

	/* Next copy in the actual packet */
	m_copydata(m, 0, m->m_pkthdr.len, c->sc_buf + frm_len);
	frm_len += m->m_pkthdr.len;

	c->sc_mbuf = m;

	usbd_setup_xfer(c->sc_xfer, sc->sc_ep[SMSC_ENDPT_TX],
	    c, c->sc_buf, frm_len, USBD_FORCE_SHORT_XFER | USBD_NO_COPY,
	    10000, smsc_txeof);

	err = usbd_transfer(c->sc_xfer);
	/* XXXNH get task to stop interface */
	if (err != USBD_IN_PROGRESS) {
		smsc_stop(ifp, 0);
		return (EIO);
	}

	sc->sc_cdata.tx_cnt++;

	return (0);
}
