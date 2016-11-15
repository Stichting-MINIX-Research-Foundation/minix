/*	$NetBSD: motg.c,v 1.13 2015/08/19 06:23:35 skrll Exp $	*/

/*
 * Copyright (c) 1998, 2004, 2011, 2012, 2014 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology, Jared D. McNeill (jmcneill@invisible.ca),
 * Matthew R. Green (mrg@eterna.com.au), and Manuel Bouyer (bouyer@netbsd.org).
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
 * This file contains the driver for the Mentor Graphics Inventra USB
 * 2.0 High Speed Dual-Role controller.
 *
 * NOTE: The current implementation only supports Device Side Mode!
 */

#include "opt_motg.h"

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: motg.c,v 1.13 2015/08/19 06:23:35 skrll Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/device.h>
#include <sys/select.h>
#include <sys/extent.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/bus.h>
#include <sys/cpu.h>

#include <machine/endian.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>
#include <dev/usb/usb_quirks.h>

#ifdef MOTG_ALLWINNER
#include <arch/arm/allwinner/awin_otgreg.h>
#else
#include <dev/usb/motgreg.h>
#endif

#include <dev/usb/motgvar.h>
#include <dev/usb/usbroothub_subr.h>

#define MOTG_DEBUG
#ifdef MOTG_DEBUG
#define DPRINTF(x)	if (motgdebug) printf x
#define DPRINTFN(n,x)	if (motgdebug & (n)) printf x
#define MD_ROOT 0x0002
#define MD_CTRL 0x0004
#define MD_BULK 0x0008
// int motgdebug = MD_ROOT | MD_CTRL | MD_BULK;
int motgdebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

/* various timeouts, for various speeds */
/* control NAK timeouts */
#define NAK_TO_CTRL	10	/* 1024 frames, about 1s */
#define NAK_TO_CTRL_HIGH 13	/* 8k microframes, about 0.8s */

/* intr/iso polling intervals */
#define POLL_TO		100	/* 100 frames, about 0.1s */
#define POLL_TO_HIGH	10	/* 100 microframes, about 0.12s */

/* bulk NAK timeouts */
#define NAK_TO_BULK	0 /* disabled */
#define NAK_TO_BULK_HIGH 0

static void 		motg_hub_change(struct motg_softc *);
static usbd_status	motg_root_ctrl_transfer(usbd_xfer_handle);
static usbd_status	motg_root_ctrl_start(usbd_xfer_handle);
static void		motg_root_ctrl_abort(usbd_xfer_handle);
static void		motg_root_ctrl_close(usbd_pipe_handle);
static void		motg_root_ctrl_done(usbd_xfer_handle);

static usbd_status	motg_root_intr_transfer(usbd_xfer_handle);
static usbd_status	motg_root_intr_start(usbd_xfer_handle);
static void		motg_root_intr_abort(usbd_xfer_handle);
static void		motg_root_intr_close(usbd_pipe_handle);
static void		motg_root_intr_done(usbd_xfer_handle);

static usbd_status	motg_open(usbd_pipe_handle);
static void		motg_poll(struct usbd_bus *);
static void		motg_softintr(void *);
static usbd_status	motg_allocm(struct usbd_bus *, usb_dma_t *, u_int32_t);
static void		motg_freem(struct usbd_bus *, usb_dma_t *);
static usbd_xfer_handle	motg_allocx(struct usbd_bus *);
static void		motg_freex(struct usbd_bus *, usbd_xfer_handle);
static void		motg_get_lock(struct usbd_bus *, kmutex_t **);
static void		motg_noop(usbd_pipe_handle pipe);
static usbd_status	motg_portreset(struct motg_softc*);

static usbd_status	motg_device_ctrl_transfer(usbd_xfer_handle);
static usbd_status	motg_device_ctrl_start(usbd_xfer_handle);
static void		motg_device_ctrl_abort(usbd_xfer_handle);
static void		motg_device_ctrl_close(usbd_pipe_handle);
static void		motg_device_ctrl_done(usbd_xfer_handle);
static usbd_status	motg_device_ctrl_start1(struct motg_softc *);
static void		motg_device_ctrl_read(usbd_xfer_handle);
static void		motg_device_ctrl_intr_rx(struct motg_softc *);
static void		motg_device_ctrl_intr_tx(struct motg_softc *);

static usbd_status	motg_device_data_transfer(usbd_xfer_handle);
static usbd_status	motg_device_data_start(usbd_xfer_handle);
static usbd_status	motg_device_data_start1(struct motg_softc *,
			    struct motg_hw_ep *);
static void		motg_device_data_abort(usbd_xfer_handle);
static void		motg_device_data_close(usbd_pipe_handle);
static void		motg_device_data_done(usbd_xfer_handle);
static void		motg_device_intr_rx(struct motg_softc *, int);
static void		motg_device_intr_tx(struct motg_softc *, int);
static void		motg_device_data_read(usbd_xfer_handle);
static void		motg_device_data_write(usbd_xfer_handle);

static void		motg_waitintr(struct motg_softc *, usbd_xfer_handle);
static void		motg_device_clear_toggle(usbd_pipe_handle);
static void		motg_device_xfer_abort(usbd_xfer_handle);

#define MOTG_INTR_ENDPT 1
#define UBARR(sc) bus_space_barrier((sc)->sc_iot, (sc)->sc_ioh, 0, (sc)->sc_size, \
			BUS_SPACE_BARRIER_READ|BUS_SPACE_BARRIER_WRITE)
#define UWRITE1(sc, r, x) \
 do { UBARR(sc); bus_space_write_1((sc)->sc_iot, (sc)->sc_ioh, (r), (x)); \
 } while (/*CONSTCOND*/0)
#define UWRITE2(sc, r, x) \
 do { UBARR(sc); bus_space_write_2((sc)->sc_iot, (sc)->sc_ioh, (r), (x)); \
 } while (/*CONSTCOND*/0)
#define UWRITE4(sc, r, x) \
 do { UBARR(sc); bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (r), (x)); \
 } while (/*CONSTCOND*/0)

static __inline uint32_t
UREAD1(struct motg_softc *sc, bus_size_t r)
{

	UBARR(sc);
	return bus_space_read_1(sc->sc_iot, sc->sc_ioh, r);
}
static __inline uint32_t
UREAD2(struct motg_softc *sc, bus_size_t r)
{

	UBARR(sc);
	return bus_space_read_2(sc->sc_iot, sc->sc_ioh, r);
}

#if 0
static __inline uint32_t
UREAD4(struct motg_softc *sc, bus_size_t r)
{

	UBARR(sc);
	return bus_space_read_4(sc->sc_iot, sc->sc_ioh, r);
}
#endif

static void
musbotg_pull_common(struct motg_softc *sc, uint8_t on)
{
        uint8_t val;

        val = UREAD1(sc, MUSB2_REG_POWER);
        if (on)
                val |= MUSB2_MASK_SOFTC;
        else
                val &= ~MUSB2_MASK_SOFTC;

        UWRITE1(sc, MUSB2_REG_POWER, val);
}

const struct usbd_bus_methods motg_bus_methods = {
	.open_pipe =	motg_open,
	.soft_intr =	motg_softintr,
	.do_poll =	motg_poll,
	.allocm =	motg_allocm,
	.freem =	motg_freem,
	.allocx =	motg_allocx,
	.freex =	motg_freex,
	.get_lock =	motg_get_lock,
	.new_device =	NULL,
};

const struct usbd_pipe_methods motg_root_ctrl_methods = {
	.transfer =	motg_root_ctrl_transfer,
	.start =	motg_root_ctrl_start,
	.abort =	motg_root_ctrl_abort,
	.close =	motg_root_ctrl_close,
	.cleartoggle =	motg_noop,
	.done =		motg_root_ctrl_done,
};

const struct usbd_pipe_methods motg_root_intr_methods = {
	.transfer =	motg_root_intr_transfer,
	.start =	motg_root_intr_start,
	.abort =	motg_root_intr_abort,
	.close =	motg_root_intr_close,
	.cleartoggle =	motg_noop,
	.done =		motg_root_intr_done,
};

const struct usbd_pipe_methods motg_device_ctrl_methods = {
	.transfer =	motg_device_ctrl_transfer,
	.start =	motg_device_ctrl_start,
	.abort =	motg_device_ctrl_abort,
	.close =	motg_device_ctrl_close,
	.cleartoggle =	motg_noop,
	.done =		motg_device_ctrl_done,
};

const struct usbd_pipe_methods motg_device_data_methods = {
	.transfer =	motg_device_data_transfer,
	.start =	motg_device_data_start,
	.abort =	motg_device_data_abort,
	.close =	motg_device_data_close,
	.cleartoggle =	motg_device_clear_toggle,
	.done =		motg_device_data_done,
};

