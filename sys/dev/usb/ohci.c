/*	$NetBSD: ohci.c,v 1.256 2015/08/19 06:23:35 skrll Exp $	*/

/*
 * Copyright (c) 1998, 2004, 2005, 2012 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology, Jared D. McNeill (jmcneill@invisible.ca)
 * and Matthew R. Green (mrg@eterna.com.au).
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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
 * USB Open Host Controller driver.
 *
 * OHCI spec: http://www.compaq.com/productinfo/development/openhci.html
 * USB spec: http://www.usb.org/developers/docs/
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ohci.c,v 1.256 2015/08/19 06:23:35 skrll Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/select.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/cpu.h>

#include <machine/endian.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>
#include <dev/usb/usb_quirks.h>

#include <dev/usb/ohcireg.h>
#include <dev/usb/ohcivar.h>
#include <dev/usb/usbroothub_subr.h>



#ifdef OHCI_DEBUG
#define DPRINTF(x)	if (ohcidebug) printf x
#define DPRINTFN(n,x)	if (ohcidebug>(n)) printf x
int ohcidebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#if BYTE_ORDER == BIG_ENDIAN
#define	SWAP_ENDIAN	OHCI_LITTLE_ENDIAN
#else
#define	SWAP_ENDIAN	OHCI_BIG_ENDIAN
#endif

#define	O16TOH(val)	(sc->sc_endian == SWAP_ENDIAN ? bswap16(val) : val)
#define	O32TOH(val)	(sc->sc_endian == SWAP_ENDIAN ? bswap32(val) : val)
#define	HTOO16(val)	O16TOH(val)
#define	HTOO32(val)	O32TOH(val)

struct ohci_pipe;

Static ohci_soft_ed_t  *ohci_alloc_sed(ohci_softc_t *);
Static void		ohci_free_sed(ohci_softc_t *, ohci_soft_ed_t *);

Static ohci_soft_td_t  *ohci_alloc_std(ohci_softc_t *);
Static void		ohci_free_std(ohci_softc_t *, ohci_soft_td_t *);

Static ohci_soft_itd_t *ohci_alloc_sitd(ohci_softc_t *);
Static void		ohci_free_sitd(ohci_softc_t *,ohci_soft_itd_t *);

Static void		ohci_free_std_chain(ohci_softc_t *, ohci_soft_td_t *,
					    ohci_soft_td_t *);
Static usbd_status	ohci_alloc_std_chain(struct ohci_pipe *,
			    ohci_softc_t *, int, int, usbd_xfer_handle,
			    ohci_soft_td_t *, ohci_soft_td_t **);

Static usbd_status	ohci_open(usbd_pipe_handle);
Static void		ohci_poll(struct usbd_bus *);
Static void		ohci_softintr(void *);
Static void		ohci_waitintr(ohci_softc_t *, usbd_xfer_handle);
Static void		ohci_rhsc(ohci_softc_t *, usbd_xfer_handle);
Static void		ohci_rhsc_softint(void *arg);

Static usbd_status	ohci_device_request(usbd_xfer_handle xfer);
Static void		ohci_add_ed(ohci_softc_t *, ohci_soft_ed_t *,
			    ohci_soft_ed_t *);

Static void		ohci_rem_ed(ohci_softc_t *, ohci_soft_ed_t *,
				    ohci_soft_ed_t *);
Static void		ohci_hash_add_td(ohci_softc_t *, ohci_soft_td_t *);
Static void		ohci_hash_rem_td(ohci_softc_t *, ohci_soft_td_t *);
Static ohci_soft_td_t  *ohci_hash_find_td(ohci_softc_t *, ohci_physaddr_t);
Static void		ohci_hash_add_itd(ohci_softc_t *, ohci_soft_itd_t *);
Static void		ohci_hash_rem_itd(ohci_softc_t *, ohci_soft_itd_t *);
Static ohci_soft_itd_t  *ohci_hash_find_itd(ohci_softc_t *, ohci_physaddr_t);

Static usbd_status	ohci_setup_isoc(usbd_pipe_handle pipe);
Static void		ohci_device_isoc_enter(usbd_xfer_handle);

Static usbd_status	ohci_allocm(struct usbd_bus *, usb_dma_t *, u_int32_t);
Static void		ohci_freem(struct usbd_bus *, usb_dma_t *);

Static usbd_xfer_handle	ohci_allocx(struct usbd_bus *);
Static void		ohci_freex(struct usbd_bus *, usbd_xfer_handle);
Static void		ohci_get_lock(struct usbd_bus *, kmutex_t **);

Static usbd_status	ohci_root_ctrl_transfer(usbd_xfer_handle);
Static usbd_status	ohci_root_ctrl_start(usbd_xfer_handle);
Static void		ohci_root_ctrl_abort(usbd_xfer_handle);
Static void		ohci_root_ctrl_close(usbd_pipe_handle);
Static void		ohci_root_ctrl_done(usbd_xfer_handle);

Static usbd_status	ohci_root_intr_transfer(usbd_xfer_handle);
Static usbd_status	ohci_root_intr_start(usbd_xfer_handle);
Static void		ohci_root_intr_abort(usbd_xfer_handle);
Static void		ohci_root_intr_close(usbd_pipe_handle);
Static void		ohci_root_intr_done(usbd_xfer_handle);

Static usbd_status	ohci_device_ctrl_transfer(usbd_xfer_handle);
Static usbd_status	ohci_device_ctrl_start(usbd_xfer_handle);
Static void		ohci_device_ctrl_abort(usbd_xfer_handle);
Static void		ohci_device_ctrl_close(usbd_pipe_handle);
Static void		ohci_device_ctrl_done(usbd_xfer_handle);

Static usbd_status	ohci_device_bulk_transfer(usbd_xfer_handle);
Static usbd_status	ohci_device_bulk_start(usbd_xfer_handle);
Static void		ohci_device_bulk_abort(usbd_xfer_handle);
Static void		ohci_device_bulk_close(usbd_pipe_handle);
Static void		ohci_device_bulk_done(usbd_xfer_handle);

Static usbd_status	ohci_device_intr_transfer(usbd_xfer_handle);
Static usbd_status	ohci_device_intr_start(usbd_xfer_handle);
Static void		ohci_device_intr_abort(usbd_xfer_handle);
Static void		ohci_device_intr_close(usbd_pipe_handle);
Static void		ohci_device_intr_done(usbd_xfer_handle);

Static usbd_status	ohci_device_isoc_transfer(usbd_xfer_handle);
Static usbd_status	ohci_device_isoc_start(usbd_xfer_handle);
Static void		ohci_device_isoc_abort(usbd_xfer_handle);
Static void		ohci_device_isoc_close(usbd_pipe_handle);
Static void		ohci_device_isoc_done(usbd_xfer_handle);

Static usbd_status	ohci_device_setintr(ohci_softc_t *sc,
			    struct ohci_pipe *pipe, int ival);

Static void		ohci_timeout(void *);
Static void		ohci_timeout_task(void *);
Static void		ohci_rhsc_enable(void *);

Static void		ohci_close_pipe(usbd_pipe_handle, ohci_soft_ed_t *);
Static void		ohci_abort_xfer(usbd_xfer_handle, usbd_status);

Static void		ohci_device_clear_toggle(usbd_pipe_handle pipe);
Static void		ohci_noop(usbd_pipe_handle pipe);

#ifdef OHCI_DEBUG
Static void		ohci_dumpregs(ohci_softc_t *);
Static void		ohci_dump_tds(ohci_softc_t *, ohci_soft_td_t *);
Static void		ohci_dump_td(ohci_softc_t *, ohci_soft_td_t *);
Static void		ohci_dump_ed(ohci_softc_t *, ohci_soft_ed_t *);
Static void		ohci_dump_itd(ohci_softc_t *, ohci_soft_itd_t *);
Static void		ohci_dump_itds(ohci_softc_t *, ohci_soft_itd_t *);
#endif

#define OBARR(sc) bus_space_barrier((sc)->iot, (sc)->ioh, 0, (sc)->sc_size, \
			BUS_SPACE_BARRIER_READ|BUS_SPACE_BARRIER_WRITE)
#define OWRITE1(sc, r, x) \
 do { OBARR(sc); bus_space_write_1((sc)->iot, (sc)->ioh, (r), (x)); } while (0)
#define OWRITE2(sc, r, x) \
 do { OBARR(sc); bus_space_write_2((sc)->iot, (sc)->ioh, (r), (x)); } while (0)
#define OWRITE4(sc, r, x) \
 do { OBARR(sc); bus_space_write_4((sc)->iot, (sc)->ioh, (r), (x)); } while (0)

static __inline uint32_t
OREAD4(ohci_softc_t *sc, bus_size_t r)
{

	OBARR(sc);
	return bus_space_read_4(sc->iot, sc->ioh, r);
}

/* Reverse the bits in a value 0 .. 31 */
Static u_int8_t revbits[OHCI_NO_INTRS] =
  { 0x00, 0x10, 0x08, 0x18, 0x04, 0x14, 0x0c, 0x1c,
    0x02, 0x12, 0x0a, 0x1a, 0x06, 0x16, 0x0e, 0x1e,
    0x01, 0x11, 0x09, 0x19, 0x05, 0x15, 0x0d, 0x1d,
    0x03, 0x13, 0x0b, 0x1b, 0x07, 0x17, 0x0f, 0x1f };

struct ohci_pipe {
	struct usbd_pipe pipe;
	ohci_soft_ed_t *sed;
	union {
		ohci_soft_td_t *td;
		ohci_soft_itd_t *itd;
	} tail;
	/* Info needed for different pipe kinds. */
	union {
		/* Control pipe */
		struct {
			usb_dma_t reqdma;
			u_int length;
			ohci_soft_td_t *setup, *data, *stat;
		} ctl;
		/* Interrupt pipe */
		struct {
			int nslots;
			int pos;
		} intr;
		/* Bulk pipe */
		struct {
			u_int length;
			int isread;
		} bulk;
		/* Iso pipe */
		struct iso {
			int next, inuse;
		} iso;
	} u;
};

#define OHCI_INTR_ENDPT 1

Static const struct usbd_bus_methods ohci_bus_methods = {
	.open_pipe =	ohci_open,
	.soft_intr =	ohci_softintr,
	.do_poll =	ohci_poll,
	.allocm =	ohci_allocm,
	.freem =	ohci_freem,
	.allocx =	ohci_allocx,
	.freex =	ohci_freex,
	.get_lock =	ohci_get_lock,
	.new_device =	NULL,
};

Static const struct usbd_pipe_methods ohci_root_ctrl_methods = {
	.transfer =	ohci_root_ctrl_transfer,
	.start =	ohci_root_ctrl_start,
	.abort =	ohci_root_ctrl_abort,
	.close =	ohci_root_ctrl_close,
	.cleartoggle =	ohci_noop,
	.done =		ohci_root_ctrl_done,
};

Static const struct usbd_pipe_methods ohci_root_intr_methods = {
	.transfer =	ohci_root_intr_transfer,
	.start =	ohci_root_intr_start,
	.abort =	ohci_root_intr_abort,
	.close =	ohci_root_intr_close,
	.cleartoggle =	ohci_noop,
	.done =		ohci_root_intr_done,
};

Static const struct usbd_pipe_methods ohci_device_ctrl_methods = {
	.transfer =	ohci_device_ctrl_transfer,
	.start =	ohci_device_ctrl_start,
	.abort =	ohci_device_ctrl_abort,
	.close =	ohci_device_ctrl_close,
	.cleartoggle =	ohci_noop,
	.done =		ohci_device_ctrl_done,
};

Static const struct usbd_pipe_methods ohci_device_intr_methods = {
	.transfer =	ohci_device_intr_transfer,
	.start =	ohci_device_intr_start,
	.abort =	ohci_device_intr_abort,
	.close =	ohci_device_intr_close,
	.cleartoggle =	ohci_device_clear_toggle,
	.done =		ohci_device_intr_done,
};

Static const struct usbd_pipe_methods ohci_device_bulk_methods = {
	.transfer =	ohci_device_bulk_transfer,
	.start =	ohci_device_bulk_start,
	.abort =	ohci_device_bulk_abort,
	.close =	ohci_device_bulk_close,
	.cleartoggle =	ohci_device_clear_toggle,
	.done =		ohci_device_bulk_done,
};

Static const struct usbd_pipe_methods ohci_device_isoc_methods = {
	.transfer =	ohci_device_isoc_transfer,
	.start =	ohci_device_isoc_start,
	.abort =	ohci_device_isoc_abort,
	.close =	ohci_device_isoc_close,
	.cleartoggle =	ohci_noop,
	.done =		ohci_device_isoc_done,
};

