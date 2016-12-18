/*	$NetBSD: if_udav.c,v 1.44 2015/04/13 16:33:25 riastradh Exp $	*/
/*	$nabe: if_udav.c,v 1.3 2003/08/21 16:57:19 nabe Exp $	*/

/*
 * Copyright (c) 2003
 *     Shingo WATANABE <nabe@nabechan.org>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 *
 */

/*
 * DM9601(DAVICOM USB to Ethernet MAC Controller with Integrated 10/100 PHY)
 * The spec can be found at the following url.
 *   http://www.davicom.com.tw/big5/download/Data%20Sheet/DM9601-DS-F01-062202s.pdf
 */

/*
 * TODO:
 *	Interrupt Endpoint support
 *	External PHYs
 *	powerhook() support?
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_udav.c,v 1.44 2015/04/13 16:33:25 riastradh Exp $");

#ifdef _KERNEL_OPT
#include "opt_inet.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mutex.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <sys/rndsource.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <net/bpf.h>

#include <net/if_ether.h>
#ifdef INET
#include <netinet/in.h>
#include <netinet/if_inarp.h>
#endif

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>

#include <dev/usb/if_udavreg.h>


/* Function declarations */
int             udav_match(device_t, cfdata_t, void *);
void            udav_attach(device_t, device_t, void *);
int             udav_detach(device_t, int);
int             udav_activate(device_t, enum devact);
extern struct cfdriver udav_cd;
CFATTACH_DECL_NEW(udav, sizeof(struct udav_softc), udav_match, udav_attach, udav_detach, udav_activate);

Static int udav_openpipes(struct udav_softc *);
Static int udav_rx_list_init(struct udav_softc *);
Static int udav_tx_list_init(struct udav_softc *);
Static int udav_newbuf(struct udav_softc *, struct udav_chain *, struct mbuf *);
Static void udav_start(struct ifnet *);
Static int udav_send(struct udav_softc *, struct mbuf *, int);
Static void udav_txeof(usbd_xfer_handle, usbd_private_handle, usbd_status);
Static void udav_rxeof(usbd_xfer_handle, usbd_private_handle, usbd_status);
Static void udav_tick(void *);
Static void udav_tick_task(void *);
Static int udav_ioctl(struct ifnet *, u_long, void *);
Static void udav_stop_task(struct udav_softc *);
Static void udav_stop(struct ifnet *, int);
Static void udav_watchdog(struct ifnet *);
Static int udav_ifmedia_change(struct ifnet *);
Static void udav_ifmedia_status(struct ifnet *, struct ifmediareq *);
Static void udav_lock_mii(struct udav_softc *);
Static void udav_unlock_mii(struct udav_softc *);
Static int udav_miibus_readreg(device_t, int, int);
Static void udav_miibus_writereg(device_t, int, int, int);
Static void udav_miibus_statchg(struct ifnet *);
Static int udav_init(struct ifnet *);
Static void udav_setmulti(struct udav_softc *);
Static void udav_reset(struct udav_softc *);

Static int udav_csr_read(struct udav_softc *, int, void *, int);
Static int udav_csr_write(struct udav_softc *, int, void *, int);
Static int udav_csr_read1(struct udav_softc *, int);
Static int udav_csr_write1(struct udav_softc *, int, unsigned char);

#if 0
Static int udav_mem_read(struct udav_softc *, int, void *, int);
Static int udav_mem_write(struct udav_softc *, int, void *, int);
Static int udav_mem_write1(struct udav_softc *, int, unsigned char);
#endif

/* Macros */
#ifdef UDAV_DEBUG
#define DPRINTF(x)	if (udavdebug) printf x
#define DPRINTFN(n,x)	if (udavdebug >= (n)) printf x
int udavdebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define	UDAV_SETBIT(sc, reg, x)	\
	udav_csr_write1(sc, reg, udav_csr_read1(sc, reg) | (x))

#define	UDAV_CLRBIT(sc, reg, x)	\
	udav_csr_write1(sc, reg, udav_csr_read1(sc, reg) & ~(x))

static const struct udav_type {
	struct usb_devno udav_dev;
	u_int16_t udav_flags;
#define UDAV_EXT_PHY	0x0001
#define UDAV_NO_PHY	0x0002
} udav_devs [] = {
	/* Corega USB-TXC */
	{{ USB_VENDOR_COREGA, USB_PRODUCT_COREGA_FETHER_USB_TXC }, 0},
	/* ShanTou ST268 USB NIC */
	{{ USB_VENDOR_SHANTOU, USB_PRODUCT_SHANTOU_ST268_USB_NIC }, 0},
	/* ShanTou ADM8515 */
	{{ USB_VENDOR_SHANTOU, USB_PRODUCT_SHANTOU_ADM8515 }, 0},
	/* SUNRISING SR9600 */
	{{ USB_VENDOR_SUNRISING, USB_PRODUCT_SUNRISING_SR9600 }, 0 },
	/* SUNRISING QF9700 */
	{{ USB_VENDOR_SUNRISING, USB_PRODUCT_SUNRISING_QF9700 }, UDAV_NO_PHY },
	/* QUAN DM9601 */
	{{USB_VENDOR_QUAN, USB_PRODUCT_QUAN_DM9601 }, 0},
#if 0
	/* DAVICOM DM9601 Generic? */
	/*  XXX: The following ids was obtained from the data sheet. */
	{{ 0x0a46, 0x9601 }, 0},
#endif
};
#define udav_lookup(v, p) ((const struct udav_type *)usb_lookup(udav_devs, v, p))


/* Probe */
int
udav_match(device_t parent, cfdata_t match, void *aux)
{
	struct usb_attach_arg *uaa = aux;

	return (udav_lookup(uaa->vendor, uaa->product) != NULL ?
		UMATCH_VENDOR_PRODUCT : UMATCH_NONE);
}

