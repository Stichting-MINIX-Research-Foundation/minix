/* 	$NetBSD: px.c,v 1.40 2013/11/04 16:53:09 christos Exp $	*/

/*-
 * Copyright (c) 1999, 2000, 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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
 * Driver for DEC PixelStamp graphics adapters (PMAG-C).
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: px.c,v 1.40 2013/11/04 16:53:09 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/callout.h>

#include <uvm/uvm.h>	/* XXX uvm_pageboot_alloc */

#if defined(pmax)
#include <mips/cpuregs.h>
#elif defined(alpha)
#include <alpha/alpha_cpu.h>
#endif

#include <machine/autoconf.h>
#include <sys/cpu.h>
#include <sys/bus.h>

#include <dev/cons.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>

#include <dev/ic/bt459reg.h>

#include <dev/tc/tcvar.h>
#include <dev/tc/sticreg.h>
#include <dev/tc/sticio.h>
#include <dev/tc/sticvar.h>

#define	PX_STIC_POLL_OFFSET	0x000000	/* STIC DMA poll space */
#define	PX_STAMP_OFFSET		0x0c0000	/* pixelstamp space on STIC */
#define	PX_STIC_OFFSET		0x180000	/* STIC registers */
#define	PX_VDAC_OFFSET		0x200000	/* VDAC registers (bt459) */
#define	PX_VDAC_RESET_OFFSET	0x300000	/* VDAC reset register */
#define	PX_ROM_OFFSET		0x300000	/* ROM code */

#define	PX_BUF_COUNT		16
#define	PX_BUF_INC(x)		((x + 1) & (PX_BUF_COUNT - 1))

/*
 * We need enough aligned memory to hold:
 *
 * - Xserver communication area (4096 bytes)
 * - 16 packet buffers (4096 bytes each)
 * - 2 image buffers (5120 bytes each)
 *
 */
#define	PX_BUF_SIZE		\
    (STIC_PACKET_SIZE * PX_BUF_COUNT + STIC_IMGBUF_SIZE*2 + STIC_XCOMM_SIZE)
#define	PX_BUF_ALIGN		32768

#define	PXF_QUEUE	0x01

static void	px_attach(device_t, device_t, void *);
static void	px_init(struct stic_info *, int);
static int	px_ioctl(struct stic_info *, u_long, void *, int,
			 struct lwp *);
static int	px_match(device_t, cfdata_t, void *);

static int	px_intr(void *);
static uint32_t	*px_pbuf_get(struct stic_info *);
static int	px_pbuf_post(struct stic_info *, uint32_t *);

void	px_cnattach(tc_addr_t);

struct px_softc {
	device_t px_dev;
	struct	stic_info *px_si;
	volatile uint32_t	*px_qpoll[PX_BUF_COUNT];
};

CFATTACH_DECL_NEW(px, sizeof(struct px_softc),
    px_match, px_attach, NULL, NULL);

static int
px_match(device_t parent, cfdata_t match, void *aux)
{
	struct tc_attach_args *ta;

	ta = aux;

	return (strncmp("PMAG-CA ", ta->ta_modname, TC_ROM_LLEN) == 0);
}

static void
px_attach(device_t parent, device_t self, void *aux)
{
	struct stic_info *si;
	struct tc_attach_args *ta;
	struct px_softc *px;
	int console, i;
	u_long v;

	px = device_private(self);
	ta = aux;

	px->px_dev = self;

	if (ta->ta_addr == stic_consinfo.si_slotbase) {
		si = &stic_consinfo;
		console = 1;
	} else {
		if (stic_consinfo.si_slotbase == 0)
			si = &stic_consinfo;
		else {
			si = malloc(sizeof(*si), M_DEVBUF, M_NOWAIT|M_ZERO);
		}
		si->si_slotbase = ta->ta_addr;
		px_init(si, 0);
		console = 0;
	}

	px->px_si = si;
	si->si_dv = self;
	tc_intr_establish(parent, ta->ta_cookie, IPL_TTY, px_intr, si);

	printf(": 8 plane, %dx%d stamp\n", si->si_stampw, si->si_stamph);

	for (i = 0; i < PX_BUF_COUNT; i++) {
		v = i * STIC_PACKET_SIZE +
		    si->si_buf_phys + STIC_XCOMM_SIZE;
		v = ((v & 0xffff8000) << 3) | (v & 0x7fff);
		px->px_qpoll[i] = (volatile uint32_t *)
		    ((char *)si->si_slotbase + (v >> 9));
	}

	stic_attach(self, si, console);
}

