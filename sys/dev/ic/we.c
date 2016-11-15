/*	$NetBSD: we.c,v 1.17 2010/03/19 15:59:22 tsutsui Exp $	*/

/*-
 * Copyright (c) 1997, 1998 The NetBSD Foundation, Inc.
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
 * Device driver for National Semiconductor DS8390/WD83C690 based ethernet
 * adapters.
 *
 * Copyright (c) 1994, 1995 Charles M. Hannum.  All rights reserved.
 *
 * Copyright (C) 1993, David Greenman.  This software may be used, modified,
 * copied, distributed, and sold, in both source and binary form provided that
 * the above copyright and these terms are retained.  Under no circumstances is
 * the author responsible for the proper functioning of this software, nor does
 * the author assume any responsibility for damages incurred with its use.
 */

/*
 * Device driver for the Western Digital/SMC 8003 and 8013 series,
 * and the SMC Elite Ultra (8216).
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: we.c,v 1.17 2010/03/19 15:59:22 tsutsui Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/socket.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_media.h>

#include <net/if_ether.h>

#include <sys/bus.h>
#include <sys/bswap.h>
#include <sys/intr.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#include <dev/ic/dp8390reg.h>
#include <dev/ic/dp8390var.h>
#include <dev/ic/wereg.h>
#include <dev/ic/wevar.h>

#ifndef __BUS_SPACE_HAS_STREAM_METHODS
#define	bus_space_read_region_stream_2	bus_space_read_region_2
#define	bus_space_write_stream_2	bus_space_write_2
#define	bus_space_write_region_stream_2	bus_space_write_region_2
#endif

static void	we_set_media(struct we_softc *, int);

static void	we_media_init(struct dp8390_softc *);

static int	we_mediachange(struct dp8390_softc *);
static void	we_mediastatus(struct dp8390_softc *, struct ifmediareq *);

static void	we_recv_int(struct dp8390_softc *);
static void	we_init_card(struct dp8390_softc *);
static int	we_write_mbuf(struct dp8390_softc *, struct mbuf *, int);
static int	we_ring_copy(struct dp8390_softc *, int, void *, u_short);
static void	we_read_hdr(struct dp8390_softc *, int, struct dp8390_ring *);
static int	we_test_mem(struct dp8390_softc *);

static inline void we_readmem(struct we_softc *, int, uint8_t *, int);

/*
 * Delay needed when switching 16-bit access to shared memory.
 */
#define	WE_DELAY(wsc) delay(3)

/*
 * Enable card RAM, and 16-bit access.
 */
#define	WE_MEM_ENABLE(wsc) \
if (((wsc)->sc_flags & WE_16BIT_NOTOGGLE) == 0) {			\
	if ((wsc)->sc_flags & WE_16BIT_ENABLE)				\
		bus_space_write_1((wsc)->sc_asict, (wsc)->sc_asich,	\
		    WE_LAAR, (wsc)->sc_laar_proto | WE_LAAR_M16EN);	\
	bus_space_write_1((wsc)->sc_asict, (wsc)->sc_asich,		\
	    WE_MSR, wsc->sc_msr_proto | WE_MSR_MENB);			\
	WE_DELAY((wsc));						\
}

/*
 * Disable card RAM, and 16-bit access.
 */
#define	WE_MEM_DISABLE(wsc) \
if (((wsc)->sc_flags & WE_16BIT_NOTOGGLE) == 0) {			\
	bus_space_write_1((wsc)->sc_asict, (wsc)->sc_asich,		\
	    WE_MSR, (wsc)->sc_msr_proto);				\
	if ((wsc)->sc_flags & WE_16BIT_ENABLE)				\
		bus_space_write_1((wsc)->sc_asict, (wsc)->sc_asich,	\
		    WE_LAAR, (wsc)->sc_laar_proto);			\
	WE_DELAY((wsc));						\
}