/* Attach */
void
udav_attach(device_t parent, device_t self, void *aux)
{
	struct udav_softc *sc = device_private(self);
	struct usb_attach_arg *uaa = aux;
	usbd_device_handle dev = uaa->device;
	usbd_interface_handle iface;
	usbd_status err;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	char *devinfop;
	struct ifnet *ifp;
	struct mii_data *mii;
	u_char eaddr[ETHER_ADDR_LEN];
	int i, s;

	sc->sc_dev = self;

	aprint_naive("\n");
	aprint_normal("\n");

	devinfop = usbd_devinfo_alloc(dev, 0);
	aprint_normal_dev(self, "%s\n", devinfop);
	usbd_devinfo_free(devinfop);

	/* Move the device into the configured state. */
	err = usbd_set_config_no(dev, UDAV_CONFIG_NO, 1); /* idx 0 */
	if (err) {
		aprint_error_dev(self, "failed to set configuration"
		    ", err=%s\n", usbd_errstr(err));
		goto bad;
	}

	usb_init_task(&sc->sc_tick_task, udav_tick_task, sc, 0);
	mutex_init(&sc->sc_mii_lock, MUTEX_DEFAULT, IPL_NONE);
	usb_init_task(&sc->sc_stop_task, (void (*)(void *))udav_stop_task, sc, 0);

	/* get control interface */
	err = usbd_device2interface_handle(dev, UDAV_IFACE_INDEX, &iface);
	if (err) {
		aprint_error_dev(self, "failed to get interface, err=%s\n",
		       usbd_errstr(err));
		goto bad;
	}

	sc->sc_udev = dev;
	sc->sc_ctl_iface = iface;
	sc->sc_flags = udav_lookup(uaa->vendor, uaa->product)->udav_flags;

	/* get interface descriptor */
	id = usbd_get_interface_descriptor(sc->sc_ctl_iface);

	/* find endpoints */
	sc->sc_bulkin_no = sc->sc_bulkout_no = sc->sc_intrin_no = -1;
	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->sc_ctl_iface, i);
		if (ed == NULL) {
			aprint_error_dev(self, "couldn't get endpoint %d\n", i);
			goto bad;
		}
		if ((ed->bmAttributes & UE_XFERTYPE) == UE_BULK &&
		    UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN)
			sc->sc_bulkin_no = ed->bEndpointAddress; /* RX */
		else if ((ed->bmAttributes & UE_XFERTYPE) == UE_BULK &&
			 UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT)
			sc->sc_bulkout_no = ed->bEndpointAddress; /* TX */
		else if ((ed->bmAttributes & UE_XFERTYPE) == UE_INTERRUPT &&
			 UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN)
			sc->sc_intrin_no = ed->bEndpointAddress; /* Status */
	}

	if (sc->sc_bulkin_no == -1 || sc->sc_bulkout_no == -1 ||
	    sc->sc_intrin_no == -1) {
		aprint_error_dev(self, "missing endpoint\n");
		goto bad;
	}

	s = splnet();

	/* reset the adapter */
	udav_reset(sc);

	/* Get Ethernet Address */
	err = udav_csr_read(sc, UDAV_PAR, (void *)eaddr, ETHER_ADDR_LEN);
	if (err) {
		aprint_error_dev(self, "read MAC address failed\n");
		splx(s);
		goto bad;
	}

	/* Print Ethernet Address */
	aprint_normal_dev(self, "Ethernet address %s\n", ether_sprintf(eaddr));

	/* initialize interface information */
	ifp = GET_IFP(sc);
	ifp->if_softc = sc;
	ifp->if_mtu = ETHERMTU;
	strncpy(ifp->if_xname, device_xname(self), IFNAMSIZ);
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_start = udav_start;
	ifp->if_ioctl = udav_ioctl;
	ifp->if_watchdog = udav_watchdog;
	ifp->if_init = udav_init;
	ifp->if_stop = udav_stop;

	IFQ_SET_READY(&ifp->if_snd);

	if (ISSET(sc->sc_flags, UDAV_NO_PHY)) {
		sc->sc_link = 1;
		goto skipmii;
	}

	/*
	 * Do ifmedia setup.
	 */
	mii = &sc->sc_mii;
	mii->mii_ifp = ifp;
	mii->mii_readreg = udav_miibus_readreg;
	mii->mii_writereg = udav_miibus_writereg;
	mii->mii_statchg = udav_miibus_statchg;
	mii->mii_flags = MIIF_AUTOTSLEEP;
	sc->sc_ec.ec_mii = mii;
	ifmedia_init(&mii->mii_media, 0,
		     udav_ifmedia_change, udav_ifmedia_status);
	mii_attach(self, mii, 0xffffffff, MII_PHY_ANY, MII_OFFSET_ANY, 0);
	if (LIST_FIRST(&mii->mii_phys) == NULL) {
		ifmedia_add(&mii->mii_media, IFM_ETHER | IFM_NONE, 0, NULL);
		ifmedia_set(&mii->mii_media, IFM_ETHER | IFM_NONE);
	} else
		ifmedia_set(&mii->mii_media, IFM_ETHER | IFM_AUTO);

skipmii:
	/* attach the interface */
	if_attach(ifp);
	ether_ifattach(ifp, eaddr);

	rnd_attach_source(&sc->rnd_source, device_xname(self),
	    RND_TYPE_NET, RND_FLAG_DEFAULT);

	callout_init(&sc->sc_stat_ch, 0);
	sc->sc_attached = 1;
	splx(s);

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, dev, sc->sc_dev);

	return;

 bad:
	sc->sc_dying = 1;
	return;
}

/* detach */
int
udav_detach(device_t self, int flags)
{
	struct udav_softc *sc = device_private(self);
	struct ifnet *ifp = GET_IFP(sc);
	int s;

	DPRINTF(("%s: %s: enter\n", device_xname(sc->sc_dev), __func__));

	/* Detached before attached finished */
	if (!sc->sc_attached)
		return (0);

	callout_stop(&sc->sc_stat_ch);

	/* Remove any pending tasks */
	usb_rem_task(sc->sc_udev, &sc->sc_tick_task);
	usb_rem_task(sc->sc_udev, &sc->sc_stop_task);

	s = splusb();

	if (--sc->sc_refcnt >= 0) {
		/* Wait for processes to go away */
		usb_detach_waitold(sc->sc_dev);
	}
	if (ifp->if_flags & IFF_RUNNING)
		udav_stop(GET_IFP(sc), 1);

	rnd_detach_source(&sc->rnd_source);
	mii_detach(&sc->sc_mii, MII_PHY_ANY, MII_OFFSET_ANY);
	ifmedia_delete_instance(&sc->sc_mii.mii_media, IFM_INST_ANY);
	ether_ifdetach(ifp);
	if_detach(ifp);

#ifdef DIAGNOSTIC
	if (sc->sc_pipe_tx != NULL)
		aprint_debug_dev(self, "detach has active tx endpoint.\n");
	if (sc->sc_pipe_rx != NULL)
		aprint_debug_dev(self, "detach has active rx endpoint.\n");
	if (sc->sc_pipe_intr != NULL)
		aprint_debug_dev(self, "detach has active intr endpoint.\n");
#endif
	sc->sc_attached = 0;

	splx(s);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
			   sc->sc_dev);

	mutex_destroy(&sc->sc_mii_lock);

	return (0);
}

