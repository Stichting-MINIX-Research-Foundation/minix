/* $NetBSD: xc5kvar.h,v 1.2 2011/07/09 15:00:44 jmcneill Exp $ */

/*-
 * Copyright (c) 2010 Jared D. McNeill <jmcneill@invisible.ca>
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

#ifndef _XC5KVAR_H
#define _XC5KVAR_H

#include <dev/i2c/i2cvar.h>
#include <dev/dtv/dtvio.h>
#include <dev/video_if.h>

typedef int (*xc5k_reset_cb)(void *);

struct xc5k {
	device_t	parent;
	i2c_tag_t	i2c;
	i2c_addr_t	i2c_addr;

	xc5k_reset_cb	reset;
	void		*reset_priv;

	unsigned int	if_freq;
	fe_type_t	fe_type;
};

struct xc5k_params {
	uint16_t		signal_source;
	uint32_t		frequency;
	enum video_standard	standard;
};

struct xc5k *	xc5k_open(device_t, i2c_tag_t, i2c_addr_t,
			  xc5k_reset_cb, void *, unsigned int, fe_type_t);
void		xc5k_close(struct xc5k *);
int		xc5k_tune_video(struct xc5k *, struct xc5k_params *);
int		xc5k_tune_dtv(struct xc5k *, const struct dvb_frontend_parameters *);
fe_status_t	xc5k_get_status(struct xc5k *);

#endif /* !_XC5KVAR_H */