int
ohci_activate(device_t self, enum devact act)
{
	struct ohci_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

void
ohci_childdet(device_t self, device_t child)
{
	struct ohci_softc *sc = device_private(self);

	KASSERT(sc->sc_child == child);
	sc->sc_child = NULL;
}

int
ohci_detach(struct ohci_softc *sc, int flags)
{
	int rv = 0;

	if (sc->sc_child != NULL)
		rv = config_detach(sc->sc_child, flags);

	if (rv != 0)
		return (rv);

	callout_halt(&sc->sc_tmo_rhsc, &sc->sc_lock);

	usb_delay_ms(&sc->sc_bus, 300); /* XXX let stray task complete */
	callout_destroy(&sc->sc_tmo_rhsc);

	softint_disestablish(sc->sc_rhsc_si);

	cv_destroy(&sc->sc_softwake_cv);

	mutex_destroy(&sc->sc_lock);
	mutex_destroy(&sc->sc_intr_lock);

	if (sc->sc_hcca != NULL)
		usb_freemem(&sc->sc_bus, &sc->sc_hccadma);
	pool_cache_destroy(sc->sc_xferpool);

	return (rv);
}

ohci_soft_ed_t *
ohci_alloc_sed(ohci_softc_t *sc)
{
	ohci_soft_ed_t *sed;
	usbd_status err;
	int i, offs;
	usb_dma_t dma;

	if (sc->sc_freeeds == NULL) {
		DPRINTFN(2, ("ohci_alloc_sed: allocating chunk\n"));
		err = usb_allocmem(&sc->sc_bus, OHCI_SED_SIZE * OHCI_SED_CHUNK,
			  OHCI_ED_ALIGN, &dma);
		if (err)
			return (0);
		for (i = 0; i < OHCI_SED_CHUNK; i++) {
			offs = i * OHCI_SED_SIZE;
			sed = KERNADDR(&dma, offs);
			sed->physaddr = DMAADDR(&dma, offs);
			sed->dma = dma;
			sed->offs = offs;
			sed->next = sc->sc_freeeds;
			sc->sc_freeeds = sed;
		}
	}
	sed = sc->sc_freeeds;
	sc->sc_freeeds = sed->next;
	memset(&sed->ed, 0, sizeof(ohci_ed_t));
	sed->next = 0;
	return (sed);
}

void
ohci_free_sed(ohci_softc_t *sc, ohci_soft_ed_t *sed)
{
	sed->next = sc->sc_freeeds;
	sc->sc_freeeds = sed;
}

ohci_soft_td_t *
ohci_alloc_std(ohci_softc_t *sc)
{
	ohci_soft_td_t *std;
	usbd_status err;
	int i, offs;
	usb_dma_t dma;

	KASSERT(sc->sc_bus.use_polling || mutex_owned(&sc->sc_lock));

	if (sc->sc_freetds == NULL) {
		DPRINTFN(2, ("ohci_alloc_std: allocating chunk\n"));
		err = usb_allocmem(&sc->sc_bus, OHCI_STD_SIZE * OHCI_STD_CHUNK,
			  OHCI_TD_ALIGN, &dma);
		if (err)
			return (NULL);
		for(i = 0; i < OHCI_STD_CHUNK; i++) {
			offs = i * OHCI_STD_SIZE;
			std = KERNADDR(&dma, offs);
			std->physaddr = DMAADDR(&dma, offs);
			std->dma = dma;
			std->offs = offs;
			std->nexttd = sc->sc_freetds;
			sc->sc_freetds = std;
		}
	}

	std = sc->sc_freetds;
	sc->sc_freetds = std->nexttd;
	memset(&std->td, 0, sizeof(ohci_td_t));
	std->nexttd = NULL;
	std->xfer = NULL;
	ohci_hash_add_td(sc, std);

	return (std);
}

void
ohci_free_std(ohci_softc_t *sc, ohci_soft_td_t *std)
{
	KASSERT(sc->sc_bus.use_polling || mutex_owned(&sc->sc_lock));
	
	ohci_hash_rem_td(sc, std);
	std->nexttd = sc->sc_freetds;
	sc->sc_freetds = std;
}

usbd_status
ohci_alloc_std_chain(struct ohci_pipe *opipe, ohci_softc_t *sc,
		     int alen, int rd, usbd_xfer_handle xfer,
		     ohci_soft_td_t *sp, ohci_soft_td_t **ep)
{
	ohci_soft_td_t *next, *cur;
	ohci_physaddr_t dataphys, dataphysend;
	u_int32_t tdflags;
	int len, curlen;
	usb_dma_t *dma = &xfer->dmabuf;
	u_int16_t flags = xfer->flags;

	DPRINTFN(alen < 4096,("ohci_alloc_std_chain: start len=%d\n", alen));

	KASSERT(mutex_owned(&sc->sc_lock));

	len = alen;
	cur = sp;
	dataphys = DMAADDR(dma, 0);
	dataphysend = OHCI_PAGE(dataphys + len - 1);
	usb_syncmem(dma, 0, len,
	    rd ? BUS_DMASYNC_PREREAD : BUS_DMASYNC_PREWRITE);
	tdflags = HTOO32(
	    (rd ? OHCI_TD_IN : OHCI_TD_OUT) |
	    (flags & USBD_SHORT_XFER_OK ? OHCI_TD_R : 0) |
	    OHCI_TD_NOCC | OHCI_TD_TOGGLE_CARRY | OHCI_TD_NOINTR);

	for (;;) {
		next = ohci_alloc_std(sc);
		if (next == NULL)
			goto nomem;

		/* The OHCI hardware can handle at most one page crossing. */
		if (OHCI_PAGE(dataphys) == dataphysend ||
		    OHCI_PAGE(dataphys) + OHCI_PAGE_SIZE == dataphysend) {
			/* we can handle it in this TD */
			curlen = len;
		} else {
			/* must use multiple TDs, fill as much as possible. */
			curlen = 2 * OHCI_PAGE_SIZE -
				 (dataphys & (OHCI_PAGE_SIZE-1));
			/* the length must be a multiple of the max size */
			curlen -= curlen % UGETW(opipe->pipe.endpoint->edesc->wMaxPacketSize);
#ifdef DIAGNOSTIC
			if (curlen == 0)
				panic("ohci_alloc_std: curlen == 0");
#endif
		}
		DPRINTFN(4,("ohci_alloc_std_chain: dataphys=0x%08x "
			    "dataphysend=0x%08x len=%d curlen=%d\n",
			    dataphys, dataphysend,
			    len, curlen));
		len -= curlen;

		cur->td.td_flags = tdflags;
		cur->td.td_cbp = HTOO32(dataphys);
		cur->nexttd = next;
		cur->td.td_nexttd = HTOO32(next->physaddr);
		cur->td.td_be = HTOO32(dataphys + curlen - 1);
		cur->len = curlen;
		cur->flags = OHCI_ADD_LEN;
		cur->xfer = xfer;
		usb_syncmem(&cur->dma, cur->offs, sizeof(cur->td),
		    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
		DPRINTFN(10,("ohci_alloc_std_chain: cbp=0x%08x be=0x%08x\n",
			    dataphys, dataphys + curlen - 1));
		if (len == 0)
			break;
		DPRINTFN(10,("ohci_alloc_std_chain: extend chain\n"));
		dataphys += curlen;
		cur = next;
	}
	if (!rd && (flags & USBD_FORCE_SHORT_XFER) &&
	    alen % UGETW(opipe->pipe.endpoint->edesc->wMaxPacketSize) == 0) {
		/* Force a 0 length transfer at the end. */

		cur = next;
		next = ohci_alloc_std(sc);
		if (next == NULL)
			goto nomem;

		cur->td.td_flags = tdflags;
		cur->td.td_cbp = 0; /* indicate 0 length packet */
		cur->nexttd = next;
		cur->td.td_nexttd = HTOO32(next->physaddr);
		cur->td.td_be = ~0;
		cur->len = 0;
		cur->flags = 0;
		cur->xfer = xfer;
		usb_syncmem(&cur->dma, cur->offs, sizeof(cur->td),
		    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
		DPRINTFN(2,("ohci_alloc_std_chain: add 0 xfer\n"));
	}
	*ep = cur;

	return (USBD_NORMAL_COMPLETION);

 nomem:

	/* Don't free sp - let the caller do that */
	ohci_free_std_chain(sc, sp->nexttd, NULL);

	return (USBD_NOMEM);
}

Static void
ohci_free_std_chain(ohci_softc_t *sc, ohci_soft_td_t *std,
		    ohci_soft_td_t *stdend)
{
	ohci_soft_td_t *p;

	for (; std != stdend; std = p) {
		p = std->nexttd;
		ohci_free_std(sc, std);
	}
}

ohci_soft_itd_t *
ohci_alloc_sitd(ohci_softc_t *sc)
{
	ohci_soft_itd_t *sitd;
	usbd_status err;
	int i, offs;
	usb_dma_t dma;

	if (sc->sc_freeitds == NULL) {
		DPRINTFN(2, ("ohci_alloc_sitd: allocating chunk\n"));
		err = usb_allocmem(&sc->sc_bus, OHCI_SITD_SIZE * OHCI_SITD_CHUNK,
			  OHCI_ITD_ALIGN, &dma);
		if (err)
			return (NULL);
		for(i = 0; i < OHCI_SITD_CHUNK; i++) {
			offs = i * OHCI_SITD_SIZE;
			sitd = KERNADDR(&dma, offs);
			sitd->physaddr = DMAADDR(&dma, offs);
			sitd->dma = dma;
			sitd->offs = offs;
			sitd->nextitd = sc->sc_freeitds;
			sc->sc_freeitds = sitd;
		}
	}

	sitd = sc->sc_freeitds;
	sc->sc_freeitds = sitd->nextitd;
	memset(&sitd->itd, 0, sizeof(ohci_itd_t));
	sitd->nextitd = NULL;
	sitd->xfer = NULL;
	ohci_hash_add_itd(sc, sitd);

#ifdef DIAGNOSTIC
	sitd->isdone = 0;
#endif

	return (sitd);
}

void
ohci_free_sitd(ohci_softc_t *sc, ohci_soft_itd_t *sitd)
{

	DPRINTFN(10,("ohci_free_sitd: sitd=%p\n", sitd));

#ifdef DIAGNOSTIC
	if (!sitd->isdone) {
		panic("ohci_free_sitd: sitd=%p not done", sitd);
		return;
	}
	/* Warn double free */
	sitd->isdone = 0;
#endif

	ohci_hash_rem_itd(sc, sitd);
	sitd->nextitd = sc->sc_freeitds;
	sc->sc_freeitds = sitd;
}

usbd_status
ohci_init(ohci_softc_t *sc)
{
	ohci_soft_ed_t *sed, *psed;
	usbd_status err;
	int i;
	u_int32_t s, ctl, rwc, ival, hcr, fm, per, rev, desca /*, descb */;

	DPRINTF(("ohci_init: start\n"));
	aprint_normal_dev(sc->sc_dev, "");

	sc->sc_hcca = NULL;
	callout_init(&sc->sc_tmo_rhsc, CALLOUT_MPSAFE);

	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_SOFTUSB);
	mutex_init(&sc->sc_intr_lock, MUTEX_DEFAULT, IPL_USB);
	cv_init(&sc->sc_softwake_cv, "ohciab");

	sc->sc_rhsc_si = softint_establish(SOFTINT_NET | SOFTINT_MPSAFE,
	    ohci_rhsc_softint, sc);

	for (i = 0; i < OHCI_HASH_SIZE; i++)
		LIST_INIT(&sc->sc_hash_tds[i]);
	for (i = 0; i < OHCI_HASH_SIZE; i++)
		LIST_INIT(&sc->sc_hash_itds[i]);

	sc->sc_xferpool = pool_cache_init(sizeof(struct ohci_xfer), 0, 0, 0,
	    "ohcixfer", NULL, IPL_USB, NULL, NULL, NULL);

	rev = OREAD4(sc, OHCI_REVISION);
	aprint_normal("OHCI version %d.%d%s\n",
	    OHCI_REV_HI(rev), OHCI_REV_LO(rev),
	    OHCI_REV_LEGACY(rev) ? ", legacy support" : "");

	if (OHCI_REV_HI(rev) != 1 || OHCI_REV_LO(rev) != 0) {
		aprint_error_dev(sc->sc_dev, "unsupported OHCI revision\n");
		sc->sc_bus.usbrev = USBREV_UNKNOWN;
		return (USBD_INVAL);
	}
	sc->sc_bus.usbrev = USBREV_1_0;

	usb_setup_reserve(sc->sc_dev, &sc->sc_dma_reserve, sc->sc_bus.dmatag,
	    USB_MEM_RESERVE);

	/* XXX determine alignment by R/W */
	/* Allocate the HCCA area. */
	err = usb_allocmem(&sc->sc_bus, OHCI_HCCA_SIZE,
			 OHCI_HCCA_ALIGN, &sc->sc_hccadma);
	if (err) {
		sc->sc_hcca = NULL;
		return err;
	}
	sc->sc_hcca = KERNADDR(&sc->sc_hccadma, 0);
	memset(sc->sc_hcca, 0, OHCI_HCCA_SIZE);

	sc->sc_eintrs = OHCI_NORMAL_INTRS;

	/* Allocate dummy ED that starts the control list. */
	sc->sc_ctrl_head = ohci_alloc_sed(sc);
	if (sc->sc_ctrl_head == NULL) {
		err = USBD_NOMEM;
		goto bad1;
	}
	sc->sc_ctrl_head->ed.ed_flags |= HTOO32(OHCI_ED_SKIP);

	/* Allocate dummy ED that starts the bulk list. */
	sc->sc_bulk_head = ohci_alloc_sed(sc);
	if (sc->sc_bulk_head == NULL) {
		err = USBD_NOMEM;
		goto bad2;
	}
	sc->sc_bulk_head->ed.ed_flags |= HTOO32(OHCI_ED_SKIP);
	usb_syncmem(&sc->sc_bulk_head->dma, sc->sc_bulk_head->offs,
	    sizeof(sc->sc_bulk_head->ed),
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);

	/* Allocate dummy ED that starts the isochronous list. */
	sc->sc_isoc_head = ohci_alloc_sed(sc);
	if (sc->sc_isoc_head == NULL) {
		err = USBD_NOMEM;
		goto bad3;
	}
	sc->sc_isoc_head->ed.ed_flags |= HTOO32(OHCI_ED_SKIP);
	usb_syncmem(&sc->sc_isoc_head->dma, sc->sc_isoc_head->offs,
	    sizeof(sc->sc_isoc_head->ed),
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);

	/* Allocate all the dummy EDs that make up the interrupt tree. */
	for (i = 0; i < OHCI_NO_EDS; i++) {
		sed = ohci_alloc_sed(sc);
		if (sed == NULL) {
			while (--i >= 0)
				ohci_free_sed(sc, sc->sc_eds[i]);
			err = USBD_NOMEM;
			goto bad4;
		}
		/* All ED fields are set to 0. */
		sc->sc_eds[i] = sed;
		sed->ed.ed_flags |= HTOO32(OHCI_ED_SKIP);
		if (i != 0)
			psed = sc->sc_eds[(i-1) / 2];
		else
			psed= sc->sc_isoc_head;
		sed->next = psed;
		sed->ed.ed_nexted = HTOO32(psed->physaddr);
		usb_syncmem(&sed->dma, sed->offs, sizeof(sed->ed),
		    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	}
	/*
	 * Fill HCCA interrupt table.  The bit reversal is to get
	 * the tree set up properly to spread the interrupts.
	 */
	for (i = 0; i < OHCI_NO_INTRS; i++)
		sc->sc_hcca->hcca_interrupt_table[revbits[i]] =
		    HTOO32(sc->sc_eds[OHCI_NO_EDS-OHCI_NO_INTRS+i]->physaddr);
	usb_syncmem(&sc->sc_hccadma, 0, OHCI_HCCA_SIZE,
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);

#ifdef OHCI_DEBUG
	if (ohcidebug > 15) {
		for (i = 0; i < OHCI_NO_EDS; i++) {
			printf("ed#%d ", i);
			ohci_dump_ed(sc, sc->sc_eds[i]);
		}
		printf("iso ");
		ohci_dump_ed(sc, sc->sc_isoc_head);
	}
#endif

	/* Preserve values programmed by SMM/BIOS but lost over reset. */
	ctl = OREAD4(sc, OHCI_CONTROL);
	rwc = ctl & OHCI_RWC;
	fm = OREAD4(sc, OHCI_FM_INTERVAL);
	desca = OREAD4(sc, OHCI_RH_DESCRIPTOR_A);
	/* descb = OREAD4(sc, OHCI_RH_DESCRIPTOR_B); */

	/* Determine in what context we are running. */
	if (ctl & OHCI_IR) {
		/* SMM active, request change */
		DPRINTF(("ohci_init: SMM active, request owner change\n"));
		if ((sc->sc_intre & (OHCI_OC | OHCI_MIE)) ==
		    (OHCI_OC | OHCI_MIE))
			OWRITE4(sc, OHCI_INTERRUPT_ENABLE, OHCI_MIE);
		s = OREAD4(sc, OHCI_COMMAND_STATUS);
		OWRITE4(sc, OHCI_COMMAND_STATUS, s | OHCI_OCR);
		for (i = 0; i < 100 && (ctl & OHCI_IR); i++) {
			usb_delay_ms(&sc->sc_bus, 1);
			ctl = OREAD4(sc, OHCI_CONTROL);
		}
		OWRITE4(sc, OHCI_INTERRUPT_DISABLE, OHCI_MIE);
		if ((ctl & OHCI_IR) == 0) {
			aprint_error_dev(sc->sc_dev,
			    "SMM does not respond, resetting\n");
			OWRITE4(sc, OHCI_CONTROL, OHCI_HCFS_RESET | rwc);
			goto reset;
		}
#if 0
/* Don't bother trying to reuse the BIOS init, we'll reset it anyway. */
	} else if ((ctl & OHCI_HCFS_MASK) != OHCI_HCFS_RESET) {
		/* BIOS started controller. */
		DPRINTF(("ohci_init: BIOS active\n"));
		if ((ctl & OHCI_HCFS_MASK) != OHCI_HCFS_OPERATIONAL) {
			OWRITE4(sc, OHCI_CONTROL, OHCI_HCFS_OPERATIONAL | rwc);
			usb_delay_ms(&sc->sc_bus, USB_RESUME_DELAY);
		}
#endif
	} else {
		DPRINTF(("ohci_init: cold started\n"));
	reset:
		/* Controller was cold started. */
		usb_delay_ms(&sc->sc_bus, USB_BUS_RESET_DELAY);
	}

	/*
	 * This reset should not be necessary according to the OHCI spec, but
	 * without it some controllers do not start.
	 */
	DPRINTF(("%s: resetting\n", device_xname(sc->sc_dev)));
	OWRITE4(sc, OHCI_CONTROL, OHCI_HCFS_RESET | rwc);
	usb_delay_ms(&sc->sc_bus, USB_BUS_RESET_DELAY);

	/* We now own the host controller and the bus has been reset. */

	OWRITE4(sc, OHCI_COMMAND_STATUS, OHCI_HCR); /* Reset HC */
	/* Nominal time for a reset is 10 us. */
	for (i = 0; i < 10; i++) {
		delay(10);
		hcr = OREAD4(sc, OHCI_COMMAND_STATUS) & OHCI_HCR;
		if (!hcr)
			break;
	}
	if (hcr) {
		aprint_error_dev(sc->sc_dev, "reset timeout\n");
		err = USBD_IOERROR;
		goto bad5;
	}
#ifdef OHCI_DEBUG
	if (ohcidebug > 15)
		ohci_dumpregs(sc);
#endif

	/* The controller is now in SUSPEND state, we have 2ms to finish. */

	/* Set up HC registers. */
	OWRITE4(sc, OHCI_HCCA, DMAADDR(&sc->sc_hccadma, 0));
	OWRITE4(sc, OHCI_CONTROL_HEAD_ED, sc->sc_ctrl_head->physaddr);
	OWRITE4(sc, OHCI_BULK_HEAD_ED, sc->sc_bulk_head->physaddr);
	/* disable all interrupts and then switch on all desired interrupts */
	OWRITE4(sc, OHCI_INTERRUPT_DISABLE, OHCI_ALL_INTRS);
	/* switch on desired functional features */
	ctl = OREAD4(sc, OHCI_CONTROL);
	ctl &= ~(OHCI_CBSR_MASK | OHCI_LES | OHCI_HCFS_MASK | OHCI_IR);
	ctl |= OHCI_PLE | OHCI_IE | OHCI_CLE | OHCI_BLE |
		OHCI_RATIO_1_4 | OHCI_HCFS_OPERATIONAL | rwc;
	/* And finally start it! */
	OWRITE4(sc, OHCI_CONTROL, ctl);

	/*
	 * The controller is now OPERATIONAL.  Set a some final
	 * registers that should be set earlier, but that the
	 * controller ignores when in the SUSPEND state.
	 */
	ival = OHCI_GET_IVAL(fm);
	fm = (OREAD4(sc, OHCI_FM_INTERVAL) & OHCI_FIT) ^ OHCI_FIT;
	fm |= OHCI_FSMPS(ival) | ival;
	OWRITE4(sc, OHCI_FM_INTERVAL, fm);
	per = OHCI_PERIODIC(ival); /* 90% periodic */
	OWRITE4(sc, OHCI_PERIODIC_START, per);

	if (sc->sc_flags & OHCIF_SUPERIO) {
		/* no overcurrent protection */
		desca |= OHCI_NOCP;
		/*
		 * Clear NoPowerSwitching and PowerOnToPowerGoodTime meaning
		 * that
		 *  - ports are always power switched
		 *  - don't wait for powered root hub port
		 */
		desca &= ~(__SHIFTIN(0xff, OHCI_POTPGT_MASK) | OHCI_NPS);
	}

	/* Fiddle the No OverCurrent Protection bit to avoid chip bug. */
	OWRITE4(sc, OHCI_RH_DESCRIPTOR_A, desca | OHCI_NOCP);
	OWRITE4(sc, OHCI_RH_STATUS, OHCI_LPSC); /* Enable port power */
	usb_delay_ms(&sc->sc_bus, OHCI_ENABLE_POWER_DELAY);
	OWRITE4(sc, OHCI_RH_DESCRIPTOR_A, desca);

	/*
	 * The AMD756 requires a delay before re-reading the register,
	 * otherwise it will occasionally report 0 ports.
	 */
	sc->sc_noport = 0;
	for (i = 0; i < 10 && sc->sc_noport == 0; i++) {
		usb_delay_ms(&sc->sc_bus, OHCI_READ_DESC_DELAY);
		sc->sc_noport = OHCI_GET_NDP(OREAD4(sc, OHCI_RH_DESCRIPTOR_A));
	}

#ifdef OHCI_DEBUG
	if (ohcidebug > 5)
		ohci_dumpregs(sc);
#endif

	/* Set up the bus struct. */
	sc->sc_bus.methods = &ohci_bus_methods;
	sc->sc_bus.pipe_size = sizeof(struct ohci_pipe);

	sc->sc_control = sc->sc_intre = 0;

	/* Finally, turn on interrupts. */
	DPRINTFN(1,("ohci_init: enabling %#x\n", sc->sc_eintrs | OHCI_MIE));
	OWRITE4(sc, OHCI_INTERRUPT_ENABLE, sc->sc_eintrs | OHCI_MIE);

	return (USBD_NORMAL_COMPLETION);

 bad5:
	for (i = 0; i < OHCI_NO_EDS; i++)
		ohci_free_sed(sc, sc->sc_eds[i]);
 bad4:
	ohci_free_sed(sc, sc->sc_isoc_head);
 bad3:
	ohci_free_sed(sc, sc->sc_bulk_head);
 bad2:
	ohci_free_sed(sc, sc->sc_ctrl_head);
 bad1:
	usb_freemem(&sc->sc_bus, &sc->sc_hccadma);
	sc->sc_hcca = NULL;
	return (err);
}

usbd_status
ohci_allocm(struct usbd_bus *bus, usb_dma_t *dma, u_int32_t size)
{
	struct ohci_softc *sc = bus->hci_private;
	usbd_status status;

	status = usb_allocmem(&sc->sc_bus, size, 0, dma);
	if (status == USBD_NOMEM)
		status = usb_reserve_allocm(&sc->sc_dma_reserve, dma, size);
	return status;
}

void
ohci_freem(struct usbd_bus *bus, usb_dma_t *dma)
{
	struct ohci_softc *sc = bus->hci_private;
	if (dma->block->flags & USB_DMA_RESERVE) {
		usb_reserve_freem(&sc->sc_dma_reserve, dma);
		return;
	}
	usb_freemem(&sc->sc_bus, dma);
}

usbd_xfer_handle
ohci_allocx(struct usbd_bus *bus)
{
	struct ohci_softc *sc = bus->hci_private;
	usbd_xfer_handle xfer;

	xfer = pool_cache_get(sc->sc_xferpool, PR_NOWAIT);
	if (xfer != NULL) {
		memset(xfer, 0, sizeof(struct ohci_xfer));
#ifdef DIAGNOSTIC
		xfer->busy_free = XFER_BUSY;
#endif
	}
	return (xfer);
}

void
ohci_freex(struct usbd_bus *bus, usbd_xfer_handle xfer)
{
	struct ohci_softc *sc = bus->hci_private;

#ifdef DIAGNOSTIC
	if (xfer->busy_free != XFER_BUSY) {
		printf("ohci_freex: xfer=%p not busy, 0x%08x\n", xfer,
		       xfer->busy_free);
	}
	xfer->busy_free = XFER_FREE;
#endif
	pool_cache_put(sc->sc_xferpool, xfer);
}

Static void
ohci_get_lock(struct usbd_bus *bus, kmutex_t **lock)
{
	struct ohci_softc *sc = bus->hci_private;

	*lock = &sc->sc_lock;
}

/*
 * Shut down the controller when the system is going down.
 */
bool
ohci_shutdown(device_t self, int flags)
{
	ohci_softc_t *sc = device_private(self);

	DPRINTF(("ohci_shutdown: stopping the HC\n"));
	OWRITE4(sc, OHCI_CONTROL, OHCI_HCFS_RESET);
	return true;
}

bool
ohci_resume(device_t dv, const pmf_qual_t *qual)
{
	ohci_softc_t *sc = device_private(dv);
	uint32_t ctl;

	mutex_spin_enter(&sc->sc_intr_lock);
	sc->sc_bus.use_polling++;
	mutex_spin_exit(&sc->sc_intr_lock);

	/* Some broken BIOSes do not recover these values */
	OWRITE4(sc, OHCI_HCCA, DMAADDR(&sc->sc_hccadma, 0));
	OWRITE4(sc, OHCI_CONTROL_HEAD_ED,
	    sc->sc_ctrl_head->physaddr);
	OWRITE4(sc, OHCI_BULK_HEAD_ED,
	    sc->sc_bulk_head->physaddr);
	if (sc->sc_intre)
		OWRITE4(sc, OHCI_INTERRUPT_ENABLE, sc->sc_intre &
		    (OHCI_ALL_INTRS | OHCI_MIE));
	if (sc->sc_control)
		ctl = sc->sc_control;
	else
		ctl = OREAD4(sc, OHCI_CONTROL);
	ctl |= OHCI_HCFS_RESUME;
	OWRITE4(sc, OHCI_CONTROL, ctl);
	usb_delay_ms(&sc->sc_bus, USB_RESUME_DELAY);
	ctl = (ctl & ~OHCI_HCFS_MASK) | OHCI_HCFS_OPERATIONAL;
	OWRITE4(sc, OHCI_CONTROL, ctl);
	usb_delay_ms(&sc->sc_bus, USB_RESUME_RECOVERY);
	sc->sc_control = sc->sc_intre = 0;

	mutex_spin_enter(&sc->sc_intr_lock);
	sc->sc_bus.use_polling--;
	mutex_spin_exit(&sc->sc_intr_lock);

	return true;
}

bool
ohci_suspend(device_t dv, const pmf_qual_t *qual)
{
	ohci_softc_t *sc = device_private(dv);
	uint32_t ctl;

	mutex_spin_enter(&sc->sc_intr_lock);
	sc->sc_bus.use_polling++;
	mutex_spin_exit(&sc->sc_intr_lock);

	ctl = OREAD4(sc, OHCI_CONTROL) & ~OHCI_HCFS_MASK;
	if (sc->sc_control == 0) {
		/*
		 * Preserve register values, in case that BIOS
		 * does not recover them.
		 */
		sc->sc_control = ctl;
		sc->sc_intre = OREAD4(sc,
		    OHCI_INTERRUPT_ENABLE);
	}
	ctl |= OHCI_HCFS_SUSPEND;
	OWRITE4(sc, OHCI_CONTROL, ctl);
	usb_delay_ms(&sc->sc_bus, USB_RESUME_WAIT);

	mutex_spin_enter(&sc->sc_intr_lock);
	sc->sc_bus.use_polling--;
	mutex_spin_exit(&sc->sc_intr_lock);

	return true;
}

#ifdef OHCI_DEBUG
void
ohci_dumpregs(ohci_softc_t *sc)
{
	DPRINTF(("ohci_dumpregs: rev=0x%08x control=0x%08x command=0x%08x\n",
		 OREAD4(sc, OHCI_REVISION),
		 OREAD4(sc, OHCI_CONTROL),
		 OREAD4(sc, OHCI_COMMAND_STATUS)));
	DPRINTF(("               intrstat=0x%08x intre=0x%08x intrd=0x%08x\n",
		 OREAD4(sc, OHCI_INTERRUPT_STATUS),
		 OREAD4(sc, OHCI_INTERRUPT_ENABLE),
		 OREAD4(sc, OHCI_INTERRUPT_DISABLE)));
	DPRINTF(("               hcca=0x%08x percur=0x%08x ctrlhd=0x%08x\n",
		 OREAD4(sc, OHCI_HCCA),
		 OREAD4(sc, OHCI_PERIOD_CURRENT_ED),
		 OREAD4(sc, OHCI_CONTROL_HEAD_ED)));
	DPRINTF(("               ctrlcur=0x%08x bulkhd=0x%08x bulkcur=0x%08x\n",
		 OREAD4(sc, OHCI_CONTROL_CURRENT_ED),
		 OREAD4(sc, OHCI_BULK_HEAD_ED),
		 OREAD4(sc, OHCI_BULK_CURRENT_ED)));
	DPRINTF(("               done=0x%08x fmival=0x%08x fmrem=0x%08x\n",
		 OREAD4(sc, OHCI_DONE_HEAD),
		 OREAD4(sc, OHCI_FM_INTERVAL),
		 OREAD4(sc, OHCI_FM_REMAINING)));
	DPRINTF(("               fmnum=0x%08x perst=0x%08x lsthrs=0x%08x\n",
		 OREAD4(sc, OHCI_FM_NUMBER),
		 OREAD4(sc, OHCI_PERIODIC_START),
		 OREAD4(sc, OHCI_LS_THRESHOLD)));
	DPRINTF(("               desca=0x%08x descb=0x%08x stat=0x%08x\n",
		 OREAD4(sc, OHCI_RH_DESCRIPTOR_A),
		 OREAD4(sc, OHCI_RH_DESCRIPTOR_B),
		 OREAD4(sc, OHCI_RH_STATUS)));
	DPRINTF(("               port1=0x%08x port2=0x%08x\n",
		 OREAD4(sc, OHCI_RH_PORT_STATUS(1)),
		 OREAD4(sc, OHCI_RH_PORT_STATUS(2))));
	DPRINTF(("         HCCA: frame_number=0x%04x done_head=0x%08x\n",
		 O32TOH(sc->sc_hcca->hcca_frame_number),
		 O32TOH(sc->sc_hcca->hcca_done_head)));
}
#endif

Static int ohci_intr1(ohci_softc_t *);

int
ohci_intr(void *p)
{
	ohci_softc_t *sc = p;
	int ret = 0;

	if (sc == NULL)
		return (0);

	mutex_spin_enter(&sc->sc_intr_lock);

	if (sc->sc_dying || !device_has_power(sc->sc_dev))
		goto done;

	/* If we get an interrupt while polling, then just ignore it. */
	if (sc->sc_bus.use_polling) {
#ifdef DIAGNOSTIC
		DPRINTFN(16, ("ohci_intr: ignored interrupt while polling\n"));
#endif
		/* for level triggered intrs, should do something to ack */
		OWRITE4(sc, OHCI_INTERRUPT_STATUS,
			OREAD4(sc, OHCI_INTERRUPT_STATUS));

		goto done;
	}

	ret = ohci_intr1(sc);

done:
	mutex_spin_exit(&sc->sc_intr_lock);
	return ret;
}

Static int
ohci_intr1(ohci_softc_t *sc)
{
	u_int32_t intrs, eintrs;

	DPRINTFN(14,("ohci_intr1: enter\n"));

	/* In case the interrupt occurs before initialization has completed. */
	if (sc == NULL || sc->sc_hcca == NULL) {
#ifdef DIAGNOSTIC
		printf("ohci_intr: sc->sc_hcca == NULL\n");
#endif
		return (0);
	}

	KASSERT(mutex_owned(&sc->sc_intr_lock));

	intrs = OREAD4(sc, OHCI_INTERRUPT_STATUS);
	if (!intrs)
		return (0);

	OWRITE4(sc, OHCI_INTERRUPT_STATUS, intrs & ~(OHCI_MIE|OHCI_WDH)); /* Acknowledge */
	eintrs = intrs & sc->sc_eintrs;
	DPRINTFN(7, ("ohci_intr: sc=%p intrs=%#x(%#x) eintrs=%#x(%#x)\n",
		     sc, (u_int)intrs, OREAD4(sc, OHCI_INTERRUPT_STATUS),
		     (u_int)eintrs, sc->sc_eintrs));

	if (!eintrs) {
		return (0);
	}

	sc->sc_bus.no_intrs++;
	if (eintrs & OHCI_SO) {
		sc->sc_overrun_cnt++;
		if (usbd_ratecheck(&sc->sc_overrun_ntc)) {
			printf("%s: %u scheduling overruns\n",
			    device_xname(sc->sc_dev), sc->sc_overrun_cnt);
			sc->sc_overrun_cnt = 0;
		}
		/* XXX do what */
		eintrs &= ~OHCI_SO;
	}
	if (eintrs & OHCI_WDH) {
		/*
		 * We block the interrupt below, and reenable it later from
		 * ohci_softintr().
		 */
		usb_schedsoftintr(&sc->sc_bus);
	}
	if (eintrs & OHCI_RD) {
		printf("%s: resume detect\n", device_xname(sc->sc_dev));
		/* XXX process resume detect */
	}
	if (eintrs & OHCI_UE) {
		printf("%s: unrecoverable error, controller halted\n",
		       device_xname(sc->sc_dev));
		OWRITE4(sc, OHCI_CONTROL, OHCI_HCFS_RESET);
		/* XXX what else */
	}
	if (eintrs & OHCI_RHSC) {
		/*
		 * We block the interrupt below, and reenable it later from
		 * a timeout.
		 */
		softint_schedule(sc->sc_rhsc_si);
	}

	if (eintrs != 0) {
		/* Block unprocessed interrupts. */
		OWRITE4(sc, OHCI_INTERRUPT_DISABLE, eintrs);
		sc->sc_eintrs &= ~eintrs;
		DPRINTFN(1, ("%s: blocking intrs 0x%x\n",
		    device_xname(sc->sc_dev), eintrs));
	}

	return (1);
}

void
ohci_rhsc_enable(void *v_sc)
{
	ohci_softc_t *sc = v_sc;

	DPRINTFN(1, ("%s: %s\n", __func__, device_xname(sc->sc_dev)));
	mutex_spin_enter(&sc->sc_intr_lock);
	sc->sc_eintrs |= OHCI_RHSC;
	OWRITE4(sc, OHCI_INTERRUPT_ENABLE, OHCI_RHSC);
	mutex_spin_exit(&sc->sc_intr_lock);
}

#ifdef OHCI_DEBUG
const char *ohci_cc_strs[] = {
	"NO_ERROR",
	"CRC",
	"BIT_STUFFING",
	"DATA_TOGGLE_MISMATCH",
	"STALL",
	"DEVICE_NOT_RESPONDING",
	"PID_CHECK_FAILURE",
	"UNEXPECTED_PID",
	"DATA_OVERRUN",
	"DATA_UNDERRUN",
	"BUFFER_OVERRUN",
	"BUFFER_UNDERRUN",
	"reserved",
	"reserved",
	"NOT_ACCESSED",
	"NOT_ACCESSED",
};
#endif

void
ohci_softintr(void *v)
{
	struct usbd_bus *bus = v;
	ohci_softc_t *sc = bus->hci_private;
	ohci_soft_itd_t *sitd, *sidone, *sitdnext;
	ohci_soft_td_t  *std,  *sdone,  *stdnext;
	usbd_xfer_handle xfer;
	struct ohci_pipe *opipe;
	int len, cc;
	int i, j, actlen, iframes, uedir;
	ohci_physaddr_t done;

	KASSERT(sc->sc_bus.use_polling || mutex_owned(&sc->sc_lock));

	DPRINTFN(10,("ohci_softintr: enter\n"));

	usb_syncmem(&sc->sc_hccadma, offsetof(struct ohci_hcca, hcca_done_head),
	    sizeof(sc->sc_hcca->hcca_done_head),
	    BUS_DMASYNC_POSTWRITE | BUS_DMASYNC_POSTREAD);
	done = O32TOH(sc->sc_hcca->hcca_done_head) & ~OHCI_DONE_INTRS;
	sc->sc_hcca->hcca_done_head = 0;
	usb_syncmem(&sc->sc_hccadma, offsetof(struct ohci_hcca, hcca_done_head),
	    sizeof(sc->sc_hcca->hcca_done_head),
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	OWRITE4(sc, OHCI_INTERRUPT_STATUS, OHCI_WDH);
	sc->sc_eintrs |= OHCI_WDH;
	OWRITE4(sc, OHCI_INTERRUPT_ENABLE, OHCI_WDH);

	/* Reverse the done list. */
	for (sdone = NULL, sidone = NULL; done != 0; ) {
		std = ohci_hash_find_td(sc, done);
		if (std != NULL) {
			usb_syncmem(&std->dma, std->offs, sizeof(std->td),
			    BUS_DMASYNC_POSTWRITE | BUS_DMASYNC_POSTREAD);
			std->dnext = sdone;
			done = O32TOH(std->td.td_nexttd);
			sdone = std;
			DPRINTFN(10,("add TD %p\n", std));
			continue;
		}
		sitd = ohci_hash_find_itd(sc, done);
		if (sitd != NULL) {
			usb_syncmem(&sitd->dma, sitd->offs, sizeof(sitd->itd),
			    BUS_DMASYNC_POSTWRITE | BUS_DMASYNC_POSTREAD);
			sitd->dnext = sidone;
			done = O32TOH(sitd->itd.itd_nextitd);
			sidone = sitd;
			DPRINTFN(5,("add ITD %p\n", sitd));
			continue;
		}
		device_printf(sc->sc_dev, "WARNING: addr 0x%08lx not found\n",
		    (u_long)done);
		break;
	}

	DPRINTFN(10,("ohci_softintr: sdone=%p sidone=%p\n", sdone, sidone));

#ifdef OHCI_DEBUG
	if (ohcidebug > 10) {
		DPRINTF(("ohci_process_done: TD done:\n"));
		for (std = sdone; std; std = std->dnext)
			ohci_dump_td(sc, std);	
	}
#endif

	for (std = sdone; std; std = stdnext) {
		xfer = std->xfer;
		stdnext = std->dnext;
		DPRINTFN(10, ("ohci_process_done: std=%p xfer=%p hcpriv=%p\n",
				std, xfer, xfer ? xfer->hcpriv : 0));
		if (xfer == NULL) {
			/*
			 * xfer == NULL: There seems to be no xfer associated
			 * with this TD. It is tailp that happened to end up on
			 * the done queue.
			 * Shouldn't happen, but some chips are broken(?).
			 */
			continue;
		}
		if (xfer->status == USBD_CANCELLED ||
		    xfer->status == USBD_TIMEOUT) {
			DPRINTF(("ohci_process_done: cancel/timeout %p\n",
				 xfer));
			/* Handled by abort routine. */
			continue;
		}
		callout_stop(&xfer->timeout_handle);

		len = std->len;
		if (std->td.td_cbp != 0)
			len -= O32TOH(std->td.td_be) -
			       O32TOH(std->td.td_cbp) + 1;
		DPRINTFN(10, ("ohci_process_done: len=%d, flags=0x%x\n", len,
		    std->flags));
		if (std->flags & OHCI_ADD_LEN)
			xfer->actlen += len;

		cc = OHCI_TD_GET_CC(O32TOH(std->td.td_flags));
		if (cc == OHCI_CC_NO_ERROR) {
			if (std->flags & OHCI_CALL_DONE) {
				xfer->status = USBD_NORMAL_COMPLETION;
				usb_transfer_complete(xfer);
			}
			ohci_free_std(sc, std);
		} else {
			/*
			 * Endpoint is halted.  First unlink all the TDs
			 * belonging to the failed transfer, and then restart
			 * the endpoint.
			 */
			ohci_soft_td_t *p, *n;
			opipe = (struct ohci_pipe *)xfer->pipe;

			DPRINTFN(15,("ohci_process_done: error cc=%d (%s)\n",
			  OHCI_TD_GET_CC(O32TOH(std->td.td_flags)),
			  ohci_cc_strs[OHCI_TD_GET_CC(O32TOH(std->td.td_flags))]));

			/* remove TDs */
			for (p = std; p->xfer == xfer; p = n) {
				n = p->nexttd;
				ohci_free_std(sc, p);
			}

			/* clear halt */
			opipe->sed->ed.ed_headp = HTOO32(p->physaddr);
			OWRITE4(sc, OHCI_COMMAND_STATUS, OHCI_CLF);

			if (cc == OHCI_CC_STALL)
				xfer->status = USBD_STALLED;
			else
				xfer->status = USBD_IOERROR;
			usb_transfer_complete(xfer);
		}
	}

#ifdef OHCI_DEBUG
	if (ohcidebug > 10) {
		DPRINTF(("ohci_softintr: ITD done:\n"));
		for (sitd = sidone; sitd; sitd = sitd->dnext)
			ohci_dump_itd(sc, sitd);	
	}
#endif

	for (sitd = sidone; sitd != NULL; sitd = sitdnext) {
		xfer = sitd->xfer;
		sitdnext = sitd->dnext;
		DPRINTFN(1, ("ohci_process_done: sitd=%p xfer=%p hcpriv=%p\n",
			     sitd, xfer, xfer ? xfer->hcpriv : 0));
		if (xfer == NULL)
			continue;
		if (xfer->status == USBD_CANCELLED ||
		    xfer->status == USBD_TIMEOUT) {
			DPRINTF(("ohci_process_done: cancel/timeout %p\n",
				 xfer));
			/* Handled by abort routine. */
			continue;
		}
#ifdef DIAGNOSTIC
		if (sitd->isdone)
			printf("ohci_softintr: sitd=%p is done\n", sitd);
		sitd->isdone = 1;
#endif
		if (sitd->flags & OHCI_CALL_DONE) {
			ohci_soft_itd_t *next;

			opipe = (struct ohci_pipe *)xfer->pipe;
			opipe->u.iso.inuse -= xfer->nframes;
			uedir = UE_GET_DIR(xfer->pipe->endpoint->edesc->
			    bEndpointAddress);
			xfer->status = USBD_NORMAL_COMPLETION;
			actlen = 0;
			for (i = 0, sitd = xfer->hcpriv;;
			    sitd = next) {
				next = sitd->nextitd;
				if (OHCI_ITD_GET_CC(O32TOH(sitd->
				    itd.itd_flags)) != OHCI_CC_NO_ERROR)
					xfer->status = USBD_IOERROR;
				/* For input, update frlengths with actual */
				/* XXX anything necessary for output? */
				if (uedir == UE_DIR_IN &&
				    xfer->status == USBD_NORMAL_COMPLETION) {
					iframes = OHCI_ITD_GET_FC(O32TOH(
					    sitd->itd.itd_flags));
					for (j = 0; j < iframes; i++, j++) {
						len = O16TOH(sitd->
						    itd.itd_offset[j]);
						if ((OHCI_ITD_PSW_GET_CC(len) &
						    OHCI_CC_NOT_ACCESSED_MASK)
						    == OHCI_CC_NOT_ACCESSED)
							len = 0;
						else
							len = OHCI_ITD_PSW_LENGTH(len);
						xfer->frlengths[i] = len;
						actlen += len;
					}
				}
				if (sitd->flags & OHCI_CALL_DONE)
					break;
				ohci_free_sitd(sc, sitd);
			}
			ohci_free_sitd(sc, sitd);
			if (uedir == UE_DIR_IN &&
			    xfer->status == USBD_NORMAL_COMPLETION)
				xfer->actlen = actlen;
			xfer->hcpriv = NULL;

			usb_transfer_complete(xfer);
		}
	}

	if (sc->sc_softwake) {
		sc->sc_softwake = 0;
		cv_broadcast(&sc->sc_softwake_cv);
	}

	DPRINTFN(10,("ohci_softintr: done:\n"));
}

void
ohci_device_ctrl_done(usbd_xfer_handle xfer)
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)xfer->pipe;
#ifdef DIAGNOSTIC
	ohci_softc_t *sc = xfer->pipe->device->bus->hci_private;
#endif
	int len = UGETW(xfer->request.wLength);
	int isread = (xfer->request.bmRequestType & UT_READ);

	DPRINTFN(10,("ohci_device_ctrl_done: xfer=%p\n", xfer));

	KASSERT(sc->sc_bus.use_polling || mutex_owned(&sc->sc_lock));

#ifdef DIAGNOSTIC
	if (!(xfer->rqflags & URQ_REQUEST)) {
		panic("ohci_device_ctrl_done: not a request");
	}
#endif
	if (len)
		usb_syncmem(&xfer->dmabuf, 0, len,
		    isread ? BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);
	 usb_syncmem(&opipe->u.ctl.reqdma, 0,
	     sizeof(usb_device_request_t),  BUS_DMASYNC_POSTWRITE);
}

void
ohci_device_intr_done(usbd_xfer_handle xfer)
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)xfer->pipe;
	ohci_softc_t *sc = opipe->pipe.device->bus->hci_private;
	ohci_soft_ed_t *sed = opipe->sed;
	ohci_soft_td_t *data, *tail;
	int isread =
	    (UE_GET_DIR(xfer->pipe->endpoint->edesc->bEndpointAddress) == UE_DIR_IN);

	DPRINTFN(10,("ohci_device_intr_done: xfer=%p, actlen=%d\n",
		     xfer, xfer->actlen));

	KASSERT(sc->sc_bus.use_polling || mutex_owned(&sc->sc_lock));

	usb_syncmem(&xfer->dmabuf, 0, xfer->length,
	    isread ? BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);
	if (xfer->pipe->repeat) {
		data = opipe->tail.td;
		tail = ohci_alloc_std(sc); /* XXX should reuse TD */
		if (tail == NULL) {
			xfer->status = USBD_NOMEM;
			return;
		}
		tail->xfer = NULL;

		data->td.td_flags = HTOO32(
			OHCI_TD_IN | OHCI_TD_NOCC |
			OHCI_TD_SET_DI(1) | OHCI_TD_TOGGLE_CARRY);
		if (xfer->flags & USBD_SHORT_XFER_OK)
			data->td.td_flags |= HTOO32(OHCI_TD_R);
		data->td.td_cbp = HTOO32(DMAADDR(&xfer->dmabuf, 0));
		data->nexttd = tail;
		data->td.td_nexttd = HTOO32(tail->physaddr);
		data->td.td_be = HTOO32(O32TOH(data->td.td_cbp) +
			xfer->length - 1);
		data->len = xfer->length;
		data->xfer = xfer;
		data->flags = OHCI_CALL_DONE | OHCI_ADD_LEN;
		usb_syncmem(&data->dma, data->offs, sizeof(data->td),
		    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
		xfer->hcpriv = data;
		xfer->actlen = 0;

		sed->ed.ed_tailp = HTOO32(tail->physaddr);
		usb_syncmem(&sed->dma,
		    sed->offs + offsetof(ohci_ed_t, ed_tailp),
		    sizeof(sed->ed.ed_tailp),
		    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
		opipe->tail.td = tail;
	}
}

void
ohci_device_bulk_done(usbd_xfer_handle xfer)
{
#ifdef DIAGNOSTIC
	ohci_softc_t *sc = xfer->pipe->device->bus->hci_private;
#endif
	int isread =
	    (UE_GET_DIR(xfer->pipe->endpoint->edesc->bEndpointAddress) == UE_DIR_IN);

	KASSERT(mutex_owned(&sc->sc_lock));

	DPRINTFN(10,("ohci_device_bulk_done: xfer=%p, actlen=%d\n",
		     xfer, xfer->actlen));
	usb_syncmem(&xfer->dmabuf, 0, xfer->length,
	    isread ? BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);
}

Static void
ohci_rhsc_softint(void *arg)
{
	ohci_softc_t *sc = arg;

	mutex_enter(&sc->sc_lock);

	ohci_rhsc(sc, sc->sc_intrxfer);

	/* Do not allow RHSC interrupts > 1 per second */
	callout_reset(&sc->sc_tmo_rhsc, hz, ohci_rhsc_enable, sc);

	mutex_exit(&sc->sc_lock);
}

void
ohci_rhsc(ohci_softc_t *sc, usbd_xfer_handle xfer)
{
	u_char *p;
	int i, m;
	int hstatus __unused;

	KASSERT(mutex_owned(&sc->sc_lock));

	hstatus = OREAD4(sc, OHCI_RH_STATUS);
	DPRINTF(("ohci_rhsc: sc=%p xfer=%p hstatus=0x%08x\n",
		 sc, xfer, hstatus));

	if (xfer == NULL) {
		/* Just ignore the change. */
		return;
	}

	p = KERNADDR(&xfer->dmabuf, 0);
	m = min(sc->sc_noport, xfer->length * 8 - 1);
	memset(p, 0, xfer->length);
	for (i = 1; i <= m; i++) {
		/* Pick out CHANGE bits from the status reg. */
		if (OREAD4(sc, OHCI_RH_PORT_STATUS(i)) >> 16)
			p[i/8] |= 1 << (i%8);
	}
	DPRINTF(("ohci_rhsc: change=0x%02x\n", *p));
	xfer->actlen = xfer->length;
	xfer->status = USBD_NORMAL_COMPLETION;

	usb_transfer_complete(xfer);
}

void
ohci_root_intr_done(usbd_xfer_handle xfer)
{
}

void
ohci_root_ctrl_done(usbd_xfer_handle xfer)
{
}

/*
 * Wait here until controller claims to have an interrupt.
 * Then call ohci_intr and return.  Use timeout to avoid waiting
 * too long.
 */
void
ohci_waitintr(ohci_softc_t *sc, usbd_xfer_handle xfer)
{
	int timo;
	u_int32_t intrs;

	mutex_enter(&sc->sc_lock);

	xfer->status = USBD_IN_PROGRESS;
	for (timo = xfer->timeout; timo >= 0; timo--) {
		usb_delay_ms(&sc->sc_bus, 1);
		if (sc->sc_dying)
			break;
		intrs = OREAD4(sc, OHCI_INTERRUPT_STATUS) & sc->sc_eintrs;
		DPRINTFN(15,("ohci_waitintr: 0x%04x\n", intrs));
#ifdef OHCI_DEBUG
		if (ohcidebug > 15)
			ohci_dumpregs(sc);
#endif
		if (intrs) {
			mutex_spin_enter(&sc->sc_intr_lock);
			ohci_intr1(sc);
			mutex_spin_exit(&sc->sc_intr_lock);
			if (xfer->status != USBD_IN_PROGRESS)
				goto done;
		}
	}

	/* Timeout */
	DPRINTF(("ohci_waitintr: timeout\n"));
	xfer->status = USBD_TIMEOUT;
	usb_transfer_complete(xfer);

	/* XXX should free TD */

done:
	mutex_exit(&sc->sc_lock);
}

void
ohci_poll(struct usbd_bus *bus)
{
	ohci_softc_t *sc = bus->hci_private;
#ifdef OHCI_DEBUG
	static int last;
	int new;
	new = OREAD4(sc, OHCI_INTERRUPT_STATUS);
	if (new != last) {
		DPRINTFN(10,("ohci_poll: intrs=0x%04x\n", new));
		last = new;
	}
#endif
	sc->sc_eintrs |= OHCI_WDH;
	if (OREAD4(sc, OHCI_INTERRUPT_STATUS) & sc->sc_eintrs) {
		mutex_spin_enter(&sc->sc_intr_lock);
		ohci_intr1(sc);
		mutex_spin_exit(&sc->sc_intr_lock);
	}
}

usbd_status
ohci_device_request(usbd_xfer_handle xfer)
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)xfer->pipe;
	usb_device_request_t *req = &xfer->request;
	usbd_device_handle dev = opipe->pipe.device;
	ohci_softc_t *sc = dev->bus->hci_private;
	ohci_soft_td_t *setup, *stat, *next, *tail;
	ohci_soft_ed_t *sed;
	int isread;
	int len;
	usbd_status err;

	KASSERT(mutex_owned(&sc->sc_lock));

	isread = req->bmRequestType & UT_READ;
	len = UGETW(req->wLength);

	DPRINTFN(3,("ohci_device_control type=0x%02x, request=0x%02x, "
		    "wValue=0x%04x, wIndex=0x%04x len=%d, addr=%d, endpt=%d\n",
		    req->bmRequestType, req->bRequest, UGETW(req->wValue),
		    UGETW(req->wIndex), len, dev->address,
		    opipe->pipe.endpoint->edesc->bEndpointAddress));

	setup = opipe->tail.td;
	stat = ohci_alloc_std(sc);
	if (stat == NULL) {
		err = USBD_NOMEM;
		goto bad1;
	}
	tail = ohci_alloc_std(sc);
	if (tail == NULL) {
		err = USBD_NOMEM;
		goto bad2;
	}
	tail->xfer = NULL;

	sed = opipe->sed;
	opipe->u.ctl.length = len;

        KASSERTMSG(OHCI_ED_GET_FA(O32TOH(sed->ed.ed_flags)) == dev->address,
	    "address ED %d pipe %d\n",
	    OHCI_ED_GET_FA(O32TOH(sed->ed.ed_flags)), dev->address);
        KASSERTMSG(OHCI_ED_GET_MAXP(O32TOH(sed->ed.ed_flags)) ==
            UGETW(opipe->pipe.endpoint->edesc->wMaxPacketSize),
	    "MPL ED %d pipe %d\n",
	    OHCI_ED_GET_MAXP(O32TOH(sed->ed.ed_flags)),
	    UGETW(opipe->pipe.endpoint->edesc->wMaxPacketSize));

	next = stat;

	/* Set up data transaction */
	if (len != 0) {
		ohci_soft_td_t *std = stat;

		err = ohci_alloc_std_chain(opipe, sc, len, isread, xfer,
			  std, &stat);
		if (err) {
			/* stat is unchanged if error */
			goto bad3;
		}
		stat = stat->nexttd; /* point at free TD */

		/* Start toggle at 1 and then use the carried toggle. */
		std->td.td_flags &= HTOO32(~OHCI_TD_TOGGLE_MASK);
		std->td.td_flags |= HTOO32(OHCI_TD_TOGGLE_1);
		usb_syncmem(&std->dma,
		    std->offs + offsetof(ohci_td_t, td_flags),
		    sizeof(std->td.td_flags),
		    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	}

	memcpy(KERNADDR(&opipe->u.ctl.reqdma, 0), req, sizeof *req);
	usb_syncmem(&opipe->u.ctl.reqdma, 0, sizeof *req, BUS_DMASYNC_PREWRITE);

	setup->td.td_flags = HTOO32(OHCI_TD_SETUP | OHCI_TD_NOCC |
				     OHCI_TD_TOGGLE_0 | OHCI_TD_NOINTR);
	setup->td.td_cbp = HTOO32(DMAADDR(&opipe->u.ctl.reqdma, 0));
	setup->nexttd = next;
	setup->td.td_nexttd = HTOO32(next->physaddr);
	setup->td.td_be = HTOO32(O32TOH(setup->td.td_cbp) + sizeof *req - 1);
	setup->len = 0;
	setup->xfer = xfer;
	setup->flags = 0;
	xfer->hcpriv = setup;
	usb_syncmem(&setup->dma, setup->offs, sizeof(setup->td),
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);

	stat->td.td_flags = HTOO32(
		(isread ? OHCI_TD_OUT : OHCI_TD_IN) |
		OHCI_TD_NOCC | OHCI_TD_TOGGLE_1 | OHCI_TD_SET_DI(1));
	stat->td.td_cbp = 0;
	stat->nexttd = tail;
	stat->td.td_nexttd = HTOO32(tail->physaddr);
	stat->td.td_be = 0;
	stat->flags = OHCI_CALL_DONE;
	stat->len = 0;
	stat->xfer = xfer;
	usb_syncmem(&stat->dma, stat->offs, sizeof(stat->td),
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);

#ifdef OHCI_DEBUG
	if (ohcidebug > 5) {
		DPRINTF(("ohci_device_request:\n"));
		ohci_dump_ed(sc, sed);
		ohci_dump_tds(sc, setup);
	}
#endif

	/* Insert ED in schedule */
	sed->ed.ed_tailp = HTOO32(tail->physaddr);
	usb_syncmem(&sed->dma,
	    sed->offs + offsetof(ohci_ed_t, ed_tailp),
	    sizeof(sed->ed.ed_tailp),
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	opipe->tail.td = tail;
	OWRITE4(sc, OHCI_COMMAND_STATUS, OHCI_CLF);
	if (xfer->timeout && !sc->sc_bus.use_polling) {
		callout_reset(&xfer->timeout_handle, mstohz(xfer->timeout),
			    ohci_timeout, xfer);
	}

#ifdef OHCI_DEBUG
	if (ohcidebug > 20) {
		delay(10000);
		DPRINTF(("ohci_device_request: status=%x\n",
			 OREAD4(sc, OHCI_COMMAND_STATUS)));
		ohci_dumpregs(sc);
		printf("ctrl head:\n");
		ohci_dump_ed(sc, sc->sc_ctrl_head);
		printf("sed:\n");
		ohci_dump_ed(sc, sed);
		ohci_dump_tds(sc, setup);
	}
#endif

	return (USBD_NORMAL_COMPLETION);

 bad3:
	ohci_free_std(sc, tail);
 bad2:
	ohci_free_std(sc, stat);
 bad1:
	return (err);
}

/*
 * Add an ED to the schedule.  Called with USB lock held.
 */
Static void
ohci_add_ed(ohci_softc_t *sc, ohci_soft_ed_t *sed, ohci_soft_ed_t *head)
{
	DPRINTFN(8,("ohci_add_ed: sed=%p head=%p\n", sed, head));

	KASSERT(mutex_owned(&sc->sc_lock));

	usb_syncmem(&head->dma, head->offs + offsetof(ohci_ed_t, ed_nexted),
	    sizeof(head->ed.ed_nexted),
	    BUS_DMASYNC_POSTWRITE | BUS_DMASYNC_POSTREAD);
	sed->next = head->next;
	sed->ed.ed_nexted = head->ed.ed_nexted;
	usb_syncmem(&sed->dma, sed->offs + offsetof(ohci_ed_t, ed_nexted),
	    sizeof(sed->ed.ed_nexted),
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	head->next = sed;
	head->ed.ed_nexted = HTOO32(sed->physaddr);
	usb_syncmem(&head->dma, head->offs + offsetof(ohci_ed_t, ed_nexted),
	    sizeof(head->ed.ed_nexted),
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
}

/*
 * Remove an ED from the schedule.  Called with USB lock held.
 */
Static void
ohci_rem_ed(ohci_softc_t *sc, ohci_soft_ed_t *sed, ohci_soft_ed_t *head)
{
	ohci_soft_ed_t *p;

	KASSERT(mutex_owned(&sc->sc_lock));

	/* XXX */
	for (p = head; p != NULL && p->next != sed; p = p->next)
		;
	KASSERT(p != NULL);

	usb_syncmem(&sed->dma, sed->offs + offsetof(ohci_ed_t, ed_nexted),
	    sizeof(sed->ed.ed_nexted),
	    BUS_DMASYNC_POSTWRITE | BUS_DMASYNC_POSTREAD);
	p->next = sed->next;
	p->ed.ed_nexted = sed->ed.ed_nexted;
	usb_syncmem(&p->dma, p->offs + offsetof(ohci_ed_t, ed_nexted),
	    sizeof(p->ed.ed_nexted),
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
}

/*
 * When a transfer is completed the TD is added to the done queue by
 * the host controller.  This queue is the processed by software.
 * Unfortunately the queue contains the physical address of the TD
 * and we have no simple way to translate this back to a kernel address.
 * To make the translation possible (and fast) we use a hash table of
 * TDs currently in the schedule.  The physical address is used as the
 * hash value.
 */

#define HASH(a) (((a) >> 4) % OHCI_HASH_SIZE)
/* Called with USB lock held. */
void
ohci_hash_add_td(ohci_softc_t *sc, ohci_soft_td_t *std)
{
	int h = HASH(std->physaddr);

	KASSERT(sc->sc_bus.use_polling || mutex_owned(&sc->sc_lock));

	LIST_INSERT_HEAD(&sc->sc_hash_tds[h], std, hnext);
}

/* Called with USB lock held. */
void
ohci_hash_rem_td(ohci_softc_t *sc, ohci_soft_td_t *std)
{

	KASSERT(sc->sc_bus.use_polling || mutex_owned(&sc->sc_lock));

	LIST_REMOVE(std, hnext);
}

ohci_soft_td_t *
ohci_hash_find_td(ohci_softc_t *sc, ohci_physaddr_t a)
{
	int h = HASH(a);
	ohci_soft_td_t *std;

	for (std = LIST_FIRST(&sc->sc_hash_tds[h]);
	     std != NULL;
	     std = LIST_NEXT(std, hnext))
		if (std->physaddr == a)
			return (std);
	return (NULL);
}

/* Called with USB lock held. */
void
ohci_hash_add_itd(ohci_softc_t *sc, ohci_soft_itd_t *sitd)
{
	int h = HASH(sitd->physaddr);

	KASSERT(mutex_owned(&sc->sc_lock));

	DPRINTFN(10,("ohci_hash_add_itd: sitd=%p physaddr=0x%08lx\n",
		    sitd, (u_long)sitd->physaddr));

	LIST_INSERT_HEAD(&sc->sc_hash_itds[h], sitd, hnext);
}

/* Called with USB lock held. */
void
ohci_hash_rem_itd(ohci_softc_t *sc, ohci_soft_itd_t *sitd)
{

	KASSERT(mutex_owned(&sc->sc_lock));

	DPRINTFN(10,("ohci_hash_rem_itd: sitd=%p physaddr=0x%08lx\n",
		    sitd, (u_long)sitd->physaddr));

	LIST_REMOVE(sitd, hnext);
}

ohci_soft_itd_t *
ohci_hash_find_itd(ohci_softc_t *sc, ohci_physaddr_t a)
{
	int h = HASH(a);
	ohci_soft_itd_t *sitd;

	for (sitd = LIST_FIRST(&sc->sc_hash_itds[h]);
	     sitd != NULL;
	     sitd = LIST_NEXT(sitd, hnext))
		if (sitd->physaddr == a)
			return (sitd);
	return (NULL);
}

void
ohci_timeout(void *addr)
{
	struct ohci_xfer *oxfer = addr;
	struct ohci_pipe *opipe = (struct ohci_pipe *)oxfer->xfer.pipe;
	ohci_softc_t *sc = opipe->pipe.device->bus->hci_private;

	DPRINTF(("ohci_timeout: oxfer=%p\n", oxfer));

	if (sc->sc_dying) {
		mutex_enter(&sc->sc_lock);
		ohci_abort_xfer(&oxfer->xfer, USBD_TIMEOUT);
		mutex_exit(&sc->sc_lock);
		return;
	}

	/* Execute the abort in a process context. */
	usb_init_task(&oxfer->abort_task, ohci_timeout_task, addr,
	    USB_TASKQ_MPSAFE);
	usb_add_task(oxfer->xfer.pipe->device, &oxfer->abort_task,
	    USB_TASKQ_HC);
}

void
ohci_timeout_task(void *addr)
{
	usbd_xfer_handle xfer = addr;
	ohci_softc_t *sc = xfer->pipe->device->bus->hci_private;

	DPRINTF(("ohci_timeout_task: xfer=%p\n", xfer));

	mutex_enter(&sc->sc_lock);
	ohci_abort_xfer(xfer, USBD_TIMEOUT);
	mutex_exit(&sc->sc_lock);
}

#ifdef OHCI_DEBUG
void
ohci_dump_tds(ohci_softc_t *sc, ohci_soft_td_t *std)
{
	for (; std; std = std->nexttd)
		ohci_dump_td(sc, std);
}

void
ohci_dump_td(ohci_softc_t *sc, ohci_soft_td_t *std)
{
	char sbuf[128];

	usb_syncmem(&std->dma, std->offs, sizeof(std->td),
	    BUS_DMASYNC_POSTWRITE | BUS_DMASYNC_POSTREAD);
	snprintb(sbuf, sizeof(sbuf),
	    "\177\20"
	    "b\22R\0"
	    "f\23\02DP\0"
		"=\x0" "setup\0"
		"=\x1" "out\0"
		"=\x2" "in\0"
		"=\x3" "reserved\0"
	    "f\25\03DI\0"
		"=\x07" "none\0"
	    "f\30\02T\0"
		"=\x0" "carry\0"
		"=\x1" "carry\0"
		"=\x2" "0\0"
		"=\x3" "1\0"
	    "f\32\02EC\0"
	    "f\34\04CC\0",
	    (u_int32_t)O32TOH(std->td.td_flags));
	printf("TD(%p) at %08lx:\n\tflags=%s\n\tcbp=0x%08lx nexttd=0x%08lx be=0x%08lx\n",
	       std, (u_long)std->physaddr, sbuf,
	       (u_long)O32TOH(std->td.td_cbp),
	       (u_long)O32TOH(std->td.td_nexttd),
	       (u_long)O32TOH(std->td.td_be));
}

void
ohci_dump_itd(ohci_softc_t *sc, ohci_soft_itd_t *sitd)
{
	int i;

	usb_syncmem(&sitd->dma, sitd->offs, sizeof(sitd->itd),
	    BUS_DMASYNC_POSTWRITE | BUS_DMASYNC_POSTREAD);
	printf("ITD(%p) at %08lx: sf=%d di=%d fc=%d cc=%d\n"
	       "bp0=0x%08lx next=0x%08lx be=0x%08lx\n",
	       sitd, (u_long)sitd->physaddr,
	       OHCI_ITD_GET_SF(O32TOH(sitd->itd.itd_flags)),
	       OHCI_ITD_GET_DI(O32TOH(sitd->itd.itd_flags)),
	       OHCI_ITD_GET_FC(O32TOH(sitd->itd.itd_flags)),
	       OHCI_ITD_GET_CC(O32TOH(sitd->itd.itd_flags)),
	       (u_long)O32TOH(sitd->itd.itd_bp0),
	       (u_long)O32TOH(sitd->itd.itd_nextitd),
	       (u_long)O32TOH(sitd->itd.itd_be));
	for (i = 0; i < OHCI_ITD_NOFFSET; i++)
		printf("offs[%d]=0x%04x ", i,
		       (u_int)O16TOH(sitd->itd.itd_offset[i]));
	printf("\n");
}

void
ohci_dump_itds(ohci_softc_t *sc, ohci_soft_itd_t *sitd)
{
	for (; sitd; sitd = sitd->nextitd)
		ohci_dump_itd(sc, sitd);
}

void
ohci_dump_ed(ohci_softc_t *sc, ohci_soft_ed_t *sed)
{
	char sbuf[128], sbuf2[128];

	usb_syncmem(&sed->dma, sed->offs, sizeof(sed->ed),
	    BUS_DMASYNC_POSTWRITE | BUS_DMASYNC_POSTREAD);
	snprintb(sbuf, sizeof(sbuf),
	    "\20\14OUT\15IN\16LOWSPEED\17SKIP\20ISO",
	    (u_int32_t)O32TOH(sed->ed.ed_flags));
	snprintb(sbuf2, sizeof(sbuf2), "\20\1HALT\2CARRY",
	    (u_int32_t)O32TOH(sed->ed.ed_headp));

	printf("ED(%p) at 0x%08lx: addr=%d endpt=%d maxp=%d flags=%s\ntailp=0x%08lx "
		 "headflags=%s headp=0x%08lx nexted=0x%08lx\n",
		 sed, (u_long)sed->physaddr,
		 OHCI_ED_GET_FA(O32TOH(sed->ed.ed_flags)),
		 OHCI_ED_GET_EN(O32TOH(sed->ed.ed_flags)),
		 OHCI_ED_GET_MAXP(O32TOH(sed->ed.ed_flags)), sbuf,
		 (u_long)O32TOH(sed->ed.ed_tailp), sbuf2,
		 (u_long)O32TOH(sed->ed.ed_headp),
		 (u_long)O32TOH(sed->ed.ed_nexted));
}
#endif

usbd_status
ohci_open(usbd_pipe_handle pipe)
{
	usbd_device_handle dev = pipe->device;
	ohci_softc_t *sc = dev->bus->hci_private;
	usb_endpoint_descriptor_t *ed = pipe->endpoint->edesc;
	struct ohci_pipe *opipe = (struct ohci_pipe *)pipe;
	u_int8_t addr = dev->address;
	u_int8_t xfertype = ed->bmAttributes & UE_XFERTYPE;
	ohci_soft_ed_t *sed;
	ohci_soft_td_t *std;
	ohci_soft_itd_t *sitd;
	ohci_physaddr_t tdphys;
	u_int32_t fmt;
	usbd_status err = USBD_NOMEM;
	int ival;

	DPRINTFN(1, ("ohci_open: pipe=%p, addr=%d, endpt=%d (%d)\n",
		     pipe, addr, ed->bEndpointAddress, sc->sc_addr));

	if (sc->sc_dying) {
		return USBD_IOERROR;
	}

	std = NULL;
	sed = NULL;

	if (addr == sc->sc_addr) {
		switch (ed->bEndpointAddress) {
		case USB_CONTROL_ENDPOINT:
			pipe->methods = &ohci_root_ctrl_methods;
			break;
		case UE_DIR_IN | OHCI_INTR_ENDPT:
			pipe->methods = &ohci_root_intr_methods;
			break;
		default:
			err = USBD_INVAL;
			goto bad;
		}
	} else {
		sed = ohci_alloc_sed(sc);
		if (sed == NULL)
			goto bad;
		opipe->sed = sed;
		if (xfertype == UE_ISOCHRONOUS) {
			mutex_enter(&sc->sc_lock);
			sitd = ohci_alloc_sitd(sc);
			mutex_exit(&sc->sc_lock);
			if (sitd == NULL)
				goto bad;

			opipe->tail.itd = sitd;
			tdphys = sitd->physaddr;
			fmt = OHCI_ED_FORMAT_ISO;
			if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN)
				fmt |= OHCI_ED_DIR_IN;
			else
				fmt |= OHCI_ED_DIR_OUT;
		} else {
			mutex_enter(&sc->sc_lock);
			std = ohci_alloc_std(sc);
			mutex_exit(&sc->sc_lock);
			if (std == NULL)
				goto bad;

			opipe->tail.td = std;
			tdphys = std->physaddr;
			fmt = OHCI_ED_FORMAT_GEN | OHCI_ED_DIR_TD;
		}
		sed->ed.ed_flags = HTOO32(
			OHCI_ED_SET_FA(addr) |
			OHCI_ED_SET_EN(UE_GET_ADDR(ed->bEndpointAddress)) |
			(dev->speed == USB_SPEED_LOW ? OHCI_ED_SPEED : 0) |
			fmt |
			OHCI_ED_SET_MAXP(UGETW(ed->wMaxPacketSize)));
		sed->ed.ed_headp = HTOO32(tdphys |
		    (pipe->endpoint->datatoggle ? OHCI_TOGGLECARRY : 0));
		sed->ed.ed_tailp = HTOO32(tdphys);
		usb_syncmem(&sed->dma, sed->offs, sizeof(sed->ed),
		    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);

		switch (xfertype) {
		case UE_CONTROL:
			pipe->methods = &ohci_device_ctrl_methods;
			err = usb_allocmem(&sc->sc_bus,
				  sizeof(usb_device_request_t),
				  0, &opipe->u.ctl.reqdma);
			if (err)
				goto bad;
			mutex_enter(&sc->sc_lock);
			ohci_add_ed(sc, sed, sc->sc_ctrl_head);
			mutex_exit(&sc->sc_lock);
			break;
		case UE_INTERRUPT:
			pipe->methods = &ohci_device_intr_methods;
			ival = pipe->interval;
			if (ival == USBD_DEFAULT_INTERVAL)
				ival = ed->bInterval;
			err = ohci_device_setintr(sc, opipe, ival);
			if (err)
				goto bad;
			break;
		case UE_ISOCHRONOUS:
			pipe->methods = &ohci_device_isoc_methods;
			return (ohci_setup_isoc(pipe));
		case UE_BULK:
			pipe->methods = &ohci_device_bulk_methods;
			mutex_enter(&sc->sc_lock);
			ohci_add_ed(sc, sed, sc->sc_bulk_head);
			mutex_exit(&sc->sc_lock);
			break;
		}
	}

	return USBD_NORMAL_COMPLETION;

 bad:
	if (std != NULL) {
		mutex_enter(&sc->sc_lock);
		ohci_free_std(sc, std);
		mutex_exit(&sc->sc_lock);
	}
	if (sed != NULL)
		ohci_free_sed(sc, sed);
	return err;

}

/*
 * Close a reqular pipe.
 * Assumes that there are no pending transactions.
 */
void
ohci_close_pipe(usbd_pipe_handle pipe, ohci_soft_ed_t *head)
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)pipe;
	ohci_softc_t *sc = pipe->device->bus->hci_private;
	ohci_soft_ed_t *sed = opipe->sed;

	KASSERT(mutex_owned(&sc->sc_lock));

#ifdef DIAGNOSTIC
	sed->ed.ed_flags |= HTOO32(OHCI_ED_SKIP);
	if ((O32TOH(sed->ed.ed_tailp) & OHCI_HEADMASK) !=
	    (O32TOH(sed->ed.ed_headp) & OHCI_HEADMASK)) {
		ohci_soft_td_t *std;
		std = ohci_hash_find_td(sc, O32TOH(sed->ed.ed_headp));
		printf("ohci_close_pipe: pipe not empty sed=%p hd=0x%x "
		       "tl=0x%x pipe=%p, std=%p\n", sed,
		       (int)O32TOH(sed->ed.ed_headp),
		       (int)O32TOH(sed->ed.ed_tailp),
		       pipe, std);
#ifdef OHCI_DEBUG
		usbd_dump_pipe(&opipe->pipe);
		ohci_dump_ed(sc, sed);
		if (std)
			ohci_dump_td(sc, std);
#endif
		usb_delay_ms(&sc->sc_bus, 2);
		if ((O32TOH(sed->ed.ed_tailp) & OHCI_HEADMASK) !=
		    (O32TOH(sed->ed.ed_headp) & OHCI_HEADMASK))
			printf("ohci_close_pipe: pipe still not empty\n");
	}
#endif
	ohci_rem_ed(sc, sed, head);
	/* Make sure the host controller is not touching this ED */
	usb_delay_ms(&sc->sc_bus, 1);
	pipe->endpoint->datatoggle =
	    (O32TOH(sed->ed.ed_headp) & OHCI_TOGGLECARRY) ? 1 : 0;
	ohci_free_sed(sc, opipe->sed);
}

/*
 * Abort a device request.
 * If this routine is called at splusb() it guarantees that the request
 * will be removed from the hardware scheduling and that the callback
 * for it will be called with USBD_CANCELLED status.
 * It's impossible to guarantee that the requested transfer will not
 * have happened since the hardware runs concurrently.
 * If the transaction has already happened we rely on the ordinary
 * interrupt processing to process it.
 * XXX This is most probably wrong.
 * XXXMRG this doesn't make sense anymore.
 */
void
ohci_abort_xfer(usbd_xfer_handle xfer, usbd_status status)
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)xfer->pipe;
	ohci_softc_t *sc = opipe->pipe.device->bus->hci_private;
	ohci_soft_ed_t *sed = opipe->sed;
	ohci_soft_td_t *p, *n;
	ohci_physaddr_t headp;
	int hit;
	int wake;

	DPRINTF(("ohci_abort_xfer: xfer=%p pipe=%p sed=%p\n", xfer, opipe,sed));

	KASSERT(mutex_owned(&sc->sc_lock));

	if (sc->sc_dying) {
		/* If we're dying, just do the software part. */
		xfer->status = status;	/* make software ignore it */
		callout_halt(&xfer->timeout_handle, &sc->sc_lock);
		usb_transfer_complete(xfer);
		return;
	}

	if (cpu_intr_p() || cpu_softintr_p())
		panic("ohci_abort_xfer: not in process context");

	/*
	 * If an abort is already in progress then just wait for it to
	 * complete and return.
	 */
	if (xfer->hcflags & UXFER_ABORTING) {
		DPRINTFN(2, ("ohci_abort_xfer: already aborting\n"));
#ifdef DIAGNOSTIC
		if (status == USBD_TIMEOUT)
			printf("%s: TIMEOUT while aborting\n", __func__);
#endif
		/* Override the status which might be USBD_TIMEOUT. */
		xfer->status = status;
		DPRINTFN(2, ("ohci_abort_xfer: waiting for abort to finish\n"));
		xfer->hcflags |= UXFER_ABORTWAIT;
		while (xfer->hcflags & UXFER_ABORTING)
			cv_wait(&xfer->hccv, &sc->sc_lock);
		goto done;
	}
	xfer->hcflags |= UXFER_ABORTING;

	/*
	 * Step 1: Make interrupt routine and hardware ignore xfer.
	 */
	xfer->status = status;	/* make software ignore it */
	callout_stop(&xfer->timeout_handle);
	DPRINTFN(1,("ohci_abort_xfer: stop ed=%p\n", sed));
	usb_syncmem(&sed->dma, sed->offs + offsetof(ohci_ed_t, ed_flags),
	    sizeof(sed->ed.ed_flags),
	    BUS_DMASYNC_POSTWRITE | BUS_DMASYNC_POSTREAD);
	sed->ed.ed_flags |= HTOO32(OHCI_ED_SKIP); /* force hardware skip */
	usb_syncmem(&sed->dma, sed->offs + offsetof(ohci_ed_t, ed_flags),
	    sizeof(sed->ed.ed_flags),
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);

	/*
	 * Step 2: Wait until we know hardware has finished any possible
	 * use of the xfer.  Also make sure the soft interrupt routine
	 * has run.
	 */
	/* Hardware finishes in 1ms */
	usb_delay_ms_locked(opipe->pipe.device->bus, 20, &sc->sc_lock);
	sc->sc_softwake = 1;
	usb_schedsoftintr(&sc->sc_bus);
	cv_wait(&sc->sc_softwake_cv, &sc->sc_lock);

	/*
	 * Step 3: Remove any vestiges of the xfer from the hardware.
	 * The complication here is that the hardware may have executed
	 * beyond the xfer we're trying to abort.  So as we're scanning
	 * the TDs of this xfer we check if the hardware points to
	 * any of them.
	 */
	p = xfer->hcpriv;
#ifdef DIAGNOSTIC
	if (p == NULL) {
		xfer->hcflags &= ~UXFER_ABORTING; /* XXX */
		printf("ohci_abort_xfer: hcpriv is NULL\n");
		goto done;
	}
#endif
#ifdef OHCI_DEBUG
	if (ohcidebug > 1) {
		DPRINTF(("ohci_abort_xfer: sed=\n"));
		ohci_dump_ed(sc, sed);
		ohci_dump_tds(sc, p);
	}
#endif
	headp = O32TOH(sed->ed.ed_headp) & OHCI_HEADMASK;
	hit = 0;
	for (; p->xfer == xfer; p = n) {
		hit |= headp == p->physaddr;
		n = p->nexttd;
		ohci_free_std(sc, p);
	}
	/* Zap headp register if hardware pointed inside the xfer. */
	if (hit) {
		DPRINTFN(1,("ohci_abort_xfer: set hd=0x%08x, tl=0x%08x\n",
			    (int)p->physaddr, (int)O32TOH(sed->ed.ed_tailp)));
		sed->ed.ed_headp = HTOO32(p->physaddr); /* unlink TDs */
		usb_syncmem(&sed->dma,
		    sed->offs + offsetof(ohci_ed_t, ed_headp),
		    sizeof(sed->ed.ed_headp),
		    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	} else {
		DPRINTFN(1,("ohci_abort_xfer: no hit\n"));
	}

	/*
	 * Step 4: Turn on hardware again.
	 */
	usb_syncmem(&sed->dma, sed->offs + offsetof(ohci_ed_t, ed_flags),
	    sizeof(sed->ed.ed_flags),
	    BUS_DMASYNC_POSTWRITE | BUS_DMASYNC_POSTREAD);
	sed->ed.ed_flags &= HTOO32(~OHCI_ED_SKIP); /* remove hardware skip */
	usb_syncmem(&sed->dma, sed->offs + offsetof(ohci_ed_t, ed_flags),
	    sizeof(sed->ed.ed_flags),
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);

	/*
	 * Step 5: Execute callback.
	 */
	wake = xfer->hcflags & UXFER_ABORTWAIT;
	xfer->hcflags &= ~(UXFER_ABORTING | UXFER_ABORTWAIT);
	usb_transfer_complete(xfer);
	if (wake)
		cv_broadcast(&xfer->hccv);

done:
	KASSERT(mutex_owned(&sc->sc_lock));
}

/*
 * Data structures and routines to emulate the root hub.
 */
Static usb_device_descriptor_t ohci_devd = {
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

Static const usb_config_descriptor_t ohci_confd = {
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

Static const usb_interface_descriptor_t ohci_ifcd = {
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

Static const usb_endpoint_descriptor_t ohci_endpd = {
	.bLength = USB_ENDPOINT_DESCRIPTOR_SIZE,
	.bDescriptorType = UDESC_ENDPOINT,
	.bEndpointAddress = UE_DIR_IN | OHCI_INTR_ENDPT,
	.bmAttributes = UE_INTERRUPT,
	.wMaxPacketSize = {8, 0},			/* max packet */
	.bInterval = 255,
};

Static const usb_hub_descriptor_t ohci_hubd = {
	.bDescLength = USB_HUB_DESCRIPTOR_SIZE,
	.bDescriptorType = UDESC_HUB,
};

/*
 * Simulate a hardware hub by handling all the necessary requests.
 */
Static usbd_status
ohci_root_ctrl_transfer(usbd_xfer_handle xfer)
{
	ohci_softc_t *sc = xfer->pipe->device->bus->hci_private;
	usbd_status err;

	/* Insert last in queue. */
	mutex_enter(&sc->sc_lock);
	err = usb_insert_transfer(xfer);
	mutex_exit(&sc->sc_lock);
	if (err)
		return (err);

	/* Pipe isn't running, start first */
	return (ohci_root_ctrl_start(SIMPLEQ_FIRST(&xfer->pipe->queue)));
}

Static usbd_status
ohci_root_ctrl_start(usbd_xfer_handle xfer)
{
	ohci_softc_t *sc = xfer->pipe->device->bus->hci_private;
	usb_device_request_t *req;
	void *buf = NULL;
	int port, i;
	int len, value, index, l, totlen = 0;
	usb_port_status_t ps;
	usb_hub_descriptor_t hubd;
	usbd_status err;
	u_int32_t v;

	if (sc->sc_dying)
		return (USBD_IOERROR);

#ifdef DIAGNOSTIC
	if (!(xfer->rqflags & URQ_REQUEST))
		/* XXX panic */
		return (USBD_INVAL);
#endif
	req = &xfer->request;

	DPRINTFN(4,("ohci_root_ctrl_control type=0x%02x request=%02x\n",
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
			*(u_int8_t *)buf = sc->sc_conf;
			totlen = 1;
		}
		break;
	case C(UR_GET_DESCRIPTOR, UT_READ_DEVICE):
		DPRINTFN(8,("ohci_root_ctrl_control wValue=0x%04x\n", value));
		if (len == 0)
			break;
		switch(value >> 8) {
		case UDESC_DEVICE:
			if ((value & 0xff) != 0) {
				err = USBD_IOERROR;
				goto ret;
			}
			totlen = l = min(len, USB_DEVICE_DESCRIPTOR_SIZE);
			USETW(ohci_devd.idVendor, sc->sc_id_vendor);
			memcpy(buf, &ohci_devd, l);
			break;
		case UDESC_CONFIG:
			if ((value & 0xff) != 0) {
				err = USBD_IOERROR;
				goto ret;
			}
			totlen = l = min(len, USB_CONFIG_DESCRIPTOR_SIZE);
			memcpy(buf, &ohci_confd, l);
			buf = (char *)buf + l;
			len -= l;
			l = min(len, USB_INTERFACE_DESCRIPTOR_SIZE);
			totlen += l;
			memcpy(buf, &ohci_ifcd, l);
			buf = (char *)buf + l;
			len -= l;
			l = min(len, USB_ENDPOINT_DESCRIPTOR_SIZE);
			totlen += l;
			memcpy(buf, &ohci_endpd, l);
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
							 "OHCI root hub");
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
		sc->sc_addr = value;
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
		DPRINTFN(8, ("ohci_root_ctrl_control: UR_CLEAR_PORT_FEATURE "
			     "port=%d feature=%d\n",
			     index, value));
		if (index < 1 || index > sc->sc_noport) {
			err = USBD_IOERROR;
			goto ret;
		}
		port = OHCI_RH_PORT_STATUS(index);
		switch(value) {
		case UHF_PORT_ENABLE:
			OWRITE4(sc, port, UPS_CURRENT_CONNECT_STATUS);
			break;
		case UHF_PORT_SUSPEND:
			OWRITE4(sc, port, UPS_OVERCURRENT_INDICATOR);
			break;
		case UHF_PORT_POWER:
			/* Yes, writing to the LOW_SPEED bit clears power. */
			OWRITE4(sc, port, UPS_LOW_SPEED);
			break;
		case UHF_C_PORT_CONNECTION:
			OWRITE4(sc, port, UPS_C_CONNECT_STATUS << 16);
			break;
		case UHF_C_PORT_ENABLE:
			OWRITE4(sc, port, UPS_C_PORT_ENABLED << 16);
			break;
		case UHF_C_PORT_SUSPEND:
			OWRITE4(sc, port, UPS_C_SUSPEND << 16);
			break;
		case UHF_C_PORT_OVER_CURRENT:
			OWRITE4(sc, port, UPS_C_OVERCURRENT_INDICATOR << 16);
			break;
		case UHF_C_PORT_RESET:
			OWRITE4(sc, port, UPS_C_PORT_RESET << 16);
			break;
		default:
			err = USBD_IOERROR;
			goto ret;
		}
		switch(value) {
		case UHF_C_PORT_CONNECTION:
		case UHF_C_PORT_ENABLE:
		case UHF_C_PORT_SUSPEND:
		case UHF_C_PORT_OVER_CURRENT:
		case UHF_C_PORT_RESET:
			/* Enable RHSC interrupt if condition is cleared. */
			if ((OREAD4(sc, port) >> 16) == 0)
				ohci_rhsc_enable(sc);
			break;
		default:
			break;
		}
		break;
	case C(UR_GET_DESCRIPTOR, UT_READ_CLASS_DEVICE):
		if (len == 0)
			break;
		if ((value & 0xff) != 0) {
			err = USBD_IOERROR;
			goto ret;
		}
		v = OREAD4(sc, OHCI_RH_DESCRIPTOR_A);
		hubd = ohci_hubd;
		hubd.bNbrPorts = sc->sc_noport;
		USETW(hubd.wHubCharacteristics,
		      (v & OHCI_NPS ? UHD_PWR_NO_SWITCH :
		       v & OHCI_PSM ? UHD_PWR_GANGED : UHD_PWR_INDIVIDUAL)
		      /* XXX overcurrent */
		      );
		hubd.bPwrOn2PwrGood = OHCI_GET_POTPGT(v);
		v = OREAD4(sc, OHCI_RH_DESCRIPTOR_B);
		for (i = 0, l = sc->sc_noport; l > 0; i++, l -= 8, v >>= 8)
			hubd.DeviceRemovable[i++] = (u_int8_t)v;
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
		DPRINTFN(8,("ohci_root_ctrl_transfer: get port status i=%d\n",
			    index));
		if (index < 1 || index > sc->sc_noport) {
			err = USBD_IOERROR;
			goto ret;
		}
		if (len != 4) {
			err = USBD_IOERROR;
			goto ret;
		}
		v = OREAD4(sc, OHCI_RH_PORT_STATUS(index));
		DPRINTFN(8,("ohci_root_ctrl_transfer: port status=0x%04x\n",
			    v));
		USETW(ps.wPortStatus, v);
		USETW(ps.wPortChange, v >> 16);
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
		if (index < 1 || index > sc->sc_noport) {
			err = USBD_IOERROR;
			goto ret;
		}
		port = OHCI_RH_PORT_STATUS(index);
		switch(value) {
		case UHF_PORT_ENABLE:
			OWRITE4(sc, port, UPS_PORT_ENABLED);
			break;
		case UHF_PORT_SUSPEND:
			OWRITE4(sc, port, UPS_SUSPEND);
			break;
		case UHF_PORT_RESET:
			DPRINTFN(5,("ohci_root_ctrl_transfer: reset port %d\n",
				    index));
			OWRITE4(sc, port, UPS_RESET);
			for (i = 0; i < 5; i++) {
				usb_delay_ms(&sc->sc_bus,
					     USB_PORT_ROOT_RESET_DELAY);
				if (sc->sc_dying) {
					err = USBD_IOERROR;
					goto ret;
				}
				if ((OREAD4(sc, port) & UPS_RESET) == 0)
					break;
			}
			DPRINTFN(8,("ohci port %d reset, status = 0x%04x\n",
				    index, OREAD4(sc, port)));
			break;
		case UHF_PORT_POWER:
			DPRINTFN(2,("ohci_root_ctrl_transfer: set port power "
				    "%d\n", index));
			OWRITE4(sc, port, UPS_PORT_POWER);
			break;
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
Static void
ohci_root_ctrl_abort(usbd_xfer_handle xfer)
{
	/* Nothing to do, all transfers are synchronous. */
}

/* Close the root pipe. */
Static void
ohci_root_ctrl_close(usbd_pipe_handle pipe)
{
	DPRINTF(("ohci_root_ctrl_close\n"));
	/* Nothing to do. */
}

Static usbd_status
ohci_root_intr_transfer(usbd_xfer_handle xfer)
{
	ohci_softc_t *sc = xfer->pipe->device->bus->hci_private;
	usbd_status err;

	/* Insert last in queue. */
	mutex_enter(&sc->sc_lock);
	err = usb_insert_transfer(xfer);
	mutex_exit(&sc->sc_lock);
	if (err)
		return (err);

	/* Pipe isn't running, start first */
	return (ohci_root_intr_start(SIMPLEQ_FIRST(&xfer->pipe->queue)));
}

Static usbd_status
ohci_root_intr_start(usbd_xfer_handle xfer)
{
	usbd_pipe_handle pipe = xfer->pipe;
	ohci_softc_t *sc = pipe->device->bus->hci_private;

	if (sc->sc_dying)
		return (USBD_IOERROR);

	mutex_enter(&sc->sc_lock);
	KASSERT(sc->sc_intrxfer == NULL);
	sc->sc_intrxfer = xfer;
	mutex_exit(&sc->sc_lock);

	return (USBD_IN_PROGRESS);
}

/* Abort a root interrupt request. */
Static void
ohci_root_intr_abort(usbd_xfer_handle xfer)
{
	ohci_softc_t *sc = xfer->pipe->device->bus->hci_private;

	KASSERT(mutex_owned(&sc->sc_lock));
	KASSERT(xfer->pipe->intrxfer == xfer);

	sc->sc_intrxfer = NULL;

	xfer->status = USBD_CANCELLED;
	usb_transfer_complete(xfer);
}

/* Close the root pipe. */
Static void
ohci_root_intr_close(usbd_pipe_handle pipe)
{
	ohci_softc_t *sc = pipe->device->bus->hci_private;

	KASSERT(mutex_owned(&sc->sc_lock));

	DPRINTF(("ohci_root_intr_close\n"));

	sc->sc_intrxfer = NULL;
}

/************************/

Static usbd_status
ohci_device_ctrl_transfer(usbd_xfer_handle xfer)
{
	ohci_softc_t *sc = xfer->pipe->device->bus->hci_private;
	usbd_status err;

	/* Insert last in queue. */
	mutex_enter(&sc->sc_lock);
	err = usb_insert_transfer(xfer);
	mutex_exit(&sc->sc_lock);
	if (err)
		return (err);

	/* Pipe isn't running, start first */
	return (ohci_device_ctrl_start(SIMPLEQ_FIRST(&xfer->pipe->queue)));
}

Static usbd_status
ohci_device_ctrl_start(usbd_xfer_handle xfer)
{
	ohci_softc_t *sc = xfer->pipe->device->bus->hci_private;
	usbd_status err;

	if (sc->sc_dying)
		return (USBD_IOERROR);

#ifdef DIAGNOSTIC
	if (!(xfer->rqflags & URQ_REQUEST)) {
		/* XXX panic */
		printf("ohci_device_ctrl_transfer: not a request\n");
		return (USBD_INVAL);
	}
#endif

	mutex_enter(&sc->sc_lock);
	err = ohci_device_request(xfer);
	mutex_exit(&sc->sc_lock);
	if (err)
		return (err);

	if (sc->sc_bus.use_polling)
		ohci_waitintr(sc, xfer);
	return (USBD_IN_PROGRESS);
}

/* Abort a device control request. */
Static void
ohci_device_ctrl_abort(usbd_xfer_handle xfer)
{
#ifdef DIAGNOSTIC
	ohci_softc_t *sc = xfer->pipe->device->bus->hci_private;
#endif

	KASSERT(mutex_owned(&sc->sc_lock));

	DPRINTF(("ohci_device_ctrl_abort: xfer=%p\n", xfer));
	ohci_abort_xfer(xfer, USBD_CANCELLED);
}

/* Close a device control pipe. */
Static void
ohci_device_ctrl_close(usbd_pipe_handle pipe)
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)pipe;
	ohci_softc_t *sc = pipe->device->bus->hci_private;

	KASSERT(mutex_owned(&sc->sc_lock));

	DPRINTF(("ohci_device_ctrl_close: pipe=%p\n", pipe));
	ohci_close_pipe(pipe, sc->sc_ctrl_head);
	ohci_free_std(sc, opipe->tail.td);
}

/************************/

Static void
ohci_device_clear_toggle(usbd_pipe_handle pipe)
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)pipe;
	ohci_softc_t *sc = pipe->device->bus->hci_private;

	opipe->sed->ed.ed_headp &= HTOO32(~OHCI_TOGGLECARRY);
}

Static void
ohci_noop(usbd_pipe_handle pipe)
{
}

Static usbd_status
ohci_device_bulk_transfer(usbd_xfer_handle xfer)
{
	ohci_softc_t *sc = xfer->pipe->device->bus->hci_private;
	usbd_status err;

	/* Insert last in queue. */
	mutex_enter(&sc->sc_lock);
	err = usb_insert_transfer(xfer);
	mutex_exit(&sc->sc_lock);
	if (err)
		return (err);

	/* Pipe isn't running, start first */
	return (ohci_device_bulk_start(SIMPLEQ_FIRST(&xfer->pipe->queue)));
}

Static usbd_status
ohci_device_bulk_start(usbd_xfer_handle xfer)
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)xfer->pipe;
	usbd_device_handle dev = opipe->pipe.device;
	ohci_softc_t *sc = dev->bus->hci_private;
	int addr = dev->address;
	ohci_soft_td_t *data, *tail, *tdp;
	ohci_soft_ed_t *sed;
	int len, isread, endpt;
	usbd_status err;

	if (sc->sc_dying)
		return (USBD_IOERROR);

#ifdef DIAGNOSTIC
	if (xfer->rqflags & URQ_REQUEST) {
		/* XXX panic */
		printf("ohci_device_bulk_start: a request\n");
		return (USBD_INVAL);
	}
#endif

	mutex_enter(&sc->sc_lock);

	len = xfer->length;
	endpt = xfer->pipe->endpoint->edesc->bEndpointAddress;
	isread = UE_GET_DIR(endpt) == UE_DIR_IN;
	sed = opipe->sed;

	DPRINTFN(4,("ohci_device_bulk_start: xfer=%p len=%d isread=%d "
		    "flags=%d endpt=%d\n", xfer, len, isread, xfer->flags,
		    endpt));

	opipe->u.bulk.isread = isread;
	opipe->u.bulk.length = len;

	usb_syncmem(&sed->dma, sed->offs, sizeof(sed->ed),
	    BUS_DMASYNC_POSTWRITE | BUS_DMASYNC_POSTREAD);
	/* Update device address */
	sed->ed.ed_flags = HTOO32(
		(O32TOH(sed->ed.ed_flags) & ~OHCI_ED_ADDRMASK) |
		OHCI_ED_SET_FA(addr));

	/* Allocate a chain of new TDs (including a new tail). */
	data = opipe->tail.td;
	err = ohci_alloc_std_chain(opipe, sc, len, isread, xfer,
		  data, &tail);
	if (err)
		return err;
	
	/* We want interrupt at the end of the transfer. */
	tail->td.td_flags &= HTOO32(~OHCI_TD_INTR_MASK);
	tail->td.td_flags |= HTOO32(OHCI_TD_SET_DI(1));
	tail->flags |= OHCI_CALL_DONE;
	tail = tail->nexttd;	/* point at sentinel */
	usb_syncmem(&tail->dma, tail->offs + offsetof(ohci_td_t, td_flags),
	    sizeof(tail->td.td_flags),
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	if (err) {
		mutex_exit(&sc->sc_lock);
		return (err);
	}

	tail->xfer = NULL;
	xfer->hcpriv = data;

	DPRINTFN(4,("ohci_device_bulk_start: ed_flags=0x%08x td_flags=0x%08x "
		    "td_cbp=0x%08x td_be=0x%08x\n",
		    (int)O32TOH(sed->ed.ed_flags),
		    (int)O32TOH(data->td.td_flags),
		    (int)O32TOH(data->td.td_cbp),
		    (int)O32TOH(data->td.td_be)));

#ifdef OHCI_DEBUG
	if (ohcidebug > 5) {
		ohci_dump_ed(sc, sed);
		ohci_dump_tds(sc, data);
	}
#endif

	/* Insert ED in schedule */
	for (tdp = data; tdp != tail; tdp = tdp->nexttd) {
		tdp->xfer = xfer;
	}
	sed->ed.ed_tailp = HTOO32(tail->physaddr);
	opipe->tail.td = tail;
	sed->ed.ed_flags &= HTOO32(~OHCI_ED_SKIP);
	usb_syncmem(&sed->dma, sed->offs, sizeof(sed->ed),
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	OWRITE4(sc, OHCI_COMMAND_STATUS, OHCI_BLF);
	if (xfer->timeout && !sc->sc_bus.use_polling) {
		callout_reset(&xfer->timeout_handle, mstohz(xfer->timeout),
			    ohci_timeout, xfer);
	}
	mutex_exit(&sc->sc_lock);

#if 0
/* This goes wrong if we are too slow. */
	if (ohcidebug > 10) {
		delay(10000);
		DPRINTF(("ohci_device_intr_transfer: status=%x\n",
			 OREAD4(sc, OHCI_COMMAND_STATUS)));
		ohci_dump_ed(sc, sed);
		ohci_dump_tds(sc, data);
	}
#endif

	return (USBD_IN_PROGRESS);
}

Static void
ohci_device_bulk_abort(usbd_xfer_handle xfer)
{
#ifdef DIAGNOSTIC
	ohci_softc_t *sc = xfer->pipe->device->bus->hci_private;
#endif

	KASSERT(mutex_owned(&sc->sc_lock));

	DPRINTF(("ohci_device_bulk_abort: xfer=%p\n", xfer));
	ohci_abort_xfer(xfer, USBD_CANCELLED);
}

/*
 * Close a device bulk pipe.
 */
Static void
ohci_device_bulk_close(usbd_pipe_handle pipe)
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)pipe;
	ohci_softc_t *sc = pipe->device->bus->hci_private;

	KASSERT(mutex_owned(&sc->sc_lock));

	DPRINTF(("ohci_device_bulk_close: pipe=%p\n", pipe));
	ohci_close_pipe(pipe, sc->sc_bulk_head);
	ohci_free_std(sc, opipe->tail.td);
}

/************************/

Static usbd_status
ohci_device_intr_transfer(usbd_xfer_handle xfer)
{
	ohci_softc_t *sc = xfer->pipe->device->bus->hci_private;
	usbd_status err;

	/* Insert last in queue. */
	mutex_enter(&sc->sc_lock);
	err = usb_insert_transfer(xfer);
	mutex_exit(&sc->sc_lock);
	if (err)
		return (err);

	/* Pipe isn't running, start first */
	return (ohci_device_intr_start(SIMPLEQ_FIRST(&xfer->pipe->queue)));
}

Static usbd_status
ohci_device_intr_start(usbd_xfer_handle xfer)
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)xfer->pipe;
	usbd_device_handle dev = opipe->pipe.device;
	ohci_softc_t *sc = dev->bus->hci_private;
	ohci_soft_ed_t *sed = opipe->sed;
	ohci_soft_td_t *data, *tail;
	int len, isread, endpt;

	if (sc->sc_dying)
		return (USBD_IOERROR);

	DPRINTFN(3, ("ohci_device_intr_transfer: xfer=%p len=%d "
		     "flags=%d priv=%p\n",
		     xfer, xfer->length, xfer->flags, xfer->priv));

