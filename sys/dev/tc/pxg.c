/* 	$NetBSD: pxg.c,v 1.35 2013/11/04 16:53:09 christos Exp $	*/

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
 * Driver for DEC PixelStamp graphics accelerators with onboard SRAM and
 * Intel i860 co-processor (PMAG-D, E and F).
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pxg.c,v 1.35 2013/11/04 16:53:09 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/callout.h>
#include <sys/proc.h>
#include <sys/kauth.h>

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
#include <dev/tc/pxgvar.h>

#define	PXG_STIC_POLL_OFFSET	0x000000	/* STIC DMA poll space */
#define	PXG_STAMP_OFFSET	0x0c0000	/* pixelstamp space on STIC */
#define	PXG_STIC_OFFSET		0x180000	/* STIC registers */
#define	PXG_SRAM_OFFSET		0x200000	/* 128 or 256kB of SRAM */
#define	PXG_HOST_INTR_OFFSET	0x280000	/* i860 host interrupt */
#define	PXG_COPROC_INTR_OFFSET	0x2c0000	/* i860 coprocessor interrupt */
#define	PXG_VDAC_OFFSET		0x300000	/* VDAC registers (bt459) */
#define	PXG_VDAC_RESET_OFFSET	0x340000	/* VDAC reset register */
#define	PXG_ROM_OFFSET		0x380000	/* ROM code */
#define	PXG_I860_START_OFFSET	0x380000	/* i860 start register */
#define	PXG_I860_RESET_OFFSET	0x3c0000	/* i860 stop register */

static void	pxg_attach(device_t, device_t, void *);
static int	pxg_intr(void *);
static int	pxg_match(device_t, cfdata_t, void *);

static void	pxg_init(struct stic_info *);
static int	pxg_ioctl(struct stic_info *, u_long, void *, int, struct lwp *);
static uint32_t	*pxg_pbuf_get(struct stic_info *);
static int	pxg_pbuf_post(struct stic_info *, uint32_t *);
static int	pxg_probe_planes(struct stic_info *);
static int	pxg_probe_sram(struct stic_info *);

void	pxg_cnattach(tc_addr_t);

struct pxg_softc {
	struct	stic_info *pxg_si;
};

CFATTACH_DECL_NEW(pxg, sizeof(struct pxg_softc),
    pxg_match, pxg_attach, NULL, NULL);

static const char *pxg_types[] = {
	"PMAG-DA ",
	"PMAG-FA ",
	"PMAG-FB ",
	"PMAGB-FA",
	"PMAGB-FB",
};

static int
pxg_match(device_t parent, cfdata_t match, void *aux)
{
	struct tc_attach_args *ta;
	int i;

	ta = aux;

	for (i = 0; i < sizeof(pxg_types) / sizeof(pxg_types[0]); i++)
		if (strncmp(pxg_types[i], ta->ta_modname, TC_ROM_LLEN) == 0)
			return (1);

	return (0);
}

static void
pxg_attach(device_t parent, device_t self, void *aux)
{
	struct stic_info *si;
	struct tc_attach_args *ta;
	struct pxg_softc *pxg;
	int console;

	pxg = device_private(self);
	ta = (struct tc_attach_args *)aux;

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
		pxg_init(si);
		console = 0;
	}

	pxg->pxg_si = si;
	si->si_dv = self;
	tc_intr_establish(parent, ta->ta_cookie, IPL_TTY, pxg_intr, si);

	printf(": %d plane, %dx%d stamp, %dkB SRAM\n", si->si_depth,
	    si->si_stampw, si->si_stamph, (int)si->si_buf_size >> 10);

	stic_attach(self, si, console);

#ifdef notyet
	/* Load the co-processor "firmware". */
	for (i = 0; i < sizeof(pxg_fwsegs) / sizeof(pxg_fwsegs[0]); i++)
		pxg_load_fwseg(si, &pxg_fwsegs[i]);

	/* Start the i860. */
	si->si_slotbase[PXG_I860_START_OFFSET >> 2] = 1;
	tc_wmb();
	tc_syncbus();
	DELAY(40000);
