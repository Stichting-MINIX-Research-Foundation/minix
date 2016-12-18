/*	$NetBSD: if_upl.c,v 1.48 2015/04/13 16:33:25 riastradh Exp $	*/
/*
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
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
 * Prolific PL2301/PL2302 driver
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_upl.c,v 1.48 2015/04/13 16:33:25 riastradh Exp $");

#ifdef _KERNEL_OPT
#include "opt_inet.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>

#include <sys/device.h>
#include <sys/rndsource.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/netisr.h>

#include <net/bpf.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/if_inarp.h>
#endif


#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>

/*
 * 7  6  5  4  3  2  1  0
 * tx rx 1  0
 * 1110 0000 rxdata
 * 1010 0000 idle
 * 0010 0000 tx over
 * 0110      tx over + rxd
 */

#define UPL_RXDATA		0x40
#define UPL_TXOK		0x80

#define UPL_INTR_PKTLEN		1

#define UPL_CONFIG_NO		1
#define UPL_IFACE_IDX		0

/***/

#define UPL_INTR_INTERVAL	20

#define UPL_BUFSZ		1024

#define UPL_RX_FRAMES		1
#define UPL_TX_FRAMES		1

#define UPL_RX_LIST_CNT		1
#define UPL_TX_LIST_CNT		1

#define UPL_ENDPT_RX		0x0
#define UPL_ENDPT_TX		0x1
#define UPL_ENDPT_INTR		0x2
#define UPL_ENDPT_MAX		0x3

struct upl_type {
	u_int16_t		upl_vid;
	u_int16_t		upl_did;
};

struct upl_softc;

struct upl_chain {
	struct upl_softc	*upl_sc;
	usbd_xfer_handle	upl_xfer;
	char			*upl_buf;
	struct mbuf		*upl_mbuf;
	int			upl_idx;
};

struct upl_cdata {
	struct upl_chain	upl_tx_chain[UPL_TX_LIST_CNT];
	struct upl_chain	upl_rx_chain[UPL_RX_LIST_CNT];
	int			upl_tx_prod;
	int			upl_tx_cons;
	int			upl_tx_cnt;
	int			upl_rx_prod;
};

struct upl_softc {
	device_t		sc_dev;

	struct ifnet		sc_if;
	krndsource_t	sc_rnd_source;

	struct callout		sc_stat_ch;

	usbd_device_handle	sc_udev;
	usbd_interface_handle	sc_iface;
	u_int16_t		sc_vendor;
	u_int16_t		sc_product;
	int			sc_ed[UPL_ENDPT_MAX];
	usbd_pipe_handle	sc_ep[UPL_ENDPT_MAX];
	struct upl_cdata	sc_cdata;

	uByte			sc_ibuf;

	char			sc_dying;
	char			sc_attached;
	u_int			sc_rx_errs;
	struct timeval		sc_rx_notice;
	u_int			sc_intr_errs;
};

#ifdef UPL_DEBUG
#define DPRINTF(x)	if (upldebug) printf x
#define DPRINTFN(n,x)	if (upldebug >= (n)) printf x
int	upldebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

/*
 * Various supported device vendors/products.
 */
Static struct upl_type sc_devs[] = {
	{ USB_VENDOR_PROLIFIC, USB_PRODUCT_PROLIFIC_PL2301 },
	{ USB_VENDOR_PROLIFIC, USB_PRODUCT_PROLIFIC_PL2302 },
	{ 0, 0 }
};

int             upl_match(device_t, cfdata_t, void *);
void            upl_attach(device_t, device_t, void *);
int             upl_detach(device_t, int);
int             upl_activate(device_t, enum devact);
extern struct cfdriver upl_cd;
CFATTACH_DECL_NEW(upl, sizeof(struct upl_softc), upl_match, upl_attach, upl_detach, upl_activate);

