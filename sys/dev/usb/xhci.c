/*	$NetBSD: xhci.c,v 1.29 2015/08/19 06:23:35 skrll Exp $	*/

/*
 * Copyright (c) 2013 Jonathan A. Kollasch
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: xhci.c,v 1.29 2015/08/19 06:23:35 skrll Exp $");

#include "opt_usb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/select.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/bus.h>
#include <sys/cpu.h>
#include <sys/sysctl.h>

#include <machine/endian.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usbhist.h>
#include <dev/usb/usb_mem.h>
#include <dev/usb/usb_quirks.h>

#include <dev/usb/xhcireg.h>
#include <dev/usb/xhcivar.h>
#include <dev/usb/usbroothub_subr.h>


#ifdef USB_DEBUG
#ifndef XHCI_DEBUG
#define xhcidebug 0
#else
static int xhcidebug = 0;

SYSCTL_SETUP(sysctl_hw_xhci_setup, "sysctl hw.xhci setup")
{
	int err;
	const struct sysctlnode *rnode;
	const struct sysctlnode *cnode;

	err = sysctl_createv(clog, 0, NULL, &rnode,
	    CTLFLAG_PERMANENT, CTLTYPE_NODE, "xhci",
	    SYSCTL_DESCR("xhci global controls"),
	    NULL, 0, NULL, 0, CTL_HW, CTL_CREATE, CTL_EOL);

	if (err)
		goto fail;

	/* control debugging printfs */
	err = sysctl_createv(clog, 0, &rnode, &cnode,
	    CTLFLAG_PERMANENT|CTLFLAG_READWRITE, CTLTYPE_INT,
	    "debug", SYSCTL_DESCR("Enable debugging output"),
	    NULL, 0, &xhcidebug, sizeof(xhcidebug), CTL_CREATE, CTL_EOL);
	if (err)
		goto fail;

	return;
fail:
	aprint_error("%s: sysctl_createv failed (err = %d)\n", __func__, err);
}

#endif /* XHCI_DEBUG */
#endif /* USB_DEBUG */

#define DPRINTFN(N,FMT,A,B,C,D) USBHIST_LOGN(xhcidebug,N,FMT,A,B,C,D)
#define XHCIHIST_FUNC() USBHIST_FUNC()
#define XHCIHIST_CALLED(name) USBHIST_CALLED(xhcidebug)

#define XHCI_DCI_SLOT 0
#define XHCI_DCI_EP_CONTROL 1

#define XHCI_ICI_INPUT_CONTROL 0

struct xhci_pipe {
	struct usbd_pipe xp_pipe;
};

#define XHCI_INTR_ENDPT 1
#define XHCI_COMMAND_RING_TRBS 256
#define XHCI_EVENT_RING_TRBS 256
#define XHCI_EVENT_RING_SEGMENTS 1
#define XHCI_TRB_3_ED_BIT XHCI_TRB_3_ISP_BIT

static usbd_status xhci_open(usbd_pipe_handle);
static int xhci_intr1(struct xhci_softc * const);
static void xhci_softintr(void *);
static void xhci_poll(struct usbd_bus *);
static usbd_status xhci_allocm(struct usbd_bus *, usb_dma_t *, uint32_t);
static void xhci_freem(struct usbd_bus *, usb_dma_t *);
static usbd_xfer_handle xhci_allocx(struct usbd_bus *);
static void xhci_freex(struct usbd_bus *, usbd_xfer_handle);
static void xhci_get_lock(struct usbd_bus *, kmutex_t **);
static usbd_status xhci_new_device(device_t, usbd_bus_handle, int, int, int,
    struct usbd_port *);

static usbd_status xhci_configure_endpoint(usbd_pipe_handle);
static usbd_status xhci_unconfigure_endpoint(usbd_pipe_handle);
static usbd_status xhci_reset_endpoint(usbd_pipe_handle);
//static usbd_status xhci_stop_endpoint(usbd_pipe_handle);

static usbd_status xhci_set_dequeue(usbd_pipe_handle);

static usbd_status xhci_do_command(struct xhci_softc * const,
    struct xhci_trb * const, int);
static usbd_status xhci_init_slot(struct xhci_softc * const, uint32_t,
    int, int, int, int);
static usbd_status xhci_enable_slot(struct xhci_softc * const,
    uint8_t * const);
static usbd_status xhci_address_device(struct xhci_softc * const,
    uint64_t, uint8_t, bool);
static usbd_status xhci_update_ep0_mps(struct xhci_softc * const,
    struct xhci_slot * const, u_int);
static usbd_status xhci_ring_init(struct xhci_softc * const,
    struct xhci_ring * const, size_t, size_t);
static void xhci_ring_free(struct xhci_softc * const, struct xhci_ring * const);

static void xhci_noop(usbd_pipe_handle);

static usbd_status xhci_root_ctrl_transfer(usbd_xfer_handle);
static usbd_status xhci_root_ctrl_start(usbd_xfer_handle);
static void xhci_root_ctrl_abort(usbd_xfer_handle);
static void xhci_root_ctrl_close(usbd_pipe_handle);
static void xhci_root_ctrl_done(usbd_xfer_handle);

static usbd_status xhci_root_intr_transfer(usbd_xfer_handle);
static usbd_status xhci_root_intr_start(usbd_xfer_handle);
static void xhci_root_intr_abort(usbd_xfer_handle);
static void xhci_root_intr_close(usbd_pipe_handle);
static void xhci_root_intr_done(usbd_xfer_handle);

static usbd_status xhci_device_ctrl_transfer(usbd_xfer_handle);
static usbd_status xhci_device_ctrl_start(usbd_xfer_handle);
static void xhci_device_ctrl_abort(usbd_xfer_handle);
static void xhci_device_ctrl_close(usbd_pipe_handle);
static void xhci_device_ctrl_done(usbd_xfer_handle);

static usbd_status xhci_device_intr_transfer(usbd_xfer_handle);
static usbd_status xhci_device_intr_start(usbd_xfer_handle);
static void xhci_device_intr_abort(usbd_xfer_handle);
static void xhci_device_intr_close(usbd_pipe_handle);
static void xhci_device_intr_done(usbd_xfer_handle);

static usbd_status xhci_device_bulk_transfer(usbd_xfer_handle);
static usbd_status xhci_device_bulk_start(usbd_xfer_handle);
static void xhci_device_bulk_abort(usbd_xfer_handle);
static void xhci_device_bulk_close(usbd_pipe_handle);
static void xhci_device_bulk_done(usbd_xfer_handle);

static void xhci_timeout(void *);
static void xhci_timeout_task(void *);

static const struct usbd_bus_methods xhci_bus_methods = {
	.open_pipe = xhci_open,
	.soft_intr = xhci_softintr,
	.do_poll = xhci_poll,
	.allocm = xhci_allocm,
	.freem = xhci_freem,
	.allocx = xhci_allocx,
	.freex = xhci_freex,
	.get_lock = xhci_get_lock,
	.new_device = xhci_new_device,
};

static const struct usbd_pipe_methods xhci_root_ctrl_methods = {
	.transfer = xhci_root_ctrl_transfer,
	.start = xhci_root_ctrl_start,
	.abort = xhci_root_ctrl_abort,
	.close = xhci_root_ctrl_close,
	.cleartoggle = xhci_noop,
	.done = xhci_root_ctrl_done,
};

static const struct usbd_pipe_methods xhci_root_intr_methods = {
	.transfer = xhci_root_intr_transfer,
	.start = xhci_root_intr_start,
	.abort = xhci_root_intr_abort,
	.close = xhci_root_intr_close,
	.cleartoggle = xhci_noop,
	.done = xhci_root_intr_done,
};


static const struct usbd_pipe_methods xhci_device_ctrl_methods = {
	.transfer = xhci_device_ctrl_transfer,
	.start = xhci_device_ctrl_start,
	.abort = xhci_device_ctrl_abort,
	.close = xhci_device_ctrl_close,
	.cleartoggle = xhci_noop,
	.done = xhci_device_ctrl_done,
};

static const struct usbd_pipe_methods xhci_device_isoc_methods = {
	.cleartoggle = xhci_noop,
};

static const struct usbd_pipe_methods xhci_device_bulk_methods = {
	.transfer = xhci_device_bulk_transfer,
	.start = xhci_device_bulk_start,
	.abort = xhci_device_bulk_abort,
	.close = xhci_device_bulk_close,
	.cleartoggle = xhci_noop,
	.done = xhci_device_bulk_done,
};

static const struct usbd_pipe_methods xhci_device_intr_methods = {
	.transfer = xhci_device_intr_transfer,
	.start = xhci_device_intr_start,
	.abort = xhci_device_intr_abort,
	.close = xhci_device_intr_close,
	.cleartoggle = xhci_noop,
	.done = xhci_device_intr_done,
};

static inline uint32_t
xhci_read_4(const struct xhci_softc * const sc, bus_size_t offset)
{
	return bus_space_read_4(sc->sc_iot, sc->sc_ioh, offset);
}

#if 0 /* unused */
static inline void
xhci_write_4(const struct xhci_softc * const sc, bus_size_t offset,
    uint32_t value)
{
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, offset, value);
}
#endif /* unused */

static inline uint32_t
xhci_cap_read_4(const struct xhci_softc * const sc, bus_size_t offset)
{
	return bus_space_read_4(sc->sc_iot, sc->sc_cbh, offset);
}

static inline uint32_t
xhci_op_read_4(const struct xhci_softc * const sc, bus_size_t offset)
{
	return bus_space_read_4(sc->sc_iot, sc->sc_obh, offset);
}

static inline void
xhci_op_write_4(const struct xhci_softc * const sc, bus_size_t offset,
    uint32_t value)
{
	bus_space_write_4(sc->sc_iot, sc->sc_obh, offset, value);
}

#if 0 /* unused */
static inline uint64_t
xhci_op_read_8(const struct xhci_softc * const sc, bus_size_t offset)
{
	uint64_t value;

	if (sc->sc_ac64) {
#ifdef XHCI_USE_BUS_SPACE_8
		value = bus_space_read_8(sc->sc_iot, sc->sc_obh, offset);
#else
		value = bus_space_read_4(sc->sc_iot, sc->sc_obh, offset);
		value |= (uint64_t)bus_space_read_4(sc->sc_iot, sc->sc_obh,
		    offset + 4) << 32;
#endif
	} else {
		value = bus_space_read_4(sc->sc_iot, sc->sc_obh, offset);
	}

	return value;
}
#endif /* unused */

static inline void
xhci_op_write_8(const struct xhci_softc * const sc, bus_size_t offset,
    uint64_t value)
{
	if (sc->sc_ac64) {
#ifdef XHCI_USE_BUS_SPACE_8
		bus_space_write_8(sc->sc_iot, sc->sc_obh, offset, value);
#else
		bus_space_write_4(sc->sc_iot, sc->sc_obh, offset + 0,
		    (value >> 0) & 0xffffffff);
		bus_space_write_4(sc->sc_iot, sc->sc_obh, offset + 4,
		    (value >> 32) & 0xffffffff);
#endif
	} else {
		bus_space_write_4(sc->sc_iot, sc->sc_obh, offset, value);
	}
}

static inline uint32_t
xhci_rt_read_4(const struct xhci_softc * const sc, bus_size_t offset)
{
	return bus_space_read_4(sc->sc_iot, sc->sc_rbh, offset);
}

static inline void
xhci_rt_write_4(const struct xhci_softc * const sc, bus_size_t offset,
    uint32_t value)
{
	bus_space_write_4(sc->sc_iot, sc->sc_rbh, offset, value);
}

#if 0 /* unused */
static inline uint64_t
xhci_rt_read_8(const struct xhci_softc * const sc, bus_size_t offset)
{
	uint64_t value;

	if (sc->sc_ac64) {
#ifdef XHCI_USE_BUS_SPACE_8
		value = bus_space_read_8(sc->sc_iot, sc->sc_rbh, offset);
#else
		value = bus_space_read_4(sc->sc_iot, sc->sc_rbh, offset);
		value |= (uint64_t)bus_space_read_4(sc->sc_iot, sc->sc_rbh,
		    offset + 4) << 32;
#endif
	} else {
		value = bus_space_read_4(sc->sc_iot, sc->sc_rbh, offset);
	}

	return value;
}
#endif /* unused */

static inline void
xhci_rt_write_8(const struct xhci_softc * const sc, bus_size_t offset,
    uint64_t value)
{
	if (sc->sc_ac64) {
#ifdef XHCI_USE_BUS_SPACE_8
		bus_space_write_8(sc->sc_iot, sc->sc_rbh, offset, value);
#else
		bus_space_write_4(sc->sc_iot, sc->sc_rbh, offset + 0,
		    (value >> 0) & 0xffffffff);
		bus_space_write_4(sc->sc_iot, sc->sc_rbh, offset + 4,
		    (value >> 32) & 0xffffffff);
#endif
	} else {
		bus_space_write_4(sc->sc_iot, sc->sc_rbh, offset, value);
	}
}

#if 0 /* unused */
static inline uint32_t
xhci_db_read_4(const struct xhci_softc * const sc, bus_size_t offset)
{
	return bus_space_read_4(sc->sc_iot, sc->sc_dbh, offset);
}
#endif /* unused */

static inline void
xhci_db_write_4(const struct xhci_softc * const sc, bus_size_t offset,
    uint32_t value)
{
	bus_space_write_4(sc->sc_iot, sc->sc_dbh, offset, value);
}

/* --- */

static inline uint8_t
xhci_ep_get_type(usb_endpoint_descriptor_t * const ed)
{
	u_int eptype;

	switch (UE_GET_XFERTYPE(ed->bmAttributes)) {
	case UE_CONTROL:
		eptype = 0x0;
		break;
	case UE_ISOCHRONOUS:
		eptype = 0x1;
		break;
	case UE_BULK:
		eptype = 0x2;
		break;
	case UE_INTERRUPT:
		eptype = 0x3;
		break;
	}

	if ((UE_GET_XFERTYPE(ed->bmAttributes) == UE_CONTROL) ||
	    (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN))
		return eptype | 0x4;
	else
		return eptype;
}