int
we_config(device_t self, struct we_softc *wsc, const char *typestr)
{
	struct dp8390_softc *sc = &wsc->sc_dp8390;
	uint8_t x;
	int i, forced_16bit = 0;

	/*
	 * Allow user to override 16-bit mode.  8-bit takes precedence.
	 */
	if (device_cfdata(self)->cf_flags & DP8390_FORCE_16BIT_MODE) {
		wsc->sc_flags |= WE_16BIT_ENABLE;
		forced_16bit = 1;
	}
	if (device_cfdata(self)->cf_flags & DP8390_FORCE_8BIT_MODE)
		wsc->sc_flags &= ~WE_16BIT_ENABLE;

	/* Registers are linear. */
	for (i = 0; i < 16; i++)
		sc->sc_reg_map[i] = i;

	/* Now we can use the NIC_{GET,PUT}() macros. */

	aprint_normal_dev(self, "%s Ethernet (%s-bit)\n",
	    typestr, wsc->sc_flags & WE_16BIT_ENABLE ? "16" : "8");

	/* Get station address from EEPROM. */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		sc->sc_enaddr[i] =
		    bus_space_read_1(wsc->sc_asict, wsc->sc_asich, WE_PROM + i);

	/*
	 * Set upper address bits and 8/16 bit access to shared memory.
	 */
	if (sc->is790) {
		wsc->sc_laar_proto =
		    bus_space_read_1(wsc->sc_asict, wsc->sc_asich, WE_LAAR) &
		    ~WE_LAAR_M16EN;
		bus_space_write_1(wsc->sc_asict, wsc->sc_asich, WE_LAAR,
		    wsc->sc_laar_proto |
		    (wsc->sc_flags & WE_16BIT_ENABLE ? WE_LAAR_M16EN : 0));
	} else if ((wsc->sc_type & WE_SOFTCONFIG) ||
#ifdef TOSH_ETHER
	    (wsc->sc_type == WE_TYPE_TOSHIBA1) ||
	    (wsc->sc_type == WE_TYPE_TOSHIBA4) ||
#endif
	    (forced_16bit) ||
	    (wsc->sc_type == WE_TYPE_WD8013EBT)) {
		wsc->sc_laar_proto = (wsc->sc_maddr >> 19) & WE_LAAR_ADDRHI;
		if (wsc->sc_flags & WE_16BIT_ENABLE)
			wsc->sc_laar_proto |= WE_LAAR_L16EN;
		bus_space_write_1(wsc->sc_asict, wsc->sc_asich, WE_LAAR,
		    wsc->sc_laar_proto |
		    (wsc->sc_flags & WE_16BIT_ENABLE ? WE_LAAR_M16EN : 0));
	}

	/*
	 * Set address and enable interface shared memory.
	 */
	if (sc->is790) {
		x = bus_space_read_1(wsc->sc_asict, wsc->sc_asich, WE790_HWR);
		bus_space_write_1(wsc->sc_asict, wsc->sc_asich,
		    WE790_HWR, x | WE790_HWR_SWH);
		bus_space_write_1(wsc->sc_asict, wsc->sc_asich, WE790_RAR,
		    ((wsc->sc_maddr >> WE790_RAR_OFF_SHIFT) & WE790_RAR_OFF) |
		    ((wsc->sc_maddr & (1 << WE790_RAR_BASE_SHIFT)) != 0 ?
		     WE790_RAR_BASE1 : WE790_RAR_BASE0) |
		    (bus_space_read_1(wsc->sc_asict, wsc->sc_asich, WE790_RAR) &
		     ~(WE790_RAR_OFF | WE790_RAR_BASE)));
		bus_space_write_1(wsc->sc_asict, wsc->sc_asich, WE790_HWR, x);
		wsc->sc_msr_proto = 0x00;
		sc->cr_proto = 0x00;
	} else {
#ifdef TOSH_ETHER
		if (wsc->sc_type == WE_TYPE_TOSHIBA1 ||
		    wsc->sc_type == WE_TYPE_TOSHIBA4) {
			/* XXX MAGIC CONSTANTS XXX */
			bus_space_write_1(wsc->sc_asict, wsc->sc_asich,
			    WE_MSR + 1,
			    ((wsc->sc_maddr >> 8) & 0xe0) | 0x04);
			bus_space_write_1(wsc->sc_asict, wsc->sc_asich,
			    WE_MSR + 2,
			    ((wsc->sc_maddr >> 16) & 0x0f));
			wsc->sc_msr_proto = WE_MSR_POW;
		} else
#endif
			wsc->sc_msr_proto = (wsc->sc_maddr >> 13) &
			    WE_MSR_ADDR;

		sc->cr_proto = ED_CR_RD2;
	}

	bus_space_write_1(wsc->sc_asict, wsc->sc_asich, WE_MSR,
	    wsc->sc_msr_proto | WE_MSR_MENB);
	WE_DELAY(wsc);

	/*
	 * DCR gets:
	 *
	 *	FIFO threshold to 8, No auto-init Remote DMA,
	 *	byte order=80x86.
	 *
	 * 16-bit cards also get word-wide DMA transfers.
	 */
	sc->dcr_reg = ED_DCR_FT1 | ED_DCR_LS |
	    (wsc->sc_flags & WE_16BIT_ENABLE ? ED_DCR_WTS : 0);

	sc->test_mem = we_test_mem;
	sc->ring_copy = we_ring_copy;
	sc->write_mbuf = we_write_mbuf;
	sc->read_hdr = we_read_hdr;
	sc->recv_int = we_recv_int;
	sc->init_card = we_init_card;

	sc->sc_mediachange = we_mediachange;
	sc->sc_mediastatus = we_mediastatus;

	sc->mem_start = 0;
	/* sc->mem_size has to be set by frontend */

	sc->sc_flags = device_cfdata(self)->cf_flags;

	/* Do generic parts of attach. */
	if (wsc->sc_type & WE_SOFTCONFIG)
		sc->sc_media_init = we_media_init;
	else
		sc->sc_media_init = dp8390_media_init;
	if (dp8390_config(sc)) {
		aprint_error_dev(self, "configuration failed\n");
		return 1;
	}

	/*
	 * Disable 16-bit access to shared memory - we leave it disabled
	 * so that:
	 *
	 *	(1) machines reboot properly when the board is set to
	 *	    16-bit mode and there are conflicting 8-bit devices
	 *	    within the same 128k address space as this board's
	 *	    shared memory, and
	 *
	 *	(2) so that other 8-bit devices with shared memory
	 *	    in this same 128k address space will work.
	 */
	WE_MEM_DISABLE(wsc);

	return 0;
}