#if 0
/* read memory */
Static int
udav_mem_read(struct udav_softc *sc, int offset, void *buf, int len)
{
	usb_device_request_t req;
	usbd_status err;

	if (sc == NULL)
		return (0);

	DPRINTFN(0x200,
		("%s: %s: enter\n", device_xname(sc->sc_dev), __func__));

	if (sc->sc_dying)
		return (0);

	offset &= 0xffff;
	len &= 0xff;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = UDAV_REQ_MEM_READ;
	USETW(req.wValue, 0x0000);
	USETW(req.wIndex, offset);
	USETW(req.wLength, len);

	sc->sc_refcnt++;
	err = usbd_do_request(sc->sc_udev, &req, buf);
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeupold(sc->sc_dev);
	if (err) {
		DPRINTF(("%s: %s: read failed. off=%04x, err=%d\n",
			 device_xname(sc->sc_dev), __func__, offset, err));
	}

	return (err);
}

/* write memory */
Static int
udav_mem_write(struct udav_softc *sc, int offset, void *buf, int len)
{
	usb_device_request_t req;
	usbd_status err;

	if (sc == NULL)
		return (0);

	DPRINTFN(0x200,
		("%s: %s: enter\n", device_xname(sc->sc_dev), __func__));

	if (sc->sc_dying)
		return (0);

	offset &= 0xffff;
	len &= 0xff;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = UDAV_REQ_MEM_WRITE;
	USETW(req.wValue, 0x0000);
	USETW(req.wIndex, offset);
	USETW(req.wLength, len);

	sc->sc_refcnt++;
	err = usbd_do_request(sc->sc_udev, &req, buf);
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeupold(sc->sc_dev);
	if (err) {
		DPRINTF(("%s: %s: write failed. off=%04x, err=%d\n",
			 device_xname(sc->sc_dev), __func__, offset, err));
	}

	return (err);
}

/* write memory */
Static int
udav_mem_write1(struct udav_softc *sc, int offset, unsigned char ch)
{
	usb_device_request_t req;
	usbd_status err;

	if (sc == NULL)
		return (0);

	DPRINTFN(0x200,
		("%s: %s: enter\n", device_xname(sc->sc_dev), __func__));

	if (sc->sc_dying)
		return (0);

	offset &= 0xffff;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = UDAV_REQ_MEM_WRITE1;
	USETW(req.wValue, ch);
	USETW(req.wIndex, offset);
	USETW(req.wLength, 0x0000);

	sc->sc_refcnt++;
	err = usbd_do_request(sc->sc_udev, &req, NULL);
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeupold(sc->sc_dev);
	if (err) {
		DPRINTF(("%s: %s: write failed. off=%04x, err=%d\n",
			 device_xname(sc->sc_dev), __func__, offset, err));
	}

	return (err);
}
#endif

/* read register(s) */
Static int
udav_csr_read(struct udav_softc *sc, int offset, void *buf, int len)
{
	usb_device_request_t req;
	usbd_status err;

	if (sc == NULL)
		return (0);

	DPRINTFN(0x200,
		("%s: %s: enter\n", device_xname(sc->sc_dev), __func__));

	if (sc->sc_dying)
		return (0);

	offset &= 0xff;
	len &= 0xff;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = UDAV_REQ_REG_READ;
	USETW(req.wValue, 0x0000);
	USETW(req.wIndex, offset);
	USETW(req.wLength, len);

	sc->sc_refcnt++;
	err = usbd_do_request(sc->sc_udev, &req, buf);
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeupold(sc->sc_dev);
	if (err) {
		DPRINTF(("%s: %s: read failed. off=%04x, err=%d\n",
			 device_xname(sc->sc_dev), __func__, offset, err));
	}

	return (err);
}

/* write register(s) */
Static int
udav_csr_write(struct udav_softc *sc, int offset, void *buf, int len)
{
	usb_device_request_t req;
	usbd_status err;

	if (sc == NULL)
		return (0);

	DPRINTFN(0x200,
		("%s: %s: enter\n", device_xname(sc->sc_dev), __func__));

	if (sc->sc_dying)
		return (0);

	offset &= 0xff;
	len &= 0xff;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = UDAV_REQ_REG_WRITE;
	USETW(req.wValue, 0x0000);
	USETW(req.wIndex, offset);
	USETW(req.wLength, len);

	sc->sc_refcnt++;
	err = usbd_do_request(sc->sc_udev, &req, buf);
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeupold(sc->sc_dev);
	if (err) {
		DPRINTF(("%s: %s: write failed. off=%04x, err=%d\n",
			 device_xname(sc->sc_dev), __func__, offset, err));
	}

	return (err);
}

Static int
udav_csr_read1(struct udav_softc *sc, int offset)
{
	u_int8_t val = 0;

	if (sc == NULL)
		return (0);

	DPRINTFN(0x200,
		("%s: %s: enter\n", device_xname(sc->sc_dev), __func__));

	if (sc->sc_dying)
		return (0);

	return (udav_csr_read(sc, offset, &val, 1) ? 0 : val);
}

/* write a register */
Static int
udav_csr_write1(struct udav_softc *sc, int offset, unsigned char ch)
{
	usb_device_request_t req;
	usbd_status err;

	if (sc == NULL)
		return (0);

	DPRINTFN(0x200,
		("%s: %s: enter\n", device_xname(sc->sc_dev), __func__));

	if (sc->sc_dying)
		return (0);

	offset &= 0xff;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = UDAV_REQ_REG_WRITE1;
	USETW(req.wValue, ch);
	USETW(req.wIndex, offset);
	USETW(req.wLength, 0x0000);

	sc->sc_refcnt++;
	err = usbd_do_request(sc->sc_udev, &req, NULL);
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeupold(sc->sc_dev);
	if (err) {
		DPRINTF(("%s: %s: write failed. off=%04x, err=%d\n",
			 device_xname(sc->sc_dev), __func__, offset, err));
	}

	return (err);
}