usbd_status
motg_init(struct motg_softc *sc)
{
	uint32_t nrx, ntx, val;
	int dynfifo;
	int offset, i;

	if (sc->sc_mode == MOTG_MODE_DEVICE)
		return USBD_NORMAL_COMPLETION; /* not supported */

	/* disable all interrupts */
	UWRITE1(sc, MUSB2_REG_INTUSBE, 0);
	UWRITE2(sc, MUSB2_REG_INTTXE, 0);
	UWRITE2(sc, MUSB2_REG_INTRXE, 0);
	/* disable pullup */

	musbotg_pull_common(sc, 0);

#ifdef MUSB2_REG_RXDBDIS
	/* disable double packet buffering XXX what's this ? */
	UWRITE2(sc, MUSB2_REG_RXDBDIS, 0xFFFF);
	UWRITE2(sc, MUSB2_REG_TXDBDIS, 0xFFFF);
#endif

	/* enable HighSpeed and ISO Update flags */

	UWRITE1(sc, MUSB2_REG_POWER,
	    MUSB2_MASK_HSENAB | MUSB2_MASK_ISOUPD);

	if (sc->sc_mode == MOTG_MODE_DEVICE) {
		/* clear Session bit, if set */
		val = UREAD1(sc, MUSB2_REG_DEVCTL);
		val &= ~MUSB2_MASK_SESS;
		UWRITE1(sc, MUSB2_REG_DEVCTL, val);
	} else {
		/* Enter session for Host mode */
		val = UREAD1(sc, MUSB2_REG_DEVCTL);
		val |= MUSB2_MASK_SESS;
		UWRITE1(sc, MUSB2_REG_DEVCTL, val);
	}
	delay(1000);
	DPRINTF(("DEVCTL 0x%x\n", UREAD1(sc, MUSB2_REG_DEVCTL)));

	/* disable testmode */

	UWRITE1(sc, MUSB2_REG_TESTMODE, 0);

#ifdef MUSB2_REG_MISC
	/* set default value */

	UWRITE1(sc, MUSB2_REG_MISC, 0);
#endif

	/* select endpoint index 0 */

	UWRITE1(sc, MUSB2_REG_EPINDEX, 0);

	if (sc->sc_ep_max == 0) {
		/* read out number of endpoints */
		nrx = (UREAD1(sc, MUSB2_REG_EPINFO) / 16);

		ntx = (UREAD1(sc, MUSB2_REG_EPINFO) % 16);

		/* these numbers exclude the control endpoint */

		DPRINTF(("RX/TX endpoints: %u/%u\n", nrx, ntx));

		sc->sc_ep_max = MAX(nrx, ntx);
	} else {
		nrx = ntx = sc->sc_ep_max;
	}
	if (sc->sc_ep_max == 0) {
		aprint_error_dev(sc->sc_dev, " no endpoints\n");
		return USBD_INVAL;
	}
	KASSERT(sc->sc_ep_max <= MOTG_MAX_HW_EP);
	/* read out configuration data */
	val = UREAD1(sc, MUSB2_REG_CONFDATA);

	DPRINTF(("Config Data: 0x%02x\n", val));

	dynfifo = (val & MUSB2_MASK_CD_DYNFIFOSZ) ? 1 : 0;

	if (dynfifo) {
		aprint_normal_dev(sc->sc_dev, "Dynamic FIFO sizing detected, "
		    "assuming 16Kbytes of FIFO RAM\n");
	}

	DPRINTF(("HW version: 0x%04x\n", UREAD1(sc, MUSB2_REG_HWVERS)));

	/* initialise endpoint profiles */
	sc->sc_in_ep[0].ep_fifo_size = 64;
	sc->sc_out_ep[0].ep_fifo_size = 0; /* not used */
	sc->sc_out_ep[0].ep_number = sc->sc_in_ep[0].ep_number = 0;
	SIMPLEQ_INIT(&sc->sc_in_ep[0].ep_pipes);
	offset = 64;

	for (i = 1; i <= sc->sc_ep_max; i++) {
		int fiforx_size, fifotx_size, fifo_size;

		/* select endpoint */
		UWRITE1(sc, MUSB2_REG_EPINDEX, i);

		if (sc->sc_ep_fifosize) {
			fiforx_size = fifotx_size = sc->sc_ep_fifosize;
		} else {
			val = UREAD1(sc, MUSB2_REG_FSIZE);
			fiforx_size = (val & MUSB2_MASK_RX_FSIZE) >> 4;
			fifotx_size = (val & MUSB2_MASK_TX_FSIZE);
		}

		DPRINTF(("Endpoint %u FIFO size: IN=%u, OUT=%u, DYN=%d\n",
		    i, fifotx_size, fiforx_size, dynfifo));

		if (dynfifo) {
			if (sc->sc_ep_fifosize) {
				fifo_size = ffs(sc->sc_ep_fifosize) - 1;
			} else {
				if (i < 3) {
					fifo_size = 12;       /* 4K */
				} else if (i < 10) {
					fifo_size = 10;       /* 1K */
				} else {
					fifo_size = 7;        /* 128 bytes */
				}
			}
			if (fiforx_size && (i <= nrx)) {
				fiforx_size = fifo_size;
				if (fifo_size > 7) {
#if 0
					UWRITE1(sc, MUSB2_REG_RXFIFOSZ,
					    MUSB2_VAL_FIFOSZ(fifo_size) |
					    MUSB2_MASK_FIFODB);
#else
					UWRITE1(sc, MUSB2_REG_RXFIFOSZ,
					    MUSB2_VAL_FIFOSZ(fifo_size));
#endif
				} else {
					UWRITE1(sc, MUSB2_REG_RXFIFOSZ,
					    MUSB2_VAL_FIFOSZ(fifo_size));
				}
				UWRITE2(sc, MUSB2_REG_RXFIFOADD,
				    offset >> 3);
				offset += (1 << fiforx_size);
			}
			if (fifotx_size && (i <= ntx)) {
				fifotx_size = fifo_size;
				if (fifo_size > 7) {
#if 0
					UWRITE1(sc, MUSB2_REG_TXFIFOSZ,
					    MUSB2_VAL_FIFOSZ(fifo_size) |
					    MUSB2_MASK_FIFODB);
#else
					UWRITE1(sc, MUSB2_REG_TXFIFOSZ,
					    MUSB2_VAL_FIFOSZ(fifo_size));
#endif
				} else {
					UWRITE1(sc, MUSB2_REG_TXFIFOSZ,
					    MUSB2_VAL_FIFOSZ(fifo_size));
				}

				UWRITE2(sc, MUSB2_REG_TXFIFOADD,
				    offset >> 3);

				offset += (1 << fifotx_size);
			}
		}
		if (fiforx_size && (i <= nrx)) {
			sc->sc_in_ep[i].ep_fifo_size = (1 << fiforx_size);
			SIMPLEQ_INIT(&sc->sc_in_ep[i].ep_pipes);
		}
		if (fifotx_size && (i <= ntx)) {
			sc->sc_out_ep[i].ep_fifo_size = (1 << fifotx_size);
			SIMPLEQ_INIT(&sc->sc_out_ep[i].ep_pipes);
		}
		sc->sc_out_ep[i].ep_number = sc->sc_in_ep[i].ep_number = i;
	}


	DPRINTF(("Dynamic FIFO size = %d bytes\n", offset));

	/* turn on default interrupts */

	if (sc->sc_mode == MOTG_MODE_HOST) {
		UWRITE1(sc, MUSB2_REG_INTUSBE, 0xff);
		UWRITE2(sc, MUSB2_REG_INTTXE, 0xffff);
		UWRITE2(sc, MUSB2_REG_INTRXE, 0xffff);
	} else
		UWRITE1(sc, MUSB2_REG_INTUSBE, MUSB2_MASK_IRESET);

	sc->sc_xferpool = pool_cache_init(sizeof(struct motg_xfer), 0, 0, 0,
	    "motgxfer", NULL, IPL_USB, NULL, NULL, NULL);

	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_SOFTUSB);
	mutex_init(&sc->sc_intr_lock, MUTEX_DEFAULT, IPL_USB);

	/* Set up the bus struct. */
	sc->sc_bus.methods = &motg_bus_methods;
	sc->sc_bus.pipe_size = sizeof(struct motg_pipe);
	sc->sc_bus.usbrev = USBREV_2_0;
	sc->sc_bus.hci_private = sc;
	snprintf(sc->sc_vendor, sizeof(sc->sc_vendor),
	    "Mentor Graphics");
	sc->sc_child = config_found(sc->sc_dev, &sc->sc_bus, usbctlprint);
	return USBD_NORMAL_COMPLETION;
}

static int
motg_select_ep(struct motg_softc *sc, usbd_pipe_handle pipe)
{
	struct motg_pipe *otgpipe = (struct motg_pipe *)pipe;
	usb_endpoint_descriptor_t *ed = pipe->endpoint->edesc;
	struct motg_hw_ep *ep;
	int i, size;

	ep = (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN) ?
	    sc->sc_in_ep : sc->sc_out_ep;
	size = UE_GET_SIZE(UGETW(pipe->endpoint->edesc->wMaxPacketSize));

	for (i = sc->sc_ep_max; i >= 1; i--) {
		DPRINTF(("%s_ep[%d].ep_fifo_size %d size %d ref %d\n",
		    (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN) ?
		    "in" : "out", i, ep[i].ep_fifo_size, size, ep[i].refcount));
		if (ep[i].ep_fifo_size >= size) {
			/* found a suitable endpoint */
			otgpipe->hw_ep = &ep[i];
			mutex_enter(&sc->sc_lock);
			if (otgpipe->hw_ep->refcount > 0) {
				/* no luck, try next */
				mutex_exit(&sc->sc_lock);
				otgpipe->hw_ep = NULL;
			} else {
				otgpipe->hw_ep->refcount++;
				SIMPLEQ_INSERT_TAIL(&otgpipe->hw_ep->ep_pipes,
				    otgpipe, ep_pipe_list);
				mutex_exit(&sc->sc_lock);
				return 0;
			}
		}
	}
	return -1;
}

/* Open a new pipe. */
usbd_status
motg_open(usbd_pipe_handle pipe)
{
	struct motg_softc *sc = pipe->device->bus->hci_private;
	struct motg_pipe *otgpipe = (struct motg_pipe *)pipe;
	usb_endpoint_descriptor_t *ed = pipe->endpoint->edesc;

	DPRINTF(("motg_open: pipe=%p, addr=%d, endpt=%d (%d)\n",
		     pipe, pipe->device->address,
		     ed->bEndpointAddress, sc->sc_root_addr));

	if (sc->sc_dying)
		return USBD_IOERROR;

	/* toggle state needed for bulk endpoints */
	otgpipe->nexttoggle = pipe->endpoint->datatoggle;

	if (pipe->device->address == sc->sc_root_addr) {
		switch (ed->bEndpointAddress) {
		case USB_CONTROL_ENDPOINT:
			pipe->methods = &motg_root_ctrl_methods;
			break;
		case UE_DIR_IN | MOTG_INTR_ENDPT:
			pipe->methods = &motg_root_intr_methods;
			break;
		default:
			return (USBD_INVAL);
		}
	} else {
		switch (ed->bmAttributes & UE_XFERTYPE) {
		case UE_CONTROL:
			pipe->methods = &motg_device_ctrl_methods;
			/* always use sc_in_ep[0] for in and out */
			otgpipe->hw_ep = &sc->sc_in_ep[0];
			mutex_enter(&sc->sc_lock);
			otgpipe->hw_ep->refcount++;
			SIMPLEQ_INSERT_TAIL(&otgpipe->hw_ep->ep_pipes,
			    otgpipe, ep_pipe_list);
			mutex_exit(&sc->sc_lock);
			break;
		case UE_BULK:
		case UE_INTERRUPT:
			DPRINTFN(MD_BULK,
			    ("new %s %s pipe wMaxPacketSize %d\n",
			    (ed->bmAttributes & UE_XFERTYPE) == UE_BULK ?
			    "bulk" : "interrupt",
			    (UE_GET_DIR(pipe->endpoint->edesc->bEndpointAddress) == UE_DIR_IN) ? "read" : "write",
			    UGETW(pipe->endpoint->edesc->wMaxPacketSize)));
			if (motg_select_ep(sc, pipe) != 0)
				goto bad;
			KASSERT(otgpipe->hw_ep != NULL);
			pipe->methods = &motg_device_data_methods;
			otgpipe->nexttoggle = pipe->endpoint->datatoggle;
			break;
		default:
			goto bad;
#ifdef notyet
		case UE_ISOCHRONOUS:
			...
			break;
#endif /* notyet */
		}
	}
	return (USBD_NORMAL_COMPLETION);

 bad:
	return (USBD_NOMEM);
}

void
motg_softintr(void *v)
{
	struct usbd_bus *bus = v;
	struct motg_softc *sc = bus->hci_private;
	uint16_t rx_status, tx_status;
	uint8_t ctrl_status;
	uint32_t val;
	int i;

	KASSERT(sc->sc_bus.use_polling || mutex_owned(&sc->sc_lock));

	DPRINTFN(MD_ROOT | MD_CTRL,
	    ("%s: motg_softintr\n", device_xname(sc->sc_dev)));

	mutex_spin_enter(&sc->sc_intr_lock);
	rx_status = sc->sc_intr_rx_ep;
	sc->sc_intr_rx_ep = 0;
	tx_status = sc->sc_intr_tx_ep;
	sc->sc_intr_tx_ep = 0;
	ctrl_status = sc->sc_intr_ctrl;
	sc->sc_intr_ctrl = 0;
	mutex_spin_exit(&sc->sc_intr_lock);

	ctrl_status |= UREAD1(sc, MUSB2_REG_INTUSB);

	if (ctrl_status & (MUSB2_MASK_IRESET |
	    MUSB2_MASK_IRESUME | MUSB2_MASK_ISUSP |
	    MUSB2_MASK_ICONN | MUSB2_MASK_IDISC)) {
		DPRINTFN(MD_ROOT | MD_CTRL, ("motg_softintr bus 0x%x\n",
		    ctrl_status));

		if (ctrl_status & MUSB2_MASK_IRESET) {
			sc->sc_isreset = 1;
			sc->sc_port_suspended = 0;
			sc->sc_port_suspended_change = 1;
			sc->sc_connected_changed = 1;
			sc->sc_port_enabled = 1;

			val = UREAD1(sc, MUSB2_REG_POWER);
			if (val & MUSB2_MASK_HSMODE)
				sc->sc_high_speed = 1;
			else
				sc->sc_high_speed = 0;
			DPRINTFN(MD_ROOT | MD_CTRL, ("motg_softintr speed %d\n",
			    sc->sc_high_speed));

			/* turn off interrupts */
			val = MUSB2_MASK_IRESET;
			val &= ~MUSB2_MASK_IRESUME;
			val |= MUSB2_MASK_ISUSP;
			UWRITE1(sc, MUSB2_REG_INTUSBE, val);
			UWRITE2(sc, MUSB2_REG_INTTXE, 0);
			UWRITE2(sc, MUSB2_REG_INTRXE, 0);
		}
		if (ctrl_status & MUSB2_MASK_IRESUME) {
			if (sc->sc_port_suspended) {
				sc->sc_port_suspended = 0;
				sc->sc_port_suspended_change = 1;
				val = UREAD1(sc, MUSB2_REG_INTUSBE);
				/* disable resume interrupt */
				val &= ~MUSB2_MASK_IRESUME;
				/* enable suspend interrupt */
				val |= MUSB2_MASK_ISUSP;
				UWRITE1(sc, MUSB2_REG_INTUSBE, val);
			}
		} else if (ctrl_status & MUSB2_MASK_ISUSP) {
			if (!sc->sc_port_suspended) {
				sc->sc_port_suspended = 1;
				sc->sc_port_suspended_change = 1;

				val = UREAD1(sc, MUSB2_REG_INTUSBE);
				/* disable suspend interrupt */
				val &= ~MUSB2_MASK_ISUSP;
				/* enable resume interrupt */
				val |= MUSB2_MASK_IRESUME;
				UWRITE1(sc, MUSB2_REG_INTUSBE, val);
			}
		}
		if (ctrl_status & MUSB2_MASK_ICONN) {
			sc->sc_connected = 1;
			sc->sc_connected_changed = 1;
			sc->sc_isreset = 1;
			sc->sc_port_enabled = 1;
		} else if (ctrl_status & MUSB2_MASK_IDISC) {
			sc->sc_connected = 0;
			sc->sc_connected_changed = 1;
			sc->sc_isreset = 0;
			sc->sc_port_enabled = 0;
		}

		/* complete root HUB interrupt endpoint */

		motg_hub_change(sc);
	}
	/*
	 * read in interrupt status and mix with the status we
	 * got from the wrapper
	 */
	rx_status |= UREAD2(sc, MUSB2_REG_INTRX);
	tx_status |= UREAD2(sc, MUSB2_REG_INTTX);

	if (rx_status & 0x01)
		panic("ctrl_rx %08x", rx_status);
	if (tx_status & 0x01)
		motg_device_ctrl_intr_tx(sc);
	for (i = 1; i <= sc->sc_ep_max; i++) {
		if (rx_status & (0x01 << i))
			motg_device_intr_rx(sc, i);
		if (tx_status & (0x01 << i))
			motg_device_intr_tx(sc, i);
	}
	return;
}

