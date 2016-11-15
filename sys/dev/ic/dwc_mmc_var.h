/* $NetBSD: dwc_mmc_var.h,v 1.4 2014/12/30 00:19:50 jmcneill Exp $ */

/*-
 * Copyright (c) 2014 Jared D. McNeill <jmcneill@invisible.ca>
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _DWC_MMC_VAR_H
#define _DWC_MMC_VAR_H

struct dwc_mmc_softc {
	device_t		sc_dev;
	bus_space_tag_t		sc_bst;
	bus_space_handle_t	sc_bsh;
	bus_dma_tag_t		sc_dmat;
	void			*sc_ih;
	unsigned int		sc_clock_freq;
	unsigned int		sc_clock_max;
	unsigned int		sc_fifo_depth;
	uint32_t		sc_flags;
#define DWC_MMC_F_USE_HOLD_REG	0x0001	/* set USE_HOLD_REG with every cmd */
#define DWC_MMC_F_PWREN_CLEAR	0x0002	/* clear POWER_ENABLE bit to enable */
#define DWC_MMC_F_FORCE_CLK	0x0004	/* update clk div with every cmd */
	int			(*sc_set_clkdiv)(struct dwc_mmc_softc *, int);

	device_t		sc_sdmmc_dev;
	kmutex_t		sc_intr_lock;
	kcondvar_t		sc_intr_cv;

	uint32_t		sc_intr_rint;
	u_int			sc_cur_freq;
};

void	dwc_mmc_init(struct dwc_mmc_softc *);
int	dwc_mmc_intr(void *);

#endif /* !_DWC_MMC_VAR_H */