Static int
udav_init(struct ifnet *ifp)
{
	struct udav_softc *sc = ifp->if_softc;
	struct mii_data *mii = GET_MII(sc);
	uint8_t eaddr[ETHER_ADDR_LEN];
	int rc, s;

	DPRINTF(("%s: %s: enter\n", device_xname(sc->sc_dev), __func__));

	if (sc->sc_dying)
		return (EIO);

	s = splnet();

	/* Cancel pending I/O and free all TX/RX buffers */
	udav_stop(ifp, 1);

	memcpy(eaddr, CLLADDR(ifp->if_sadl), sizeof(eaddr));
	udav_csr_write(sc, UDAV_PAR, eaddr, ETHER_ADDR_LEN);

	/* Initialize network control register */
	/*  Disable loopback  */
	UDAV_CLRBIT(sc, UDAV_NCR, UDAV_NCR_LBK0 | UDAV_NCR_LBK1);

	/* Initialize RX control register */
	UDAV_SETBIT(sc, UDAV_RCR, UDAV_RCR_DIS_LONG | UDAV_RCR_DIS_CRC);

	/* If we want promiscuous mode, accept all physical frames. */
	if (ifp->if_flags & IFF_PROMISC)
		UDAV_SETBIT(sc, UDAV_RCR, UDAV_RCR_ALL|UDAV_RCR_PRMSC);
	else
		UDAV_CLRBIT(sc, UDAV_RCR, UDAV_RCR_ALL|UDAV_RCR_PRMSC);

	/* Initialize transmit ring */
	if (udav_tx_list_init(sc) == ENOBUFS) {
		printf("%s: tx list init failed\n", device_xname(sc->sc_dev));
		splx(s);
		return (EIO);
	}

	/* Initialize receive ring */
	if (udav_rx_list_init(sc) == ENOBUFS) {
		printf("%s: rx list init failed\n", device_xname(sc->sc_dev));
		splx(s);
		return (EIO);
	}

	/* Load the multicast filter */
	udav_setmulti(sc);

	/* Enable RX */
	UDAV_SETBIT(sc, UDAV_RCR, UDAV_RCR_RXEN);

	/* clear POWER_DOWN state of internal PHY */
	UDAV_SETBIT(sc, UDAV_GPCR, UDAV_GPCR_GEP_CNTL0);
	UDAV_CLRBIT(sc, UDAV_GPR, UDAV_GPR_GEPIO0);

	if ((rc = mii_mediachg(mii)) == ENXIO)
		rc = 0;
	else if (rc != 0)
		goto out;

	if (sc->sc_pipe_tx == NULL || sc->sc_pipe_rx == NULL) {
		if (udav_openpipes(sc)) {
			splx(s);
			return (EIO);
		}
	}

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	callout_reset(&sc->sc_stat_ch, hz, udav_tick, sc);

out:
	splx(s);
	return rc;
}

Static void
udav_reset(struct udav_softc *sc)
{
	int i;

	DPRINTF(("%s: %s: enter\n", device_xname(sc->sc_dev), __func__));

	if (sc->sc_dying)
		return;

	/* Select PHY */
#if 1
	/*
	 * XXX: force select internal phy.
	 *	external phy routines are not tested.
	 */
	UDAV_CLRBIT(sc, UDAV_NCR, UDAV_NCR_EXT_PHY);
#else
	if (sc->sc_flags & UDAV_EXT_PHY) {
		UDAV_SETBIT(sc, UDAV_NCR, UDAV_NCR_EXT_PHY);
	} else {
		UDAV_CLRBIT(sc, UDAV_NCR, UDAV_NCR_EXT_PHY);
	}
#endif

	UDAV_SETBIT(sc, UDAV_NCR, UDAV_NCR_RST);

	for (i = 0; i < UDAV_TX_TIMEOUT; i++) {
		if (!(udav_csr_read1(sc, UDAV_NCR) & UDAV_NCR_RST))
			break;
		delay(10);	/* XXX */
	}
	delay(10000);		/* XXX */
}