#ifdef DIAGNOSTIC
	if (xfer->rqflags & URQ_REQUEST)
		panic("ohci_device_intr_transfer: a request");
#endif

	len = xfer->length;
	endpt = xfer->pipe->endpoint->edesc->bEndpointAddress;
	isread = UE_GET_DIR(endpt) == UE_DIR_IN;

	data = opipe->tail.td;
	mutex_enter(&sc->sc_lock);
	tail = ohci_alloc_std(sc);
	mutex_exit(&sc->sc_lock);
	if (tail == NULL)
		return (USBD_NOMEM);
	tail->xfer = NULL;

	data->td.td_flags = HTOO32(
		isread ? OHCI_TD_IN : OHCI_TD_OUT |
		OHCI_TD_NOCC |
		OHCI_TD_SET_DI(1) | OHCI_TD_TOGGLE_CARRY);
	if (xfer->flags & USBD_SHORT_XFER_OK)
		data->td.td_flags |= HTOO32(OHCI_TD_R);
	data->td.td_cbp = HTOO32(DMAADDR(&xfer->dmabuf, 0));
	data->nexttd = tail;
	data->td.td_nexttd = HTOO32(tail->physaddr);
	data->td.td_be = HTOO32(O32TOH(data->td.td_cbp) + len - 1);
	data->len = len;
	data->xfer = xfer;
	data->flags = OHCI_CALL_DONE | OHCI_ADD_LEN;
	usb_syncmem(&data->dma, data->offs, sizeof(data->td),
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	xfer->hcpriv = data;

#ifdef OHCI_DEBUG
	if (ohcidebug > 5) {
		DPRINTF(("ohci_device_intr_transfer:\n"));
		ohci_dump_ed(sc, sed);
		ohci_dump_tds(sc, data);
	}
#endif

	/* Insert ED in schedule */
	mutex_enter(&sc->sc_lock);
	usb_syncmem(&sed->dma, sed->offs, sizeof(sed->ed),
	    BUS_DMASYNC_POSTWRITE | BUS_DMASYNC_POSTREAD);
	sed->ed.ed_tailp = HTOO32(tail->physaddr);
	opipe->tail.td = tail;
	sed->ed.ed_flags &= HTOO32(~OHCI_ED_SKIP);
	usb_syncmem(&sed->dma, sed->offs, sizeof(sed->ed),
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);

#if 0
/*
 * This goes horribly wrong, printing thousands of descriptors,
 * because false references are followed due to the fact that the
 * TD is gone.
 */
	if (ohcidebug > 5) {
		usb_delay_ms_locked(&sc->sc_bus, 5, &sc->sc_lock);
		DPRINTF(("ohci_device_intr_transfer: status=%x\n",
			 OREAD4(sc, OHCI_COMMAND_STATUS)));
		ohci_dump_ed(sc, sed);
		ohci_dump_tds(sc, data);
	}
#endif
	mutex_exit(&sc->sc_lock);

	return (USBD_IN_PROGRESS);
}