static int
we_test_mem(struct dp8390_softc *sc)
{
	struct we_softc *wsc = (struct we_softc *)sc;
	bus_space_tag_t memt = sc->sc_buft;
	bus_space_handle_t memh = sc->sc_bufh;
	bus_size_t memsize = sc->mem_size;
	int i;

	if (wsc->sc_flags & WE_16BIT_ENABLE)
		bus_space_set_region_2(memt, memh, 0, 0, memsize >> 1);
	else
		bus_space_set_region_1(memt, memh, 0, 0, memsize);

	if (wsc->sc_flags & WE_16BIT_ENABLE) {
		for (i = 0; i < memsize; i += 2) {
			if (bus_space_read_2(memt, memh, i) != 0)
				goto fail;
		}
	} else {
		for (i = 0; i < memsize; i++) {
			if (bus_space_read_1(memt, memh, i) != 0)
				goto fail;
		}
	}

	return 0;

 fail:
	aprint_error_dev(sc->sc_dev,
	    "failed to clear shared memory at offset 0x%x\n", i);
	WE_MEM_DISABLE(wsc);
	return 1;
}

/*
 * Given a NIC memory source address and a host memory destination address,
 * copy 'len' from NIC to host using shared memory.  The 'len' is rounded
 * up to a word - ok as long as mbufs are word-sized.
 */
static inline void
we_readmem(struct we_softc *wsc, int from, uint8_t *to, int len)
{
	bus_space_tag_t memt = wsc->sc_dp8390.sc_buft;
	bus_space_handle_t memh = wsc->sc_dp8390.sc_bufh;

	if (len & 1)
		++len;

	if (wsc->sc_flags & WE_16BIT_ENABLE)
		bus_space_read_region_stream_2(memt, memh, from,
		    (uint16_t *)to, len >> 1);
	else
		bus_space_read_region_1(memt, memh, from,
		    to, len);
}