void
motg_poll(struct usbd_bus *bus)
{
	struct motg_softc *sc = bus->hci_private;

	sc->sc_intr_poll(sc->sc_intr_poll_arg);
	mutex_enter(&sc->sc_lock);
	motg_softintr(bus);
	mutex_exit(&sc->sc_lock);
}

int
motg_intr(struct motg_softc *sc, uint16_t rx_ep, uint16_t tx_ep,
    uint8_t ctrl)
{
	KASSERT(mutex_owned(&sc->sc_intr_lock));
	sc->sc_intr_tx_ep = tx_ep;
	sc->sc_intr_rx_ep = rx_ep;
	sc->sc_intr_ctrl = ctrl;

	if (!sc->sc_bus.use_polling) {
		sc->sc_bus.no_intrs++;
		usb_schedsoftintr(&sc->sc_bus);
	}
	return 1;
}

int
motg_intr_vbus(struct motg_softc *sc, int vbus)
{
	uint8_t val;
	if (sc->sc_mode == MOTG_MODE_HOST && vbus == 0) {
		DPRINTF(("motg_intr_vbus: vbus down, try to re-enable\n"));
		/* try to re-enter session for Host mode */
		val = UREAD1(sc, MUSB2_REG_DEVCTL);
		val |= MUSB2_MASK_SESS;
		UWRITE1(sc, MUSB2_REG_DEVCTL, val);
	}
	return 1;
}

usbd_status
motg_allocm(struct usbd_bus *bus, usb_dma_t *dma, u_int32_t size)
{
	struct motg_softc *sc = bus->hci_private;
	usbd_status status;

	status = usb_allocmem(&sc->sc_bus, size, 0, dma);
	if (status == USBD_NOMEM)
		status = usb_reserve_allocm(&sc->sc_dma_reserve, dma, size);
	return status;
}

void
motg_freem(struct usbd_bus *bus, usb_dma_t *dma)
{
	if (dma->block->flags & USB_DMA_RESERVE) {
		usb_reserve_freem(&((struct motg_softc *)bus)->sc_dma_reserve,
		    dma);
		return;
	}
	usb_freemem(&((struct motg_softc *)bus)->sc_bus, dma);
}

usbd_xfer_handle
motg_allocx(struct usbd_bus *bus)
{
	struct motg_softc *sc = bus->hci_private;
	usbd_xfer_handle xfer;

	xfer = pool_cache_get(sc->sc_xferpool, PR_NOWAIT);
	if (xfer != NULL) {
		memset(xfer, 0, sizeof(struct motg_xfer));
		UXFER(xfer)->sc = sc;
#ifdef DIAGNOSTIC
		// XXX UXFER(xfer)->iinfo.isdone = 1;
		xfer->busy_free = XFER_BUSY;
#endif
	}
	return (xfer);
}

void
motg_freex(struct usbd_bus *bus, usbd_xfer_handle xfer)
{
	struct motg_softc *sc = bus->hci_private;

#ifdef DIAGNOSTIC
	if (xfer->busy_free != XFER_BUSY) {
		printf("motg_freex: xfer=%p not busy, 0x%08x\n", xfer,
		       xfer->busy_free);
	}
	xfer->busy_free = XFER_FREE;
#endif
	pool_cache_put(sc->sc_xferpool, xfer);
}

static void
motg_get_lock(struct usbd_bus *bus, kmutex_t **lock)
{
	struct motg_softc *sc = bus->hci_private;

	*lock = &sc->sc_lock;
}

/*
 * Data structures and routines to emulate the root hub.
 */
usb_device_descriptor_t motg_devd = {
	USB_DEVICE_DESCRIPTOR_SIZE,
	UDESC_DEVICE,		/* type */
	{0x00, 0x01},		/* USB version */
	UDCLASS_HUB,		/* class */
	UDSUBCLASS_HUB,		/* subclass */
	UDPROTO_FSHUB,		/* protocol */
	64,			/* max packet */
	{0},{0},{0x00,0x01},	/* device id */
	1,2,0,			/* string indicies */
	1			/* # of configurations */
};

const usb_config_descriptor_t motg_confd = {
	USB_CONFIG_DESCRIPTOR_SIZE,
	UDESC_CONFIG,
	{USB_CONFIG_DESCRIPTOR_SIZE +
	 USB_INTERFACE_DESCRIPTOR_SIZE +
	 USB_ENDPOINT_DESCRIPTOR_SIZE},
	1,
	1,
	0,
	UC_ATTR_MBO | UC_SELF_POWERED,
	0			/* max power */
};

const usb_interface_descriptor_t motg_ifcd = {
	USB_INTERFACE_DESCRIPTOR_SIZE,
	UDESC_INTERFACE,
	0,
	0,
	1,
	UICLASS_HUB,
	UISUBCLASS_HUB,
	UIPROTO_FSHUB,
	0
};

const usb_endpoint_descriptor_t motg_endpd = {
	USB_ENDPOINT_DESCRIPTOR_SIZE,
	UDESC_ENDPOINT,
	UE_DIR_IN | MOTG_INTR_ENDPT,
	UE_INTERRUPT,
	{8},
	255
};

const usb_hub_descriptor_t motg_hubd = {
	USB_HUB_DESCRIPTOR_SIZE,
	UDESC_HUB,
	1,
	{ UHD_PWR_NO_SWITCH | UHD_OC_INDIVIDUAL, 0 },
	50,			/* power on to power good */
	0,
	{ 0x00 },		/* port is removable */
	{ 0 },
};

/*
 * Simulate a hardware hub by handling all the necessary requests.
 */
usbd_status
motg_root_ctrl_transfer(usbd_xfer_handle xfer)
{
	struct motg_softc *sc = xfer->pipe->device->bus->hci_private;
	usbd_status err;

	/* Insert last in queue. */
	mutex_enter(&sc->sc_lock);
	err = usb_insert_transfer(xfer);
	mutex_exit(&sc->sc_lock);
	if (err)
		return (err);

	/*
	 * Pipe isn't running (otherwise err would be USBD_INPROG),
	 * so start it first.
	 */
	return (motg_root_ctrl_start(SIMPLEQ_FIRST(&xfer->pipe->queue)));
}