/* Abort a device interrupt request. */
Static void
ohci_device_intr_abort(usbd_xfer_handle xfer)
{
#ifdef DIAGNOSTIC
	ohci_softc_t *sc = xfer->pipe->device->bus->hci_private;
#endif

	KASSERT(mutex_owned(&sc->sc_lock));
	KASSERT(xfer->pipe->intrxfer == xfer);

	ohci_abort_xfer(xfer, USBD_CANCELLED);
}

/* Close a device interrupt pipe. */
Static void
ohci_device_intr_close(usbd_pipe_handle pipe)
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)pipe;
	ohci_softc_t *sc = pipe->device->bus->hci_private;
	int nslots = opipe->u.intr.nslots;
	int pos = opipe->u.intr.pos;
	int j;
	ohci_soft_ed_t *p, *sed = opipe->sed;

	KASSERT(mutex_owned(&sc->sc_lock));

	DPRINTFN(1,("ohci_device_intr_close: pipe=%p nslots=%d pos=%d\n",
		    pipe, nslots, pos));
	usb_syncmem(&sed->dma, sed->offs,
	    sizeof(sed->ed), BUS_DMASYNC_POSTWRITE | BUS_DMASYNC_POSTREAD);
	sed->ed.ed_flags |= HTOO32(OHCI_ED_SKIP);
	usb_syncmem(&sed->dma, sed->offs + offsetof(ohci_ed_t, ed_flags),
	    sizeof(sed->ed.ed_flags),
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	if ((O32TOH(sed->ed.ed_tailp) & OHCI_HEADMASK) !=
	    (O32TOH(sed->ed.ed_headp) & OHCI_HEADMASK))
		usb_delay_ms_locked(&sc->sc_bus, 2, &sc->sc_lock);

	for (p = sc->sc_eds[pos]; p && p->next != sed; p = p->next)
		continue;
#ifdef DIAGNOSTIC
	if (p == NULL)
		panic("ohci_device_intr_close: ED not found");
#endif
	p->next = sed->next;
	p->ed.ed_nexted = sed->ed.ed_nexted;
	usb_syncmem(&p->dma, p->offs + offsetof(ohci_ed_t, ed_nexted),
	    sizeof(p->ed.ed_nexted),
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);

	for (j = 0; j < nslots; j++)
		--sc->sc_bws[(pos * nslots + j) % OHCI_NO_INTRS];

	ohci_free_std(sc, opipe->tail.td);
	ohci_free_sed(sc, opipe->sed);
}