Static int upl_openpipes(struct upl_softc *);
Static int upl_tx_list_init(struct upl_softc *);
Static int upl_rx_list_init(struct upl_softc *);
Static int upl_newbuf(struct upl_softc *, struct upl_chain *, struct mbuf *);
Static int upl_send(struct upl_softc *, struct mbuf *, int);
Static void upl_intr(usbd_xfer_handle, usbd_private_handle, usbd_status);
Static void upl_rxeof(usbd_xfer_handle, usbd_private_handle, usbd_status);
Static void upl_txeof(usbd_xfer_handle, usbd_private_handle, usbd_status);
Static void upl_start(struct ifnet *);
Static int upl_ioctl(struct ifnet *, u_long, void *);
Static void upl_init(void *);
Static void upl_stop(struct upl_softc *);
Static void upl_watchdog(struct ifnet *);

Static int upl_output(struct ifnet *, struct mbuf *, const struct sockaddr *,
		      struct rtentry *);
Static void upl_input(struct ifnet *, struct mbuf *);

/*
 * Probe for a Prolific chip.
 */
int
upl_match(device_t parent, cfdata_t match, void *aux)
{
	struct usb_attach_arg *uaa = aux;
	struct upl_type			*t;

	for (t = sc_devs; t->upl_vid != 0; t++)
		if (uaa->vendor == t->upl_vid && uaa->product == t->upl_did)
			return (UMATCH_VENDOR_PRODUCT);

	return (UMATCH_NONE);
}

void
upl_attach(device_t parent, device_t self, void *aux)
{
	struct upl_softc *sc = device_private(self);
	struct usb_attach_arg *uaa = aux;
	char			*devinfop;
	int			s;
	usbd_device_handle	dev = uaa->device;
	usbd_interface_handle	iface;
	usbd_status		err;
	struct ifnet		*ifp;
	usb_interface_descriptor_t	*id;
	usb_endpoint_descriptor_t	*ed;
	int			i;

	DPRINTFN(5,(" : upl_attach: sc=%p, dev=%p", sc, dev));

	sc->sc_dev = self;

	aprint_naive("\n");
	aprint_normal("\n");

	devinfop = usbd_devinfo_alloc(dev, 0);
	aprint_normal_dev(self, "%s\n", devinfop);
	usbd_devinfo_free(devinfop);

	err = usbd_set_config_no(dev, UPL_CONFIG_NO, 1);
	if (err) {
		aprint_error_dev(self, "failed to set configuration"
		    ", err=%s\n", usbd_errstr(err));
		return;
	}

	sc->sc_udev = dev;
	sc->sc_product = uaa->product;
	sc->sc_vendor = uaa->vendor;

	err = usbd_device2interface_handle(dev, UPL_IFACE_IDX, &iface);
	if (err) {
		aprint_error_dev(self, "getting interface handle failed\n");
		return;
	}

	sc->sc_iface = iface;
	id = usbd_get_interface_descriptor(iface);

	/* Find endpoints. */
	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(iface, i);
		if (ed == NULL) {
			aprint_error_dev(self, "couldn't get ep %d\n", i);
			return;
		}
		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->sc_ed[UPL_ENDPT_RX] = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
			   UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->sc_ed[UPL_ENDPT_TX] = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
			   UE_GET_XFERTYPE(ed->bmAttributes) == UE_INTERRUPT) {
			sc->sc_ed[UPL_ENDPT_INTR] = ed->bEndpointAddress;
		}
	}

	if (sc->sc_ed[UPL_ENDPT_RX] == 0 || sc->sc_ed[UPL_ENDPT_TX] == 0 ||
	    sc->sc_ed[UPL_ENDPT_INTR] == 0) {
		aprint_error_dev(self, "missing endpoint\n");
		return;
	}

	s = splnet();

	/* Initialize interface info.*/
	ifp = &sc->sc_if;
	ifp->if_softc = sc;
	ifp->if_mtu = UPL_BUFSZ;
	ifp->if_flags = IFF_POINTOPOINT | IFF_NOARP | IFF_SIMPLEX;
	ifp->if_ioctl = upl_ioctl;
	ifp->if_start = upl_start;
	ifp->if_watchdog = upl_watchdog;
	strncpy(ifp->if_xname, device_xname(sc->sc_dev), IFNAMSIZ);

	ifp->if_type = IFT_OTHER;
	ifp->if_addrlen = 0;
	ifp->if_hdrlen = 0;
	ifp->if_output = upl_output;
	ifp->if_input = upl_input;
	ifp->if_baudrate = 12000000;
	ifp->if_dlt = DLT_RAW;
	IFQ_SET_READY(&ifp->if_snd);

	/* Attach the interface. */
	if_attach(ifp);
	if_alloc_sadl(ifp);

	bpf_attach(ifp, DLT_RAW, 0);
	rnd_attach_source(&sc->sc_rnd_source, device_xname(sc->sc_dev),
	    RND_TYPE_NET, RND_FLAG_DEFAULT);

	sc->sc_attached = 1;
	splx(s);

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
	    sc->sc_dev);

	return;
}