usbd_status
motg_root_ctrl_start(usbd_xfer_handle xfer)
{
	struct motg_softc *sc = xfer->pipe->device->bus->hci_private;
	usb_device_request_t *req;
	void *buf = NULL;
	int len, value, index, status, change, l, totlen = 0;
	usb_port_status_t ps;
	usbd_status err;
	uint32_t val;

	if (sc->sc_dying)
		return (USBD_IOERROR);

#ifdef DIAGNOSTIC
	if (!(xfer->rqflags & URQ_REQUEST))
		panic("motg_root_ctrl_start: not a request");
#endif
	req = &xfer->request;

	DPRINTFN(MD_ROOT,("motg_root_ctrl_control type=0x%02x request=%02x\n",
		    req->bmRequestType, req->bRequest));

	len = UGETW(req->wLength);
	value = UGETW(req->wValue);
	index = UGETW(req->wIndex);

	if (len != 0)
		buf = KERNADDR(&xfer->dmabuf, 0);

#define C(x,y) ((x) | ((y) << 8))
	switch(C(req->bRequest, req->bmRequestType)) {
	case C(UR_CLEAR_FEATURE, UT_WRITE_DEVICE):
	case C(UR_CLEAR_FEATURE, UT_WRITE_INTERFACE):
	case C(UR_CLEAR_FEATURE, UT_WRITE_ENDPOINT):
		/*
		 * DEVICE_REMOTE_WAKEUP and ENDPOINT_HALT are no-ops
		 * for the integrated root hub.
		 */
		break;
	case C(UR_GET_CONFIG, UT_READ_DEVICE):
		if (len > 0) {
			*(u_int8_t *)buf = sc->sc_root_conf;
			totlen = 1;
		}
		break;
	case C(UR_GET_DESCRIPTOR, UT_READ_DEVICE):
		DPRINTFN(MD_ROOT,("motg_root_ctrl_control wValue=0x%04x\n", value));
		if (len == 0)
			break;
		switch(value >> 8) {
		case UDESC_DEVICE:
			if ((value & 0xff) != 0) {
				err = USBD_IOERROR;
				goto ret;
			}
			totlen = l = min(len, USB_DEVICE_DESCRIPTOR_SIZE);
			USETW(motg_devd.idVendor, sc->sc_id_vendor);
			memcpy(buf, &motg_devd, l);
			break;
		case UDESC_CONFIG:
			if ((value & 0xff) != 0) {
				err = USBD_IOERROR;
				goto ret;
			}
			totlen = l = min(len, USB_CONFIG_DESCRIPTOR_SIZE);
			memcpy(buf, &motg_confd, l);
			buf = (char *)buf + l;
			len -= l;
			l = min(len, USB_INTERFACE_DESCRIPTOR_SIZE);
			totlen += l;
			memcpy(buf, &motg_ifcd, l);
			buf = (char *)buf + l;
			len -= l;
			l = min(len, USB_ENDPOINT_DESCRIPTOR_SIZE);
			totlen += l;
			memcpy(buf, &motg_endpd, l);
			break;
		case UDESC_STRING:
#define sd ((usb_string_descriptor_t *)buf)
			switch (value & 0xff) {
			case 0: /* Language table */
				totlen = usb_makelangtbl(sd, len);
				break;
			case 1: /* Vendor */
				totlen = usb_makestrdesc(sd, len,
							 sc->sc_vendor);
				break;
			case 2: /* Product */
				totlen = usb_makestrdesc(sd, len,
							 "MOTG root hub");
				break;
			}
#undef sd
			break;
		default:
			err = USBD_IOERROR;
			goto ret;
		}
		break;
	case C(UR_GET_INTERFACE, UT_READ_INTERFACE):
		if (len > 0) {
			*(u_int8_t *)buf = 0;
			totlen = 1;
		}
		break;
	case C(UR_GET_STATUS, UT_READ_DEVICE):
		if (len > 1) {
			USETW(((usb_status_t *)buf)->wStatus,UDS_SELF_POWERED);
			totlen = 2;
		}
		break;
	case C(UR_GET_STATUS, UT_READ_INTERFACE):
	case C(UR_GET_STATUS, UT_READ_ENDPOINT):
		if (len > 1) {
			USETW(((usb_status_t *)buf)->wStatus, 0);
			totlen = 2;
		}
		break;
	case C(UR_SET_ADDRESS, UT_WRITE_DEVICE):
		if (value >= USB_MAX_DEVICES) {
			err = USBD_IOERROR;
			goto ret;
		}
		sc->sc_root_addr = value;
		break;
	case C(UR_SET_CONFIG, UT_WRITE_DEVICE):
		if (value != 0 && value != 1) {
			err = USBD_IOERROR;
			goto ret;
		}
		sc->sc_root_conf = value;
		break;
	case C(UR_SET_DESCRIPTOR, UT_WRITE_DEVICE):
		break;
	case C(UR_SET_FEATURE, UT_WRITE_DEVICE):
	case C(UR_SET_FEATURE, UT_WRITE_INTERFACE):
	case C(UR_SET_FEATURE, UT_WRITE_ENDPOINT):
		err = USBD_IOERROR;
		goto ret;
	case C(UR_SET_INTERFACE, UT_WRITE_INTERFACE):
		break;
	case C(UR_SYNCH_FRAME, UT_WRITE_ENDPOINT):
		break;
	/* Hub requests */
	case C(UR_CLEAR_FEATURE, UT_WRITE_CLASS_DEVICE):
		break;
	case C(UR_CLEAR_FEATURE, UT_WRITE_CLASS_OTHER):
		DPRINTFN(MD_ROOT,
		    ("motg_root_ctrl_control: UR_CLEAR_PORT_FEATURE "
			     "port=%d feature=%d\n",
			     index, value));
		if (index != 1) {
			err = USBD_IOERROR;
			goto ret;
		}
		switch(value) {
		case UHF_PORT_ENABLE:
			sc->sc_port_enabled = 0;
			break;
		case UHF_PORT_SUSPEND:
			if (sc->sc_port_suspended != 0) {
				val = UREAD1(sc, MUSB2_REG_POWER);
				val &= ~MUSB2_MASK_SUSPMODE;
				val |= MUSB2_MASK_RESUME;
				UWRITE1(sc, MUSB2_REG_POWER, val);
				/* wait 20 milliseconds */
				usb_delay_ms(&sc->sc_bus, 20);
				val = UREAD1(sc, MUSB2_REG_POWER);
				val &= ~MUSB2_MASK_RESUME;
				UWRITE1(sc, MUSB2_REG_POWER, val);
				sc->sc_port_suspended = 0;
				sc->sc_port_suspended_change = 1;
			}
			break;
		case UHF_PORT_RESET:
			break;
		case UHF_C_PORT_CONNECTION:
			break;
		case UHF_C_PORT_ENABLE:
			break;
		case UHF_C_PORT_OVER_CURRENT:
			break;
		case UHF_C_PORT_RESET:
			sc->sc_isreset = 0;
			err = USBD_NORMAL_COMPLETION;
			goto ret;
		case UHF_PORT_POWER:
			/* XXX todo */
			break;
		case UHF_PORT_CONNECTION:
		case UHF_PORT_OVER_CURRENT:
		case UHF_PORT_LOW_SPEED:
		case UHF_C_PORT_SUSPEND:
		default:
			err = USBD_IOERROR;
			goto ret;
		}
		break;
	case C(UR_GET_BUS_STATE, UT_READ_CLASS_OTHER):
		err = USBD_IOERROR;
		goto ret;
	case C(UR_GET_DESCRIPTOR, UT_READ_CLASS_DEVICE):
		if (len == 0)
			break;
		if ((value & 0xff) != 0) {
			err = USBD_IOERROR;
			goto ret;
		}
		l = min(len, USB_HUB_DESCRIPTOR_SIZE);
		totlen = l;
		memcpy(buf, &motg_hubd, l);
		break;
	case C(UR_GET_STATUS, UT_READ_CLASS_DEVICE):
		if (len != 4) {
			err = USBD_IOERROR;
			goto ret;
		}
		memset(buf, 0, len);
		totlen = len;
		break;
	case C(UR_GET_STATUS, UT_READ_CLASS_OTHER):
		if (index != 1) {
			err = USBD_IOERROR;
			goto ret;
		}
		if (len != 4) {
			err = USBD_IOERROR;
			goto ret;
		}
		status = change = 0;
		if (sc->sc_connected)
			status |= UPS_CURRENT_CONNECT_STATUS;
		if (sc->sc_connected_changed) {
			change |= UPS_C_CONNECT_STATUS;
			sc->sc_connected_changed = 0;
		}
		if (sc->sc_port_enabled)
			status |= UPS_PORT_ENABLED;
		if (sc->sc_port_enabled_changed) {
			change |= UPS_C_PORT_ENABLED;
			sc->sc_port_enabled_changed = 0;
		}
		if (sc->sc_port_suspended)
			status |= UPS_SUSPEND;
		if (sc->sc_high_speed)
			status |= UPS_HIGH_SPEED;
		status |= UPS_PORT_POWER; /* XXX */
		if (sc->sc_isreset)
			change |= UPS_C_PORT_RESET;
		USETW(ps.wPortStatus, status);
		USETW(ps.wPortChange, change);
		l = min(len, sizeof ps);
		memcpy(buf, &ps, l);
		totlen = l;
		break;
	case C(UR_SET_DESCRIPTOR, UT_WRITE_CLASS_DEVICE):
		err = USBD_IOERROR;
		goto ret;
	case C(UR_SET_FEATURE, UT_WRITE_CLASS_DEVICE):
		break;
	case C(UR_SET_FEATURE, UT_WRITE_CLASS_OTHER):
		if (index != 1) {
			err = USBD_IOERROR;
			goto ret;
		}
		switch(value) {
		case UHF_PORT_ENABLE:
			sc->sc_port_enabled = 1;
			break;
		case UHF_PORT_SUSPEND:
			if (sc->sc_port_suspended == 0) {
				val = UREAD1(sc, MUSB2_REG_POWER);
				val |= MUSB2_MASK_SUSPMODE;
				UWRITE1(sc, MUSB2_REG_POWER, val);
				/* wait 20 milliseconds */
				usb_delay_ms(&sc->sc_bus, 20);
				sc->sc_port_suspended = 1;
				sc->sc_port_suspended_change = 1;
			}
			break;
		case UHF_PORT_RESET:
			err = motg_portreset(sc);
			goto ret;
		case UHF_PORT_POWER:
			/* XXX todo */
			err = USBD_NORMAL_COMPLETION;
			goto ret;
		case UHF_C_PORT_CONNECTION:
		case UHF_C_PORT_ENABLE:
		case UHF_C_PORT_OVER_CURRENT:
		case UHF_PORT_CONNECTION:
		case UHF_PORT_OVER_CURRENT:
		case UHF_PORT_LOW_SPEED:
		case UHF_C_PORT_SUSPEND:
		case UHF_C_PORT_RESET:
		default:
			err = USBD_IOERROR;
			goto ret;
		}
		break;
	default:
		err = USBD_IOERROR;
		goto ret;
	}
	xfer->actlen = totlen;
	err = USBD_NORMAL_COMPLETION;
 ret:
	xfer->status = err;
	mutex_enter(&sc->sc_lock);
	usb_transfer_complete(xfer);
	mutex_exit(&sc->sc_lock);
	return (USBD_IN_PROGRESS);
}

/* Abort a root control request. */
void
motg_root_ctrl_abort(usbd_xfer_handle xfer)
{
	/* Nothing to do, all transfers are synchronous. */
}

/* Close the root pipe. */
void
motg_root_ctrl_close(usbd_pipe_handle pipe)
{
	DPRINTFN(MD_ROOT, ("motg_root_ctrl_close\n"));
}

void
motg_root_ctrl_done(usbd_xfer_handle xfer)
{
}

/* Abort a root interrupt request. */
void
motg_root_intr_abort(usbd_xfer_handle xfer)
{
	struct motg_softc *sc = xfer->pipe->device->bus->hci_private;

	KASSERT(mutex_owned(&sc->sc_lock));
	KASSERT(xfer->pipe->intrxfer == xfer);

	sc->sc_intr_xfer = NULL;

#ifdef DIAGNOSTIC
	// XXX UXFER(xfer)->iinfo.isdone = 1;
#endif
	xfer->status = USBD_CANCELLED;
	usb_transfer_complete(xfer);
}

usbd_status
motg_root_intr_transfer(usbd_xfer_handle xfer)
{
	struct motg_softc *sc = xfer->pipe->device->bus->hci_private;
	usbd_status err;

	/* Insert last in queue. */
	mutex_enter(&sc->sc_lock);
	err = usb_insert_transfer(xfer);
	mutex_exit(&sc->sc_lock);
	if (err)
		return (err);

	/*
	 * Pipe isn't running (otherwise err would be USBD_INPROG),
	 * start first
	 */
	return (motg_root_intr_start(SIMPLEQ_FIRST(&xfer->pipe->queue)));
}

/* Start a transfer on the root interrupt pipe */
usbd_status
motg_root_intr_start(usbd_xfer_handle xfer)
{
	usbd_pipe_handle pipe = xfer->pipe;
	struct motg_softc *sc = pipe->device->bus->hci_private;

	DPRINTFN(MD_ROOT, ("motg_root_intr_start: xfer=%p len=%d flags=%d\n",
		     xfer, xfer->length, xfer->flags));

	if (sc->sc_dying)
		return (USBD_IOERROR);

	sc->sc_intr_xfer = xfer;
	return (USBD_IN_PROGRESS);
}

/* Close the root interrupt pipe. */
void
motg_root_intr_close(usbd_pipe_handle pipe)
{
	struct motg_softc *sc = pipe->device->bus->hci_private;

	KASSERT(mutex_owned(&sc->sc_lock));

	sc->sc_intr_xfer = NULL;
	DPRINTFN(MD_ROOT, ("motg_root_intr_close\n"));
}

void
motg_root_intr_done(usbd_xfer_handle xfer)
{
}

void
motg_noop(usbd_pipe_handle pipe)
{
}

static usbd_status
motg_portreset(struct motg_softc *sc)
{
	uint32_t val;

	val = UREAD1(sc, MUSB2_REG_POWER);
	val |= MUSB2_MASK_RESET;
	UWRITE1(sc, MUSB2_REG_POWER, val);
	/* Wait for 20 msec */
	usb_delay_ms(&sc->sc_bus, 20);

	val = UREAD1(sc, MUSB2_REG_POWER);
	val &= ~MUSB2_MASK_RESET;
	UWRITE1(sc, MUSB2_REG_POWER, val);

	/* determine line speed */
	val = UREAD1(sc, MUSB2_REG_POWER);
	if (val & MUSB2_MASK_HSMODE)
		sc->sc_high_speed = 1;
	else
		sc->sc_high_speed = 0;
	DPRINTFN(MD_ROOT | MD_CTRL, ("motg_portreset speed %d\n",
	    sc->sc_high_speed));

	sc->sc_isreset = 1;
	sc->sc_port_enabled = 1;
	return (USBD_NORMAL_COMPLETION);
}

/*
 * This routine is executed when an interrupt on the root hub is detected
 */
static void
motg_hub_change(struct motg_softc *sc)
{
	usbd_xfer_handle xfer = sc->sc_intr_xfer;
	usbd_pipe_handle pipe;
	u_char *p;

	DPRINTFN(MD_ROOT, ("motg_hub_change\n"));

	if (xfer == NULL)
		return; /* the interrupt pipe is not open */

	pipe = xfer->pipe;
	if (pipe->device == NULL || pipe->device->bus == NULL)
		return;	/* device has detached */

	p = KERNADDR(&xfer->dmabuf, 0);
	p[0] = 1<<1;
	xfer->actlen = 1;
	xfer->status = USBD_NORMAL_COMPLETION;
	usb_transfer_complete(xfer);
}