int
udav_activate(device_t self, enum devact act)
{
	struct udav_softc *sc = device_private(self);

	DPRINTF(("%s: %s: enter, act=%d\n", device_xname(sc->sc_dev),
		 __func__, act));
	switch (act) {
	case DVACT_DEACTIVATE:
		if_deactivate(&sc->sc_ec.ec_if);
		sc->sc_dying = 1;
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

#define UDAV_BITS	6

#define UDAV_CALCHASH(addr) \
	(ether_crc32_le((addr), ETHER_ADDR_LEN) & ((1 << UDAV_BITS) - 1))

Static void
udav_setmulti(struct udav_softc *sc)
{
	struct ifnet *ifp;
	struct ether_multi *enm;
	struct ether_multistep step;
	u_int8_t hashes[8];
	int h = 0;

	DPRINTF(("%s: %s: enter\n", device_xname(sc->sc_dev), __func__));

	if (sc->sc_dying)
		return;

	ifp = GET_IFP(sc);

	if (ISSET(sc->sc_flags, UDAV_NO_PHY)) {
		UDAV_SETBIT(sc, UDAV_RCR, UDAV_RCR_ALL);
		UDAV_SETBIT(sc, UDAV_RCR, UDAV_RCR_PRMSC);
		return;
	}

	if (ifp->if_flags & IFF_PROMISC) {
		UDAV_SETBIT(sc, UDAV_RCR, UDAV_RCR_ALL|UDAV_RCR_PRMSC);
		return;
	} else if (ifp->if_flags & IFF_ALLMULTI) {
	allmulti:
		ifp->if_flags |= IFF_ALLMULTI;
		UDAV_SETBIT(sc, UDAV_RCR, UDAV_RCR_ALL);
		UDAV_CLRBIT(sc, UDAV_RCR, UDAV_RCR_PRMSC);
		return;
	}

	/* first, zot all the existing hash bits */
	memset(hashes, 0x00, sizeof(hashes));
	hashes[7] |= 0x80;	/* broadcast address */
	udav_csr_write(sc, UDAV_MAR, hashes, sizeof(hashes));

	/* now program new ones */
	ETHER_FIRST_MULTI(step, &sc->sc_ec, enm);
	while (enm != NULL) {
		if (memcmp(enm->enm_addrlo, enm->enm_addrhi,
			   ETHER_ADDR_LEN) != 0)
			goto allmulti;

		h = UDAV_CALCHASH(enm->enm_addrlo);
		hashes[h>>3] |= 1 << (h & 0x7);
		ETHER_NEXT_MULTI(step, enm);
	}

	/* disable all multicast */
	ifp->if_flags &= ~IFF_ALLMULTI;
	UDAV_CLRBIT(sc, UDAV_RCR, UDAV_RCR_ALL);

	/* write hash value to the register */
	udav_csr_write(sc, UDAV_MAR, hashes, sizeof(hashes));
}

Static int
udav_openpipes(struct udav_softc *sc)
{
	struct udav_chain *c;
	usbd_status err;
	int i;
	int error = 0;

	if (sc->sc_dying)
		return (EIO);

	sc->sc_refcnt++;

	/* Open RX pipe */
	err = usbd_open_pipe(sc->sc_ctl_iface, sc->sc_bulkin_no,
			     USBD_EXCLUSIVE_USE, &sc->sc_pipe_rx);
	if (err) {
		printf("%s: open rx pipe failed: %s\n",
		       device_xname(sc->sc_dev), usbd_errstr(err));
		error = EIO;
		goto done;
	}

	/* Open TX pipe */
	err = usbd_open_pipe(sc->sc_ctl_iface, sc->sc_bulkout_no,
			     USBD_EXCLUSIVE_USE, &sc->sc_pipe_tx);
	if (err) {
		printf("%s: open tx pipe failed: %s\n",
		       device_xname(sc->sc_dev), usbd_errstr(err));
		error = EIO;
		goto done;
	}

#if 0
	/* XXX: interrupt endpoint is not yet supported */
	/* Open Interrupt pipe */
	err = usbd_open_pipe_intr(sc->sc_ctl_iface, sc->sc_intrin_no,
				  USBD_EXCLUSIVE_USE, &sc->sc_pipe_intr, sc,
				  &sc->sc_cdata.udav_ibuf, UDAV_INTR_PKGLEN,
				  udav_intr, USBD_DEFAULT_INTERVAL);
	if (err) {
		printf("%s: open intr pipe failed: %s\n",
		       device_xname(sc->sc_dev), usbd_errstr(err));
		error = EIO;
		goto done;
	}
#endif


	/* Start up the receive pipe. */
	for (i = 0; i < UDAV_RX_LIST_CNT; i++) {
		c = &sc->sc_cdata.udav_rx_chain[i];
		usbd_setup_xfer(c->udav_xfer, sc->sc_pipe_rx,
				c, c->udav_buf, UDAV_BUFSZ,
				USBD_SHORT_XFER_OK | USBD_NO_COPY,
				USBD_NO_TIMEOUT, udav_rxeof);
		(void)usbd_transfer(c->udav_xfer);
		DPRINTF(("%s: %s: start read\n", device_xname(sc->sc_dev),
			 __func__));
	}

 done:
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeupold(sc->sc_dev);

	return (error);
}

Static int
udav_newbuf(struct udav_softc *sc, struct udav_chain *c, struct mbuf *m)
{
	struct mbuf *m_new = NULL;

	DPRINTF(("%s: %s: enter\n", device_xname(sc->sc_dev), __func__));

	if (m == NULL) {
		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL) {
			printf("%s: no memory for rx list "
			       "-- packet dropped!\n", device_xname(sc->sc_dev));
			return (ENOBUFS);
		}
		MCLGET(m_new, M_DONTWAIT);
		if (!(m_new->m_flags & M_EXT)) {
			printf("%s: no memory for rx list "
			       "-- packet dropped!\n", device_xname(sc->sc_dev));
			m_freem(m_new);
			return (ENOBUFS);
		}
		m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;
	} else {
		m_new = m;
		m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;
		m_new->m_data = m_new->m_ext.ext_buf;
	}

	m_adj(m_new, ETHER_ALIGN);
	c->udav_mbuf = m_new;

	return (0);
}


Static int
udav_rx_list_init(struct udav_softc *sc)
{
	struct udav_cdata *cd;
	struct udav_chain *c;
	int i;

	DPRINTF(("%s: %s: enter\n", device_xname(sc->sc_dev), __func__));

	cd = &sc->sc_cdata;
	for (i = 0; i < UDAV_RX_LIST_CNT; i++) {
		c = &cd->udav_rx_chain[i];
		c->udav_sc = sc;
		c->udav_idx = i;
		if (udav_newbuf(sc, c, NULL) == ENOBUFS)
			return (ENOBUFS);
		if (c->udav_xfer == NULL) {
			c->udav_xfer = usbd_alloc_xfer(sc->sc_udev);
			if (c->udav_xfer == NULL)
				return (ENOBUFS);
			c->udav_buf = usbd_alloc_buffer(c->udav_xfer, UDAV_BUFSZ);
			if (c->udav_buf == NULL) {
				usbd_free_xfer(c->udav_xfer);
				return (ENOBUFS);
			}
		}
	}

	return (0);
}

Static int
udav_tx_list_init(struct udav_softc *sc)
{
	struct udav_cdata *cd;
	struct udav_chain *c;
	int i;

	DPRINTF(("%s: %s: enter\n", device_xname(sc->sc_dev), __func__));

	cd = &sc->sc_cdata;
	for (i = 0; i < UDAV_TX_LIST_CNT; i++) {
		c = &cd->udav_tx_chain[i];
		c->udav_sc = sc;
		c->udav_idx = i;
		c->udav_mbuf = NULL;
		if (c->udav_xfer == NULL) {
			c->udav_xfer = usbd_alloc_xfer(sc->sc_udev);
			if (c->udav_xfer == NULL)
				return (ENOBUFS);
			c->udav_buf = usbd_alloc_buffer(c->udav_xfer, UDAV_BUFSZ);
			if (c->udav_buf == NULL) {
				usbd_free_xfer(c->udav_xfer);
				return (ENOBUFS);
			}
		}
	}

	return (0);
}

Static void
udav_start(struct ifnet *ifp)
{
	struct udav_softc *sc = ifp->if_softc;
	struct mbuf *m_head = NULL;

	DPRINTF(("%s: %s: enter, link=%d\n", device_xname(sc->sc_dev),
		 __func__, sc->sc_link));

	if (sc->sc_dying)
		return;

	if (!sc->sc_link)
		return;

	if (ifp->if_flags & IFF_OACTIVE)
		return;

	IFQ_POLL(&ifp->if_snd, m_head);
	if (m_head == NULL)
		return;

	if (udav_send(sc, m_head, 0)) {
		ifp->if_flags |= IFF_OACTIVE;
		return;
	}

	IFQ_DEQUEUE(&ifp->if_snd, m_head);

	bpf_mtap(ifp, m_head);

	ifp->if_flags |= IFF_OACTIVE;

	/* Set a timeout in case the chip goes out to lunch. */
	ifp->if_timer = 5;
}

Static int
udav_send(struct udav_softc *sc, struct mbuf *m, int idx)
{
	int total_len;
	struct udav_chain *c;
	usbd_status err;

	DPRINTF(("%s: %s: enter\n", device_xname(sc->sc_dev),__func__));

	c = &sc->sc_cdata.udav_tx_chain[idx];

	/* Copy the mbuf data into a contiguous buffer */
	/*  first 2 bytes are packet length */
	m_copydata(m, 0, m->m_pkthdr.len, c->udav_buf + 2);
	c->udav_mbuf = m;
	total_len = m->m_pkthdr.len;
	if (total_len < UDAV_MIN_FRAME_LEN) {
		memset(c->udav_buf + 2 + total_len, 0,
		    UDAV_MIN_FRAME_LEN - total_len);
		total_len = UDAV_MIN_FRAME_LEN;
	}

	/* Frame length is specified in the first 2bytes of the buffer */
	c->udav_buf[0] = (u_int8_t)total_len;
	c->udav_buf[1] = (u_int8_t)(total_len >> 8);
	total_len += 2;

	usbd_setup_xfer(c->udav_xfer, sc->sc_pipe_tx, c, c->udav_buf, total_len,
			USBD_FORCE_SHORT_XFER | USBD_NO_COPY,
			UDAV_TX_TIMEOUT, udav_txeof);

	/* Transmit */
	sc->sc_refcnt++;
	err = usbd_transfer(c->udav_xfer);
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeupold(sc->sc_dev);
	if (err != USBD_IN_PROGRESS) {
		printf("%s: udav_send error=%s\n", device_xname(sc->sc_dev),
		       usbd_errstr(err));
		/* Stop the interface */
		usb_add_task(sc->sc_udev, &sc->sc_stop_task,
		    USB_TASKQ_DRIVER);
		return (EIO);
	}

	DPRINTF(("%s: %s: send %d bytes\n", device_xname(sc->sc_dev),
		 __func__, total_len));

	sc->sc_cdata.udav_tx_cnt++;

	return (0);
}

Static void
udav_txeof(usbd_xfer_handle xfer, usbd_private_handle priv,
    usbd_status status)
{
	struct udav_chain *c = priv;
	struct udav_softc *sc = c->udav_sc;
	struct ifnet *ifp = GET_IFP(sc);
	int s;

	if (sc->sc_dying)
		return;

	s = splnet();

	DPRINTF(("%s: %s: enter\n", device_xname(sc->sc_dev), __func__));

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
		if (status == USBD_STALLED) {
			sc->sc_refcnt++;
			usbd_clear_endpoint_stall_async(sc->sc_pipe_tx);
			if (--sc->sc_refcnt < 0)
				usb_detach_wakeupold(sc->sc_dev);
		}
		splx(s);
		return;
	}

	ifp->if_opackets++;

	m_freem(c->udav_mbuf);
	c->udav_mbuf = NULL;

	if (IFQ_IS_EMPTY(&ifp->if_snd) == 0)
		udav_start(ifp);

	splx(s);
}