static int
we_write_mbuf(struct dp8390_softc *sc, struct mbuf *m, int buf)
{
	struct we_softc *wsc = (struct we_softc *)sc;
	bus_space_tag_t memt = wsc->sc_dp8390.sc_buft;
	bus_space_handle_t memh = wsc->sc_dp8390.sc_bufh;
	uint8_t *data, savebyte[2];
	int savelen, len, leftover;
#ifdef DIAGNOSTIC
	uint8_t *lim;
#endif

	savelen = m->m_pkthdr.len;

	WE_MEM_ENABLE(wsc);

	/*
	 * 8-bit boards are simple; no alignment tricks are necessary.
	 */
	if ((wsc->sc_flags & WE_16BIT_ENABLE) == 0) {
		for (; m != NULL; buf += m->m_len, m = m->m_next)
			bus_space_write_region_1(memt, memh,
			    buf, mtod(m, uint8_t *), m->m_len);
		if (savelen < ETHER_MIN_LEN - ETHER_CRC_LEN) {
			bus_space_set_region_1(memt, memh,
			    buf, 0, ETHER_MIN_LEN - ETHER_CRC_LEN - savelen);
			savelen = ETHER_MIN_LEN - ETHER_CRC_LEN;
		}
		goto out;
	}

	/* Start out with no leftover data. */
	leftover = 0;
	savebyte[0] = savebyte[1] = 0;

	for (; m != NULL; m = m->m_next) {
		len = m->m_len;
		if (len == 0)
			continue;
		data = mtod(m, uint8_t *);
#ifdef DIAGNOSTIC
		lim = data + len;
#endif
		while (len > 0) {
			if (leftover) {
				/*
				 * Data left over (from mbuf or realignment).
				 * Buffer the next byte, and write it and
				 * the leftover data out.
				 */
				savebyte[1] = *data++;
				len--;
				bus_space_write_stream_2(memt, memh, buf,
				    *(uint16_t *)savebyte);
				buf += 2;
				leftover = 0;
			} else if (BUS_SPACE_ALIGNED_POINTER(data, uint16_t)
			    == 0) {
				/*
				 * Unaligned dta; buffer the next byte.
				 */
				savebyte[0] = *data++;
				len--;
				leftover = 1;
			} else {
				/*
				 * Aligned data; output contiguous words as
				 * much as we can, then buffer the remaining
				 * byte, if any.
				 */
				leftover = len & 1;
				len &= ~1;
				bus_space_write_region_stream_2(memt, memh,
				    buf, (uint16_t *)data, len >> 1);
				data += len;
				buf += len;
				if (leftover)
					savebyte[0] = *data++;
				len = 0;
			}
		}
		if (len < 0)
			panic("we_write_mbuf: negative len");
#ifdef DIAGNOSTIC
		if (data != lim)
			panic("we_write_mbuf: data != lim");
#endif
	}
	if (leftover) {
		savebyte[1] = 0;
		bus_space_write_stream_2(memt, memh, buf,
		    *(uint16_t *)savebyte);
		buf += 2;
	}
	if (savelen < ETHER_MIN_LEN - ETHER_CRC_LEN) {
		bus_space_set_region_2(memt, memh,
		    buf, 0, (ETHER_MIN_LEN - ETHER_CRC_LEN - savelen) >> 1);
		savelen = ETHER_MIN_LEN - ETHER_CRC_LEN;
	}

 out:
	WE_MEM_DISABLE(wsc);

	return savelen;
}

static int
we_ring_copy(struct dp8390_softc *sc, int src, void *dstv, u_short amount)
{
	uint8_t *dst = dstv;
	struct we_softc *wsc = (struct we_softc *)sc;
	u_short tmp_amount;

	/* Does copy wrap to lower addr in ring buffer? */
	if (src + amount > sc->mem_end) {
		tmp_amount = sc->mem_end - src;

		/* Copy amount up to end of NIC memory. */
		we_readmem(wsc, src, dst, tmp_amount);

		amount -= tmp_amount;
		src = sc->mem_ring;
		dst += tmp_amount;
	}

	we_readmem(wsc, src, dst, amount);

	return src + amount;
}

static void
we_read_hdr(struct dp8390_softc *sc, int packet_ptr,
    struct dp8390_ring *packet_hdrp)
{
	struct we_softc *wsc = (struct we_softc *)sc;

	we_readmem(wsc, packet_ptr, (uint8_t *)packet_hdrp,
	    sizeof(struct dp8390_ring));
#if BYTE_ORDER == BIG_ENDIAN
	packet_hdrp->count = bswap16(packet_hdrp->count);
#endif
}