static u_int
xhci_ep_get_dci(usb_endpoint_descriptor_t * const ed)
{
	/* xHCI 1.0 section 4.5.1 */
	u_int epaddr = UE_GET_ADDR(ed->bEndpointAddress);
	u_int in = 0;

	if ((UE_GET_XFERTYPE(ed->bmAttributes) == UE_CONTROL) ||
	    (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN))
		in = 1;

	return epaddr * 2 + in;
}

static inline u_int
xhci_dci_to_ici(const u_int i)
{
	return i + 1;
}

static inline void *
xhci_slot_get_dcv(struct xhci_softc * const sc, struct xhci_slot * const xs,
    const u_int dci)
{
	return KERNADDR(&xs->xs_dc_dma, sc->sc_ctxsz * dci);
}

#if 0 /* unused */
static inline bus_addr_t
xhci_slot_get_dcp(struct xhci_softc * const sc, struct xhci_slot * const xs,
    const u_int dci)
{
	return DMAADDR(&xs->xs_dc_dma, sc->sc_ctxsz * dci);
}
#endif /* unused */

static inline void *
xhci_slot_get_icv(struct xhci_softc * const sc, struct xhci_slot * const xs,
    const u_int ici)
{
	return KERNADDR(&xs->xs_ic_dma, sc->sc_ctxsz * ici);
}

static inline bus_addr_t
xhci_slot_get_icp(struct xhci_softc * const sc, struct xhci_slot * const xs,
    const u_int ici)
{
	return DMAADDR(&xs->xs_ic_dma, sc->sc_ctxsz * ici);
}

static inline struct xhci_trb *
xhci_ring_trbv(struct xhci_ring * const xr, u_int idx)
{
	return KERNADDR(&xr->xr_dma, XHCI_TRB_SIZE * idx);
}

static inline bus_addr_t
xhci_ring_trbp(struct xhci_ring * const xr, u_int idx)
{
	return DMAADDR(&xr->xr_dma, XHCI_TRB_SIZE * idx);
}

static inline void
xhci_trb_put(struct xhci_trb * const trb, uint64_t parameter, uint32_t status,
    uint32_t control)
{
	trb->trb_0 = parameter;
	trb->trb_2 = status;
	trb->trb_3 = control;
}

/* --- */

void
xhci_childdet(device_t self, device_t child)
{
	struct xhci_softc * const sc = device_private(self);

	KASSERT(sc->sc_child == child);
	if (child == sc->sc_child)
		sc->sc_child = NULL;
}

int
xhci_detach(struct xhci_softc *sc, int flags)
{
	int rv = 0;

	if (sc->sc_child != NULL)
		rv = config_detach(sc->sc_child, flags);

	if (rv != 0)
		return (rv);

	/* XXX unconfigure/free slots */

	/* verify: */
	xhci_rt_write_4(sc, XHCI_IMAN(0), 0);
	xhci_op_write_4(sc, XHCI_USBCMD, 0);
	/* do we need to wait for stop? */

	xhci_op_write_8(sc, XHCI_CRCR, 0);
	xhci_ring_free(sc, &sc->sc_cr);
	cv_destroy(&sc->sc_command_cv);

	xhci_rt_write_4(sc, XHCI_ERSTSZ(0), 0);
	xhci_rt_write_8(sc, XHCI_ERSTBA(0), 0);
	xhci_rt_write_8(sc, XHCI_ERDP(0), 0|XHCI_ERDP_LO_BUSY);
	xhci_ring_free(sc, &sc->sc_er);

	usb_freemem(&sc->sc_bus, &sc->sc_eventst_dma);

	xhci_op_write_8(sc, XHCI_DCBAAP, 0);
	usb_freemem(&sc->sc_bus, &sc->sc_dcbaa_dma);

	kmem_free(sc->sc_slots, sizeof(*sc->sc_slots) * sc->sc_maxslots);

	mutex_destroy(&sc->sc_lock);
	mutex_destroy(&sc->sc_intr_lock);

	pool_cache_destroy(sc->sc_xferpool);

	return rv;
}