#endif
}

void
pxg_cnattach(tc_addr_t addr)
{
	struct stic_info *si;

	si = &stic_consinfo;
	si->si_slotbase = addr;
	pxg_init(si);
	stic_cnattach(si);
}

static void
pxg_init(struct stic_info *si)
{
	volatile uint32_t *slot;
	char *kva;

	kva = (void *)si->si_slotbase;

	si->si_vdac = (uint32_t *)(kva + PXG_VDAC_OFFSET);
	si->si_vdac_reset = (uint32_t *)(kva + PXG_VDAC_RESET_OFFSET);
	si->si_stic = (volatile struct stic_regs *)(kva + PXG_STIC_OFFSET);
	si->si_stamp = (uint32_t *)(kva + PXG_STAMP_OFFSET);
	si->si_buf = (uint32_t *)(kva + PXG_SRAM_OFFSET);
	si->si_buf_phys = STIC_KSEG_TO_PHYS(si->si_buf);
	si->si_buf_size = pxg_probe_sram(si);
	si->si_disptype = WSDISPLAY_TYPE_PXG;
	si->si_sxc = (volatile struct stic_xcomm *)si->si_buf;

	si->si_pbuf_get = pxg_pbuf_get;
	si->si_pbuf_post = pxg_pbuf_post;
	si->si_ioctl = pxg_ioctl;

	/* Disable the co-processor. */
	slot = (volatile uint32_t *)kva;
	slot[PXG_I860_RESET_OFFSET >> 2] = 0;
	tc_wmb();
	slot[PXG_HOST_INTR_OFFSET >> 2] = 0;
	tc_wmb();
	tc_syncbus();
	DELAY(40000);

	/* XXX Check for a second PixelStamp. */
	if (((si->si_stic->sr_modcl & 0x600) >> 9) > 1)
		si->si_depth = 24;
	else
		si->si_depth = pxg_probe_planes(si);

	stic_init(si);
}

static int
pxg_probe_sram(struct stic_info *si)
{
	volatile uint32_t *a, *b;

	a = (volatile uint32_t *)si->si_slotbase + (PXG_SRAM_OFFSET >> 2);
	b = a + (0x20000 >> 2);
	*a = 4321;
	*b = 1234;
	tc_mb();
	return ((*a == *b) ? 0x20000 : 0x40000);
}

static int
pxg_probe_planes(struct stic_info *si)
{
	volatile uint32_t *vdac;
	int id;

	/*
	 * For the visible framebuffer (# 0), we can cheat and use the VDAC
	 * ID.
	 */
	vdac = si->si_vdac;
	vdac[BT459_REG_ADDR_LOW] = (BT459_IREG_ID & 0xff) |
	    ((BT459_IREG_ID & 0xff) << 8) | ((BT459_IREG_ID & 0xff) << 16);
	vdac[BT459_REG_ADDR_HIGH] = ((BT459_IREG_ID & 0xff00) >> 8) |
	    (BT459_IREG_ID & 0xff00) | ((BT459_IREG_ID & 0xff00) << 8);
	tc_mb();
	id = vdac[BT459_REG_IREG_DATA] & 0x00ffffff;

	/* 3 VDACs */
	if (id == 0x004a4a4a)
		return (24);

	/* 1 VDAC */
	if ((id & 0xff0000) == 0x4a0000 || (id & 0x00ff00) == 0x004a00 ||
	    (id & 0x0000ff) == 0x00004a)
		return (8);

	/* XXX Assume 8 planes. */
	printf("pxg_probe_planes: invalid VDAC ID %x\n", id);
	return (8);
}