Static void
udav_rxeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct udav_chain *c = priv;
	struct udav_softc *sc = c->udav_sc;
	struct ifnet *ifp = GET_IFP(sc);
	struct mbuf *m;
	u_int32_t total_len;
	u_int8_t *pktstat;
	int s;

	DPRINTF(("%s: %s: enter\n", device_xname(sc->sc_dev),__func__));

	if (sc->sc_dying)
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;
		sc->sc_rx_errs++;
		if (usbd_ratecheck(&sc->sc_rx_notice)) {
			printf("%s: %u usb errors on rx: %s\n",
			       device_xname(sc->sc_dev), sc->sc_rx_errs,
			       usbd_errstr(status));
			sc->sc_rx_errs = 0;
		}
		if (status == USBD_STALLED) {
			sc->sc_refcnt++;
			usbd_clear_endpoint_stall_async(sc->sc_pipe_rx);
			if (--sc->sc_refcnt < 0)
				usb_detach_wakeupold(sc->sc_dev);
		}
		goto done;
	}

	usbd_get_xfer_status(xfer, NULL, NULL, &total_len, NULL);

	/* copy data to mbuf */
	m = c->udav_mbuf;
	memcpy(mtod(m, char *), c->udav_buf, total_len);

	/* first byte in received data */
	pktstat = mtod(m, u_int8_t *);
	m_adj(m, sizeof(u_int8_t));
	DPRINTF(("%s: RX Status: 0x%02x\n", device_xname(sc->sc_dev),
				*pktstat));

	total_len = UGETW(mtod(m, u_int8_t *));
	m_adj(m, sizeof(u_int16_t));

	if (*pktstat & UDAV_RSR_LCS) {
		ifp->if_collisions++;
		goto done;
	}

	if (total_len < sizeof(struct ether_header) ||
	    *pktstat & UDAV_RSR_ERR) {
		ifp->if_ierrors++;
		goto done;
	}

	ifp->if_ipackets++;
	total_len -= ETHER_CRC_LEN;

	m->m_pkthdr.len = m->m_len = total_len;
	m->m_pkthdr.rcvif = ifp;

	s = splnet();

	if (udav_newbuf(sc, c, NULL) == ENOBUFS) {
		ifp->if_ierrors++;
		goto done1;
	}

	bpf_mtap(ifp, m);

	DPRINTF(("%s: %s: deliver %d\n", device_xname(sc->sc_dev),
		 __func__, m->m_len));
	(*(ifp)->if_input)((ifp), (m));

 done1:
	splx(s);

 done:
	/* Setup new transfer */
	usbd_setup_xfer(xfer, sc->sc_pipe_rx, c, c->udav_buf, UDAV_BUFSZ,
			USBD_SHORT_XFER_OK | USBD_NO_COPY,
			USBD_NO_TIMEOUT, udav_rxeof);
	sc->sc_refcnt++;
	usbd_transfer(xfer);
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeupold(sc->sc_dev);

	DPRINTF(("%s: %s: start rx\n", device_xname(sc->sc_dev), __func__));
}