Static usbd_status
ohci_device_setintr(ohci_softc_t *sc, struct ohci_pipe *opipe, int ival)
{
	int i, j, best;
	u_int npoll, slow, shigh, nslots;
	u_int bestbw, bw;
	ohci_soft_ed_t *hsed, *sed = opipe->sed;

	DPRINTFN(2, ("ohci_setintr: pipe=%p\n", opipe));
	if (ival == 0) {
		printf("ohci_setintr: 0 interval\n");
		return (USBD_INVAL);
	}

	npoll = OHCI_NO_INTRS;
	while (npoll > ival)
		npoll /= 2;
	DPRINTFN(2, ("ohci_setintr: ival=%d npoll=%d\n", ival, npoll));

	/*
	 * We now know which level in the tree the ED must go into.
	 * Figure out which slot has most bandwidth left over.
	 * Slots to examine:
	 * npoll
	 * 1	0
	 * 2	1 2
	 * 4	3 4 5 6
	 * 8	7 8 9 10 11 12 13 14
	 * N    (N-1) .. (N-1+N-1)
	 */
	slow = npoll-1;
	shigh = slow + npoll;
	nslots = OHCI_NO_INTRS / npoll;
	for (best = i = slow, bestbw = ~0; i < shigh; i++) {
		bw = 0;
		for (j = 0; j < nslots; j++)
			bw += sc->sc_bws[(i * nslots + j) % OHCI_NO_INTRS];
		if (bw < bestbw) {
			best = i;
			bestbw = bw;
		}
	}
	DPRINTFN(2, ("ohci_setintr: best=%d(%d..%d) bestbw=%d\n",
		     best, slow, shigh, bestbw));

	mutex_enter(&sc->sc_lock);
	hsed = sc->sc_eds[best];
	sed->next = hsed->next;
	usb_syncmem(&hsed->dma, hsed->offs + offsetof(ohci_ed_t, ed_flags),
	    sizeof(hsed->ed.ed_flags),
	    BUS_DMASYNC_POSTWRITE | BUS_DMASYNC_POSTREAD);
	sed->ed.ed_nexted = hsed->ed.ed_nexted;
	usb_syncmem(&sed->dma, sed->offs + offsetof(ohci_ed_t, ed_flags),
	    sizeof(sed->ed.ed_flags),
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	hsed->next = sed;
	hsed->ed.ed_nexted = HTOO32(sed->physaddr);
	usb_syncmem(&hsed->dma, hsed->offs + offsetof(ohci_ed_t, ed_flags),
	    sizeof(hsed->ed.ed_flags),
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	mutex_exit(&sc->sc_lock);

	for (j = 0; j < nslots; j++)
		++sc->sc_bws[(best * nslots + j) % OHCI_NO_INTRS];
	opipe->u.intr.nslots = nslots;
	opipe->u.intr.pos = best;

	DPRINTFN(5, ("ohci_setintr: returns %p\n", opipe));
	return (USBD_NORMAL_COMPLETION);
}

/***********************/

usbd_status
ohci_device_isoc_transfer(usbd_xfer_handle xfer)
{
	ohci_softc_t *sc = xfer->pipe->device->bus->hci_private;
	usbd_status err;

	DPRINTFN(5,("ohci_device_isoc_transfer: xfer=%p\n", xfer));

	/* Put it on our queue, */
	mutex_enter(&sc->sc_lock);
	err = usb_insert_transfer(xfer);
	mutex_exit(&sc->sc_lock);

	/* bail out on error, */
	if (err && err != USBD_IN_PROGRESS)
		return (err);

	/* XXX should check inuse here */

	/* insert into schedule, */
	ohci_device_isoc_enter(xfer);

	/* and start if the pipe wasn't running */
	if (!err)
		ohci_device_isoc_start(SIMPLEQ_FIRST(&xfer->pipe->queue));

	return (err);
}

void
ohci_device_isoc_enter(usbd_xfer_handle xfer)
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)xfer->pipe;
	usbd_device_handle dev = opipe->pipe.device;
	ohci_softc_t *sc = dev->bus->hci_private;
	ohci_soft_ed_t *sed = opipe->sed;
	struct iso *iso = &opipe->u.iso;
	ohci_soft_itd_t *sitd, *nsitd;
	ohci_physaddr_t buf, offs, noffs, bp0;
	int i, ncur, nframes;

	DPRINTFN(1,("ohci_device_isoc_enter: used=%d next=%d xfer=%p "
		    "nframes=%d\n",
		    iso->inuse, iso->next, xfer, xfer->nframes));

	if (sc->sc_dying)
		return;

	if (iso->next == -1) {
		/* Not in use yet, schedule it a few frames ahead. */
		iso->next = O32TOH(sc->sc_hcca->hcca_frame_number) + 5;
		DPRINTFN(2,("ohci_device_isoc_enter: start next=%d\n",
			    iso->next));
	}

	sitd = opipe->tail.itd;
	buf = DMAADDR(&xfer->dmabuf, 0);
	bp0 = OHCI_PAGE(buf);
	offs = OHCI_PAGE_OFFSET(buf);
	nframes = xfer->nframes;
	xfer->hcpriv = sitd;
	for (i = ncur = 0; i < nframes; i++, ncur++) {
		noffs = offs + xfer->frlengths[i];
		if (ncur == OHCI_ITD_NOFFSET ||	/* all offsets used */
		    OHCI_PAGE(buf + noffs) > bp0 + OHCI_PAGE_SIZE) { /* too many page crossings */

			/* Allocate next ITD */
			mutex_enter(&sc->sc_lock);
			nsitd = ohci_alloc_sitd(sc);
			mutex_exit(&sc->sc_lock);
			if (nsitd == NULL) {
				/* XXX what now? */
				printf("%s: isoc TD alloc failed\n",
				       device_xname(sc->sc_dev));
				return;
			}

			/* Fill current ITD */
			sitd->itd.itd_flags = HTOO32(
				OHCI_ITD_NOCC |
				OHCI_ITD_SET_SF(iso->next) |
				OHCI_ITD_SET_DI(6) | /* delay intr a little */
				OHCI_ITD_SET_FC(ncur));
			sitd->itd.itd_bp0 = HTOO32(bp0);
			sitd->nextitd = nsitd;
			sitd->itd.itd_nextitd = HTOO32(nsitd->physaddr);
			sitd->itd.itd_be = HTOO32(bp0 + offs - 1);
			sitd->xfer = xfer;
			sitd->flags = 0;
			usb_syncmem(&sitd->dma, sitd->offs, sizeof(sitd->itd),
			    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);

			sitd = nsitd;
			iso->next = iso->next + ncur;
			bp0 = OHCI_PAGE(buf + offs);
			ncur = 0;
		}
		sitd->itd.itd_offset[ncur] = HTOO16(OHCI_ITD_MK_OFFS(offs));
		offs = noffs;
	}
	mutex_enter(&sc->sc_lock);
	nsitd = ohci_alloc_sitd(sc);
	mutex_exit(&sc->sc_lock);
	if (nsitd == NULL) {
		/* XXX what now? */
		printf("%s: isoc TD alloc failed\n",
		       device_xname(sc->sc_dev));
		return;
	}
	/* Fixup last used ITD */
	sitd->itd.itd_flags = HTOO32(
		OHCI_ITD_NOCC |
		OHCI_ITD_SET_SF(iso->next) |
		OHCI_ITD_SET_DI(0) |
		OHCI_ITD_SET_FC(ncur));
	sitd->itd.itd_bp0 = HTOO32(bp0);
	sitd->nextitd = nsitd;
	sitd->itd.itd_nextitd = HTOO32(nsitd->physaddr);
	sitd->itd.itd_be = HTOO32(bp0 + offs - 1);
	sitd->xfer = xfer;
	sitd->flags = OHCI_CALL_DONE;
	usb_syncmem(&sitd->dma, sitd->offs, sizeof(sitd->itd),
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);

	iso->next = iso->next + ncur;
	iso->inuse += nframes;

	xfer->actlen = offs;	/* XXX pretend we did it all */

	xfer->status = USBD_IN_PROGRESS;

#ifdef OHCI_DEBUG
	if (ohcidebug > 5) {
		DPRINTF(("ohci_device_isoc_enter: frame=%d\n",
			 O32TOH(sc->sc_hcca->hcca_frame_number)));
		ohci_dump_itds(sc, xfer->hcpriv);
		ohci_dump_ed(sc, sed);
	}
#endif

	mutex_enter(&sc->sc_lock);
	usb_syncmem(&sed->dma, sed->offs, sizeof(sed->ed),
	    BUS_DMASYNC_POSTWRITE | BUS_DMASYNC_POSTREAD);
	sed->ed.ed_tailp = HTOO32(nsitd->physaddr);
	opipe->tail.itd = nsitd;
	sed->ed.ed_flags &= HTOO32(~OHCI_ED_SKIP);
	usb_syncmem(&sed->dma, sed->offs + offsetof(ohci_ed_t, ed_flags),
	    sizeof(sed->ed.ed_flags),
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	mutex_exit(&sc->sc_lock);

#ifdef OHCI_DEBUG
	if (ohcidebug > 5) {
		delay(150000);
		DPRINTF(("ohci_device_isoc_enter: after frame=%d\n",
			 O32TOH(sc->sc_hcca->hcca_frame_number)));
		ohci_dump_itds(sc, xfer->hcpriv);
		ohci_dump_ed(sc, sed);
	}
#endif
}

usbd_status
ohci_device_isoc_start(usbd_xfer_handle xfer)
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)xfer->pipe;
	ohci_softc_t *sc = opipe->pipe.device->bus->hci_private;

	DPRINTFN(5,("ohci_device_isoc_start: xfer=%p\n", xfer));

	mutex_enter(&sc->sc_lock);

	if (sc->sc_dying) {
		mutex_exit(&sc->sc_lock);
		return (USBD_IOERROR);
	}