int
upl_detach(device_t self, int flags)
{
	struct upl_softc *sc = device_private(self);
	struct ifnet		*ifp = &sc->sc_if;
	int			s;

	DPRINTFN(2,("%s: %s: enter\n", device_xname(sc->sc_dev), __func__));

	s = splusb();

	if (!sc->sc_attached) {
		/* Detached before attached finished, so just bail out. */
		splx(s);
		return (0);
	}

	if (ifp->if_flags & IFF_RUNNING)
		upl_stop(sc);

	rnd_detach_source(&sc->sc_rnd_source);
	bpf_detach(ifp);

	if_detach(ifp);

#ifdef DIAGNOSTIC
	if (sc->sc_ep[UPL_ENDPT_TX] != NULL ||
	    sc->sc_ep[UPL_ENDPT_RX] != NULL ||
	    sc->sc_ep[UPL_ENDPT_INTR] != NULL)
		aprint_debug_dev(self, "detach has active endpoints\n");
#endif

	sc->sc_attached = 0;
	splx(s);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
	    sc->sc_dev);

	return (0);
}

int
upl_activate(device_t self, enum devact act)
{
	struct upl_softc *sc = device_private(self);

	DPRINTFN(2,("%s: %s: enter\n", device_xname(sc->sc_dev), __func__));

	switch (act) {
	case DVACT_DEACTIVATE:
		/* Deactivate the interface. */
		if_deactivate(&sc->sc_if);
		sc->sc_dying = 1;
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

/*
 * Initialize an RX descriptor and attach an MBUF cluster.
 */
Static int
upl_newbuf(struct upl_softc *sc, struct upl_chain *c, struct mbuf *m)
{
	struct mbuf		*m_new = NULL;

	DPRINTFN(8,("%s: %s: enter\n", device_xname(sc->sc_dev), __func__));

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

	c->upl_mbuf = m_new;

	return (0);
}

Static int
upl_rx_list_init(struct upl_softc *sc)
{
	struct upl_cdata	*cd;
	struct upl_chain	*c;
	int			i;

	DPRINTFN(5,("%s: %s: enter\n", device_xname(sc->sc_dev), __func__));

	cd = &sc->sc_cdata;
	for (i = 0; i < UPL_RX_LIST_CNT; i++) {
		c = &cd->upl_rx_chain[i];
		c->upl_sc = sc;
		c->upl_idx = i;
		if (upl_newbuf(sc, c, NULL) == ENOBUFS)
			return (ENOBUFS);
		if (c->upl_xfer == NULL) {
			c->upl_xfer = usbd_alloc_xfer(sc->sc_udev);
			if (c->upl_xfer == NULL)
				return (ENOBUFS);
			c->upl_buf = usbd_alloc_buffer(c->upl_xfer, UPL_BUFSZ);
			if (c->upl_buf == NULL) {
				usbd_free_xfer(c->upl_xfer);
				return (ENOBUFS);
			}
		}
	}

	return (0);
}

Static int
upl_tx_list_init(struct upl_softc *sc)
{
	struct upl_cdata	*cd;
	struct upl_chain	*c;
	int			i;

	DPRINTFN(5,("%s: %s: enter\n", device_xname(sc->sc_dev), __func__));

	cd = &sc->sc_cdata;
	for (i = 0; i < UPL_TX_LIST_CNT; i++) {
		c = &cd->upl_tx_chain[i];
		c->upl_sc = sc;
		c->upl_idx = i;
		c->upl_mbuf = NULL;
		if (c->upl_xfer == NULL) {
			c->upl_xfer = usbd_alloc_xfer(sc->sc_udev);
			if (c->upl_xfer == NULL)
				return (ENOBUFS);
			c->upl_buf = usbd_alloc_buffer(c->upl_xfer, UPL_BUFSZ);
			if (c->upl_buf == NULL) {
				usbd_free_xfer(c->upl_xfer);
				return (ENOBUFS);
			}
		}
	}

	return (0);
}

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 */
Static void
upl_rxeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct upl_chain	*c = priv;
	struct upl_softc	*sc = c->upl_sc;
	struct ifnet		*ifp = &sc->sc_if;
	struct mbuf		*m;
	int			total_len = 0;
	int			s;

	if (sc->sc_dying)
		return;

	if (!(ifp->if_flags & IFF_RUNNING))
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
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->sc_ep[UPL_ENDPT_RX]);
		goto done;
	}

	usbd_get_xfer_status(xfer, NULL, NULL, &total_len, NULL);

	DPRINTFN(9,("%s: %s: enter status=%d length=%d\n",
		    device_xname(sc->sc_dev), __func__, status, total_len));

	m = c->upl_mbuf;
	memcpy(mtod(c->upl_mbuf, char *), c->upl_buf, total_len);

	ifp->if_ipackets++;
	m->m_pkthdr.len = m->m_len = total_len;

	m->m_pkthdr.rcvif = ifp;

	s = splnet();

	/* XXX ugly */
	if (upl_newbuf(sc, c, NULL) == ENOBUFS) {
		ifp->if_ierrors++;
		goto done1;
	}

	/*
	 * Handle BPF listeners. Let the BPF user see the packet, but
	 * don't pass it up to the ether_input() layer unless it's
	 * a broadcast packet, multicast packet, matches our ethernet
	 * address or the interface is in promiscuous mode.
	 */
	bpf_mtap(ifp, m);

	DPRINTFN(10,("%s: %s: deliver %d\n", device_xname(sc->sc_dev),
		    __func__, m->m_len));

	(*(ifp)->if_input)((ifp), (m));

 done1:
	splx(s);

 done:
#if 1
	/* Setup new transfer. */
	usbd_setup_xfer(c->upl_xfer, sc->sc_ep[UPL_ENDPT_RX],
	    c, c->upl_buf, UPL_BUFSZ, USBD_SHORT_XFER_OK | USBD_NO_COPY,
	    USBD_NO_TIMEOUT, upl_rxeof);
	usbd_transfer(c->upl_xfer);

	DPRINTFN(10,("%s: %s: start rx\n", device_xname(sc->sc_dev),
		    __func__));
#endif
}

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */
Static void
upl_txeof(usbd_xfer_handle xfer, usbd_private_handle priv,
    usbd_status status)
{
	struct upl_chain	*c = priv;
	struct upl_softc	*sc = c->upl_sc;
	struct ifnet		*ifp = &sc->sc_if;
	int			s;

	if (sc->sc_dying)
		return;

	s = splnet();

	DPRINTFN(10,("%s: %s: enter status=%d\n", device_xname(sc->sc_dev),
		    __func__, status));

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
			usbd_clear_endpoint_stall_async(sc->sc_ep[UPL_ENDPT_TX]);
		splx(s);
		return;
	}

	ifp->if_opackets++;

	m_freem(c->upl_mbuf);
	c->upl_mbuf = NULL;

	if (IFQ_IS_EMPTY(&ifp->if_snd) == 0)
		upl_start(ifp);

	splx(s);
}

Static int
upl_send(struct upl_softc *sc, struct mbuf *m, int idx)
{
	int			total_len;
	struct upl_chain	*c;
	usbd_status		err;

	c = &sc->sc_cdata.upl_tx_chain[idx];

	/*
	 * Copy the mbuf data into a contiguous buffer, leaving two
	 * bytes at the beginning to hold the frame length.
	 */
	m_copydata(m, 0, m->m_pkthdr.len, c->upl_buf);
	c->upl_mbuf = m;

	total_len = m->m_pkthdr.len;

	DPRINTFN(10,("%s: %s: total_len=%d\n",
		     device_xname(sc->sc_dev), __func__, total_len));

	usbd_setup_xfer(c->upl_xfer, sc->sc_ep[UPL_ENDPT_TX],
	    c, c->upl_buf, total_len, USBD_NO_COPY, USBD_DEFAULT_TIMEOUT,
	    upl_txeof);

	/* Transmit */
	err = usbd_transfer(c->upl_xfer);
	if (err != USBD_IN_PROGRESS) {
		printf("%s: upl_send error=%s\n", device_xname(sc->sc_dev),
		       usbd_errstr(err));
		upl_stop(sc);
		return (EIO);
	}

	sc->sc_cdata.upl_tx_cnt++;

	return (0);
}