#if 0
Static void udav_intr(void)
{
}
#endif

Static int
udav_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct udav_softc *sc = ifp->if_softc;
	int s, error = 0;

	DPRINTF(("%s: %s: enter\n", device_xname(sc->sc_dev), __func__));

	if (sc->sc_dying)
		return (EIO);

	s = splnet();

	error = ether_ioctl(ifp, cmd, data);
	if (error == ENETRESET) {
		if (ifp->if_flags & IFF_RUNNING)
			udav_setmulti(sc);
		error = 0;
	}

	splx(s);

	return (error);
}

Static void
udav_watchdog(struct ifnet *ifp)
{
	struct udav_softc *sc = ifp->if_softc;
	struct udav_chain *c;
	usbd_status stat;
	int s;

	DPRINTF(("%s: %s: enter\n", device_xname(sc->sc_dev), __func__));

	ifp->if_oerrors++;
	printf("%s: watchdog timeout\n", device_xname(sc->sc_dev));

	s = splusb();
	c = &sc->sc_cdata.udav_tx_chain[0];
	usbd_get_xfer_status(c->udav_xfer, NULL, NULL, NULL, &stat);
	udav_txeof(c->udav_xfer, c, stat);

	if (IFQ_IS_EMPTY(&ifp->if_snd) == 0)
		udav_start(ifp);
	splx(s);
}

Static void
udav_stop_task(struct udav_softc *sc)
{
	udav_stop(GET_IFP(sc), 1);
}

/* Stop the adapter and free any mbufs allocated to the RX and TX lists. */
Static void
udav_stop(struct ifnet *ifp, int disable)
{
	struct udav_softc *sc = ifp->if_softc;
	usbd_status err;
	int i;

	DPRINTF(("%s: %s: enter\n", device_xname(sc->sc_dev), __func__));

	ifp->if_timer = 0;

	udav_reset(sc);

	callout_stop(&sc->sc_stat_ch);

	/* Stop transfers */
	/* RX endpoint */
	if (sc->sc_pipe_rx != NULL) {
		err = usbd_abort_pipe(sc->sc_pipe_rx);
		if (err)
			printf("%s: abort rx pipe failed: %s\n",
			       device_xname(sc->sc_dev), usbd_errstr(err));
		err = usbd_close_pipe(sc->sc_pipe_rx);
		if (err)
			printf("%s: close rx pipe failed: %s\n",
			       device_xname(sc->sc_dev), usbd_errstr(err));
		sc->sc_pipe_rx = NULL;
	}

	/* TX endpoint */
	if (sc->sc_pipe_tx != NULL) {
		err = usbd_abort_pipe(sc->sc_pipe_tx);
		if (err)
			printf("%s: abort tx pipe failed: %s\n",
			       device_xname(sc->sc_dev), usbd_errstr(err));
		err = usbd_close_pipe(sc->sc_pipe_tx);
		if (err)
			printf("%s: close tx pipe failed: %s\n",
			       device_xname(sc->sc_dev), usbd_errstr(err));
		sc->sc_pipe_tx = NULL;
	}

#if 0
	/* XXX: Interrupt endpoint is not yet supported!! */
	/* Interrupt endpoint */
	if (sc->sc_pipe_intr != NULL) {
		err = usbd_abort_pipe(sc->sc_pipe_intr);
		if (err)
			printf("%s: abort intr pipe failed: %s\n",
			       device_xname(sc->sc_dev), usbd_errstr(err));
		err = usbd_close_pipe(sc->sc_pipe_intr);
		if (err)
			printf("%s: close intr pipe failed: %s\n",
			       device_xname(sc->sc_dev), usbd_errstr(err));
		sc->sc_pipe_intr = NULL;
	}
#endif

	/* Free RX resources. */
	for (i = 0; i < UDAV_RX_LIST_CNT; i++) {
		if (sc->sc_cdata.udav_rx_chain[i].udav_mbuf != NULL) {
			m_freem(sc->sc_cdata.udav_rx_chain[i].udav_mbuf);
			sc->sc_cdata.udav_rx_chain[i].udav_mbuf = NULL;
		}
		if (sc->sc_cdata.udav_rx_chain[i].udav_xfer != NULL) {
			usbd_free_xfer(sc->sc_cdata.udav_rx_chain[i].udav_xfer);
			sc->sc_cdata.udav_rx_chain[i].udav_xfer = NULL;
		}
	}

	/* Free TX resources. */
	for (i = 0; i < UDAV_TX_LIST_CNT; i++) {
		if (sc->sc_cdata.udav_tx_chain[i].udav_mbuf != NULL) {
			m_freem(sc->sc_cdata.udav_tx_chain[i].udav_mbuf);
			sc->sc_cdata.udav_tx_chain[i].udav_mbuf = NULL;
		}
		if (sc->sc_cdata.udav_tx_chain[i].udav_xfer != NULL) {
			usbd_free_xfer(sc->sc_cdata.udav_tx_chain[i].udav_xfer);
			sc->sc_cdata.udav_tx_chain[i].udav_xfer = NULL;
		}
	}

	if (!ISSET(sc->sc_flags, UDAV_NO_PHY))
		sc->sc_link = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
}

/* Set media options */
Static int
udav_ifmedia_change(struct ifnet *ifp)
{
	struct udav_softc *sc = ifp->if_softc;
	struct mii_data *mii = GET_MII(sc);
	int rc;

	DPRINTF(("%s: %s: enter\n", device_xname(sc->sc_dev), __func__));

	if (sc->sc_dying)
		return (0);

	sc->sc_link = 0;
	if ((rc = mii_mediachg(mii)) == ENXIO)
		return 0;
	return rc;
}