#ifdef DIAGNOSTIC
	if (xfer->status != USBD_IN_PROGRESS)
		printf("ohci_device_isoc_start: not in progress %p\n", xfer);
#endif

	/* XXX anything to do? */

	mutex_exit(&sc->sc_lock);

	return (USBD_IN_PROGRESS);
}

void
ohci_device_isoc_abort(usbd_xfer_handle xfer)
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)xfer->pipe;
	ohci_softc_t *sc = opipe->pipe.device->bus->hci_private;
	ohci_soft_ed_t *sed;
	ohci_soft_itd_t *sitd;

	DPRINTFN(1,("ohci_device_isoc_abort: xfer=%p lock=%p\n", xfer, &sc->sc_lock));

	KASSERT(mutex_owned(&sc->sc_lock));

	/* Transfer is already done. */
	if (xfer->status != USBD_NOT_STARTED &&
	    xfer->status != USBD_IN_PROGRESS) {
		printf("ohci_device_isoc_abort: early return\n");
		goto done;
	}

	/* Give xfer the requested abort code. */
	xfer->status = USBD_CANCELLED;

	sed = opipe->sed;
	usb_syncmem(&sed->dma, sed->offs, sizeof(sed->ed),
	    BUS_DMASYNC_POSTWRITE | BUS_DMASYNC_POSTREAD);
	sed->ed.ed_flags |= HTOO32(OHCI_ED_SKIP); /* force hardware skip */
	usb_syncmem(&sed->dma, sed->offs + offsetof(ohci_ed_t, ed_flags),
	    sizeof(sed->ed.ed_flags),
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);

	sitd = xfer->hcpriv;