static void
we_recv_int(struct dp8390_softc *sc)
{
	struct we_softc *wsc = (struct we_softc *)sc;

	WE_MEM_ENABLE(wsc);
	dp8390_rint(sc);
	WE_MEM_DISABLE(wsc);
}

static void
we_media_init(struct dp8390_softc *sc)
{
	struct we_softc *wsc = (struct we_softc *)sc;
	int defmedia = IFM_ETHER;
	uint8_t x;

	if (sc->is790) {
		x = bus_space_read_1(wsc->sc_asict, wsc->sc_asich, WE790_HWR);
		bus_space_write_1(wsc->sc_asict, wsc->sc_asich, WE790_HWR,
		    x | WE790_HWR_SWH);
		if (bus_space_read_1(wsc->sc_asict, wsc->sc_asich, WE790_GCR) &
		    WE790_GCR_GPOUT)
			defmedia |= IFM_10_2;
		else
			defmedia |= IFM_10_5;
		bus_space_write_1(wsc->sc_asict, wsc->sc_asich, WE790_HWR,
		    x & ~WE790_HWR_SWH);
	} else {
		x = bus_space_read_1(wsc->sc_asict, wsc->sc_asich, WE_IRR);
		if (x & WE_IRR_OUT2)
			defmedia |= IFM_10_2;
		else
			defmedia |= IFM_10_5;
	}

	ifmedia_init(&sc->sc_media, 0, dp8390_mediachange, dp8390_mediastatus);
	ifmedia_add(&sc->sc_media, IFM_ETHER | IFM_10_2, 0, NULL);
	ifmedia_add(&sc->sc_media, IFM_ETHER | IFM_10_5, 0, NULL);
	ifmedia_set(&sc->sc_media, defmedia);
}

static int
we_mediachange(struct dp8390_softc *sc)
{

	/*
	 * Current media is already set up.  Just reset the interface
	 * to let the new value take hold.  The new media will be
	 * set up in we_init_card() called via dp8390_init().
	 */
	dp8390_reset(sc);
	return 0;
}

static void
we_mediastatus(struct dp8390_softc *sc, struct ifmediareq *ifmr)
{
	struct ifmedia *ifm = &sc->sc_media;

	/*
	 * The currently selected media is always the active media.
	 */
	ifmr->ifm_active = ifm->ifm_cur->ifm_media;
}

static void
we_init_card(struct dp8390_softc *sc)
{
	struct we_softc *wsc = (struct we_softc *)sc;
	struct ifmedia *ifm = &sc->sc_media;

	if (wsc->sc_init_hook)
		(*wsc->sc_init_hook)(wsc);

	we_set_media(wsc, ifm->ifm_cur->ifm_media);
}

static void
we_set_media(struct we_softc *wsc, int media)
{
	struct dp8390_softc *sc = &wsc->sc_dp8390;
	bus_space_tag_t asict = wsc->sc_asict;
	bus_space_handle_t asich = wsc->sc_asich;
	uint8_t hwr, gcr, irr;

	if (sc->is790) {
		hwr = bus_space_read_1(asict, asich, WE790_HWR);
		bus_space_write_1(asict, asich, WE790_HWR,
		    hwr | WE790_HWR_SWH);
		gcr = bus_space_read_1(asict, asich, WE790_GCR);
		if (IFM_SUBTYPE(media) == IFM_10_2)
			gcr |= WE790_GCR_GPOUT;
		else
			gcr &= ~WE790_GCR_GPOUT;
		bus_space_write_1(asict, asich, WE790_GCR,
		    gcr | WE790_GCR_LIT);
		bus_space_write_1(asict, asich, WE790_HWR,
		    hwr & ~WE790_HWR_SWH);
		return;
	}

	irr = bus_space_read_1(wsc->sc_asict, wsc->sc_asich, WE_IRR);
	if (IFM_SUBTYPE(media) == IFM_10_2)
		irr |= WE_IRR_OUT2;
	else
		irr &= ~WE_IRR_OUT2;
	bus_space_write_1(wsc->sc_asict, wsc->sc_asich, WE_IRR, irr);
}