/* Report current media status. */
Static void
udav_ifmedia_status(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct udav_softc *sc = ifp->if_softc;

	DPRINTF(("%s: %s: enter\n", device_xname(sc->sc_dev), __func__));

	if (sc->sc_dying)
		return;

	ether_mediastatus(ifp, ifmr);
}

Static void
udav_tick(void *xsc)
{
	struct udav_softc *sc = xsc;

	if (sc == NULL)
		return;

	DPRINTFN(0xff, ("%s: %s: enter\n", device_xname(sc->sc_dev),
			__func__));

	if (sc->sc_dying)
		return;

	/* Perform periodic stuff in process context */
	usb_add_task(sc->sc_udev, &sc->sc_tick_task,
	    USB_TASKQ_DRIVER);
}

Static void
udav_tick_task(void *xsc)
{
	struct udav_softc *sc = xsc;
	struct ifnet *ifp;
	struct mii_data *mii;
	int s;

	if (sc == NULL)
		return;

	DPRINTFN(0xff, ("%s: %s: enter\n", device_xname(sc->sc_dev),
			__func__));

	if (sc->sc_dying)
		return;

	ifp = GET_IFP(sc);
	mii = GET_MII(sc);

	if (mii == NULL)
		return;

	s = splnet();

	mii_tick(mii);
	if (!sc->sc_link) {
		mii_pollstat(mii);
		if (mii->mii_media_status & IFM_ACTIVE &&
		    IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE) {
			DPRINTF(("%s: %s: got link\n",
				 device_xname(sc->sc_dev), __func__));
			sc->sc_link++;
			if (IFQ_IS_EMPTY(&ifp->if_snd) == 0)
				   udav_start(ifp);
		}
	}

	callout_reset(&sc->sc_stat_ch, hz, udav_tick, sc);

	splx(s);
}

/* Get exclusive access to the MII registers */
Static void
udav_lock_mii(struct udav_softc *sc)
{
	DPRINTFN(0xff, ("%s: %s: enter\n", device_xname(sc->sc_dev),
			__func__));

	sc->sc_refcnt++;
	mutex_enter(&sc->sc_mii_lock);
}

Static void
udav_unlock_mii(struct udav_softc *sc)
{
	DPRINTFN(0xff, ("%s: %s: enter\n", device_xname(sc->sc_dev),
		       __func__));

	mutex_exit(&sc->sc_mii_lock);
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeupold(sc->sc_dev);
}

Static int
udav_miibus_readreg(device_t dev, int phy, int reg)
{
	struct udav_softc *sc;
	u_int8_t val[2];
	u_int16_t data16;

	if (dev == NULL)
		return (0);

	sc = device_private(dev);

	DPRINTFN(0xff, ("%s: %s: enter, phy=%d reg=0x%04x\n",
		 device_xname(sc->sc_dev), __func__, phy, reg));

	if (sc->sc_dying) {
#ifdef DIAGNOSTIC
		printf("%s: %s: dying\n", device_xname(sc->sc_dev),
		       __func__);
#endif
		return (0);
	}

	/* XXX: one PHY only for the internal PHY */
	if (phy != 0) {
		DPRINTFN(0xff, ("%s: %s: phy=%d is not supported\n",
			 device_xname(sc->sc_dev), __func__, phy));
		return (0);
	}

	udav_lock_mii(sc);

	/* select internal PHY and set PHY register address */
	udav_csr_write1(sc, UDAV_EPAR,
			UDAV_EPAR_PHY_ADR0 | (reg & UDAV_EPAR_EROA_MASK));

	/* select PHY operation and start read command */
	udav_csr_write1(sc, UDAV_EPCR, UDAV_EPCR_EPOS | UDAV_EPCR_ERPRR);

	/* XXX: should be wait? */

	/* end read command */
	UDAV_CLRBIT(sc, UDAV_EPCR, UDAV_EPCR_ERPRR);

	/* retrieve the result from data registers */
	udav_csr_read(sc, UDAV_EPDRL, val, 2);

	udav_unlock_mii(sc);

	data16 = val[0] | (val[1] << 8);

	DPRINTFN(0xff, ("%s: %s: phy=%d reg=0x%04x => 0x%04x\n",
		 device_xname(sc->sc_dev), __func__, phy, reg, data16));

	return (data16);
}

Static void
udav_miibus_writereg(device_t dev, int phy, int reg, int data)
{
	struct udav_softc *sc;
	u_int8_t val[2];

	if (dev == NULL)
		return;

	sc = device_private(dev);

	DPRINTFN(0xff, ("%s: %s: enter, phy=%d reg=0x%04x data=0x%04x\n",
		 device_xname(sc->sc_dev), __func__, phy, reg, data));

	if (sc->sc_dying) {
#ifdef DIAGNOSTIC
		printf("%s: %s: dying\n", device_xname(sc->sc_dev),
		       __func__);
#endif
		return;
	}

	/* XXX: one PHY only for the internal PHY */
	if (phy != 0) {
		DPRINTFN(0xff, ("%s: %s: phy=%d is not supported\n",
			 device_xname(sc->sc_dev), __func__, phy));
		return;
	}

	udav_lock_mii(sc);

	/* select internal PHY and set PHY register address */
	udav_csr_write1(sc, UDAV_EPAR,
			UDAV_EPAR_PHY_ADR0 | (reg & UDAV_EPAR_EROA_MASK));

	/* put the value to the data registers */
	val[0] = data & 0xff;
	val[1] = (data >> 8) & 0xff;
	udav_csr_write(sc, UDAV_EPDRL, val, 2);

	/* select PHY operation and start write command */
	udav_csr_write1(sc, UDAV_EPCR, UDAV_EPCR_EPOS | UDAV_EPCR_ERPRW);

	/* XXX: should be wait? */

	/* end write command */
	UDAV_CLRBIT(sc, UDAV_EPCR, UDAV_EPCR_ERPRW);

	udav_unlock_mii(sc);

	return;
}

Static void
udav_miibus_statchg(struct ifnet *ifp)
{
#ifdef UDAV_DEBUG

	if (ifp == NULL)
		return;

	DPRINTF(("%s: %s: enter\n", ifp->if_xname, __func__));
#endif
	/* Nothing to do */
}