void
px_cnattach(tc_addr_t addr)
{
	struct stic_info *si;

	si = &stic_consinfo;
	si->si_slotbase = addr;
	px_init(si, 1);
	stic_cnattach(si);
}

static void
px_init(struct stic_info *si, int bootstrap)
{
	struct pglist pglist;
	char *kva, *bva;
	paddr_t bpa;

	kva = (void *)si->si_slotbase;

	/*
	 * Allocate memory for the packet buffers.  It must be located below
	 * 8MB, since the STIC can't access outside that region.  Also, due
	 * to the holes in STIC address space, each buffer mustn't cross a
	 * 32kB boundary.
	 */
	if (bootstrap) {
		/*
		 * UVM won't be initialised at this point, so grab memory
		 * directly from vm_physmem[].
		 */
		bva = (char *)uvm_pageboot_alloc(PX_BUF_SIZE + PX_BUF_ALIGN);
		bpa = (STIC_KSEG_TO_PHYS(bva) + PX_BUF_ALIGN - 1) &
		    ~(PX_BUF_ALIGN - 1);
		if (bpa + PX_BUF_SIZE > 8192*1024)
			panic("px_init: allocation out of bounds");
	} else {
		if (uvm_pglistalloc(PX_BUF_SIZE, 0, 8192*1024, PX_BUF_ALIGN,
		    0, &pglist, 1, 0) != 0)
			panic("px_init: allocation failure");
		bpa = VM_PAGE_TO_PHYS(TAILQ_FIRST(&pglist));
	}

	si->si_vdac = (uint32_t *)(kva + PX_VDAC_OFFSET);
	si->si_vdac_reset = (uint32_t *)(kva + PX_VDAC_RESET_OFFSET);
	si->si_stic = (volatile struct stic_regs *)(kva + PX_STIC_OFFSET);
	si->si_stamp = (uint32_t *)(kva + PX_STAMP_OFFSET);
	si->si_buf = (uint32_t *)TC_PHYS_TO_UNCACHED(bpa);
	si->si_buf_phys = bpa;
	si->si_buf_size = PX_BUF_SIZE;
	si->si_disptype = WSDISPLAY_TYPE_PX;
	si->si_depth = 8;
	si->si_sxc = (volatile struct stic_xcomm *)si->si_buf;

	si->si_pbuf_get = px_pbuf_get;
	si->si_pbuf_post = px_pbuf_post;
	si->si_ioctl = px_ioctl;

	memset(si->si_buf, 0, PX_BUF_SIZE);

	stic_init(si);
}

static int
px_intr(void *cookie)
{
	volatile struct stic_regs *sr;
	volatile struct stic_xcomm *sxc;
	struct stic_info *si;
	struct px_softc *px;
	int state;

	si = cookie;
	px = device_private(si->si_dv);
	sr = si->si_stic;
	state = sr->sr_ipdvint;
	sxc = si->si_sxc;

	/*
	 * Vertical-retrace condition.
	 *
	 * Clear the flag and flush out any waiting VDAC updates.  We do
	 * this at retrace time to avoid producing `shearing' and other
	 * nasty artifacts.
	 */
	if ((state & STIC_INT_V) != 0) {
		sr->sr_ipdvint = STIC_INT_V_WE | STIC_INT_V_EN;
		tc_wmb();
		stic_flush(si);
	}

	/*
	 * Error condition.
	 *
	 * Simply clear the flag and report the error.
	 */
	if ((state & STIC_INT_E) != 0) {
		aprint_error_dev(px->px_dev, "error intr, %x %x %x %x %x",
		    sr->sr_ipdvint, sr->sr_sticsr, sr->sr_buscsr,
		    sr->sr_busadr, sr->sr_busdat);
		sr->sr_ipdvint = STIC_INT_E_WE | STIC_INT_E_EN;
		tc_wmb();
	}

	/*
	 * Check for queue stalls.
	 */
	if (sxc->sxc_tail != sxc->sxc_head && !sxc->sxc_busy)
		state |= STIC_INT_P;

	/*
	 * Packet-done condition.
	 *
	 * If packet queueing is enabled, clear the condition, and increment
	 * the tail (submitted) pointer.
	 */
	if ((si->si_hwflags & PXF_QUEUE) != 0 && (state & STIC_INT_P) != 0) {
		sr->sr_ipdvint = STIC_INT_P_WE | STIC_INT_P_EN;
		tc_wmb();

		if (sxc->sxc_tail != sxc->sxc_head) {
			sxc->sxc_done[sxc->sxc_tail] = 0;
			sxc->sxc_tail = PX_BUF_INC(sxc->sxc_tail);
		}

		if (sxc->sxc_tail != sxc->sxc_head) {
			if (*px->px_qpoll[sxc->sxc_tail] != STAMP_OK) {
				sxc->sxc_nreject++;
				sxc->sxc_busy = 0;
			} else
				sxc->sxc_busy = 1;
		} else
			sxc->sxc_busy = 0;
	}

	if ((si->si_hwflags & PXF_QUEUE) != 0 && (state & STIC_INT_P_EN) == 0)
		printf("px_intr: STIC_INT_P_EN == 0\n");

	return (1);
}