static int
pxg_intr(void *cookie)
{
#ifdef notyet
	struct stic_info *si;
	volatile struct stic_regs *sr;
	volatile uint32_t *hi;
	uint32_t state;
	int it;

	si = cookie;
	sr = si->si_stic;
	state = sr->sr_ipdvint;
	hi = (volatile uint32_t *)si->si_slotbase +
	    (PXG_HOST_INTR_OFFSET / sizeof(uint32_t));

	/* Clear the interrupt condition */
	it = hi[0] & 15;
	hi[0] = 0;
	tc_wmb();
	hi[2] = 0;
	tc_wmb();

	switch (it) {
	case 3:
		sr->sr_ipdvint = STIC_INT_V_WE | STIC_INT_V_EN;
		tc_wmb();
		stic_flush(si);
		break;
	}
#else
	printf("pxg_intr: how did this happen?\n");
#endif
	return (1);
}

static uint32_t *
pxg_pbuf_get(struct stic_info *si)
{
	u_long off;

	si->si_pbuf_select ^= STIC_PACKET_SIZE;
	off = si->si_pbuf_select + STIC_XCOMM_SIZE;
	return ((uint32_t *)((char *)si->si_buf + off));
}

static int
pxg_pbuf_post(struct stic_info *si, uint32_t *buf)
{
	volatile uint32_t *poll, junk;
	volatile struct stic_regs *sr;
	u_long v;
	int c;

	sr = si->si_stic;

	/* Get address of poll register for this buffer. */
	v = ((u_long)buf - (u_long)si->si_buf) >> 9;
	poll = (volatile uint32_t *)((char *)si->si_slotbase + v);

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
pxg_ioctl(struct stic_info *si, u_long cmd, void *data, int flag,
	  struct lwp *l)
{
	struct stic_xinfo *sxi;
	volatile uint32_t *ptr = NULL;
	int rv, s;

	switch (cmd) {
	case STICIO_START860:
	case STICIO_RESET860:
		if ((rv = kauth_authorize_machdep(l->l_cred,
		    KAUTH_MACHDEP_PXG, KAUTH_ARG(cmd == STICIO_START860 ? 1 : 0),
		    NULL, NULL, NULL)) != 0)
			return (rv);
		if (si->si_dispmode != WSDISPLAYIO_MODE_MAPPED)
			return (EBUSY);
		ptr = (volatile uint32_t *)si->si_slotbase;
		break;
	}

	switch (cmd) {
	case STICIO_START860:
		s = spltty();
		ptr[PXG_I860_START_OFFSET >> 2] = 1;
		tc_wmb();
		splx(s);
		rv = 0;
		break;

	case STICIO_RESET860:
		s = spltty();
		ptr[PXG_I860_RESET_OFFSET >> 2] = 0;
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
		sxi->sxi_buf_phys = 0;
		sxi->sxi_buf_pktoff = STIC_XCOMM_SIZE;
		sxi->sxi_buf_pktcnt = 2;
		sxi->sxi_buf_imgoff = STIC_XCOMM_SIZE + STIC_PACKET_SIZE * 2;
		rv = 0;
		break;

	default:
		rv = EPASSTHROUGH;
		break;
	}

	return (rv);
}

#ifdef notyet
void
pxg_load_fwseg(struct stic_info *si, struct pxg_fwseg *pfs)
{
	const uint32_t *src;
	uint32_t *dst;
	u_int left, i;

	dst = (uint32_t *)((void *)si->si_buf + pfs->pfs_addr);
	src = pfs->pfs_data;

	for (left = pfs->pfs_compsize; left != 0; left -= 4) {
		if (src[0] == PXGFW_RLE_MAGIC) {
			for (i = src[2]; i != 0; i--)
				*dst++ = src[1];
			src += 3;
		} else {
			*dst++ = src[0];
			src++;
		}
	}

	if (src == NULL)
		memset(dst, 0, pfs->pfs_realsize);
}
#endif
