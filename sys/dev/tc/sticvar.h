/* 	$NetBSD: sticvar.h,v 1.19 2012/10/27 17:18:38 chs Exp $	*/

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

#ifndef _TC_STICVAR_H_
#define	_TC_STICVAR_H_

#if defined(pmax)
#define	STIC_KSEG_TO_PHYS(x)	MIPS_KSEG0_TO_PHYS(x)
#elif defined(alpha)
#define	STIC_KSEG_TO_PHYS(x)	ALPHA_K0SEG_TO_PHYS((vaddr_t)x)
#else
#error No support for your architecture
#endif

#define	STIC_MAXDV	5

struct stic_hwcmap256 {
#define	CMAP_SIZE	256	/* 256 R/G/B entries */
	u_int8_t	r[CMAP_SIZE];
	u_int8_t	g[CMAP_SIZE];
	u_int8_t	b[CMAP_SIZE];
};

struct stic_hwcursor64 {
	struct	wsdisplay_curpos cc_pos;
	struct	wsdisplay_curpos cc_hot;
	struct	wsdisplay_curpos cc_size;
#define	CURSOR_MAX_SIZE	64
	u_int8_t	cc_color[6];
	u_int64_t	cc_image[CURSOR_MAX_SIZE];
	u_int64_t	cc_mask[CURSOR_MAX_SIZE];
};

struct stic_screen {
	struct	stic_info *ss_si;
	u_int16_t	*ss_backing;
	int	ss_flags;
	int	ss_curx;
	int	ss_cury;
};

#define	SS_ALLOCED		0x01
#define	SS_ACTIVE		0x02
#define	SS_CURENB		0x04

struct stic_info {
	device_t	si_dv;

	u_int32_t	*si_stamp;
	u_int32_t	*si_buf;
	u_int32_t	*si_vdac;
	u_int32_t	*si_vdac_reset;
	volatile struct	stic_regs *si_stic;
	volatile struct stic_xcomm *si_sxc;

	u_int32_t	*(*si_pbuf_get)(struct stic_info *);
	int	(*si_pbuf_post)(struct stic_info *, u_int32_t *);
	int	(*si_ioctl)(struct stic_info *, u_long, void *, int,
			   struct lwp *);
	int	si_pbuf_select;
	int	si_hwflags;

	struct	stic_screen *si_curscreen;
	struct	wsdisplay_font *si_font;

	int	si_consw;
	int	si_consh;
	int	si_fontw;
	int	si_fonth;
	int	si_stamph;
	int	si_stamphm;
	int	si_stampw;
	int	si_depth;
	int	si_disptype;
	int	si_dispmode;
	int	si_unit;

	tc_addr_t si_slotbase;
	paddr_t	si_buf_phys;
	size_t	si_buf_size;

	int	si_flags;
	struct	stic_hwcursor64 si_cursor;
	struct	stic_hwcmap256 si_cmap;

	struct	callout si_switch_callout;
	void	(*si_switchcb)(void *, int, int);
	void	*si_switchcbarg;
};

#define	SI_CURENB_CHANGED	0x0001
#define	SI_CURCMAP_CHANGED	0x0002
#define	SI_CURSHAPE_CHANGED	0x0004
#define	SI_CMAP_CHANGED		0x0008
#define	SI_ALL_CHANGED		0x000f
#define	SI_DVOPEN		0x0010

void	stic_init(struct stic_info *);
void	stic_attach(device_t, struct stic_info *, int);
void	stic_cnattach(struct stic_info *);
void	stic_reset(struct stic_info *);
void	stic_flush(struct stic_info *);

extern struct stic_info stic_consinfo;

#endif	/* !_TC_STICVAR_H_ */