static uint8_t
motg_speed(u_int8_t speed)
{
	switch(speed) {
	case USB_SPEED_LOW:
		return MUSB2_MASK_TI_SPEED_LO;
	case USB_SPEED_FULL:
		return MUSB2_MASK_TI_SPEED_FS;
	case USB_SPEED_HIGH:
		return MUSB2_MASK_TI_SPEED_HS;
	default:
		panic("motg: unknown speed %d", speed);
		/* NOTREACHED */
	}
}

static uint8_t
motg_type(u_int8_t type)
{
	switch(type) {
	case UE_CONTROL:
		return MUSB2_MASK_TI_PROTO_CTRL;
	case UE_ISOCHRONOUS:
		return MUSB2_MASK_TI_PROTO_ISOC;
	case UE_BULK:
		return MUSB2_MASK_TI_PROTO_BULK;
	case UE_INTERRUPT:
		return MUSB2_MASK_TI_PROTO_INTR;
	default:
		panic("motg: unknown type %d", type);
		/* NOTREACHED */
	}
}

static void
motg_setup_endpoint_tx(usbd_xfer_handle xfer)
{
	struct motg_softc *sc = xfer->pipe->device->bus->hci_private;
	struct motg_pipe *otgpipe = (struct motg_pipe *)xfer->pipe;
	usbd_device_handle dev = otgpipe->pipe.device;
	int epnumber = otgpipe->hw_ep->ep_number;

	UWRITE1(sc, MUSB2_REG_TXFADDR(epnumber), dev->address);
	if (dev->myhsport) {
		UWRITE1(sc, MUSB2_REG_TXHADDR(epnumber),
		    dev->myhsport->parent->address);
		UWRITE1(sc, MUSB2_REG_TXHUBPORT(epnumber),
		    dev->myhsport->portno);
	} else {
		UWRITE1(sc, MUSB2_REG_TXHADDR(epnumber), 0);
		UWRITE1(sc, MUSB2_REG_TXHUBPORT(epnumber), 0);
	}
	UWRITE1(sc, MUSB2_REG_TXTI,
	    motg_speed(dev->speed) |
	    UE_GET_ADDR(xfer->pipe->endpoint->edesc->bEndpointAddress) |
	    motg_type(UE_GET_XFERTYPE(xfer->pipe->endpoint->edesc->bmAttributes))
	    );
	if (epnumber == 0) {
		if (sc->sc_high_speed) {
			UWRITE1(sc, MUSB2_REG_TXNAKLIMIT,
			    NAK_TO_CTRL_HIGH);
		} else {
			UWRITE1(sc, MUSB2_REG_TXNAKLIMIT, NAK_TO_CTRL);
		}
	} else {
		if ((xfer->pipe->endpoint->edesc->bmAttributes & UE_XFERTYPE)
		    == UE_BULK) {
			if (sc->sc_high_speed) {
				UWRITE1(sc, MUSB2_REG_TXNAKLIMIT,
				    NAK_TO_BULK_HIGH);
			} else {
				UWRITE1(sc, MUSB2_REG_TXNAKLIMIT, NAK_TO_BULK);
			}
		} else {
			if (sc->sc_high_speed) {
				UWRITE1(sc, MUSB2_REG_TXNAKLIMIT, POLL_TO_HIGH);
			} else {
				UWRITE1(sc, MUSB2_REG_TXNAKLIMIT, POLL_TO);
			}
		}
	}
}

static void
motg_setup_endpoint_rx(usbd_xfer_handle xfer)
{
	struct motg_softc *sc = xfer->pipe->device->bus->hci_private;
	usbd_device_handle dev = xfer->pipe->device;
	struct motg_pipe *otgpipe = (struct motg_pipe *)xfer->pipe;
	int epnumber = otgpipe->hw_ep->ep_number;

	UWRITE1(sc, MUSB2_REG_RXFADDR(epnumber), dev->address);
	if (dev->myhsport) {
		UWRITE1(sc, MUSB2_REG_RXHADDR(epnumber),
		    dev->myhsport->parent->address);
		UWRITE1(sc, MUSB2_REG_RXHUBPORT(epnumber),
		    dev->myhsport->portno);
	} else {
		UWRITE1(sc, MUSB2_REG_RXHADDR(epnumber), 0);
		UWRITE1(sc, MUSB2_REG_RXHUBPORT(epnumber), 0);
	}
	UWRITE1(sc, MUSB2_REG_RXTI,
	    motg_speed(dev->speed) |
	    UE_GET_ADDR(xfer->pipe->endpoint->edesc->bEndpointAddress) |
	    motg_type(UE_GET_XFERTYPE(xfer->pipe->endpoint->edesc->bmAttributes))
	    );
	if (epnumber == 0) {
		if (sc->sc_high_speed) {
			UWRITE1(sc, MUSB2_REG_TXNAKLIMIT,
			    NAK_TO_CTRL_HIGH);
		} else {
			UWRITE1(sc, MUSB2_REG_TXNAKLIMIT, NAK_TO_CTRL);
		}
	} else {
		if ((xfer->pipe->endpoint->edesc->bmAttributes & UE_XFERTYPE)
		    == UE_BULK) {
			if (sc->sc_high_speed) {
				UWRITE1(sc, MUSB2_REG_RXNAKLIMIT,
				    NAK_TO_BULK_HIGH);
			} else {
				UWRITE1(sc, MUSB2_REG_RXNAKLIMIT, NAK_TO_BULK);
			}
		} else {
			if (sc->sc_high_speed) {
				UWRITE1(sc, MUSB2_REG_RXNAKLIMIT, POLL_TO_HIGH);
			} else {
				UWRITE1(sc, MUSB2_REG_RXNAKLIMIT, POLL_TO);
			}
		}
	}
}

static usbd_status
motg_device_ctrl_transfer(usbd_xfer_handle xfer)
{
	struct motg_softc *sc = xfer->pipe->device->bus->hci_private;
	usbd_status err;

	/* Insert last in queue. */
	mutex_enter(&sc->sc_lock);
	err = usb_insert_transfer(xfer);
	xfer->status = USBD_NOT_STARTED;
	mutex_exit(&sc->sc_lock);
	if (err)
		return (err);

	/*
	 * Pipe isn't running (otherwise err would be USBD_INPROG),
	 * so start it first.
	 */
	return (motg_device_ctrl_start(SIMPLEQ_FIRST(&xfer->pipe->queue)));
}

static usbd_status
motg_device_ctrl_start(usbd_xfer_handle xfer)
{
	struct motg_softc *sc = xfer->pipe->device->bus->hci_private;
	usbd_status err;
	mutex_enter(&sc->sc_lock);
	err = motg_device_ctrl_start1(sc);
	mutex_exit(&sc->sc_lock);
	if (err != USBD_IN_PROGRESS)
		return err;
	if (sc->sc_bus.use_polling)
		motg_waitintr(sc, xfer);
	return USBD_IN_PROGRESS;
}

static usbd_status
motg_device_ctrl_start1(struct motg_softc *sc)
{
	struct motg_hw_ep *ep = &sc->sc_in_ep[0];
	usbd_xfer_handle xfer = NULL;
	struct motg_pipe *otgpipe;
	usbd_status err = 0;

	KASSERT(mutex_owned(&sc->sc_lock));
	if (sc->sc_dying)
		return (USBD_IOERROR);

	if (!sc->sc_connected)
		return (USBD_IOERROR);

	if (ep->xfer != NULL) {
		err = USBD_IN_PROGRESS;
		goto end;
	}
	/* locate the first pipe with work to do */
	SIMPLEQ_FOREACH(otgpipe, &ep->ep_pipes, ep_pipe_list) {
		xfer = SIMPLEQ_FIRST(&otgpipe->pipe.queue);
		DPRINTFN(MD_CTRL,
		    ("motg_device_ctrl_start1 pipe %p xfer %p status %d\n",
		    otgpipe, xfer, (xfer != NULL) ? xfer->status : 0));

		if (xfer != NULL) {
			/* move this pipe to the end of the list */
			SIMPLEQ_REMOVE(&ep->ep_pipes, otgpipe,
			    motg_pipe, ep_pipe_list);
			SIMPLEQ_INSERT_TAIL(&ep->ep_pipes,
			    otgpipe, ep_pipe_list);
			break;
		}
	}
	if (xfer == NULL) {
		err = USBD_NOT_STARTED;
		goto end;
	}
	xfer->status = USBD_IN_PROGRESS;
	KASSERT(otgpipe == (struct motg_pipe *)xfer->pipe);
	KASSERT(otgpipe->hw_ep == ep);
#ifdef DIAGNOSTIC
	if (!(xfer->rqflags & URQ_REQUEST))
		panic("motg_device_ctrl_transfer: not a request");
#endif
	// KASSERT(xfer->actlen == 0);
	xfer->actlen = 0;

	ep->xfer = xfer;
	ep->datalen = xfer->length;
	if (ep->datalen > 0)
		ep->data = KERNADDR(&xfer->dmabuf, 0);
	else
		ep->data = NULL;
	if ((xfer->flags & USBD_FORCE_SHORT_XFER) &&
	    (ep->datalen % 64) == 0)
		ep->need_short_xfer = 1;
	else
		ep->need_short_xfer = 0;
	/* now we need send this request */
	DPRINTFN(MD_CTRL,
	    ("motg_device_ctrl_start1(%p) send data %p len %d short %d speed %d to %d\n",
	    xfer, ep->data, ep->datalen, ep->need_short_xfer, xfer->pipe->device->speed,
	    xfer->pipe->device->address));
	KASSERT(ep->phase == IDLE);
	ep->phase = SETUP;
	/* select endpoint 0 */
	UWRITE1(sc, MUSB2_REG_EPINDEX, 0);
	/* fifo should be empty at this point */
	KASSERT((UREAD1(sc, MUSB2_REG_TXCSRL) & MUSB2_MASK_CSR0L_TXPKTRDY) == 0);
	/* send data */
	// KASSERT(((vaddr_t)(&xfer->request) & 3) == 0);
	KASSERT(sizeof(xfer->request) == 8);
	bus_space_write_multi_1(sc->sc_iot, sc->sc_ioh, MUSB2_REG_EPFIFO(0),
	    (void *)&xfer->request, sizeof(xfer->request));

	motg_setup_endpoint_tx(xfer);
	/* start transaction */
	UWRITE1(sc, MUSB2_REG_TXCSRL,
	    MUSB2_MASK_CSR0L_TXPKTRDY | MUSB2_MASK_CSR0L_SETUPPKT);

end:
	if (err)
		return (err);

	return (USBD_IN_PROGRESS);
}

static void
motg_device_ctrl_read(usbd_xfer_handle xfer)
{
	struct motg_softc *sc = xfer->pipe->device->bus->hci_private;
	struct motg_pipe *otgpipe = (struct motg_pipe *)xfer->pipe;
	/* assume endpoint already selected */
	motg_setup_endpoint_rx(xfer);
	/* start transaction */
	UWRITE1(sc, MUSB2_REG_TXCSRL, MUSB2_MASK_CSR0L_REQPKT);
	otgpipe->hw_ep->phase = DATA_IN;
}