Static void
upl_start(struct ifnet *ifp)
{
	struct upl_softc	*sc = ifp->if_softc;
	struct mbuf		*m_head = NULL;

	if (sc->sc_dying)
		return;

	DPRINTFN(10,("%s: %s: enter\n", device_xname(sc->sc_dev),__func__));

	if (ifp->if_flags & IFF_OACTIVE)
		return;

	IFQ_POLL(&ifp->if_snd, m_head);
	if (m_head == NULL)
		return;

	if (upl_send(sc, m_head, 0)) {
		ifp->if_flags |= IFF_OACTIVE;
		return;
	}

	IFQ_DEQUEUE(&ifp->if_snd, m_head);

	/*
	 * If there's a BPF listener, bounce a copy of this frame
	 * to him.
	 */
	bpf_mtap(ifp, m_head);

	ifp->if_flags |= IFF_OACTIVE;

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;
}

Static void
upl_init(void *xsc)
{
	struct upl_softc	*sc = xsc;
	struct ifnet		*ifp = &sc->sc_if;
	int			s;

	if (sc->sc_dying)
		return;

	DPRINTFN(10,("%s: %s: enter\n", device_xname(sc->sc_dev),__func__));

	if (ifp->if_flags & IFF_RUNNING)
		return;

	s = splnet();

	/* Init TX ring. */
	if (upl_tx_list_init(sc) == ENOBUFS) {
		printf("%s: tx list init failed\n", device_xname(sc->sc_dev));
		splx(s);
		return;
	}

	/* Init RX ring. */
	if (upl_rx_list_init(sc) == ENOBUFS) {
		printf("%s: rx list init failed\n", device_xname(sc->sc_dev));
		splx(s);
		return;
	}

	if (sc->sc_ep[UPL_ENDPT_RX] == NULL) {
		if (upl_openpipes(sc)) {
			splx(s);
			return;
		}
	}

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	splx(s);
}

Static int
upl_openpipes(struct upl_softc *sc)
{
	struct upl_chain	*c;
	usbd_status		err;
	int			i;

	/* Open RX and TX pipes. */
	err = usbd_open_pipe(sc->sc_iface, sc->sc_ed[UPL_ENDPT_RX],
	    USBD_EXCLUSIVE_USE, &sc->sc_ep[UPL_ENDPT_RX]);
	if (err) {
		printf("%s: open rx pipe failed: %s\n",
		    device_xname(sc->sc_dev), usbd_errstr(err));
		return (EIO);
	}
	err = usbd_open_pipe(sc->sc_iface, sc->sc_ed[UPL_ENDPT_TX],
	    USBD_EXCLUSIVE_USE, &sc->sc_ep[UPL_ENDPT_TX]);
	if (err) {
		printf("%s: open tx pipe failed: %s\n",
		    device_xname(sc->sc_dev), usbd_errstr(err));
		return (EIO);
	}
	err = usbd_open_pipe_intr(sc->sc_iface, sc->sc_ed[UPL_ENDPT_INTR],
	    USBD_EXCLUSIVE_USE, &sc->sc_ep[UPL_ENDPT_INTR], sc,
	    &sc->sc_ibuf, UPL_INTR_PKTLEN, upl_intr,
	    UPL_INTR_INTERVAL);
	if (err) {
		printf("%s: open intr pipe failed: %s\n",
		    device_xname(sc->sc_dev), usbd_errstr(err));
		return (EIO);
	}


#if 1
	/* Start up the receive pipe. */
	for (i = 0; i < UPL_RX_LIST_CNT; i++) {
		c = &sc->sc_cdata.upl_rx_chain[i];
		usbd_setup_xfer(c->upl_xfer, sc->sc_ep[UPL_ENDPT_RX],
		    c, c->upl_buf, UPL_BUFSZ,
		    USBD_SHORT_XFER_OK | USBD_NO_COPY, USBD_NO_TIMEOUT,
		    upl_rxeof);
		usbd_transfer(c->upl_xfer);
	}
#endif

	return (0);
}