int
xhci_activate(device_t self, enum devact act)
{
	struct xhci_softc * const sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		sc->sc_dying = true;
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

bool
xhci_suspend(device_t dv, const pmf_qual_t *qual)
{
	return false;
}

bool
xhci_resume(device_t dv, const pmf_qual_t *qual)
{
	return false;
}

bool
xhci_shutdown(device_t self, int flags)
{
	return false;
}


static void
hexdump(const char *msg, const void *base, size_t len)
{
#if 0
	size_t cnt;
	const uint32_t *p;
	extern paddr_t vtophys(vaddr_t);

	p = base;
	cnt = 0;

	printf("*** %s (%zu bytes @ %p %p)\n", msg, len, base,
	    (void *)vtophys((vaddr_t)base));

	while (cnt < len) {
		if (cnt % 16 == 0)
			printf("%p: ", p);
		else if (cnt % 8 == 0)
			printf(" |");
		printf(" %08x", *p++);
		cnt += 4;
		if (cnt % 16 == 0)
			printf("\n");
	}
#endif
}


int
xhci_init(struct xhci_softc *sc)
{
	bus_size_t bsz;
	uint32_t cap, hcs1, hcs2, hcc, dboff, rtsoff;
	uint32_t ecp, ecr;
	uint32_t usbcmd, usbsts, pagesize, config;
	int i;
	uint16_t hciversion;
	uint8_t caplength;

	XHCIHIST_FUNC(); XHCIHIST_CALLED();

	/* XXX Low/Full/High speeds for now */
	sc->sc_bus.usbrev = USBREV_2_0;

	cap = xhci_read_4(sc, XHCI_CAPLENGTH);
	caplength = XHCI_CAP_CAPLENGTH(cap);
	hciversion = XHCI_CAP_HCIVERSION(cap);

	if ((hciversion < 0x0096) || (hciversion > 0x0100)) {
		aprint_normal_dev(sc->sc_dev,
		    "xHCI version %x.%x not known to be supported\n",
		    (hciversion >> 8) & 0xff, (hciversion >> 0) & 0xff);
	} else {
		aprint_verbose_dev(sc->sc_dev, "xHCI version %x.%x\n",
		    (hciversion >> 8) & 0xff, (hciversion >> 0) & 0xff);
	}

	if (bus_space_subregion(sc->sc_iot, sc->sc_ioh, 0, caplength,
	    &sc->sc_cbh) != 0) {
		aprint_error_dev(sc->sc_dev, "capability subregion failure\n");
		return ENOMEM;
	}

	hcs1 = xhci_cap_read_4(sc, XHCI_HCSPARAMS1);
	sc->sc_maxslots = XHCI_HCS1_MAXSLOTS(hcs1);
	sc->sc_maxintrs = XHCI_HCS1_MAXINTRS(hcs1);
	sc->sc_maxports = XHCI_HCS1_MAXPORTS(hcs1);
	hcs2 = xhci_cap_read_4(sc, XHCI_HCSPARAMS2);
	(void)xhci_cap_read_4(sc, XHCI_HCSPARAMS3);
	hcc = xhci_cap_read_4(sc, XHCI_HCCPARAMS);

	sc->sc_ac64 = XHCI_HCC_AC64(hcc);
	sc->sc_ctxsz = XHCI_HCC_CSZ(hcc) ? 64 : 32;
	aprint_debug_dev(sc->sc_dev, "ac64 %d ctxsz %d\n", sc->sc_ac64,
	    sc->sc_ctxsz);

	aprint_debug_dev(sc->sc_dev, "xECP %x\n", XHCI_HCC_XECP(hcc) * 4);
	ecp = XHCI_HCC_XECP(hcc) * 4;
	while (ecp != 0) {
		ecr = xhci_read_4(sc, ecp);
		aprint_debug_dev(sc->sc_dev, "ECR %x: %08x\n", ecp, ecr);
		switch (XHCI_XECP_ID(ecr)) {
		case XHCI_ID_PROTOCOLS: {
			uint32_t w0, w4, w8;
			uint16_t w2;
			w0 = xhci_read_4(sc, ecp + 0);
			w2 = (w0 >> 16) & 0xffff;
			w4 = xhci_read_4(sc, ecp + 4);
			w8 = xhci_read_4(sc, ecp + 8);
			aprint_debug_dev(sc->sc_dev, "SP: %08x %08x %08x\n",
			    w0, w4, w8);
			if (w4 == 0x20425355 && w2 == 0x0300) {
				sc->sc_ss_port_start = (w8 >> 0) & 0xff;;
				sc->sc_ss_port_count = (w8 >> 8) & 0xff;;
			}
			if (w4 == 0x20425355 && w2 == 0x0200) {
				sc->sc_hs_port_start = (w8 >> 0) & 0xff;
				sc->sc_hs_port_count = (w8 >> 8) & 0xff;
			}
			break;
		}
		default:
			break;
		}
		ecr = xhci_read_4(sc, ecp);
		if (XHCI_XECP_NEXT(ecr) == 0) {
			ecp = 0;
		} else {
			ecp += XHCI_XECP_NEXT(ecr) * 4;
		}
	}

	bsz = XHCI_PORTSC(sc->sc_maxports + 1);
	if (bus_space_subregion(sc->sc_iot, sc->sc_ioh, caplength, bsz,
	    &sc->sc_obh) != 0) {
		aprint_error_dev(sc->sc_dev, "operational subregion failure\n");
		return ENOMEM;
	}

	dboff = xhci_cap_read_4(sc, XHCI_DBOFF);
	if (bus_space_subregion(sc->sc_iot, sc->sc_ioh, dboff,
	    sc->sc_maxslots * 4, &sc->sc_dbh) != 0) {
		aprint_error_dev(sc->sc_dev, "doorbell subregion failure\n");
		return ENOMEM;
	}

	rtsoff = xhci_cap_read_4(sc, XHCI_RTSOFF);
	if (bus_space_subregion(sc->sc_iot, sc->sc_ioh, rtsoff,
	    sc->sc_maxintrs * 0x20, &sc->sc_rbh) != 0) {
		aprint_error_dev(sc->sc_dev, "runtime subregion failure\n");
		return ENOMEM;
	}

	for (i = 0; i < 100; i++) {
		usbsts = xhci_op_read_4(sc, XHCI_USBSTS);
		if ((usbsts & XHCI_STS_CNR) == 0)
			break;
		usb_delay_ms(&sc->sc_bus, 1);
	}
	if (i >= 100)
		return EIO;

	usbcmd = 0;
	xhci_op_write_4(sc, XHCI_USBCMD, usbcmd);
	usb_delay_ms(&sc->sc_bus, 1);

	usbcmd = XHCI_CMD_HCRST;
	xhci_op_write_4(sc, XHCI_USBCMD, usbcmd);
	for (i = 0; i < 100; i++) {
		usbcmd = xhci_op_read_4(sc, XHCI_USBCMD);
		if ((usbcmd & XHCI_CMD_HCRST) == 0)
			break;
		usb_delay_ms(&sc->sc_bus, 1);
	}
	if (i >= 100)
		return EIO;

	for (i = 0; i < 100; i++) {
		usbsts = xhci_op_read_4(sc, XHCI_USBSTS);
		if ((usbsts & XHCI_STS_CNR) == 0)
			break;
		usb_delay_ms(&sc->sc_bus, 1);
	}
	if (i >= 100)
		return EIO;

	pagesize = xhci_op_read_4(sc, XHCI_PAGESIZE);
	aprint_debug_dev(sc->sc_dev, "PAGESIZE 0x%08x\n", pagesize);
	pagesize = ffs(pagesize);
	if (pagesize == 0)
		return EIO;
	sc->sc_pgsz = 1 << (12 + (pagesize - 1));
	aprint_debug_dev(sc->sc_dev, "sc_pgsz 0x%08x\n", (uint32_t)sc->sc_pgsz);
	aprint_debug_dev(sc->sc_dev, "sc_maxslots 0x%08x\n",
	    (uint32_t)sc->sc_maxslots);

	usbd_status err;

	sc->sc_maxspbuf = XHCI_HCS2_MAXSPBUF(hcs2);
	aprint_debug_dev(sc->sc_dev, "sc_maxspbuf %d\n", sc->sc_maxspbuf);
	if (sc->sc_maxspbuf != 0) {
		err = usb_allocmem(&sc->sc_bus,
		    sizeof(uint64_t) * sc->sc_maxspbuf, sizeof(uint64_t),
		    &sc->sc_spbufarray_dma);
		if (err)
			return err;
		
		sc->sc_spbuf_dma = kmem_zalloc(sizeof(*sc->sc_spbuf_dma) * sc->sc_maxspbuf, KM_SLEEP);
		uint64_t *spbufarray = KERNADDR(&sc->sc_spbufarray_dma, 0);
		for (i = 0; i < sc->sc_maxspbuf; i++) {
			usb_dma_t * const dma = &sc->sc_spbuf_dma[i];
			/* allocate contexts */
			err = usb_allocmem(&sc->sc_bus, sc->sc_pgsz,
			    sc->sc_pgsz, dma);
			if (err)
				return err;
			spbufarray[i] = htole64(DMAADDR(dma, 0));
			usb_syncmem(dma, 0, sc->sc_pgsz,
			    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
		}

		usb_syncmem(&sc->sc_spbufarray_dma, 0, 
		    sizeof(uint64_t) * sc->sc_maxspbuf, BUS_DMASYNC_PREWRITE);
	}

	config = xhci_op_read_4(sc, XHCI_CONFIG);
	config &= ~0xFF;
	config |= sc->sc_maxslots & 0xFF;
	xhci_op_write_4(sc, XHCI_CONFIG, config);

	err = xhci_ring_init(sc, &sc->sc_cr, XHCI_COMMAND_RING_TRBS,
	    XHCI_COMMAND_RING_SEGMENTS_ALIGN);
	if (err) {
		aprint_error_dev(sc->sc_dev, "command ring init fail\n");
		return err;
	}

	err = xhci_ring_init(sc, &sc->sc_er, XHCI_EVENT_RING_TRBS,
	    XHCI_EVENT_RING_SEGMENTS_ALIGN);
	if (err) {
		aprint_error_dev(sc->sc_dev, "event ring init fail\n");
		return err;
	}

	usb_dma_t *dma;
	size_t size;
	size_t align;

	dma = &sc->sc_eventst_dma;
	size = roundup2(XHCI_EVENT_RING_SEGMENTS * XHCI_ERSTE_SIZE,
	    XHCI_EVENT_RING_SEGMENT_TABLE_ALIGN);
	KASSERT(size <= (512 * 1024));
	align = XHCI_EVENT_RING_SEGMENT_TABLE_ALIGN;
	err = usb_allocmem(&sc->sc_bus, size, align, dma);

	memset(KERNADDR(dma, 0), 0, size);
	usb_syncmem(dma, 0, size, BUS_DMASYNC_PREWRITE);
	aprint_debug_dev(sc->sc_dev, "eventst: %s %016jx %p %zx\n",
	    usbd_errstr(err),
	    (uintmax_t)DMAADDR(&sc->sc_eventst_dma, 0),
	    KERNADDR(&sc->sc_eventst_dma, 0),
	    sc->sc_eventst_dma.block->size);

	dma = &sc->sc_dcbaa_dma;
	size = (1 + sc->sc_maxslots) * sizeof(uint64_t);
	KASSERT(size <= 2048);
	align = XHCI_DEVICE_CONTEXT_BASE_ADDRESS_ARRAY_ALIGN;
	err = usb_allocmem(&sc->sc_bus, size, align, dma);

	memset(KERNADDR(dma, 0), 0, size);
	if (sc->sc_maxspbuf != 0) {
		/*
		 * DCBA entry 0 hold the scratchbuf array pointer.
		 */
		*(uint64_t *)KERNADDR(dma, 0) =
		    htole64(DMAADDR(&sc->sc_spbufarray_dma, 0));
	}
	usb_syncmem(dma, 0, size, BUS_DMASYNC_PREWRITE);
	aprint_debug_dev(sc->sc_dev, "dcbaa: %s %016jx %p %zx\n",
	    usbd_errstr(err),
	    (uintmax_t)DMAADDR(&sc->sc_dcbaa_dma, 0),
	    KERNADDR(&sc->sc_dcbaa_dma, 0),
	    sc->sc_dcbaa_dma.block->size);

	sc->sc_slots = kmem_zalloc(sizeof(*sc->sc_slots) * sc->sc_maxslots,
	    KM_SLEEP);

	cv_init(&sc->sc_command_cv, "xhcicmd");

	struct xhci_erste *erst;
	erst = KERNADDR(&sc->sc_eventst_dma, 0);
	erst[0].erste_0 = htole64(xhci_ring_trbp(&sc->sc_er, 0));
	erst[0].erste_2 = htole32(XHCI_EVENT_RING_TRBS);
	erst[0].erste_3 = htole32(0);
	usb_syncmem(&sc->sc_eventst_dma, 0,
	    XHCI_ERSTE_SIZE * XHCI_EVENT_RING_SEGMENTS, BUS_DMASYNC_PREWRITE);

	xhci_rt_write_4(sc, XHCI_ERSTSZ(0), XHCI_EVENT_RING_SEGMENTS);
	xhci_rt_write_8(sc, XHCI_ERSTBA(0), DMAADDR(&sc->sc_eventst_dma, 0));
	xhci_rt_write_8(sc, XHCI_ERDP(0), xhci_ring_trbp(&sc->sc_er, 0) |
	    XHCI_ERDP_LO_BUSY);
	xhci_op_write_8(sc, XHCI_DCBAAP, DMAADDR(&sc->sc_dcbaa_dma, 0));
	xhci_op_write_8(sc, XHCI_CRCR, xhci_ring_trbp(&sc->sc_cr, 0) |
	    sc->sc_cr.xr_cs);

#if 0
	hexdump("eventst", KERNADDR(&sc->sc_eventst_dma, 0),
	    XHCI_ERSTE_SIZE * XHCI_EVENT_RING_SEGMENTS);
#endif

	xhci_rt_write_4(sc, XHCI_IMAN(0), XHCI_IMAN_INTR_ENA);
	xhci_rt_write_4(sc, XHCI_IMOD(0), 0);

	xhci_op_write_4(sc, XHCI_USBCMD, XHCI_CMD_INTE|XHCI_CMD_RS); /* Go! */
	aprint_debug_dev(sc->sc_dev, "USBCMD %08"PRIx32"\n",
	    xhci_op_read_4(sc, XHCI_USBCMD));

	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_SOFTUSB);
	mutex_init(&sc->sc_intr_lock, MUTEX_DEFAULT, IPL_USB);
	cv_init(&sc->sc_softwake_cv, "xhciab");

	sc->sc_xferpool = pool_cache_init(sizeof(struct xhci_xfer), 0, 0, 0,
	    "xhcixfer", NULL, IPL_USB, NULL, NULL, NULL);

	/* Set up the bus struct. */
	sc->sc_bus.methods = &xhci_bus_methods;
	sc->sc_bus.pipe_size = sizeof(struct xhci_pipe);

	return USBD_NORMAL_COMPLETION;
}

int
xhci_intr(void *v)
{
	struct xhci_softc * const sc = v;
	int ret = 0;

	XHCIHIST_FUNC(); XHCIHIST_CALLED();

	if (sc == NULL)
		return 0;

	mutex_spin_enter(&sc->sc_intr_lock);

	if (sc->sc_dying || !device_has_power(sc->sc_dev))
		goto done;

	/* If we get an interrupt while polling, then just ignore it. */
	if (sc->sc_bus.use_polling) {
#ifdef DIAGNOSTIC
		DPRINTFN(16, "ignored interrupt while polling", 0, 0, 0, 0);
#endif
		goto done;
	}

	ret = xhci_intr1(sc);
done:
	mutex_spin_exit(&sc->sc_intr_lock);
	return ret;
}

int
xhci_intr1(struct xhci_softc * const sc)
{
	uint32_t usbsts;
	uint32_t iman;

	XHCIHIST_FUNC(); XHCIHIST_CALLED();

	usbsts = xhci_op_read_4(sc, XHCI_USBSTS);
	DPRINTFN(16, "USBSTS %08x", usbsts, 0, 0, 0);
#if 0
	if ((usbsts & (XHCI_STS_EINT|XHCI_STS_PCD)) == 0) {
		return 0;
	}
#endif
	xhci_op_write_4(sc, XHCI_USBSTS,
	    usbsts & (2|XHCI_STS_EINT|XHCI_STS_PCD)); /* XXX */
	usbsts = xhci_op_read_4(sc, XHCI_USBSTS);
	DPRINTFN(16, "USBSTS %08x", usbsts, 0, 0, 0);

	iman = xhci_rt_read_4(sc, XHCI_IMAN(0));
	DPRINTFN(16, "IMAN0 %08x", iman, 0, 0, 0);
	if ((iman & XHCI_IMAN_INTR_PEND) == 0) {
		return 0;
	}
	xhci_rt_write_4(sc, XHCI_IMAN(0), iman);
	iman = xhci_rt_read_4(sc, XHCI_IMAN(0));
	DPRINTFN(16, "IMAN0 %08x", iman, 0, 0, 0);
	usbsts = xhci_op_read_4(sc, XHCI_USBSTS);
	DPRINTFN(16, "USBSTS %08x", usbsts, 0, 0, 0);

	sc->sc_bus.no_intrs++;
	usb_schedsoftintr(&sc->sc_bus);

	return 1;
}

static usbd_status
xhci_configure_endpoint(usbd_pipe_handle pipe)
{
	struct xhci_softc * const sc = pipe->device->bus->hci_private;
	struct xhci_slot * const xs = pipe->device->hci_private;
	const u_int dci = xhci_ep_get_dci(pipe->endpoint->edesc);
	usb_endpoint_descriptor_t * const ed = pipe->endpoint->edesc;
	const uint8_t xfertype = UE_GET_XFERTYPE(ed->bmAttributes);
	struct xhci_trb trb;
	usbd_status err;
	uint32_t *cp;

	XHCIHIST_FUNC(); XHCIHIST_CALLED();
	DPRINTFN(4, "dci %u epaddr 0x%02x attr 0x%02x",
	    dci, ed->bEndpointAddress, ed->bmAttributes, 0);

	/* XXX ensure input context is available? */

	memset(xhci_slot_get_icv(sc, xs, 0), 0, sc->sc_pgsz);

	cp = xhci_slot_get_icv(sc, xs, XHCI_ICI_INPUT_CONTROL);
	cp[0] = htole32(0);
	cp[1] = htole32(XHCI_INCTX_1_ADD_MASK(dci));

	/* set up input slot context */
	cp = xhci_slot_get_icv(sc, xs, xhci_dci_to_ici(XHCI_DCI_SLOT));
	cp[0] = htole32(XHCI_SCTX_0_CTX_NUM_SET(dci));
	cp[1] = htole32(0);
	cp[2] = htole32(0);
	cp[3] = htole32(0);

	cp = xhci_slot_get_icv(sc, xs, xhci_dci_to_ici(dci));
	if (xfertype == UE_INTERRUPT) {
	cp[0] = htole32(
	    XHCI_EPCTX_0_IVAL_SET(3) /* XXX */
	    );
	cp[1] = htole32(
	    XHCI_EPCTX_1_CERR_SET(3) |
	    XHCI_EPCTX_1_EPTYPE_SET(xhci_ep_get_type(pipe->endpoint->edesc)) |
	    XHCI_EPCTX_1_MAXB_SET(0) |
	    XHCI_EPCTX_1_MAXP_SIZE_SET(8) /* XXX */
	    );
	cp[4] = htole32(
		XHCI_EPCTX_4_AVG_TRB_LEN_SET(8)
		);
	} else {
	cp[0] = htole32(0);
	cp[1] = htole32(
	    XHCI_EPCTX_1_CERR_SET(3) |
	    XHCI_EPCTX_1_EPTYPE_SET(xhci_ep_get_type(pipe->endpoint->edesc)) |
	    XHCI_EPCTX_1_MAXB_SET(0) |
	    XHCI_EPCTX_1_MAXP_SIZE_SET(512) /* XXX */
	    );
	}
	*(uint64_t *)(&cp[2]) = htole64(
	    xhci_ring_trbp(&xs->xs_ep[dci].xe_tr, 0) |
	    XHCI_EPCTX_2_DCS_SET(1));

	/* sync input contexts before they are read from memory */
	usb_syncmem(&xs->xs_ic_dma, 0, sc->sc_pgsz, BUS_DMASYNC_PREWRITE);
	hexdump("input control context", xhci_slot_get_icv(sc, xs, 0),
	    sc->sc_ctxsz * 1);
	hexdump("input endpoint context", xhci_slot_get_icv(sc, xs,
	    xhci_dci_to_ici(dci)), sc->sc_ctxsz * 1);

	trb.trb_0 = xhci_slot_get_icp(sc, xs, 0);
	trb.trb_2 = 0;
	trb.trb_3 = XHCI_TRB_3_SLOT_SET(xs->xs_idx) |
	    XHCI_TRB_3_TYPE_SET(XHCI_TRB_TYPE_CONFIGURE_EP);

	err = xhci_do_command(sc, &trb, USBD_DEFAULT_TIMEOUT);

	usb_syncmem(&xs->xs_dc_dma, 0, sc->sc_pgsz, BUS_DMASYNC_POSTREAD);
	hexdump("output context", xhci_slot_get_dcv(sc, xs, dci),
	    sc->sc_ctxsz * 1);

	return err;
}

static usbd_status
xhci_unconfigure_endpoint(usbd_pipe_handle pipe)
{
#ifdef USB_DEBUG
	struct xhci_slot * const xs = pipe->device->hci_private;
#endif

	XHCIHIST_FUNC(); XHCIHIST_CALLED();
	DPRINTFN(4, "slot %u", xs->xs_idx, 0, 0, 0);

	return USBD_NORMAL_COMPLETION;
}

static usbd_status
xhci_reset_endpoint(usbd_pipe_handle pipe)
{
	struct xhci_softc * const sc = pipe->device->bus->hci_private;
	struct xhci_slot * const xs = pipe->device->hci_private;
	const u_int dci = xhci_ep_get_dci(pipe->endpoint->edesc);
	struct xhci_trb trb;
	usbd_status err;

	XHCIHIST_FUNC(); XHCIHIST_CALLED();
	DPRINTFN(4, "dci %u", dci, 0, 0, 0);

	trb.trb_0 = 0;
	trb.trb_2 = 0;
	trb.trb_3 = XHCI_TRB_3_SLOT_SET(xs->xs_idx) |
	    XHCI_TRB_3_EP_SET(dci) |
	    XHCI_TRB_3_TYPE_SET(XHCI_TRB_TYPE_RESET_EP);

	err = xhci_do_command(sc, &trb, USBD_DEFAULT_TIMEOUT);

	return err;
}

#if 0
static usbd_status
xhci_stop_endpoint(usbd_pipe_handle pipe)
{
	struct xhci_softc * const sc = pipe->device->bus->hci_private;
	struct xhci_slot * const xs = pipe->device->hci_private;
	struct xhci_trb trb;
	usbd_status err;
	const u_int dci = xhci_ep_get_dci(pipe->endpoint->edesc);

	XHCIHIST_FUNC(); XHCIHIST_CALLED();
	DPRINTFN(4, "dci %u", dci, 0, 0, 0);

	trb.trb_0 = 0;
	trb.trb_2 = 0;
	trb.trb_3 = XHCI_TRB_3_SLOT_SET(xs->xs_idx) |
	    XHCI_TRB_3_EP_SET(dci) |
	    XHCI_TRB_3_TYPE_SET(XHCI_TRB_TYPE_STOP_EP);

	err = xhci_do_command(sc, &trb, USBD_DEFAULT_TIMEOUT);

	return err;
}
#endif

static usbd_status
xhci_set_dequeue(usbd_pipe_handle pipe)
{
	struct xhci_softc * const sc = pipe->device->bus->hci_private;
	struct xhci_slot * const xs = pipe->device->hci_private;
	const u_int dci = xhci_ep_get_dci(pipe->endpoint->edesc);
	struct xhci_ring * const xr = &xs->xs_ep[dci].xe_tr;
	struct xhci_trb trb;
	usbd_status err;

	XHCIHIST_FUNC(); XHCIHIST_CALLED();
	DPRINTFN(4, "slot %u dci %u", xs->xs_idx, dci, 0, 0);

	memset(xr->xr_trb, 0, xr->xr_ntrb * XHCI_TRB_SIZE);
	usb_syncmem(&xr->xr_dma, 0, xr->xr_ntrb * XHCI_TRB_SIZE,
	    BUS_DMASYNC_PREWRITE);

	xr->xr_ep = 0;
	xr->xr_cs = 1;

	trb.trb_0 = xhci_ring_trbp(xr, 0) | 1; /* XXX */
	trb.trb_2 = 0;
	trb.trb_3 = XHCI_TRB_3_SLOT_SET(xs->xs_idx) |
	    XHCI_TRB_3_EP_SET(dci) |
	    XHCI_TRB_3_TYPE_SET(XHCI_TRB_TYPE_SET_TR_DEQUEUE);

	err = xhci_do_command(sc, &trb, USBD_DEFAULT_TIMEOUT);

	return err;
}

static usbd_status
xhci_open(usbd_pipe_handle pipe)
{
	usbd_device_handle const dev = pipe->device;
	struct xhci_softc * const sc = dev->bus->hci_private;
	usb_endpoint_descriptor_t * const ed = pipe->endpoint->edesc;
	const uint8_t xfertype = UE_GET_XFERTYPE(ed->bmAttributes);

	XHCIHIST_FUNC(); XHCIHIST_CALLED();
	DPRINTFN(1, "addr %d depth %d port %d speed %d",
	    dev->address, dev->depth, dev->powersrc->portno, dev->speed);

	if (sc->sc_dying)
		return USBD_IOERROR;

	/* Root Hub */
	if (dev->depth == 0 && dev->powersrc->portno == 0 &&
	    dev->speed != USB_SPEED_SUPER) {
		switch (ed->bEndpointAddress) {
		case USB_CONTROL_ENDPOINT:
			pipe->methods = &xhci_root_ctrl_methods;
			break;
		case UE_DIR_IN | XHCI_INTR_ENDPT:
			pipe->methods = &xhci_root_intr_methods;
			break;
		default:
			pipe->methods = NULL;
			DPRINTFN(0, "bad bEndpointAddress 0x%02x",
			    ed->bEndpointAddress, 0, 0, 0);
			return USBD_INVAL;
		}
		return USBD_NORMAL_COMPLETION;
	}

	switch (xfertype) {
	case UE_CONTROL:
		pipe->methods = &xhci_device_ctrl_methods;
		break;
	case UE_ISOCHRONOUS:
		pipe->methods = &xhci_device_isoc_methods;
		return USBD_INVAL;
		break;
	case UE_BULK:
		pipe->methods = &xhci_device_bulk_methods;
		break;
	case UE_INTERRUPT:
		pipe->methods = &xhci_device_intr_methods;
		break;
	default:
		return USBD_IOERROR;
		break;
	}

	if (ed->bEndpointAddress != USB_CONTROL_ENDPOINT)
		xhci_configure_endpoint(pipe);

	return USBD_NORMAL_COMPLETION;
}

static void
xhci_rhpsc(struct xhci_softc * const sc, u_int port)
{
	usbd_xfer_handle const xfer = sc->sc_intrxfer;
	uint8_t *p;

	XHCIHIST_FUNC(); XHCIHIST_CALLED();
	DPRINTFN(4, "port %u status change", port, 0, 0, 0);

	if (xfer == NULL)
		return;

	if (!(port >= sc->sc_hs_port_start &&
	    port < sc->sc_hs_port_start + sc->sc_hs_port_count))
		return;

	port -= sc->sc_hs_port_start;
	port += 1;
	DPRINTFN(4, "hs port %u status change", port, 0, 0, 0);

	p = KERNADDR(&xfer->dmabuf, 0);
	memset(p, 0, xfer->length);
	p[port/NBBY] |= 1 << (port%NBBY);
	xfer->actlen = xfer->length;
	xfer->status = USBD_NORMAL_COMPLETION;
	usb_transfer_complete(xfer);
}

static void
xhci_handle_event(struct xhci_softc * const sc,
    const struct xhci_trb * const trb)
{
	uint64_t trb_0;
	uint32_t trb_2, trb_3;

	XHCIHIST_FUNC(); XHCIHIST_CALLED();

	trb_0 = le64toh(trb->trb_0);
	trb_2 = le32toh(trb->trb_2);
	trb_3 = le32toh(trb->trb_3);

	DPRINTFN(14, "event: %p 0x%016"PRIx64" 0x%08"PRIx32" 0x%08"PRIx32,
	    trb, trb_0, trb_2, trb_3);

	switch (XHCI_TRB_3_TYPE_GET(trb_3)){
	case XHCI_TRB_EVENT_TRANSFER: {
		u_int slot, dci;
		struct xhci_slot *xs;
		struct xhci_ring *xr;
		struct xhci_xfer *xx;
		usbd_xfer_handle xfer;
		usbd_status err;

		slot = XHCI_TRB_3_SLOT_GET(trb_3);
		dci = XHCI_TRB_3_EP_GET(trb_3);

		xs = &sc->sc_slots[slot];
		xr = &xs->xs_ep[dci].xe_tr;

		if ((trb_3 & XHCI_TRB_3_ED_BIT) == 0) {
			xx = xr->xr_cookies[(trb_0 - xhci_ring_trbp(xr, 0))/
			    sizeof(struct xhci_trb)];
		} else {
			xx = (void *)(uintptr_t)(trb_0 & ~0x3);
		}
		xfer = &xx->xx_xfer;
		DPRINTFN(14, "xfer %p", xfer, 0, 0, 0);

		if ((trb_3 & XHCI_TRB_3_ED_BIT) != 0) {
			DPRINTFN(14, "transfer event data: "
			    "0x%016"PRIx64" 0x%08"PRIx32" %02x",
			    trb_0, XHCI_TRB_2_REM_GET(trb_2),
			    XHCI_TRB_2_ERROR_GET(trb_2), 0);
			if ((trb_0 & 0x3) == 0x3) {
				xfer->actlen = XHCI_TRB_2_REM_GET(trb_2);
			}
		}

		if (XHCI_TRB_2_ERROR_GET(trb_2) ==
		    XHCI_TRB_ERROR_SUCCESS) {
			xfer->actlen = xfer->length - XHCI_TRB_2_REM_GET(trb_2);
			err = USBD_NORMAL_COMPLETION;
		} else if (XHCI_TRB_2_ERROR_GET(trb_2) ==
		    XHCI_TRB_ERROR_SHORT_PKT) {
			xfer->actlen = xfer->length - XHCI_TRB_2_REM_GET(trb_2);
			err = USBD_NORMAL_COMPLETION;
		} else if (XHCI_TRB_2_ERROR_GET(trb_2) ==
		    XHCI_TRB_ERROR_STALL) {
			err = USBD_STALLED;
			xr->is_halted = true;
			DPRINTFN(1, "ev: xfer done: err %u slot %u dci %u",
			    XHCI_TRB_2_ERROR_GET(trb_2), slot, dci, 0);
		} else {
			err = USBD_IOERROR;
		}
		xfer->status = err;

		//mutex_enter(&sc->sc_lock); /* XXX ??? */
		if ((trb_3 & XHCI_TRB_3_ED_BIT) != 0) {
			if ((trb_0 & 0x3) == 0x0) {
				usb_transfer_complete(xfer);
			}
		} else {
			usb_transfer_complete(xfer);
		}
		//mutex_exit(&sc->sc_lock); /* XXX ??? */

		}
		break;
	case XHCI_TRB_EVENT_CMD_COMPLETE:
		if (trb_0 == sc->sc_command_addr) {
			sc->sc_result_trb.trb_0 = trb_0;
			sc->sc_result_trb.trb_2 = trb_2;
			sc->sc_result_trb.trb_3 = trb_3;
			if (XHCI_TRB_2_ERROR_GET(trb_2) !=
			    XHCI_TRB_ERROR_SUCCESS) {
				DPRINTFN(1, "command completion "
				    "failure: 0x%016"PRIx64" 0x%08"PRIx32" "
				    "0x%08"PRIx32, trb_0, trb_2, trb_3, 0);
			}
			cv_signal(&sc->sc_command_cv);
		} else {
			DPRINTFN(1, "event: %p 0x%016"PRIx64" "
			    "0x%08"PRIx32" 0x%08"PRIx32, trb, trb_0,
			    trb_2, trb_3);
		}
		break;
	case XHCI_TRB_EVENT_PORT_STS_CHANGE:
		xhci_rhpsc(sc, (uint32_t)((trb_0 >> 24) & 0xff));
		break;
	default:
		break;
	}
}

static void
xhci_softintr(void *v)
{
	struct usbd_bus * const bus = v;
	struct xhci_softc * const sc = bus->hci_private;
	struct xhci_ring * const er = &sc->sc_er;
	struct xhci_trb *trb;
	int i, j, k;

	XHCIHIST_FUNC(); XHCIHIST_CALLED();

	i = er->xr_ep;
	j = er->xr_cs;

	DPRINTFN(16, "xr_ep %d xr_cs %d", i, j, 0, 0);

	while (1) {
		usb_syncmem(&er->xr_dma, XHCI_TRB_SIZE * i, XHCI_TRB_SIZE,
		    BUS_DMASYNC_POSTREAD);
		trb = &er->xr_trb[i];
		k = (le32toh(trb->trb_3) & XHCI_TRB_3_CYCLE_BIT) ? 1 : 0;

		if (j != k)
			break;

		xhci_handle_event(sc, trb);

		i++;
		if (i == XHCI_EVENT_RING_TRBS) {
			i = 0;
			j ^= 1;
		}
	}

	er->xr_ep = i;
	er->xr_cs = j;

	xhci_rt_write_8(sc, XHCI_ERDP(0), xhci_ring_trbp(er, er->xr_ep) |
	    XHCI_ERDP_LO_BUSY);

	DPRINTFN(16, "ends", 0, 0, 0, 0);

	return;
}

static void
xhci_poll(struct usbd_bus *bus)
{
	struct xhci_softc * const sc = bus->hci_private;

	XHCIHIST_FUNC(); XHCIHIST_CALLED();

	mutex_spin_enter(&sc->sc_intr_lock);
	xhci_intr1(sc);
	mutex_spin_exit(&sc->sc_intr_lock);

	return;
}

static usbd_status
xhci_allocm(struct usbd_bus *bus, usb_dma_t *dma, uint32_t size)
{
	struct xhci_softc * const sc = bus->hci_private;
	usbd_status err;

	XHCIHIST_FUNC(); XHCIHIST_CALLED();

	err = usb_allocmem(&sc->sc_bus, size, 0, dma);
#if 0
	if (err == USBD_NOMEM)
		err = usb_reserve_allocm(&sc->sc_dma_reserve, dma, size);
#endif
#ifdef XHCI_DEBUG
	if (err)
		DPRINTFN(1, "usb_allocmem(%u)=%d", err, size, 0, 0);
#endif

	return err;
}

static void
xhci_freem(struct usbd_bus *bus, usb_dma_t *dma)
{
	struct xhci_softc * const sc = bus->hci_private;

	XHCIHIST_FUNC(); XHCIHIST_CALLED();

#if 0
	if (dma->block->flags & USB_DMA_RESERVE) {
		usb_reserve_freem(&sc->sc_dma_reserve, dma);
		return;
	}
#endif
	usb_freemem(&sc->sc_bus, dma);
}

static usbd_xfer_handle
xhci_allocx(struct usbd_bus *bus)
{
	struct xhci_softc * const sc = bus->hci_private;
	usbd_xfer_handle xfer;

	XHCIHIST_FUNC(); XHCIHIST_CALLED();

	xfer = pool_cache_get(sc->sc_xferpool, PR_NOWAIT);
	if (xfer != NULL) {
		memset(xfer, 0, sizeof(struct xhci_xfer));
#ifdef DIAGNOSTIC
		xfer->busy_free = XFER_BUSY;
#endif
	}

	return xfer;
}

static void
xhci_freex(struct usbd_bus *bus, usbd_xfer_handle xfer)
{
	struct xhci_softc * const sc = bus->hci_private;

	XHCIHIST_FUNC(); XHCIHIST_CALLED();

#ifdef DIAGNOSTIC
	if (xfer->busy_free != XFER_BUSY) {
		DPRINTFN(0, "xfer=%p not busy, 0x%08x",
		    xfer, xfer->busy_free, 0, 0);
	}
	xfer->busy_free = XFER_FREE;
#endif
	pool_cache_put(sc->sc_xferpool, xfer);
}

static void
xhci_get_lock(struct usbd_bus *bus, kmutex_t **lock)
{
	struct xhci_softc * const sc = bus->hci_private;

	*lock = &sc->sc_lock;
}

extern u_int32_t usb_cookie_no;

static usbd_status
xhci_new_device(device_t parent, usbd_bus_handle bus, int depth,
    int speed, int port, struct usbd_port *up)
{
	struct xhci_softc * const sc = bus->hci_private;
	usbd_device_handle dev;
	usbd_status err;
	usb_device_descriptor_t *dd;
	struct usbd_device *hub;
	struct usbd_device *adev;
	int rhport = 0;
	struct xhci_slot *xs;
	uint32_t *cp;
	uint8_t slot;
	uint8_t addr;

	XHCIHIST_FUNC(); XHCIHIST_CALLED();
	DPRINTFN(4, "port=%d depth=%d speed=%d upport %d",
		 port, depth, speed, up->portno);

	dev = malloc(sizeof *dev, M_USB, M_NOWAIT|M_ZERO);
	if (dev == NULL)
		return USBD_NOMEM;

	dev->bus = bus;

	/* Set up default endpoint handle. */
	dev->def_ep.edesc = &dev->def_ep_desc;

	/* Set up default endpoint descriptor. */
	dev->def_ep_desc.bLength = USB_ENDPOINT_DESCRIPTOR_SIZE;
	dev->def_ep_desc.bDescriptorType = UDESC_ENDPOINT;
	dev->def_ep_desc.bEndpointAddress = USB_CONTROL_ENDPOINT;
	dev->def_ep_desc.bmAttributes = UE_CONTROL;
	/* XXX */
	if (speed == USB_SPEED_LOW)
		USETW(dev->def_ep_desc.wMaxPacketSize, USB_MAX_IPACKET);
	else
		USETW(dev->def_ep_desc.wMaxPacketSize, 64);
	dev->def_ep_desc.bInterval = 0;

	/* doesn't matter, just don't let it uninitialized */
	dev->def_ep.datatoggle = 0;

	DPRINTFN(4, "up %p portno %d", up, up->portno, 0, 0);

	dev->quirks = &usbd_no_quirk;
	dev->address = 0;
	dev->ddesc.bMaxPacketSize = 0;
	dev->depth = depth;
	dev->powersrc = up;
	dev->myhub = up->parent;

	up->device = dev;

	/* Locate root hub port */
	for (adev = dev, hub = dev;
	    hub != NULL;
	    adev = hub, hub = hub->myhub) {
		DPRINTFN(4, "hub %p", hub, 0, 0, 0);
	}
	DPRINTFN(4, "hub %p", hub, 0, 0, 0);

	if (hub != NULL) {
		for (int p = 0; p < hub->hub->hubdesc.bNbrPorts; p++) {
			if (hub->hub->ports[p].device == adev) {
				rhport = p;
			}
		}
	} else {
		rhport = port;
	}
	if (speed == USB_SPEED_SUPER) {
		rhport += sc->sc_ss_port_start - 1;
	} else {
		rhport += sc->sc_hs_port_start - 1;
	}
	DPRINTFN(4, "rhport %d", rhport, 0, 0, 0);

	dev->speed = speed;
	dev->langid = USBD_NOLANG;
	dev->cookie.cookie = ++usb_cookie_no;

	/* Establish the default pipe. */
	err = usbd_setup_pipe(dev, 0, &dev->def_ep, USBD_DEFAULT_INTERVAL,
	    &dev->default_pipe);
	if (err) {
		usbd_remove_device(dev, up);
		return (err);
	}

	dd = &dev->ddesc;

	if ((depth == 0) && (port == 0)) {
		KASSERT(bus->devices[dev->address] == NULL);
		bus->devices[dev->address] = dev;
		err = usbd_get_initial_ddesc(dev, dd);
		if (err)
			return err;
		err = usbd_reload_device_desc(dev);
		if (err)
			return err;
	} else {
		err = xhci_enable_slot(sc, &slot);
		if (err)
			return err;
		err = xhci_init_slot(sc, slot, depth, speed, port, rhport);
		if (err)
			return err;
		xs = &sc->sc_slots[slot];
		dev->hci_private = xs;
		cp = xhci_slot_get_dcv(sc, xs, XHCI_DCI_SLOT);
		//hexdump("slot context", cp, sc->sc_ctxsz);
		addr = XHCI_SCTX_3_DEV_ADDR_GET(cp[3]);
		DPRINTFN(4, "device address %u", addr, 0, 0, 0);
		/* XXX ensure we know when the hardware does something
		   we can't yet cope with */
		KASSERT(addr >= 1 && addr <= 127);
		dev->address = addr;
		/* XXX dev->address not necessarily unique on bus */
		KASSERT(bus->devices[dev->address] == NULL);
		bus->devices[dev->address] = dev;

		err = usbd_get_initial_ddesc(dev, dd);
		if (err)
			return err;
		/* 4.8.2.1 */
		if (speed == USB_SPEED_SUPER)
			USETW(dev->def_ep_desc.wMaxPacketSize,
			    (1 << dd->bMaxPacketSize));
		else
	 		USETW(dev->def_ep_desc.wMaxPacketSize,
			    dd->bMaxPacketSize);
		DPRINTFN(4, "bMaxPacketSize %u", dd->bMaxPacketSize, 0, 0, 0);
		xhci_update_ep0_mps(sc, xs,
		    UGETW(dev->def_ep_desc.wMaxPacketSize));
		err = usbd_reload_device_desc(dev);
		if (err)
			return err;

		usbd_kill_pipe(dev->default_pipe);
		err = usbd_setup_pipe(dev, 0, &dev->def_ep,
		    USBD_DEFAULT_INTERVAL, &dev->default_pipe);
	}

	DPRINTFN(1, "adding unit addr=%d, rev=%02x,",
		dev->address, UGETW(dd->bcdUSB), 0, 0);
	DPRINTFN(1, " class=%d, subclass=%d, protocol=%d,",
		dd->bDeviceClass, dd->bDeviceSubClass,
		dd->bDeviceProtocol, 0);
	DPRINTFN(1, " mps=%d, len=%d, noconf=%d, speed=%d",
		dd->bMaxPacketSize, dd->bLength, dd->bNumConfigurations,
		dev->speed);

	usbd_add_dev_event(USB_EVENT_DEVICE_ATTACH, dev);

	if ((depth == 0) && (port == 0)) {
		usbd_attach_roothub(parent, dev);
		DPRINTFN(1, "root_hub %p", bus->root_hub, 0, 0, 0);
		return USBD_NORMAL_COMPLETION;
	}


	err = usbd_probe_and_attach(parent, dev, port, dev->address);
	if (err) {
		usbd_remove_device(dev, up);
		return (err);
	}

	return USBD_NORMAL_COMPLETION;
}

static usbd_status
xhci_ring_init(struct xhci_softc * const sc, struct xhci_ring * const xr,
    size_t ntrb, size_t align)
{
	usbd_status err;
	size_t size = ntrb * XHCI_TRB_SIZE;

	XHCIHIST_FUNC(); XHCIHIST_CALLED();

	err = usb_allocmem(&sc->sc_bus, size, align, &xr->xr_dma);
	if (err)
		return err;
	mutex_init(&xr->xr_lock, MUTEX_DEFAULT, IPL_SOFTUSB);
	xr->xr_cookies = kmem_zalloc(sizeof(*xr->xr_cookies) * ntrb, KM_SLEEP);
	xr->xr_trb = xhci_ring_trbv(xr, 0);
	xr->xr_ntrb = ntrb;
	xr->xr_ep = 0;
	xr->xr_cs = 1;
	memset(xr->xr_trb, 0, size);
	usb_syncmem(&xr->xr_dma, 0, size, BUS_DMASYNC_PREWRITE);
	xr->is_halted = false;

	return USBD_NORMAL_COMPLETION;
}

static void
xhci_ring_free(struct xhci_softc * const sc, struct xhci_ring * const xr)
{
	usb_freemem(&sc->sc_bus, &xr->xr_dma);
	mutex_destroy(&xr->xr_lock);
	kmem_free(xr->xr_cookies, sizeof(*xr->xr_cookies) * xr->xr_ntrb);
}

static void
xhci_ring_put(struct xhci_softc * const sc, struct xhci_ring * const xr,
    void *cookie, struct xhci_trb * const trbs, size_t ntrbs)
{
	size_t i;
	u_int ri;
	u_int cs;
	uint64_t parameter;
	uint32_t status;
	uint32_t control;

	XHCIHIST_FUNC(); XHCIHIST_CALLED();

	for (i = 0; i < ntrbs; i++) {
		DPRINTFN(12, "xr %p trbs %p num %zu", xr, trbs, i, 0);
		DPRINTFN(12, " %016"PRIx64" %08"PRIx32" %08"PRIx32,
		    trbs[i].trb_0, trbs[i].trb_2, trbs[i].trb_3, 0);
		KASSERT(XHCI_TRB_3_TYPE_GET(trbs[i].trb_3) !=
		    XHCI_TRB_TYPE_LINK);
	}

	DPRINTFN(12, "%p xr_ep 0x%x xr_cs %u", xr, xr->xr_ep, xr->xr_cs, 0);

	ri = xr->xr_ep;
	cs = xr->xr_cs;

	/*
	 * Although the xhci hardware can do scatter/gather dma from
	 * arbitrary sized buffers, there is a non-obvious restriction
	 * that a LINK trb is only allowed at the end of a burst of
	 * transfers - which might be 16kB.
	 * Arbitrary aligned LINK trb definitely fail on Ivy bridge.
	 * The simple solution is not to allow a LINK trb in the middle
	 * of anything - as here.
	 * XXX: (dsl) There are xhci controllers out there (eg some made by
	 * ASMedia) that seem to lock up if they process a LINK trb but
	 * cannot process the linked-to trb yet.
	 * The code should write the 'cycle' bit on the link trb AFTER
	 * adding the other trb.
	 */
	if (ri + ntrbs >= (xr->xr_ntrb - 1)) {
		parameter = xhci_ring_trbp(xr, 0);
		status = 0;
		control = XHCI_TRB_3_TYPE_SET(XHCI_TRB_TYPE_LINK) |
		    XHCI_TRB_3_TC_BIT | (cs ? XHCI_TRB_3_CYCLE_BIT : 0);
		xhci_trb_put(&xr->xr_trb[ri], htole64(parameter),
		    htole32(status), htole32(control));
		usb_syncmem(&xr->xr_dma, XHCI_TRB_SIZE * ri, XHCI_TRB_SIZE * 1,
		    BUS_DMASYNC_PREWRITE);
		xr->xr_cookies[ri] = NULL;
		xr->xr_ep = 0;
		xr->xr_cs ^= 1;
		ri = xr->xr_ep;
		cs = xr->xr_cs;
	}

	ri++;

	/* Write any subsequent TRB first */
	for (i = 1; i < ntrbs; i++) {
		parameter = trbs[i].trb_0;
		status = trbs[i].trb_2;
		control = trbs[i].trb_3;

		if (cs) {
			control |= XHCI_TRB_3_CYCLE_BIT;
		} else {
			control &= ~XHCI_TRB_3_CYCLE_BIT;
		}

		xhci_trb_put(&xr->xr_trb[ri], htole64(parameter),
		    htole32(status), htole32(control));
		usb_syncmem(&xr->xr_dma, XHCI_TRB_SIZE * ri, XHCI_TRB_SIZE * 1,
		    BUS_DMASYNC_PREWRITE);
		xr->xr_cookies[ri] = cookie;
		ri++;
	}

	/* Write the first TRB last */
	i = 0;
	{
		parameter = trbs[i].trb_0;
		status = trbs[i].trb_2;
		control = trbs[i].trb_3;

		if (xr->xr_cs) {
			control |= XHCI_TRB_3_CYCLE_BIT;
		} else {
			control &= ~XHCI_TRB_3_CYCLE_BIT;
		}

		xhci_trb_put(&xr->xr_trb[xr->xr_ep], htole64(parameter),
		    htole32(status), htole32(control));
		usb_syncmem(&xr->xr_dma, XHCI_TRB_SIZE * ri, XHCI_TRB_SIZE * 1,
		    BUS_DMASYNC_PREWRITE);
		xr->xr_cookies[xr->xr_ep] = cookie;
	}

	xr->xr_ep = ri;
	xr->xr_cs = cs;

	DPRINTFN(12, "%p xr_ep 0x%x xr_cs %u", xr, xr->xr_ep, xr->xr_cs, 0);
}

static usbd_status
xhci_do_command(struct xhci_softc * const sc, struct xhci_trb * const trb,
    int timeout)
{
	struct xhci_ring * const cr = &sc->sc_cr;
	usbd_status err;

	XHCIHIST_FUNC(); XHCIHIST_CALLED();
	DPRINTFN(12, "input: 0x%016"PRIx64" 0x%08"PRIx32" 0x%08"PRIx32,
	    trb->trb_0, trb->trb_2, trb->trb_3, 0);

	mutex_enter(&sc->sc_lock);

	KASSERT(sc->sc_command_addr == 0);
	sc->sc_command_addr = xhci_ring_trbp(cr, cr->xr_ep);

	mutex_enter(&cr->xr_lock);
	xhci_ring_put(sc, cr, NULL, trb, 1);
	mutex_exit(&cr->xr_lock);

	xhci_db_write_4(sc, XHCI_DOORBELL(0), 0);

	if (cv_timedwait(&sc->sc_command_cv, &sc->sc_lock,
	    MAX(1, mstohz(timeout))) == EWOULDBLOCK) {
		err = USBD_TIMEOUT;
		goto timedout;
	}

	trb->trb_0 = sc->sc_result_trb.trb_0;
	trb->trb_2 = sc->sc_result_trb.trb_2;
	trb->trb_3 = sc->sc_result_trb.trb_3;

	DPRINTFN(12, "output: 0x%016"PRIx64" 0x%08"PRIx32" 0x%08"PRIx32"",
	    trb->trb_0, trb->trb_2, trb->trb_3, 0);

	switch (XHCI_TRB_2_ERROR_GET(trb->trb_2)) {
	case XHCI_TRB_ERROR_SUCCESS:
		err = USBD_NORMAL_COMPLETION;
		break;
	default:
	case 192 ... 223:
		err = USBD_IOERROR;
		break;
	case 224 ... 255:
		err = USBD_NORMAL_COMPLETION;
		break;
	}

timedout:
	sc->sc_command_addr = 0;
	mutex_exit(&sc->sc_lock);
	return err;
}

static usbd_status
xhci_enable_slot(struct xhci_softc * const sc, uint8_t * const slotp)
{
	struct xhci_trb trb;
	usbd_status err;

	XHCIHIST_FUNC(); XHCIHIST_CALLED();

	trb.trb_0 = 0;
	trb.trb_2 = 0;
	trb.trb_3 = XHCI_TRB_3_TYPE_SET(XHCI_TRB_TYPE_ENABLE_SLOT);

	err = xhci_do_command(sc, &trb, USBD_DEFAULT_TIMEOUT);
	if (err != USBD_NORMAL_COMPLETION) {
		return err;
	}

	*slotp = XHCI_TRB_3_SLOT_GET(trb.trb_3);

	return err;
}

static usbd_status
xhci_address_device(struct xhci_softc * const sc,
    uint64_t icp, uint8_t slot_id, bool bsr)
{
	struct xhci_trb trb;
	usbd_status err;

	XHCIHIST_FUNC(); XHCIHIST_CALLED();

	trb.trb_0 = icp;
	trb.trb_2 = 0;
	trb.trb_3 = XHCI_TRB_3_SLOT_SET(slot_id) |
	    XHCI_TRB_3_TYPE_SET(XHCI_TRB_TYPE_ADDRESS_DEVICE) |
	    (bsr ? XHCI_TRB_3_BSR_BIT : 0);

	err = xhci_do_command(sc, &trb, USBD_DEFAULT_TIMEOUT);
	return err;
}

static usbd_status
xhci_update_ep0_mps(struct xhci_softc * const sc,
    struct xhci_slot * const xs, u_int mps)
{
	struct xhci_trb trb;
	usbd_status err;
	uint32_t * cp;

	XHCIHIST_FUNC(); XHCIHIST_CALLED();
	DPRINTFN(4, "slot %u mps %u", xs->xs_idx, mps, 0, 0);

	cp = xhci_slot_get_icv(sc, xs, XHCI_ICI_INPUT_CONTROL);
	cp[0] = htole32(0);
	cp[1] = htole32(XHCI_INCTX_1_ADD_MASK(XHCI_DCI_EP_CONTROL));

	cp = xhci_slot_get_icv(sc, xs, xhci_dci_to_ici(XHCI_DCI_EP_CONTROL));
	cp[1] = htole32(XHCI_EPCTX_1_MAXP_SIZE_SET(mps));

	/* sync input contexts before they are read from memory */
	usb_syncmem(&xs->xs_ic_dma, 0, sc->sc_pgsz, BUS_DMASYNC_PREWRITE);
	hexdump("input context", xhci_slot_get_icv(sc, xs, 0),
	    sc->sc_ctxsz * 4);

	trb.trb_0 = xhci_slot_get_icp(sc, xs, 0);
	trb.trb_2 = 0;
	trb.trb_3 = XHCI_TRB_3_SLOT_SET(xs->xs_idx) |
	    XHCI_TRB_3_TYPE_SET(XHCI_TRB_TYPE_EVALUATE_CTX);

	err = xhci_do_command(sc, &trb, USBD_DEFAULT_TIMEOUT);
	KASSERT(err == USBD_NORMAL_COMPLETION); /* XXX */
	return err;
}

static void
xhci_set_dcba(struct xhci_softc * const sc, uint64_t dcba, int si)
{
	uint64_t * const dcbaa = KERNADDR(&sc->sc_dcbaa_dma, 0);

	XHCIHIST_FUNC(); XHCIHIST_CALLED();
	DPRINTFN(4, "dcbaa %p dc %016"PRIx64" slot %d",
	    &dcbaa[si], dcba, si, 0);

	dcbaa[si] = htole64(dcba);
	usb_syncmem(&sc->sc_dcbaa_dma, si * sizeof(uint64_t), sizeof(uint64_t),
	    BUS_DMASYNC_PREWRITE);
}

static usbd_status
xhci_init_slot(struct xhci_softc * const sc, uint32_t slot, int depth,
    int speed, int port, int rhport)
{
	struct xhci_slot *xs;
	usbd_status err;
	u_int dci;
	uint32_t *cp;
	uint32_t mps;
	uint32_t xspeed;

	XHCIHIST_FUNC(); XHCIHIST_CALLED();
	DPRINTFN(4, "slot %u depth %d speed %d",
	    slot, depth, speed, 0);
	DPRINTFN(4, " port %d rhport %d",
	    port, rhport, 0, 0);

	switch (speed) {
	case USB_SPEED_LOW:
		xspeed = 2;
		mps = USB_MAX_IPACKET;
		break;
	case USB_SPEED_FULL:
		xspeed = 1;
		mps = 64;
		break;
	case USB_SPEED_HIGH:
		xspeed = 3;
		mps = USB_2_MAX_CTRL_PACKET;
		break;
	case USB_SPEED_SUPER:
		xspeed = 4;
		mps = USB_3_MAX_CTRL_PACKET;
		break;
	default:
		DPRINTFN(0, "impossible speed: %x", speed, 0, 0, 0);
		return USBD_INVAL;
	}

	xs = &sc->sc_slots[slot];
	xs->xs_idx = slot;

	/* allocate contexts */
	err = usb_allocmem(&sc->sc_bus, sc->sc_pgsz, sc->sc_pgsz,
	    &xs->xs_dc_dma);
	if (err)
		return err;
	memset(KERNADDR(&xs->xs_dc_dma, 0), 0, sc->sc_pgsz);

	err = usb_allocmem(&sc->sc_bus, sc->sc_pgsz, sc->sc_pgsz,
	    &xs->xs_ic_dma);
	if (err)
		return err;
	memset(KERNADDR(&xs->xs_ic_dma, 0), 0, sc->sc_pgsz);

	for (dci = 0; dci < 32; dci++) {
		//CTASSERT(sizeof(xs->xs_ep[dci]) == sizeof(struct xhci_endpoint));
		memset(&xs->xs_ep[dci], 0, sizeof(xs->xs_ep[dci]));
		if (dci == XHCI_DCI_SLOT)
			continue;
		err = xhci_ring_init(sc, &xs->xs_ep[dci].xe_tr,
		    XHCI_TRANSFER_RING_TRBS, XHCI_TRB_ALIGN);
		if (err) {
			DPRINTFN(0, "ring init failure", 0, 0, 0, 0);
			return err;
		}
	}

	/* set up initial input control context */
	cp = xhci_slot_get_icv(sc, xs, XHCI_ICI_INPUT_CONTROL);
	cp[0] = htole32(0);
	cp[1] = htole32(XHCI_INCTX_1_ADD_MASK(XHCI_DCI_EP_CONTROL)|
	    XHCI_INCTX_1_ADD_MASK(XHCI_DCI_SLOT));

	/* set up input slot context */
	cp = xhci_slot_get_icv(sc, xs, xhci_dci_to_ici(XHCI_DCI_SLOT));
	cp[0] = htole32(
		XHCI_SCTX_0_CTX_NUM_SET(1) |
		XHCI_SCTX_0_SPEED_SET(xspeed)
		);
	cp[1] = htole32(
		XHCI_SCTX_1_RH_PORT_SET(rhport)
		);
	cp[2] = htole32(
		XHCI_SCTX_2_IRQ_TARGET_SET(0)
		);
	cp[3] = htole32(0);

	/* set up input EP0 context */
	cp = xhci_slot_get_icv(sc, xs, xhci_dci_to_ici(XHCI_DCI_EP_CONTROL));
	cp[0] = htole32(0);
	cp[1] = htole32(
		XHCI_EPCTX_1_MAXP_SIZE_SET(mps) |
		XHCI_EPCTX_1_EPTYPE_SET(4) |
		XHCI_EPCTX_1_CERR_SET(3)
		);
	/* can't use xhci_ep_get_dci() yet? */
	*(uint64_t *)(&cp[2]) = htole64(
	    xhci_ring_trbp(&xs->xs_ep[XHCI_DCI_EP_CONTROL].xe_tr, 0) |
	    XHCI_EPCTX_2_DCS_SET(1));
	cp[4] = htole32(
		XHCI_EPCTX_4_AVG_TRB_LEN_SET(8)
		);

	/* sync input contexts before they are read from memory */
	usb_syncmem(&xs->xs_ic_dma, 0, sc->sc_pgsz, BUS_DMASYNC_PREWRITE);
	hexdump("input context", xhci_slot_get_icv(sc, xs, 0),
	    sc->sc_ctxsz * 3);

	xhci_set_dcba(sc, DMAADDR(&xs->xs_dc_dma, 0), slot);

	err = xhci_address_device(sc, xhci_slot_get_icp(sc, xs, 0), slot,
	    false);

	usb_syncmem(&xs->xs_dc_dma, 0, sc->sc_pgsz, BUS_DMASYNC_POSTREAD);
	hexdump("output context", xhci_slot_get_dcv(sc, xs, 0),
	    sc->sc_ctxsz * 2);

	return err;
}

/* ----- */

static void
xhci_noop(usbd_pipe_handle pipe)
{
	XHCIHIST_FUNC(); XHCIHIST_CALLED();
}

/* root hub descriptors */

static const usb_device_descriptor_t xhci_devd = {
	USB_DEVICE_DESCRIPTOR_SIZE,
	UDESC_DEVICE,		/* type */
	{0x00, 0x02},		/* USB version */
	UDCLASS_HUB,		/* class */
	UDSUBCLASS_HUB,		/* subclass */
	UDPROTO_HSHUBSTT,	/* protocol */
	64,			/* max packet */
	{0},{0},{0x00,0x01},	/* device id */
	1,2,0,			/* string indexes */
	1			/* # of configurations */
};

static const usb_device_qualifier_t xhci_odevd = {
	USB_DEVICE_DESCRIPTOR_SIZE,
	UDESC_DEVICE_QUALIFIER,	/* type */
	{0x00, 0x02},		/* USB version */
	UDCLASS_HUB,		/* class */
	UDSUBCLASS_HUB,		/* subclass */
	UDPROTO_FSHUB,		/* protocol */
	64,                     /* max packet */
	1,                      /* # of configurations */
	0
};

static const usb_config_descriptor_t xhci_confd = {
	USB_CONFIG_DESCRIPTOR_SIZE,
	UDESC_CONFIG,
	{USB_CONFIG_DESCRIPTOR_SIZE +
	 USB_INTERFACE_DESCRIPTOR_SIZE +
	 USB_ENDPOINT_DESCRIPTOR_SIZE},
	1,
	1,
	0,
	UC_ATTR_MBO | UC_SELF_POWERED,
	0                      /* max power */
};

static const usb_interface_descriptor_t xhci_ifcd = {
	USB_INTERFACE_DESCRIPTOR_SIZE,
	UDESC_INTERFACE,
	0,
	0,
	1,
	UICLASS_HUB,
	UISUBCLASS_HUB,
	UIPROTO_HSHUBSTT,
	0
};

static const usb_endpoint_descriptor_t xhci_endpd = {
	USB_ENDPOINT_DESCRIPTOR_SIZE,
	UDESC_ENDPOINT,
	UE_DIR_IN | XHCI_INTR_ENDPT,
	UE_INTERRUPT,
	{8, 0},                 /* max packet */
	12
};

static const usb_hub_descriptor_t xhci_hubd = {
	USB_HUB_DESCRIPTOR_SIZE,
	UDESC_HUB,
	0,
	{0,0},
	0,
	0,
	{""},
	{""},
};

/* root hub control */

static usbd_status
xhci_root_ctrl_transfer(usbd_xfer_handle xfer)
{
	struct xhci_softc * const sc = xfer->pipe->device->bus->hci_private;
	usbd_status err;

	XHCIHIST_FUNC(); XHCIHIST_CALLED();

	/* Insert last in queue. */
	mutex_enter(&sc->sc_lock);
	err = usb_insert_transfer(xfer);
	mutex_exit(&sc->sc_lock);
	if (err)
		return err;

	/* Pipe isn't running, start first */
	return (xhci_root_ctrl_start(SIMPLEQ_FIRST(&xfer->pipe->queue)));
}

static usbd_status
xhci_root_ctrl_start(usbd_xfer_handle xfer)
{
	struct xhci_softc * const sc = xfer->pipe->device->bus->hci_private;
	usb_port_status_t ps;
	usb_device_request_t *req;
	void *buf = NULL;
	usb_hub_descriptor_t hubd;
	usbd_status err;
	int len, value, index;
	int l, totlen = 0;
	int port, i;
	uint32_t v;

	XHCIHIST_FUNC(); XHCIHIST_CALLED();

	if (sc->sc_dying)
		return USBD_IOERROR;

	req = &xfer->request;

	value = UGETW(req->wValue);
	index = UGETW(req->wIndex);
	len = UGETW(req->wLength);

	if (len != 0)
		buf = KERNADDR(&xfer->dmabuf, 0);

	DPRINTFN(12, "rhreq: %04x %04x %04x %04x",
	    req->bmRequestType | (req->bRequest << 8), value, index, len);

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
			*(uint8_t *)buf = sc->sc_conf;
			totlen = 1;
		}
		break;
	case C(UR_GET_DESCRIPTOR, UT_READ_DEVICE):
		DPRINTFN(8, "getdesc: wValue=0x%04x", value, 0, 0, 0);
		if (len == 0)
			break;
		switch(value >> 8) {
		case UDESC_DEVICE:
			if ((value & 0xff) != 0) {
				err = USBD_IOERROR;
				goto ret;
			}
			totlen = l = min(len, USB_DEVICE_DESCRIPTOR_SIZE);
			memcpy(buf, &xhci_devd, min(l, sizeof(xhci_devd)));
			break;
		case UDESC_DEVICE_QUALIFIER:
			if ((value & 0xff) != 0) {
			}
			totlen = l = min(len, USB_DEVICE_DESCRIPTOR_SIZE);
			memcpy(buf, &xhci_odevd, min(l, sizeof(xhci_odevd)));
			break;
		case UDESC_OTHER_SPEED_CONFIGURATION:
		case UDESC_CONFIG:
			if ((value & 0xff) != 0) {
				err = USBD_IOERROR;
				goto ret;
			}
			totlen = l = min(len, USB_CONFIG_DESCRIPTOR_SIZE);
			memcpy(buf, &xhci_confd, min(l, sizeof(xhci_confd)));
			((usb_config_descriptor_t *)buf)->bDescriptorType =
			    value >> 8;
			buf = (char *)buf + l;
			len -= l;
			l = min(len, USB_INTERFACE_DESCRIPTOR_SIZE);
			totlen += l;
			memcpy(buf, &xhci_ifcd, min(l, sizeof(xhci_ifcd)));
			buf = (char *)buf + l;
			len -= l;
			l = min(len, USB_ENDPOINT_DESCRIPTOR_SIZE);
			totlen += l;
			memcpy(buf, &xhci_endpd, min(l, sizeof(xhci_endpd)));
			break;
		case UDESC_STRING:
#define sd ((usb_string_descriptor_t *)buf)
			switch (value & 0xff) {
			case 0: /* Language table */
				totlen = usb_makelangtbl(sd, len);
				break;
			case 1: /* Vendor */
				totlen = usb_makestrdesc(sd, len, "NetBSD");
				break;
			case 2: /* Product */
				totlen = usb_makestrdesc(sd, len,
				    "xHCI Root Hub");
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
			*(uint8_t *)buf = 0;
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
		//sc->sc_addr = value;
		break;
	case C(UR_SET_CONFIG, UT_WRITE_DEVICE):
		if (value != 0 && value != 1) {
			err = USBD_IOERROR;
			goto ret;
		}
		sc->sc_conf = value;
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
		DPRINTFN(4, "UR_CLEAR_PORT_FEATURE port=%d feature=%d",
			     index, value, 0, 0);
		if (index < 1 || index > sc->sc_hs_port_count) {
			err = USBD_IOERROR;
			goto ret;
		}
		port = XHCI_PORTSC(sc->sc_hs_port_start - 1 + index);
		v = xhci_op_read_4(sc, port);
		DPRINTFN(4, "portsc=0x%08x", v, 0, 0, 0);
		v &= ~XHCI_PS_CLEAR;
		switch (value) {
		case UHF_PORT_ENABLE:
			xhci_op_write_4(sc, port, v &~ XHCI_PS_PED);
			break;
		case UHF_PORT_SUSPEND:
			err = USBD_IOERROR;
			goto ret;
		case UHF_PORT_POWER:
			break;
		case UHF_PORT_TEST:
		case UHF_PORT_INDICATOR:
			err = USBD_IOERROR;
			goto ret;
		case UHF_C_PORT_CONNECTION:
			xhci_op_write_4(sc, port, v | XHCI_PS_CSC);
			break;
		case UHF_C_PORT_ENABLE:
		case UHF_C_PORT_SUSPEND:
		case UHF_C_PORT_OVER_CURRENT:
			err = USBD_IOERROR;
			goto ret;
		case UHF_C_PORT_RESET:
			xhci_op_write_4(sc, port, v | XHCI_PS_PRC);
			break;
		default:
			err = USBD_IOERROR;
			goto ret;
		}

		break;
	case C(UR_GET_DESCRIPTOR, UT_READ_CLASS_DEVICE):
		if (len == 0)
			break;
		if ((value & 0xff) != 0) {
			err = USBD_IOERROR;
			goto ret;
		}
		hubd = xhci_hubd;
		hubd.bNbrPorts = sc->sc_hs_port_count;
		USETW(hubd.wHubCharacteristics, UHD_PWR_NO_SWITCH);
		hubd.bPwrOn2PwrGood = 200;
		for (i = 0, l = sc->sc_maxports; l > 0; i++, l -= 8)
			hubd.DeviceRemovable[i++] = 0; /* XXX can't find out? */
		hubd.bDescLength = USB_HUB_DESCRIPTOR_SIZE + i;
		l = min(len, hubd.bDescLength);
		totlen = l;
		memcpy(buf, &hubd, l);
		break;
	case C(UR_GET_STATUS, UT_READ_CLASS_DEVICE):
		if (len != 4) {
			err = USBD_IOERROR;
			goto ret;
		}
		memset(buf, 0, len); /* ? XXX */
		totlen = len;
		break;
	case C(UR_GET_STATUS, UT_READ_CLASS_OTHER):
		DPRINTFN(8, "get port status i=%d", index, 0, 0, 0);
		if (index < 1 || index > sc->sc_maxports) {
			err = USBD_IOERROR;
			goto ret;
		}
		if (len != 4) {
			err = USBD_IOERROR;
			goto ret;
		}
		v = xhci_op_read_4(sc, XHCI_PORTSC(sc->sc_hs_port_start - 1 +
		    index));
		DPRINTFN(4, "READ_CLASS_OTHER GET_STATUS PORTSC %d (%d) %08x",
		    index, sc->sc_hs_port_start - 1 + index, v, 0);
		switch (XHCI_PS_SPEED_GET(v)) {
		case 1:
			i = UPS_FULL_SPEED;
			break;
		case 2:
			i = UPS_LOW_SPEED;
			break;
		case 3:
			i = UPS_HIGH_SPEED;
			break;
		default:
			i = 0;
			break;
		}
		if (v & XHCI_PS_CCS)	i |= UPS_CURRENT_CONNECT_STATUS;
		if (v & XHCI_PS_PED)	i |= UPS_PORT_ENABLED;
		if (v & XHCI_PS_OCA)	i |= UPS_OVERCURRENT_INDICATOR;
		//if (v & XHCI_PS_SUSP)	i |= UPS_SUSPEND;
		if (v & XHCI_PS_PR)	i |= UPS_RESET;
		if (v & XHCI_PS_PP)	i |= UPS_PORT_POWER;
		USETW(ps.wPortStatus, i);
		i = 0;
		if (v & XHCI_PS_CSC)    i |= UPS_C_CONNECT_STATUS;
		if (v & XHCI_PS_PEC)    i |= UPS_C_PORT_ENABLED;
		if (v & XHCI_PS_OCC)    i |= UPS_C_OVERCURRENT_INDICATOR;
		if (v & XHCI_PS_PRC)	i |= UPS_C_PORT_RESET;
		USETW(ps.wPortChange, i);
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
		if (index < 1 || index > sc->sc_hs_port_count) {
			err = USBD_IOERROR;
			goto ret;
		}
		port = XHCI_PORTSC(sc->sc_hs_port_start - 1 + index);
		v = xhci_op_read_4(sc, port);
		DPRINTFN(4, "portsc=0x%08x", v, 0, 0, 0);
		v &= ~XHCI_PS_CLEAR;
		switch (value) {
		case UHF_PORT_ENABLE:
			xhci_op_write_4(sc, port, v | XHCI_PS_PED);
			break;
		case UHF_PORT_SUSPEND:
			/* XXX suspend */
			break;
		case UHF_PORT_RESET:
			v &= ~ (XHCI_PS_PED | XHCI_PS_PR);
			xhci_op_write_4(sc, port, v | XHCI_PS_PR);
			/* Wait for reset to complete. */
			usb_delay_ms(&sc->sc_bus, USB_PORT_ROOT_RESET_DELAY);
			if (sc->sc_dying) {
				err = USBD_IOERROR;
				goto ret;
			}
			v = xhci_op_read_4(sc, port);
			if (v & XHCI_PS_PR) {
				xhci_op_write_4(sc, port, v & ~XHCI_PS_PR);
				usb_delay_ms(&sc->sc_bus, 10);
				/* XXX */
			}
			break;
		case UHF_PORT_POWER:
			/* XXX power control */
			break;
		/* XXX more */
		case UHF_C_PORT_RESET:
			xhci_op_write_4(sc, port, v | XHCI_PS_PRC);
			break;
		default:
			err = USBD_IOERROR;
			goto ret;
		}
		break;
	case C(UR_CLEAR_TT_BUFFER, UT_WRITE_CLASS_OTHER):
	case C(UR_RESET_TT, UT_WRITE_CLASS_OTHER):
	case C(UR_GET_TT_STATE, UT_READ_CLASS_OTHER):
	case C(UR_STOP_TT, UT_WRITE_CLASS_OTHER):
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
	return USBD_IN_PROGRESS;
}


static void
xhci_root_ctrl_abort(usbd_xfer_handle xfer)
{
	XHCIHIST_FUNC(); XHCIHIST_CALLED();
	/* Nothing to do, all transfers are synchronous. */
}


static void
xhci_root_ctrl_close(usbd_pipe_handle pipe)
{
	XHCIHIST_FUNC(); XHCIHIST_CALLED();
	/* Nothing to do. */
}

static void
xhci_root_ctrl_done(usbd_xfer_handle xfer)
{
	XHCIHIST_FUNC(); XHCIHIST_CALLED();

	xfer->hcpriv = NULL;
}

/* root hub interrupt */

static usbd_status
xhci_root_intr_transfer(usbd_xfer_handle xfer)
{
	struct xhci_softc * const sc = xfer->pipe->device->bus->hci_private;
	usbd_status err;

	XHCIHIST_FUNC(); XHCIHIST_CALLED();

	/* Insert last in queue. */
	mutex_enter(&sc->sc_lock);
	err = usb_insert_transfer(xfer);
	mutex_exit(&sc->sc_lock);
	if (err)
		return err;

	/* Pipe isn't running, start first */
	return (xhci_root_intr_start(SIMPLEQ_FIRST(&xfer->pipe->queue)));
}

static usbd_status
xhci_root_intr_start(usbd_xfer_handle xfer)
{
	struct xhci_softc * const sc = xfer->pipe->device->bus->hci_private;

	XHCIHIST_FUNC(); XHCIHIST_CALLED();

	if (sc->sc_dying)
		return USBD_IOERROR;

	mutex_enter(&sc->sc_lock);
	sc->sc_intrxfer = xfer;
	mutex_exit(&sc->sc_lock);

	return USBD_IN_PROGRESS;
}

static void
xhci_root_intr_abort(usbd_xfer_handle xfer)
{
	struct xhci_softc * const sc = xfer->pipe->device->bus->hci_private;

	XHCIHIST_FUNC(); XHCIHIST_CALLED();

	KASSERT(mutex_owned(&sc->sc_lock));
	KASSERT(xfer->pipe->intrxfer == xfer);

	DPRINTFN(1, "remove", 0, 0, 0, 0);

	sc->sc_intrxfer = NULL;

	xfer->status = USBD_CANCELLED;
	usb_transfer_complete(xfer);
}

static void
xhci_root_intr_close(usbd_pipe_handle pipe)
{
	struct xhci_softc * const sc = pipe->device->bus->hci_private;

	XHCIHIST_FUNC(); XHCIHIST_CALLED();

	KASSERT(mutex_owned(&sc->sc_lock));

	sc->sc_intrxfer = NULL;
}

static void
xhci_root_intr_done(usbd_xfer_handle xfer)
{
	XHCIHIST_FUNC(); XHCIHIST_CALLED();

	xfer->hcpriv = NULL;
}

/* -------------- */
/* device control */

static usbd_status
xhci_device_ctrl_transfer(usbd_xfer_handle xfer)
{
	struct xhci_softc * const sc = xfer->pipe->device->bus->hci_private;
	usbd_status err;

	XHCIHIST_FUNC(); XHCIHIST_CALLED();

	/* Insert last in queue. */
	mutex_enter(&sc->sc_lock);
	err = usb_insert_transfer(xfer);
	mutex_exit(&sc->sc_lock);
	if (err)
		return (err);

	/* Pipe isn't running, start first */
	return (xhci_device_ctrl_start(SIMPLEQ_FIRST(&xfer->pipe->queue)));
}

static usbd_status
xhci_device_ctrl_start(usbd_xfer_handle xfer)
{
	struct xhci_softc * const sc = xfer->pipe->device->bus->hci_private;
	struct xhci_slot * const xs = xfer->pipe->device->hci_private;
	const u_int dci = xhci_ep_get_dci(xfer->pipe->endpoint->edesc);
	struct xhci_ring * const tr = &xs->xs_ep[dci].xe_tr;
	struct xhci_xfer * const xx = (void *)xfer;
	usb_device_request_t * const req = &xfer->request;
	const bool isread = UT_GET_DIR(req->bmRequestType) == UT_READ;
	const uint32_t len = UGETW(req->wLength);
	usb_dma_t * const dma = &xfer->dmabuf;
	uint64_t parameter;
	uint32_t status;
	uint32_t control;
	u_int i;

	XHCIHIST_FUNC(); XHCIHIST_CALLED();
	DPRINTFN(12, "req: %04x %04x %04x %04x",
	    req->bmRequestType | (req->bRequest << 8), UGETW(req->wValue),
	    UGETW(req->wIndex), UGETW(req->wLength));

	/* XXX */
	if (tr->is_halted) {
		xhci_reset_endpoint(xfer->pipe);
		tr->is_halted = false;
		xhci_set_dequeue(xfer->pipe);
	}

	/* we rely on the bottom bits for extra info */
	KASSERT(((uintptr_t)xfer & 0x3) == 0x0);

	KASSERT((xfer->rqflags & URQ_REQUEST) != 0);

	i = 0;

	/* setup phase */
	memcpy(&parameter, req, sizeof(*req));
	parameter = le64toh(parameter);
	status = XHCI_TRB_2_IRQ_SET(0) | XHCI_TRB_2_BYTES_SET(sizeof(*req));
	control = ((len == 0) ? XHCI_TRB_3_TRT_NONE :
	     (isread ? XHCI_TRB_3_TRT_IN : XHCI_TRB_3_TRT_OUT)) |
	    XHCI_TRB_3_TYPE_SET(XHCI_TRB_TYPE_SETUP_STAGE) |
	    XHCI_TRB_3_IDT_BIT;
	xhci_trb_put(&xx->xx_trb[i++], parameter, status, control);

	if (len == 0)
		goto no_data;

	/* data phase */
	parameter = DMAADDR(dma, 0);
	KASSERT(len <= 0x10000);
	status = XHCI_TRB_2_IRQ_SET(0) |
	    XHCI_TRB_2_TDSZ_SET(1) |
	    XHCI_TRB_2_BYTES_SET(len);
	control = (isread ? XHCI_TRB_3_DIR_IN : 0) |
	    XHCI_TRB_3_TYPE_SET(XHCI_TRB_TYPE_DATA_STAGE) |
	    XHCI_TRB_3_CHAIN_BIT | XHCI_TRB_3_ENT_BIT;
	xhci_trb_put(&xx->xx_trb[i++], parameter, status, control);

	parameter = (uintptr_t)xfer | 0x3;
	status = XHCI_TRB_2_IRQ_SET(0);
	control = XHCI_TRB_3_TYPE_SET(XHCI_TRB_TYPE_EVENT_DATA) |
	    XHCI_TRB_3_IOC_BIT;
	xhci_trb_put(&xx->xx_trb[i++], parameter, status, control);

no_data:
	parameter = 0;
	status = XHCI_TRB_2_IRQ_SET(0);
	/* the status stage has inverted direction */
	control = ((isread && (len > 0)) ? 0 : XHCI_TRB_3_DIR_IN) |
	    XHCI_TRB_3_TYPE_SET(XHCI_TRB_TYPE_STATUS_STAGE) |
	    XHCI_TRB_3_CHAIN_BIT | XHCI_TRB_3_ENT_BIT;
	xhci_trb_put(&xx->xx_trb[i++], parameter, status, control);

	parameter = (uintptr_t)xfer | 0x0;
	status = XHCI_TRB_2_IRQ_SET(0);
	control = XHCI_TRB_3_TYPE_SET(XHCI_TRB_TYPE_EVENT_DATA) |
	    XHCI_TRB_3_IOC_BIT;
	xhci_trb_put(&xx->xx_trb[i++], parameter, status, control);

	mutex_enter(&tr->xr_lock);
	xhci_ring_put(sc, tr, xfer, xx->xx_trb, i);
	mutex_exit(&tr->xr_lock);

	xhci_db_write_4(sc, XHCI_DOORBELL(xs->xs_idx), dci);

	if (xfer->timeout && !sc->sc_bus.use_polling) {
		callout_reset(&xfer->timeout_handle, mstohz(xfer->timeout),
		    xhci_timeout, xfer);
	}

	if (sc->sc_bus.use_polling) {
		DPRINTFN(1, "polling", 0, 0, 0, 0);
		//xhci_waitintr(sc, xfer);
	}

	return USBD_IN_PROGRESS;
}

static void
xhci_device_ctrl_done(usbd_xfer_handle xfer)
{
	XHCIHIST_FUNC(); XHCIHIST_CALLED();

	callout_stop(&xfer->timeout_handle); /* XXX wrong place */

}

static void
xhci_device_ctrl_abort(usbd_xfer_handle xfer)
{
	XHCIHIST_FUNC(); XHCIHIST_CALLED();
}

static void
xhci_device_ctrl_close(usbd_pipe_handle pipe)
{
	XHCIHIST_FUNC(); XHCIHIST_CALLED();
}

/* ----------------- */
/* device isochronus */

/* ----------- */
/* device bulk */

static usbd_status
xhci_device_bulk_transfer(usbd_xfer_handle xfer)
{
	struct xhci_softc * const sc = xfer->pipe->device->bus->hci_private;
	usbd_status err;

	XHCIHIST_FUNC(); XHCIHIST_CALLED();

	/* Insert last in queue. */
	mutex_enter(&sc->sc_lock);
	err = usb_insert_transfer(xfer);
	mutex_exit(&sc->sc_lock);
	if (err)
		return err;

	/*
	 * Pipe isn't running (otherwise err would be USBD_INPROG),
	 * so start it first.
	 */
	return (xhci_device_bulk_start(SIMPLEQ_FIRST(&xfer->pipe->queue)));
}

static usbd_status
xhci_device_bulk_start(usbd_xfer_handle xfer)
{
	struct xhci_softc * const sc = xfer->pipe->device->bus->hci_private;
	struct xhci_slot * const xs = xfer->pipe->device->hci_private;
	const u_int dci = xhci_ep_get_dci(xfer->pipe->endpoint->edesc);
	struct xhci_ring * const tr = &xs->xs_ep[dci].xe_tr;
	struct xhci_xfer * const xx = (void *)xfer;
	const uint32_t len = xfer->length;
	usb_dma_t * const dma = &xfer->dmabuf;
	uint64_t parameter;
	uint32_t status;
	uint32_t control;
	u_int i = 0;

	XHCIHIST_FUNC(); XHCIHIST_CALLED();

	DPRINTFN(15, "%p slot %u dci %u", xfer, xs->xs_idx, dci, 0);

	if (sc->sc_dying)
		return USBD_IOERROR;

	KASSERT((xfer->rqflags & URQ_REQUEST) == 0);

	parameter = DMAADDR(dma, 0);
	/*
	 * XXX: (dsl) The physical buffer must not cross a 64k boundary.
	 * If the user supplied buffer crosses such a boundary then 2
	 * (or more) TRB should be used.
	 * If multiple TRB are used the td_size field must be set correctly.
	 * For v1.0 devices (like ivy bridge) this is the number of usb data
	 * blocks needed to complete the transfer.
	 * Setting it to 1 in the last TRB causes an extra zero-length
	 * data block be sent.
	 * The earlier documentation differs, I don't know how it behaves.
	 */
	KASSERT(len <= 0x10000);
	status = XHCI_TRB_2_IRQ_SET(0) |
	    XHCI_TRB_2_TDSZ_SET(1) |
	    XHCI_TRB_2_BYTES_SET(len);
	control = XHCI_TRB_3_TYPE_SET(XHCI_TRB_TYPE_NORMAL) |
	    XHCI_TRB_3_ISP_BIT | XHCI_TRB_3_IOC_BIT;
	xhci_trb_put(&xx->xx_trb[i++], parameter, status, control);

	mutex_enter(&tr->xr_lock);
	xhci_ring_put(sc, tr, xfer, xx->xx_trb, i);
	mutex_exit(&tr->xr_lock);

	xhci_db_write_4(sc, XHCI_DOORBELL(xs->xs_idx), dci);

	if (sc->sc_bus.use_polling) {
		DPRINTFN(1, "polling", 0, 0, 0, 0);
		//xhci_waitintr(sc, xfer);
	}

	return USBD_IN_PROGRESS;
}

static void
xhci_device_bulk_done(usbd_xfer_handle xfer)
{
#ifdef USB_DEBUG
	struct xhci_slot * const xs = xfer->pipe->device->hci_private;
	const u_int dci = xhci_ep_get_dci(xfer->pipe->endpoint->edesc);
#endif
	const u_int endpt = xfer->pipe->endpoint->edesc->bEndpointAddress;
	const bool isread = UE_GET_DIR(endpt) == UE_DIR_IN;

	XHCIHIST_FUNC(); XHCIHIST_CALLED();

	DPRINTFN(15, "%p slot %u dci %u", xfer, xs->xs_idx, dci, 0);

	callout_stop(&xfer->timeout_handle); /* XXX wrong place */

	usb_syncmem(&xfer->dmabuf, 0, xfer->length,
	    isread ? BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);


}

static void
xhci_device_bulk_abort(usbd_xfer_handle xfer)
{
	XHCIHIST_FUNC(); XHCIHIST_CALLED();
}

static void
xhci_device_bulk_close(usbd_pipe_handle pipe)
{
	XHCIHIST_FUNC(); XHCIHIST_CALLED();
}

/* --------------- */
/* device intrrupt */

static usbd_status
xhci_device_intr_transfer(usbd_xfer_handle xfer)
{
	struct xhci_softc * const sc = xfer->pipe->device->bus->hci_private;
	usbd_status err;

	XHCIHIST_FUNC(); XHCIHIST_CALLED();

	/* Insert last in queue. */
	mutex_enter(&sc->sc_lock);
	err = usb_insert_transfer(xfer);
	mutex_exit(&sc->sc_lock);
	if (err)
		return err;

	/*
	 * Pipe isn't running (otherwise err would be USBD_INPROG),
	 * so start it first.
	 */
	return (xhci_device_intr_start(SIMPLEQ_FIRST(&xfer->pipe->queue)));
}

static usbd_status
xhci_device_intr_start(usbd_xfer_handle xfer)
{
	struct xhci_softc * const sc = xfer->pipe->device->bus->hci_private;
	struct xhci_slot * const xs = xfer->pipe->device->hci_private;
	const u_int dci = xhci_ep_get_dci(xfer->pipe->endpoint->edesc);
	struct xhci_ring * const tr = &xs->xs_ep[dci].xe_tr;
	struct xhci_xfer * const xx = (void *)xfer;
	const uint32_t len = xfer->length;
	usb_dma_t * const dma = &xfer->dmabuf;
	uint64_t parameter;
	uint32_t status;
	uint32_t control;
	u_int i = 0;

	XHCIHIST_FUNC(); XHCIHIST_CALLED();

	DPRINTFN(15, "%p slot %u dci %u", xfer, xs->xs_idx, dci, 0);

	if (sc->sc_dying)
		return USBD_IOERROR;

	KASSERT((xfer->rqflags & URQ_REQUEST) == 0);

	parameter = DMAADDR(dma, 0);
	KASSERT(len <= 0x10000);
	status = XHCI_TRB_2_IRQ_SET(0) |
	    XHCI_TRB_2_TDSZ_SET(1) |
	    XHCI_TRB_2_BYTES_SET(len);
	control = XHCI_TRB_3_TYPE_SET(XHCI_TRB_TYPE_NORMAL) |
	    XHCI_TRB_3_ISP_BIT | XHCI_TRB_3_IOC_BIT;
	xhci_trb_put(&xx->xx_trb[i++], parameter, status, control);

	mutex_enter(&tr->xr_lock);
	xhci_ring_put(sc, tr, xfer, xx->xx_trb, i);
	mutex_exit(&tr->xr_lock);

	xhci_db_write_4(sc, XHCI_DOORBELL(xs->xs_idx), dci);

	if (sc->sc_bus.use_polling) {
		DPRINTFN(1, "polling", 0, 0, 0, 0);
		//xhci_waitintr(sc, xfer);
	}

	return USBD_IN_PROGRESS;
}

static void
xhci_device_intr_done(usbd_xfer_handle xfer)
{
	struct xhci_softc * const sc __diagused =
		xfer->pipe->device->bus->hci_private;
#ifdef USB_DEBUG
	struct xhci_slot * const xs = xfer->pipe->device->hci_private;
	const u_int dci = xhci_ep_get_dci(xfer->pipe->endpoint->edesc);
#endif
	const u_int endpt = xfer->pipe->endpoint->edesc->bEndpointAddress;
	const bool isread = UE_GET_DIR(endpt) == UE_DIR_IN;

	XHCIHIST_FUNC(); XHCIHIST_CALLED();

	DPRINTFN(15, "%p slot %u dci %u", xfer, xs->xs_idx, dci, 0);

	KASSERT(sc->sc_bus.use_polling || mutex_owned(&sc->sc_lock));

	usb_syncmem(&xfer->dmabuf, 0, xfer->length,
	    isread ? BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);

#if 0
	device_printf(sc->sc_dev, "");
	for (size_t i = 0; i < xfer->length; i++) {
		printf(" %02x", ((uint8_t const *)xfer->buffer)[i]);
	}
	printf("\n");
#endif

	if (xfer->pipe->repeat) {
		xfer->status = xhci_device_intr_start(xfer);
	} else {
		callout_stop(&xfer->timeout_handle); /* XXX */
	}

}

static void
xhci_device_intr_abort(usbd_xfer_handle xfer)
{
	struct xhci_softc * const sc __diagused =
				    xfer->pipe->device->bus->hci_private;

	XHCIHIST_FUNC(); XHCIHIST_CALLED();

	KASSERT(mutex_owned(&sc->sc_lock));
	DPRINTFN(15, "%p", xfer, 0, 0, 0);
	KASSERT(xfer->pipe->intrxfer == xfer);
	xfer->status = USBD_CANCELLED;
	usb_transfer_complete(xfer);
}

static void
xhci_device_intr_close(usbd_pipe_handle pipe)
{
	//struct xhci_softc * const sc = pipe->device->bus->hci_private;

	XHCIHIST_FUNC(); XHCIHIST_CALLED();
	DPRINTFN(15, "%p", pipe, 0, 0, 0);

	xhci_unconfigure_endpoint(pipe);
}

/* ------------ */

static void
xhci_timeout(void *addr)
{
	struct xhci_xfer * const xx = addr;
	usbd_xfer_handle const xfer = &xx->xx_xfer;
	struct xhci_softc * const sc = xfer->pipe->device->bus->hci_private;

	XHCIHIST_FUNC(); XHCIHIST_CALLED();

	if (sc->sc_dying) {
		return;
	}

	usb_init_task(&xx->xx_abort_task, xhci_timeout_task, addr,
	    USB_TASKQ_MPSAFE);
	usb_add_task(xx->xx_xfer.pipe->device, &xx->xx_abort_task,
	    USB_TASKQ_HC);
}

static void
xhci_timeout_task(void *addr)
{
	usbd_xfer_handle const xfer = addr;
	struct xhci_softc * const sc = xfer->pipe->device->bus->hci_private;

	XHCIHIST_FUNC(); XHCIHIST_CALLED();

	mutex_enter(&sc->sc_lock);
#if 0
	xhci_abort_xfer(xfer, USBD_TIMEOUT);
#else
	xfer->status = USBD_TIMEOUT;
	usb_transfer_complete(xfer);
#endif
	mutex_exit(&sc->sc_lock);
}