#ifdef DIAGNOSTIC
	if (sitd == NULL) {
		printf("ohci_device_isoc_abort: hcpriv==0\n");
		goto done;
	}
#endif
	for (; sitd->xfer == xfer; sitd = sitd->nextitd) {
#ifdef DIAGNOSTIC
		DPRINTFN(1,("abort sets done sitd=%p\n", sitd));
		sitd->isdone = 1;
#endif
	}

	usb_delay_ms_locked(&sc->sc_bus, OHCI_ITD_NOFFSET, &sc->sc_lock);

	/* Run callback. */
	usb_transfer_complete(xfer);

	sed->ed.ed_headp = HTOO32(sitd->physaddr); /* unlink TDs */
	sed->ed.ed_flags &= HTOO32(~OHCI_ED_SKIP); /* remove hardware skip */
	usb_syncmem(&sed->dma, sed->offs, sizeof(sed->ed),
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);

 done:
	KASSERT(mutex_owned(&sc->sc_lock));
}

void
ohci_device_isoc_done(usbd_xfer_handle xfer)
{
	DPRINTFN(1,("ohci_device_isoc_done: xfer=%p\n", xfer));
}

usbd_status
ohci_setup_isoc(usbd_pipe_handle pipe)
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)pipe;
	ohci_softc_t *sc = pipe->device->bus->hci_private;
	struct iso *iso = &opipe->u.iso;

	iso->next = -1;
	iso->inuse = 0;

	mutex_enter(&sc->sc_lock);
	ohci_add_ed(sc, opipe->sed, sc->sc_isoc_head);
	mutex_exit(&sc->sc_lock);

	return (USBD_NORMAL_COMPLETION);
}

void
ohci_device_isoc_close(usbd_pipe_handle pipe)
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)pipe;
	ohci_softc_t *sc = pipe->device->bus->hci_private;

	KASSERT(mutex_owned(&sc->sc_lock));

	DPRINTF(("ohci_device_isoc_close: pipe=%p\n", pipe));
	ohci_close_pipe(pipe, sc->sc_isoc_head);
#ifdef DIAGNOSTIC
	opipe->tail.itd->isdone = 1;
#endif
	ohci_free_sitd(sc, opipe->tail.itd);
}