Static void
upl_intr(usbd_xfer_handle xfer, usbd_private_handle priv,
    usbd_status status)
{
	struct upl_softc	*sc = priv;
	struct ifnet		*ifp = &sc->sc_if;
	uByte			stat;

	DPRINTFN(15,("%s: %s: enter\n", device_xname(sc->sc_dev),__func__));

	if (sc->sc_dying)
		return;

	if (!(ifp->if_flags & IFF_RUNNING))
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED) {
			return;
		}
		sc->sc_intr_errs++;
		if (usbd_ratecheck(&sc->sc_rx_notice)) {
			printf("%s: %u usb errors on intr: %s\n",
			    device_xname(sc->sc_dev), sc->sc_rx_errs,
			    usbd_errstr(status));
			sc->sc_intr_errs = 0;
		}
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->sc_ep[UPL_ENDPT_RX]);
		return;
	}

	stat = sc->sc_ibuf;

	if (stat == 0)
		return;

	DPRINTFN(10,("%s: %s: stat=0x%02x\n", device_xname(sc->sc_dev),
		     __func__, stat));

}

Static int
upl_ioctl(struct ifnet *ifp, u_long command, void *data)
{
	struct upl_softc	*sc = ifp->if_softc;
	struct ifaddr 		*ifa = (struct ifaddr *)data;
	struct ifreq		*ifr = (struct ifreq *)data;
	int			s, error = 0;

	if (sc->sc_dying)
		return (EIO);

	DPRINTFN(5,("%s: %s: cmd=0x%08lx\n",
		    device_xname(sc->sc_dev), __func__, command));

	s = splnet();

	switch(command) {
	case SIOCINITIFADDR:
		ifp->if_flags |= IFF_UP;
		upl_init(sc);

		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			break;
#endif /* INET */
		}
		break;

	case SIOCSIFMTU:
		if (ifr->ifr_mtu > UPL_BUFSZ)
			error = EINVAL;
		else if ((error = ifioctl_common(ifp, command, data)) == ENETRESET)
			error = 0;
		break;

	case SIOCSIFFLAGS:
		if ((error = ifioctl_common(ifp, command, data)) != 0)
			break;
		/* XXX re-use ether_ioctl() */
		switch (ifp->if_flags & (IFF_UP|IFF_RUNNING)) {
		case IFF_UP:
			upl_init(sc);
			break;
		case IFF_RUNNING:
			upl_stop(sc);
			break;
		default:
			break;
		}
		break;
	default:
		error = ifioctl_common(ifp, command, data);
		break;
	}

	splx(s);

	return (error);
}