static uint32_t *
px_pbuf_get(struct stic_info *si)
{
	u_long off;

	si->si_pbuf_select ^= STIC_PACKET_SIZE;
	off = si->si_pbuf_select + STIC_XCOMM_SIZE;
	return ((uint32_t *)((char *)si->si_buf + off));
}

static int
px_pbuf_post(struct stic_info *si, uint32_t *buf)
{
	volatile uint32_t *poll, junk;
	volatile struct stic_regs *sr;
	u_long v;
	int c;

	sr = si->si_stic;

	/* Get address of poll register for this buffer. */
	v = (u_long)STIC_KSEG_TO_PHYS(buf);
	v = ((v & 0xffff8000) << 3) | (v & 0x7fff);
	poll = (volatile uint32_t *)((char *)si->si_slotbase + (v >> 9));

	/*
	 * Read the poll register and make sure the stamp wants to accept
	 * our packet.  This read will initiate the DMA.  Don't wait for
	 * ever, just in case something's wrong.
	 */
	tc_mb();

	for (c = STAMP_RETRIES; c != 0; c--) {
		if ((sr->sr_ipdvint & STIC_INT_P) != 0) {
			sr->sr_ipdvint = STIC_INT_P_WE;
			tc_wmb();
			junk = *poll;
			__USE(junk);
			return (0);
		}
		DELAY(STAMP_DELAY);
	}

	/* STIC has lost the plot, punish it. */
	stic_reset(si);
	return (-1);
}

static int
px_ioctl(struct stic_info *si, u_long cmd, void *data, int flag,
	 struct lwp *l)
{
	volatile struct stic_xcomm *sxc;
	volatile struct stic_regs *sr;
	struct stic_xinfo *sxi;
	int rv, s;

	sr = si->si_stic;

	switch (cmd) {
	case STICIO_STARTQ:
		if (si->si_dispmode != WSDISPLAYIO_MODE_MAPPED ||
		    (si->si_hwflags & PXF_QUEUE) != 0) {
			rv = EBUSY;
			break;
		}

		sxc = si->si_sxc;
	 	memset((void *)__UNVOLATILE(sxc->sxc_done), 0,
			sizeof(sxc->sxc_done));
		sxc->sxc_head = 0;
		sxc->sxc_tail = 0;
		sxc->sxc_nreject = 0;
		sxc->sxc_nstall = 0;

		s = spltty();
		si->si_hwflags |= PXF_QUEUE;
		sr->sr_ipdvint = STIC_INT_P_WE | STIC_INT_P_EN;
		tc_wmb();
		splx(s);

		rv = 0;
		break;

	case STICIO_STOPQ:
		s = spltty();
		si->si_hwflags &= ~PXF_QUEUE;
		sr->sr_ipdvint = STIC_INT_P_WE | STIC_INT_P;
		tc_wmb();
		splx(s);
		rv = 0;
		break;

	case STICIO_GXINFO:
		sxi = (struct stic_xinfo *)data;
		sxi->sxi_unit = si->si_unit;
		sxi->sxi_stampw = si->si_stampw;
		sxi->sxi_stamph = si->si_stamph;
		sxi->sxi_buf_size = si->si_buf_size;
		sxi->sxi_buf_phys = (u_int)si->si_buf_phys;
		sxi->sxi_buf_pktoff = STIC_XCOMM_SIZE;
		sxi->sxi_buf_pktcnt = PX_BUF_COUNT;
		sxi->sxi_buf_imgoff =
		    STIC_XCOMM_SIZE + STIC_PACKET_SIZE * PX_BUF_COUNT;
		rv = 0;
		break;

	default:
		rv = EPASSTHROUGH;
		break;
	}

	return (rv);
}