static void
motg_device_ctrl_intr_rx(struct motg_softc *sc)
{
	struct motg_hw_ep *ep = &sc->sc_in_ep[0];
	usbd_xfer_handle xfer = ep->xfer;
	uint8_t csr;
	int datalen, max_datalen;
	char *data;
	bool got_short;
	usbd_status new_status = USBD_IN_PROGRESS;

	KASSERT(mutex_owned(&sc->sc_lock));

#ifdef DIAGNOSTIC
	if (ep->phase != DATA_IN &&
	    ep->phase != STATUS_IN)
		panic("motg_device_ctrl_intr_rx: bad phase %d", ep->phase);
#endif
        /* select endpoint 0 */
	UWRITE1(sc, MUSB2_REG_EPINDEX, 0);

	/* read out FIFO status */
	csr = UREAD1(sc, MUSB2_REG_TXCSRL);
	DPRINTFN(MD_CTRL,
	    ("motg_device_ctrl_intr_rx phase %d csr 0x%x xfer %p status %d\n",
	    ep->phase, csr, xfer, (xfer != NULL) ? xfer->status : 0));

	if (csr & MUSB2_MASK_CSR0L_NAKTIMO) {
		csr &= ~MUSB2_MASK_CSR0L_REQPKT;
		UWRITE1(sc, MUSB2_REG_TXCSRL, csr);

		csr &= ~MUSB2_MASK_CSR0L_NAKTIMO;
		UWRITE1(sc, MUSB2_REG_TXCSRL, csr);
		new_status = USBD_TIMEOUT; /* XXX */
		goto complete;
	}
	if (csr & (MUSB2_MASK_CSR0L_RXSTALL | MUSB2_MASK_CSR0L_ERROR)) {
		if (csr & MUSB2_MASK_CSR0L_RXSTALL)
			new_status = USBD_STALLED;
		else
			new_status = USBD_IOERROR;
		/* clear status */
		UWRITE1(sc, MUSB2_REG_TXCSRL, 0);
		goto complete;
	}
	if ((csr & MUSB2_MASK_CSR0L_RXPKTRDY) == 0)
		return; /* no data yet */

	if (xfer == NULL || xfer->status != USBD_IN_PROGRESS)
		goto complete;

	if (ep->phase == STATUS_IN) {
		new_status = USBD_NORMAL_COMPLETION;
		UWRITE1(sc, MUSB2_REG_TXCSRL, 0);
		goto complete;
	}
	datalen = UREAD2(sc, MUSB2_REG_RXCOUNT);
	DPRINTFN(MD_CTRL,
	    ("motg_device_ctrl_intr_rx phase %d datalen %d\n",
	    ep->phase, datalen));
	KASSERT(UGETW(xfer->pipe->endpoint->edesc->wMaxPacketSize) > 0);
	max_datalen = min(UGETW(xfer->pipe->endpoint->edesc->wMaxPacketSize),
	    ep->datalen);
	if (datalen > max_datalen) {
		new_status = USBD_IOERROR;
		UWRITE1(sc, MUSB2_REG_TXCSRL, 0);
		goto complete;
	}
	got_short = (datalen < max_datalen);
	if (datalen > 0) {
		KASSERT(ep->phase == DATA_IN);
		data = ep->data;
		ep->data += datalen;
		ep->datalen -= datalen;
		xfer->actlen += datalen;
		if (((vaddr_t)data & 0x3) == 0 &&
		    (datalen >> 2) > 0) {
			DPRINTFN(MD_CTRL,
			    ("motg_device_ctrl_intr_rx r4 data %p len %d\n",
			    data, datalen));
			bus_space_read_multi_4(sc->sc_iot, sc->sc_ioh,
			    MUSB2_REG_EPFIFO(0), (void *)data, datalen >> 2);
			data += (datalen & ~0x3);
			datalen -= (datalen & ~0x3);
		}
		DPRINTFN(MD_CTRL,
		    ("motg_device_ctrl_intr_rx r1 data %p len %d\n",
		    data, datalen));
		if (datalen) {
			bus_space_read_multi_1(sc->sc_iot, sc->sc_ioh,
			    MUSB2_REG_EPFIFO(0), data, datalen);
		}
	}
	UWRITE1(sc, MUSB2_REG_TXCSRL, csr & ~MUSB2_MASK_CSR0L_RXPKTRDY);
	KASSERT(ep->phase == DATA_IN);
	if (got_short || (ep->datalen == 0)) {
		if (ep->need_short_xfer == 0) {
			ep->phase = STATUS_OUT;
			UWRITE1(sc, MUSB2_REG_TXCSRH,
			    UREAD1(sc, MUSB2_REG_TXCSRH) |
			    MUSB2_MASK_CSR0H_PING_DIS);
			motg_setup_endpoint_tx(xfer);
			UWRITE1(sc, MUSB2_REG_TXCSRL,
			    MUSB2_MASK_CSR0L_STATUSPKT |
			    MUSB2_MASK_CSR0L_TXPKTRDY);
			return;
		}
		ep->need_short_xfer = 0;
	}
	motg_device_ctrl_read(xfer);
	return;
complete:
	ep->phase = IDLE;
	ep->xfer = NULL;
	if (xfer && xfer->status == USBD_IN_PROGRESS) {
		KASSERT(new_status != USBD_IN_PROGRESS);
		xfer->status = new_status;
		usb_transfer_complete(xfer);
	}
	motg_device_ctrl_start1(sc);
}

static void
motg_device_ctrl_intr_tx(struct motg_softc *sc)
{
	struct motg_hw_ep *ep = &sc->sc_in_ep[0];
	usbd_xfer_handle xfer = ep->xfer;
	uint8_t csr;
	int datalen;
	char *data;
	usbd_status new_status = USBD_IN_PROGRESS;

	KASSERT(mutex_owned(&sc->sc_lock));
	if (ep->phase == DATA_IN || ep->phase == STATUS_IN) {
		motg_device_ctrl_intr_rx(sc);
		return;
	}

#ifdef DIAGNOSTIC
	if (ep->phase != SETUP && ep->phase != DATA_OUT &&
	    ep->phase != STATUS_OUT)
		panic("motg_device_ctrl_intr_tx: bad phase %d", ep->phase);
#endif
        /* select endpoint 0 */
	UWRITE1(sc, MUSB2_REG_EPINDEX, 0);

	csr = UREAD1(sc, MUSB2_REG_TXCSRL);
	DPRINTFN(MD_CTRL,
	    ("motg_device_ctrl_intr_tx phase %d csr 0x%x xfer %p status %d\n",
	    ep->phase, csr, xfer, (xfer != NULL) ? xfer->status : 0));

	if (csr & MUSB2_MASK_CSR0L_RXSTALL) {
		/* command not accepted */
		new_status = USBD_STALLED;
		/* clear status */
		UWRITE1(sc, MUSB2_REG_TXCSRL, 0);
		goto complete;
	}
	if (csr & MUSB2_MASK_CSR0L_NAKTIMO) {
		new_status = USBD_TIMEOUT; /* XXX */
		/* flush fifo */
		while (csr & MUSB2_MASK_CSR0L_TXFIFONEMPTY) {
			UWRITE1(sc, MUSB2_REG_TXCSRH,
			    UREAD1(sc, MUSB2_REG_TXCSRH) |
				MUSB2_MASK_CSR0H_FFLUSH);
			csr = UREAD1(sc, MUSB2_REG_TXCSRL);
		}
		csr &= ~MUSB2_MASK_CSR0L_NAKTIMO;
		UWRITE1(sc, MUSB2_REG_TXCSRL, csr);
		goto complete;
	}
	if (csr & MUSB2_MASK_CSR0L_ERROR) {
		new_status = USBD_IOERROR;
		/* clear status */
		UWRITE1(sc, MUSB2_REG_TXCSRL, 0);
		goto complete;
	}
	if (csr & MUSB2_MASK_CSR0L_TXFIFONEMPTY) {
		/* data still not sent */
		return;
	}
	if (xfer == NULL)
		goto complete;
	if (ep->phase == STATUS_OUT) {
		/*
		 * we have sent status and got no error;
		 * declare transfer complete
		 */
		DPRINTFN(MD_CTRL,
		    ("motg_device_ctrl_intr_tx %p status %d complete\n",
			xfer, xfer->status));
		new_status = USBD_NORMAL_COMPLETION;
		goto complete;
	}
	if (ep->datalen == 0) {
		if (ep->need_short_xfer) {
			ep->need_short_xfer = 0;
			/* one more data phase */
			if (xfer->request.bmRequestType & UT_READ) {
				DPRINTFN(MD_CTRL,
				    ("motg_device_ctrl_intr_tx %p to DATA_IN\n", xfer));
				motg_device_ctrl_read(xfer);
				return;
			} /*  else fall back to DATA_OUT */
		} else {
			DPRINTFN(MD_CTRL,
			    ("motg_device_ctrl_intr_tx %p to STATUS_IN, csrh 0x%x\n",
			    xfer, UREAD1(sc, MUSB2_REG_TXCSRH)));
			ep->phase = STATUS_IN;
			UWRITE1(sc, MUSB2_REG_RXCSRH,
			    UREAD1(sc, MUSB2_REG_RXCSRH) |
			    MUSB2_MASK_CSR0H_PING_DIS);
			motg_setup_endpoint_rx(xfer);
			UWRITE1(sc, MUSB2_REG_TXCSRL,
			    MUSB2_MASK_CSR0L_STATUSPKT |
			    MUSB2_MASK_CSR0L_REQPKT);
			return;
		}
	}
	if (xfer->request.bmRequestType & UT_READ) {
		motg_device_ctrl_read(xfer);
		return;
	}
	/* setup a dataout phase */
	datalen = min(ep->datalen,
	    UGETW(xfer->pipe->endpoint->edesc->wMaxPacketSize));
	ep->phase = DATA_OUT;
	DPRINTFN(MD_CTRL,
	    ("motg_device_ctrl_intr_tx %p to DATA_OUT, csrh 0x%x\n", xfer,
	    UREAD1(sc, MUSB2_REG_TXCSRH)));
	if (datalen) {
		data = ep->data;
		ep->data += datalen;
		ep->datalen -= datalen;
		xfer->actlen += datalen;
		if (((vaddr_t)data & 0x3) == 0 &&
		    (datalen >> 2) > 0) {
			bus_space_write_multi_4(sc->sc_iot, sc->sc_ioh,
			    MUSB2_REG_EPFIFO(0), (void *)data, datalen >> 2);
			data += (datalen & ~0x3);
			datalen -= (datalen & ~0x3);
		}
		if (datalen) {
			bus_space_write_multi_1(sc->sc_iot, sc->sc_ioh,
			    MUSB2_REG_EPFIFO(0), data, datalen);
		}
	}
	/* send data */
	motg_setup_endpoint_tx(xfer);
	UWRITE1(sc, MUSB2_REG_TXCSRL, MUSB2_MASK_CSR0L_TXPKTRDY);
	return;

complete:
	ep->phase = IDLE;
	ep->xfer = NULL;
	if (xfer && xfer->status == USBD_IN_PROGRESS) {
		KASSERT(new_status != USBD_IN_PROGRESS);
		xfer->status = new_status;
		usb_transfer_complete(xfer);
	}
	motg_device_ctrl_start1(sc);
}

/* Abort a device control request. */
void
motg_device_ctrl_abort(usbd_xfer_handle xfer)
{
	DPRINTFN(MD_CTRL, ("motg_device_ctrl_abort:\n"));
	motg_device_xfer_abort(xfer);
}

/* Close a device control pipe */
void
motg_device_ctrl_close(usbd_pipe_handle pipe)
{
	struct motg_softc *sc __diagused = pipe->device->bus->hci_private;
	struct motg_pipe *otgpipe = (struct motg_pipe *)pipe;
	struct motg_pipe *otgpipeiter;

	DPRINTFN(MD_CTRL, ("motg_device_ctrl_close:\n"));
	KASSERT(mutex_owned(&sc->sc_lock));
	KASSERT(otgpipe->hw_ep->xfer == NULL ||
	    otgpipe->hw_ep->xfer->pipe != pipe);

	SIMPLEQ_FOREACH(otgpipeiter, &otgpipe->hw_ep->ep_pipes, ep_pipe_list) {
		if (otgpipeiter == otgpipe) {
			/* remove from list */
			SIMPLEQ_REMOVE(&otgpipe->hw_ep->ep_pipes, otgpipe,
			    motg_pipe, ep_pipe_list);
			otgpipe->hw_ep->refcount--;
			/* we're done */
			return;
		}
	}
	panic("motg_device_ctrl_close: not found");
}

void
motg_device_ctrl_done(usbd_xfer_handle xfer)
{
	struct motg_pipe *otgpipe __diagused = (struct motg_pipe *)xfer->pipe;
	DPRINTFN(MD_CTRL, ("motg_device_ctrl_done:\n"));
	KASSERT(otgpipe->hw_ep->xfer != xfer);
}