Static void
upl_watchdog(struct ifnet *ifp)
{
	struct upl_softc	*sc = ifp->if_softc;

	DPRINTFN(5,("%s: %s: enter\n", device_xname(sc->sc_dev),__func__));

	if (sc->sc_dying)
		return;

	ifp->if_oerrors++;
	printf("%s: watchdog timeout\n", device_xname(sc->sc_dev));

	upl_stop(sc);
	upl_init(sc);

	if (IFQ_IS_EMPTY(&ifp->if_snd) == 0)
		upl_start(ifp);
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
Static void
upl_stop(struct upl_softc *sc)
{
	usbd_status		err;
	struct ifnet		*ifp;
	int			i;

	DPRINTFN(10,("%s: %s: enter\n", device_xname(sc->sc_dev),__func__));

	ifp = &sc->sc_if;
	ifp->if_timer = 0;

	/* Stop transfers. */
	if (sc->sc_ep[UPL_ENDPT_RX] != NULL) {
		err = usbd_abort_pipe(sc->sc_ep[UPL_ENDPT_RX]);
		if (err) {
			printf("%s: abort rx pipe failed: %s\n",
			device_xname(sc->sc_dev), usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->sc_ep[UPL_ENDPT_RX]);
		if (err) {
			printf("%s: close rx pipe failed: %s\n",
			device_xname(sc->sc_dev), usbd_errstr(err));
		}
		sc->sc_ep[UPL_ENDPT_RX] = NULL;
	}

	if (sc->sc_ep[UPL_ENDPT_TX] != NULL) {
		err = usbd_abort_pipe(sc->sc_ep[UPL_ENDPT_TX]);
		if (err) {
			printf("%s: abort tx pipe failed: %s\n",
			device_xname(sc->sc_dev), usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->sc_ep[UPL_ENDPT_TX]);
		if (err) {
			printf("%s: close tx pipe failed: %s\n",
			    device_xname(sc->sc_dev), usbd_errstr(err));
		}
		sc->sc_ep[UPL_ENDPT_TX] = NULL;
	}

	if (sc->sc_ep[UPL_ENDPT_INTR] != NULL) {
		err = usbd_abort_pipe(sc->sc_ep[UPL_ENDPT_INTR]);
		if (err) {
			printf("%s: abort intr pipe failed: %s\n",
			device_xname(sc->sc_dev), usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->sc_ep[UPL_ENDPT_INTR]);
		if (err) {
			printf("%s: close intr pipe failed: %s\n",
			    device_xname(sc->sc_dev), usbd_errstr(err));
		}
		sc->sc_ep[UPL_ENDPT_INTR] = NULL;
	}

	/* Free RX resources. */
	for (i = 0; i < UPL_RX_LIST_CNT; i++) {
		if (sc->sc_cdata.upl_rx_chain[i].upl_mbuf != NULL) {
			m_freem(sc->sc_cdata.upl_rx_chain[i].upl_mbuf);
			sc->sc_cdata.upl_rx_chain[i].upl_mbuf = NULL;
		}
		if (sc->sc_cdata.upl_rx_chain[i].upl_xfer != NULL) {
			usbd_free_xfer(sc->sc_cdata.upl_rx_chain[i].upl_xfer);
			sc->sc_cdata.upl_rx_chain[i].upl_xfer = NULL;
		}
	}

	/* Free TX resources. */
	for (i = 0; i < UPL_TX_LIST_CNT; i++) {
		if (sc->sc_cdata.upl_tx_chain[i].upl_mbuf != NULL) {
			m_freem(sc->sc_cdata.upl_tx_chain[i].upl_mbuf);
			sc->sc_cdata.upl_tx_chain[i].upl_mbuf = NULL;
		}
		if (sc->sc_cdata.upl_tx_chain[i].upl_xfer != NULL) {
			usbd_free_xfer(sc->sc_cdata.upl_tx_chain[i].upl_xfer);
			sc->sc_cdata.upl_tx_chain[i].upl_xfer = NULL;
		}
	}

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
}

Static int
upl_output(struct ifnet *ifp, struct mbuf *m, const struct sockaddr *dst,
    struct rtentry *rt0)
{
	int s, len, error;
	ALTQ_DECL(struct altq_pktattr pktattr;)

	DPRINTFN(10,("%s: %s: enter\n",
		     device_xname(((struct upl_softc *)ifp->if_softc)->sc_dev),
		     __func__));

	/*
	 * if the queueing discipline needs packet classification,
	 * do it now.
	 */
	IFQ_CLASSIFY(&ifp->if_snd, m, dst->sa_family, &pktattr);

	len = m->m_pkthdr.len;
	s = splnet();
	/*
	 * Queue message on interface, and start output if interface
	 * not yet active.
	 */
	IFQ_ENQUEUE(&ifp->if_snd, m, &pktattr, error);
	if (error) {
		/* mbuf is already freed */
		splx(s);
		return (error);
	}
	ifp->if_obytes += len;
	if ((ifp->if_flags & IFF_OACTIVE) == 0)
		(*ifp->if_start)(ifp);
	splx(s);

	return (0);
}

Static void
upl_input(struct ifnet *ifp, struct mbuf *m)
{
#ifdef INET
	size_t pktlen = m->m_len;
	int s;

	s = splnet();
	if (__predict_false(!pktq_enqueue(ip_pktq, m, 0))) {
		ifp->if_iqdrops++;
		m_freem(m);
	} else {
		ifp->if_ipackets++;
		ifp->if_ibytes += pktlen;
	}
	splx(s);
#endif
}