static usbd_status
motg_device_data_transfer(usbd_xfer_handle xfer)
{
	struct motg_softc *sc = xfer->pipe->device->bus->hci_private;
	usbd_status err;

	/* Insert last in queue. */
	mutex_enter(&sc->sc_lock);
	DPRINTF(("motg_device_data_transfer(%p) status %d\n",
	    xfer, xfer->status));
	err = usb_insert_transfer(xfer);
	xfer->status = USBD_NOT_STARTED;
	mutex_exit(&sc->sc_lock);
	if (err)
		return (err);

	/*
	 * Pipe isn't running (otherwise err would be USBD_INPROG),
	 * so start it first.
	 */
	return (motg_device_data_start(SIMPLEQ_FIRST(&xfer->pipe->queue)));
}

static usbd_status
motg_device_data_start(usbd_xfer_handle xfer)
{
	struct motg_softc *sc = xfer->pipe->device->bus->hci_private;
	struct motg_pipe *otgpipe = (struct motg_pipe *)xfer->pipe;
	usbd_status err;
	mutex_enter(&sc->sc_lock);
	DPRINTF(("motg_device_data_start(%p) status %d\n",
	    xfer, xfer->status));
	err = motg_device_data_start1(sc, otgpipe->hw_ep);
	mutex_exit(&sc->sc_lock);
	if (err != USBD_IN_PROGRESS)
		return err;
	if (sc->sc_bus.use_polling)
		motg_waitintr(sc, xfer);
	return USBD_IN_PROGRESS;
}

static usbd_status
motg_device_data_start1(struct motg_softc *sc, struct motg_hw_ep *ep)
{
	usbd_xfer_handle xfer = NULL;
	struct motg_pipe *otgpipe;
	usbd_status err = 0;
	uint32_t val __diagused;

	KASSERT(mutex_owned(&sc->sc_lock));
	if (sc->sc_dying)
		return (USBD_IOERROR);

	if (!sc->sc_connected)
		return (USBD_IOERROR);

	if (ep->xfer != NULL) {
		err = USBD_IN_PROGRESS;
		goto end;
	}
	/* locate the first pipe with work to do */
	SIMPLEQ_FOREACH(otgpipe, &ep->ep_pipes, ep_pipe_list) {
		xfer = SIMPLEQ_FIRST(&otgpipe->pipe.queue);
		DPRINTFN(MD_BULK,
		    ("motg_device_data_start1 pipe %p xfer %p status %d\n",
		    otgpipe, xfer, (xfer != NULL) ? xfer->status : 0));
		if (xfer != NULL) {
			/* move this pipe to the end of the list */
			SIMPLEQ_REMOVE(&ep->ep_pipes, otgpipe,
			    motg_pipe, ep_pipe_list);
			SIMPLEQ_INSERT_TAIL(&ep->ep_pipes,
			    otgpipe, ep_pipe_list);
			break;
		}
	}
	if (xfer == NULL) {
		err = USBD_NOT_STARTED;
		goto end;
	}
	xfer->status = USBD_IN_PROGRESS;
	KASSERT(otgpipe == (struct motg_pipe *)xfer->pipe);
	KASSERT(otgpipe->hw_ep == ep);
#ifdef DIAGNOSTIC
	if (xfer->rqflags & URQ_REQUEST)
		panic("motg_device_data_transfer: a request");
#endif
	// KASSERT(xfer->actlen == 0);
	xfer->actlen = 0;

	ep->xfer = xfer;
	ep->datalen = xfer->length;
	KASSERT(ep->datalen > 0);
	ep->data = KERNADDR(&xfer->dmabuf, 0);
	if ((xfer->flags & USBD_FORCE_SHORT_XFER) &&
	    (ep->datalen % 64) == 0)
		ep->need_short_xfer = 1;
	else
		ep->need_short_xfer = 0;
	/* now we need send this request */
	DPRINTFN(MD_BULK,
	    ("motg_device_data_start1(%p) %s data %p len %d short %d speed %d to %d\n",
	    xfer,
	    UE_GET_DIR(xfer->pipe->endpoint->edesc->bEndpointAddress) == UE_DIR_IN ? "read" : "write",
	    ep->data, ep->datalen, ep->need_short_xfer, xfer->pipe->device->speed,
	    xfer->pipe->device->address));
	KASSERT(ep->phase == IDLE);
	/* select endpoint */
	UWRITE1(sc, MUSB2_REG_EPINDEX, ep->ep_number);
	if (UE_GET_DIR(xfer->pipe->endpoint->edesc->bEndpointAddress)
	    == UE_DIR_IN) {
		val = UREAD1(sc, MUSB2_REG_RXCSRL);
		KASSERT((val & MUSB2_MASK_CSRL_RXPKTRDY) == 0);
		motg_device_data_read(xfer);
	} else {
		ep->phase = DATA_OUT;
		val = UREAD1(sc, MUSB2_REG_TXCSRL);
		KASSERT((val & MUSB2_MASK_CSRL_TXPKTRDY) == 0);
		motg_device_data_write(xfer);
	}
end:
	if (err)
		return (err);

	return (USBD_IN_PROGRESS);
}

static void
motg_device_data_read(usbd_xfer_handle xfer)
{
	struct motg_softc *sc = xfer->pipe->device->bus->hci_private;
	struct motg_pipe *otgpipe = (struct motg_pipe *)xfer->pipe;
	uint32_t val;

	KASSERT(mutex_owned(&sc->sc_lock));
	/* assume endpoint already selected */
	motg_setup_endpoint_rx(xfer);
	/* Max packet size */
	UWRITE2(sc, MUSB2_REG_RXMAXP,
	    UGETW(xfer->pipe->endpoint->edesc->wMaxPacketSize));
	/* Data Toggle */
	val = UREAD1(sc, MUSB2_REG_RXCSRH);
	val |= MUSB2_MASK_CSRH_RXDT_WREN;
	if (otgpipe->nexttoggle)
		val |= MUSB2_MASK_CSRH_RXDT_VAL;
	else
		val &= ~MUSB2_MASK_CSRH_RXDT_VAL;
	UWRITE1(sc, MUSB2_REG_RXCSRH, val);

	DPRINTFN(MD_BULK,
	    ("motg_device_data_read %p to DATA_IN on ep %d, csrh 0x%x\n",
	    xfer, otgpipe->hw_ep->ep_number, UREAD1(sc, MUSB2_REG_RXCSRH)));
	/* start transaction */
	UWRITE1(sc, MUSB2_REG_RXCSRL, MUSB2_MASK_CSRL_RXREQPKT);
	otgpipe->hw_ep->phase = DATA_IN;
}

static void
motg_device_data_write(usbd_xfer_handle xfer)
{
	struct motg_softc *sc = xfer->pipe->device->bus->hci_private;
	struct motg_pipe *otgpipe = (struct motg_pipe *)xfer->pipe;
	struct motg_hw_ep *ep = otgpipe->hw_ep;
	int datalen;
	char *data;
	uint32_t val;

	KASSERT(xfer!=NULL);
	KASSERT(mutex_owned(&sc->sc_lock));

	datalen = min(ep->datalen,
	    UGETW(xfer->pipe->endpoint->edesc->wMaxPacketSize));
	ep->phase = DATA_OUT;
	DPRINTFN(MD_BULK,
	    ("motg_device_data_write %p to DATA_OUT on ep %d, len %d csrh 0x%x\n",
	    xfer, ep->ep_number, datalen, UREAD1(sc, MUSB2_REG_TXCSRH)));

	/* assume endpoint already selected */
	/* write data to fifo */
	data = ep->data;
	ep->data += datalen;
	ep->datalen -= datalen;
	xfer->actlen += datalen;
	if (((vaddr_t)data & 0x3) == 0 &&
	    (datalen >> 2) > 0) {
		bus_space_write_multi_4(sc->sc_iot, sc->sc_ioh,
		    MUSB2_REG_EPFIFO(ep->ep_number),
		    (void *)data, datalen >> 2);
		data += (datalen & ~0x3);
		datalen -= (datalen & ~0x3);
	}
	if (datalen) {
		bus_space_write_multi_1(sc->sc_iot, sc->sc_ioh,
		    MUSB2_REG_EPFIFO(ep->ep_number), data, datalen);
	}

	motg_setup_endpoint_tx(xfer);
	/* Max packet size */
	UWRITE2(sc, MUSB2_REG_TXMAXP,
	    UGETW(xfer->pipe->endpoint->edesc->wMaxPacketSize));
	/* Data Toggle */
	val = UREAD1(sc, MUSB2_REG_TXCSRH);
	val |= MUSB2_MASK_CSRH_TXDT_WREN;
	if (otgpipe->nexttoggle)
		val |= MUSB2_MASK_CSRH_TXDT_VAL;
	else
		val &= ~MUSB2_MASK_CSRH_TXDT_VAL;
	UWRITE1(sc, MUSB2_REG_TXCSRH, val);

	/* start transaction */
	UWRITE1(sc, MUSB2_REG_TXCSRL, MUSB2_MASK_CSRL_TXPKTRDY);
}

static void
motg_device_intr_rx(struct motg_softc *sc, int epnumber)
{
	struct motg_hw_ep *ep = &sc->sc_in_ep[epnumber];
	usbd_xfer_handle xfer = ep->xfer;
	uint8_t csr;
	int datalen, max_datalen;
	char *data;
	bool got_short;
	usbd_status new_status = USBD_IN_PROGRESS;

	KASSERT(mutex_owned(&sc->sc_lock));
	KASSERT(ep->ep_number == epnumber);

	DPRINTFN(MD_BULK,
	    ("motg_device_intr_rx on ep %d\n", epnumber));
        /* select endpoint */
	UWRITE1(sc, MUSB2_REG_EPINDEX, epnumber);

	/* read out FIFO status */
	csr = UREAD1(sc, MUSB2_REG_RXCSRL);
	DPRINTFN(MD_BULK,
	    ("motg_device_intr_rx phase %d csr 0x%x\n",
	    ep->phase, csr));

	if ((csr & (MUSB2_MASK_CSRL_RXNAKTO | MUSB2_MASK_CSRL_RXSTALL |
	    MUSB2_MASK_CSRL_RXERROR | MUSB2_MASK_CSRL_RXPKTRDY)) == 0)
		return;

#ifdef DIAGNOSTIC
	if (ep->phase != DATA_IN)
		panic("motg_device_intr_rx: bad phase %d", ep->phase);
#endif
	if (csr & MUSB2_MASK_CSRL_RXNAKTO) {
		csr &= ~MUSB2_MASK_CSRL_RXREQPKT;
		UWRITE1(sc, MUSB2_REG_RXCSRL, csr);

		csr &= ~MUSB2_MASK_CSRL_RXNAKTO;
		UWRITE1(sc, MUSB2_REG_RXCSRL, csr);
		new_status = USBD_TIMEOUT; /* XXX */
		goto complete;
	}
	if (csr & (MUSB2_MASK_CSRL_RXSTALL | MUSB2_MASK_CSRL_RXERROR)) {
		if (csr & MUSB2_MASK_CSRL_RXSTALL)
			new_status = USBD_STALLED;
		else
			new_status = USBD_IOERROR;
		/* clear status */
		UWRITE1(sc, MUSB2_REG_RXCSRL, 0);
		goto complete;
	}
	KASSERT(csr & MUSB2_MASK_CSRL_RXPKTRDY);

	if (xfer == NULL || xfer->status != USBD_IN_PROGRESS) {
		UWRITE1(sc, MUSB2_REG_RXCSRL, 0);
		goto complete;
	}

	struct motg_pipe *otgpipe = (struct motg_pipe *)xfer->pipe;
	otgpipe->nexttoggle = otgpipe->nexttoggle ^ 1;

	datalen = UREAD2(sc, MUSB2_REG_RXCOUNT);
	DPRINTFN(MD_BULK,
	    ("motg_device_intr_rx phase %d datalen %d\n",
	    ep->phase, datalen));
	KASSERT(UE_GET_SIZE(UGETW(xfer->pipe->endpoint->edesc->wMaxPacketSize)) > 0);
	max_datalen = min(
	    UE_GET_SIZE(UGETW(xfer->pipe->endpoint->edesc->wMaxPacketSize)),
	    ep->datalen);
	if (datalen > max_datalen) {
		new_status = USBD_IOERROR;
		UWRITE1(sc, MUSB2_REG_RXCSRL, 0);
		goto complete;
	}
	got_short = (datalen < max_datalen);
	if (datalen > 0) {
		KASSERT(ep->phase == DATA_IN);
		data = ep->data;
		ep->data += datalen;
		ep->datalen -= datalen;
		xfer->actlen += datalen;
		if (((vaddr_t)data & 0x3) == 0 &&
		    (datalen >> 2) > 0) {
			DPRINTFN(MD_BULK,
			    ("motg_device_intr_rx r4 data %p len %d\n",
			    data, datalen));
			bus_space_read_multi_4(sc->sc_iot, sc->sc_ioh,
			    MUSB2_REG_EPFIFO(ep->ep_number),
			    (void *)data, datalen >> 2);
			data += (datalen & ~0x3);
			datalen -= (datalen & ~0x3);
		}
		DPRINTFN(MD_BULK,
		    ("motg_device_intr_rx r1 data %p len %d\n",
		    data, datalen));
		if (datalen) {
			bus_space_read_multi_1(sc->sc_iot, sc->sc_ioh,
			    MUSB2_REG_EPFIFO(ep->ep_number), data, datalen);
		}
	}
	UWRITE1(sc, MUSB2_REG_RXCSRL, 0);
	KASSERT(ep->phase == DATA_IN);
	if (got_short || (ep->datalen == 0)) {
		if (ep->need_short_xfer == 0) {
			new_status = USBD_NORMAL_COMPLETION;
			goto complete;
		}
		ep->need_short_xfer = 0;
	}
	motg_device_data_read(xfer);
	return;
complete:
	DPRINTFN(MD_BULK,
	    ("motg_device_intr_rx xfer %p complete, status %d\n", xfer,
	    (xfer != NULL) ? xfer->status : 0));
	ep->phase = IDLE;
	ep->xfer = NULL;
	if (xfer && xfer->status == USBD_IN_PROGRESS) {
		KASSERT(new_status != USBD_IN_PROGRESS);
		xfer->status = new_status;
		usb_transfer_complete(xfer);
	}
	motg_device_data_start1(sc, ep);
}

static void
motg_device_intr_tx(struct motg_softc *sc, int epnumber)
{
	struct motg_hw_ep *ep = &sc->sc_out_ep[epnumber];
	usbd_xfer_handle xfer = ep->xfer;
	uint8_t csr;
	struct motg_pipe *otgpipe;
	usbd_status new_status = USBD_IN_PROGRESS;

	KASSERT(mutex_owned(&sc->sc_lock));
	KASSERT(ep->ep_number == epnumber);

	DPRINTFN(MD_BULK,
	    ("motg_device_intr_tx on ep %d\n", epnumber));
        /* select endpoint */
	UWRITE1(sc, MUSB2_REG_EPINDEX, epnumber);

	csr = UREAD1(sc, MUSB2_REG_TXCSRL);
	DPRINTFN(MD_BULK,
	    ("motg_device_intr_tx phase %d csr 0x%x\n",
	    ep->phase, csr));

	if (csr & (MUSB2_MASK_CSRL_TXSTALLED|MUSB2_MASK_CSRL_TXERROR)) {
		/* command not accepted */
		if (csr & MUSB2_MASK_CSRL_TXSTALLED)
			new_status = USBD_STALLED;
		else
			new_status = USBD_IOERROR;
		/* clear status */
		UWRITE1(sc, MUSB2_REG_TXCSRL, 0);
		goto complete;
	}
	if (csr & MUSB2_MASK_CSRL_TXNAKTO) {
		new_status = USBD_TIMEOUT; /* XXX */
		csr &= ~MUSB2_MASK_CSRL_TXNAKTO;
		UWRITE1(sc, MUSB2_REG_TXCSRL, csr);
		/* flush fifo */
		while (csr & MUSB2_MASK_CSRL_TXFIFONEMPTY) {
			csr |= MUSB2_MASK_CSRL_TXFFLUSH;
			csr &= ~MUSB2_MASK_CSRL_TXNAKTO;
			UWRITE1(sc, MUSB2_REG_TXCSRL, csr);
			delay(1000);
			csr = UREAD1(sc, MUSB2_REG_TXCSRL);
			DPRINTFN(MD_BULK, ("TX fifo flush ep %d CSR 0x%x\n",
			    epnumber, csr));
		}
		goto complete;
	}
	if (csr & (MUSB2_MASK_CSRL_TXFIFONEMPTY|MUSB2_MASK_CSRL_TXPKTRDY)) {
		/* data still not sent */
		return;
	}
	if (xfer == NULL || xfer->status != USBD_IN_PROGRESS)
		goto complete;
#ifdef DIAGNOSTIC
	if (ep->phase != DATA_OUT)
		panic("motg_device_intr_tx: bad phase %d", ep->phase);
#endif

	otgpipe = (struct motg_pipe *)xfer->pipe;
	otgpipe->nexttoggle = otgpipe->nexttoggle ^ 1;

	if (ep->datalen == 0) {
		if (ep->need_short_xfer) {
			ep->need_short_xfer = 0;
			/* one more data phase */
		} else {
			new_status = USBD_NORMAL_COMPLETION;
			goto complete;
		}
	}
	motg_device_data_write(xfer);
	return;

complete:
	DPRINTFN(MD_BULK,
	    ("motg_device_intr_tx xfer %p complete, status %d\n", xfer,
	    (xfer != NULL) ? xfer->status : 0));
#ifdef DIAGNOSTIC
	if (xfer && xfer->status == USBD_IN_PROGRESS && ep->phase != DATA_OUT)
		panic("motg_device_intr_tx: bad phase %d", ep->phase);
#endif
	ep->phase = IDLE;
	ep->xfer = NULL;
	if (xfer && xfer->status == USBD_IN_PROGRESS) {
		KASSERT(new_status != USBD_IN_PROGRESS);
		xfer->status = new_status;
		usb_transfer_complete(xfer);
	}
	motg_device_data_start1(sc, ep);
}

/* Abort a device control request. */
void
motg_device_data_abort(usbd_xfer_handle xfer)
{
#ifdef DIAGNOSTIC
	struct motg_softc *sc = xfer->pipe->device->bus->hci_private;
#endif
	KASSERT(mutex_owned(&sc->sc_lock));

	DPRINTFN(MD_BULK, ("motg_device_data_abort:\n"));
	motg_device_xfer_abort(xfer);
}

/* Close a device control pipe */
void
motg_device_data_close(usbd_pipe_handle pipe)
{
	struct motg_softc *sc __diagused = pipe->device->bus->hci_private;
	struct motg_pipe *otgpipe = (struct motg_pipe *)pipe;
	struct motg_pipe *otgpipeiter;

	DPRINTFN(MD_CTRL, ("motg_device_data_close:\n"));
	KASSERT(mutex_owned(&sc->sc_lock));
	KASSERT(otgpipe->hw_ep->xfer == NULL ||
	    otgpipe->hw_ep->xfer->pipe != pipe);

	pipe->endpoint->datatoggle = otgpipe->nexttoggle;
	SIMPLEQ_FOREACH(otgpipeiter, &otgpipe->hw_ep->ep_pipes, ep_pipe_list) {
		if (otgpipeiter == otgpipe) {
			/* remove from list */
			SIMPLEQ_REMOVE(&otgpipe->hw_ep->ep_pipes, otgpipe,
			    motg_pipe, ep_pipe_list);
			otgpipe->hw_ep->refcount--;
			/* we're done */
			return;
		}
	}
	panic("motg_device_data_close: not found");
}

void
motg_device_data_done(usbd_xfer_handle xfer)
{
	struct motg_pipe *otgpipe __diagused = (struct motg_pipe *)xfer->pipe;
	DPRINTFN(MD_CTRL, ("motg_device_data_done:\n"));
	KASSERT(otgpipe->hw_ep->xfer != xfer);
}

/*
 * Wait here until controller claims to have an interrupt.
 * Then call motg_intr and return.  Use timeout to avoid waiting
 * too long.
 * Only used during boot when interrupts are not enabled yet.
 */
void
motg_waitintr(struct motg_softc *sc, usbd_xfer_handle xfer)
{
	int timo = xfer->timeout;

	mutex_enter(&sc->sc_lock);

	DPRINTF(("motg_waitintr: timeout = %dms\n", timo));

	for (; timo >= 0; timo--) {
		mutex_exit(&sc->sc_lock);
		usb_delay_ms(&sc->sc_bus, 1);
		mutex_spin_enter(&sc->sc_intr_lock);
		motg_poll(&sc->sc_bus);
		mutex_spin_exit(&sc->sc_intr_lock);
		mutex_enter(&sc->sc_lock);
		if (xfer->status != USBD_IN_PROGRESS)
			goto done;
	}

	/* Timeout */
	DPRINTF(("motg_waitintr: timeout\n"));
	panic("motg_waitintr: timeout");
	/* XXX handle timeout ! */

done:
	mutex_exit(&sc->sc_lock);
}

void
motg_device_clear_toggle(usbd_pipe_handle pipe)
{
	struct motg_pipe *otgpipe = (struct motg_pipe *)pipe;
	otgpipe->nexttoggle = 0;
}

/* Abort a device control request. */
static void
motg_device_xfer_abort(usbd_xfer_handle xfer)
{
	int wake;
	uint8_t csr;
	struct motg_softc *sc = xfer->pipe->device->bus->hci_private;
	struct motg_pipe *otgpipe = (struct motg_pipe *)xfer->pipe;
	KASSERT(mutex_owned(&sc->sc_lock));

	DPRINTF(("motg_device_xfer_abort:\n"));
	if (xfer->hcflags & UXFER_ABORTING) {
		DPRINTF(("motg_device_xfer_abort: already aborting\n"));
		xfer->hcflags |= UXFER_ABORTWAIT;
		while (xfer->hcflags & UXFER_ABORTING)
			cv_wait(&xfer->hccv, &sc->sc_lock);
		return;
	}
	xfer->hcflags |= UXFER_ABORTING;
	if (otgpipe->hw_ep->xfer == xfer) {
		KASSERT(xfer->status == USBD_IN_PROGRESS);
		otgpipe->hw_ep->xfer = NULL;
		if (otgpipe->hw_ep->ep_number > 0) {
			/* select endpoint */
			UWRITE1(sc, MUSB2_REG_EPINDEX,
			    otgpipe->hw_ep->ep_number);
			if (otgpipe->hw_ep->phase == DATA_OUT) {
				csr = UREAD1(sc, MUSB2_REG_TXCSRL);
				while (csr & MUSB2_MASK_CSRL_TXFIFONEMPTY) {
					csr |= MUSB2_MASK_CSRL_TXFFLUSH;
					UWRITE1(sc, MUSB2_REG_TXCSRL, csr);
					csr = UREAD1(sc, MUSB2_REG_TXCSRL);
				}
				UWRITE1(sc, MUSB2_REG_TXCSRL, 0);
			} else if (otgpipe->hw_ep->phase == DATA_IN) {
				csr = UREAD1(sc, MUSB2_REG_RXCSRL);
				while (csr & MUSB2_MASK_CSRL_RXPKTRDY) {
					csr |= MUSB2_MASK_CSRL_RXFFLUSH;
					UWRITE1(sc, MUSB2_REG_RXCSRL, csr);
					csr = UREAD1(sc, MUSB2_REG_RXCSRL);
				}
				UWRITE1(sc, MUSB2_REG_RXCSRL, 0);
			}
			otgpipe->hw_ep->phase = IDLE;
		}
	}
	xfer->status = USBD_CANCELLED; /* make software ignore it */
	wake = xfer->hcflags & UXFER_ABORTWAIT;
	xfer->hcflags &= ~(UXFER_ABORTING | UXFER_ABORTWAIT);
	usb_transfer_complete(xfer);
	if (wake)
		cv_broadcast(&xfer->hccv);
}
