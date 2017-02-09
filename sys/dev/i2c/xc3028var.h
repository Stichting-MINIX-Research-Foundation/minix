/* $NetBSD: xc3028var.h,v 1.1 2011/07/11 18:00:06 jmcneill Exp $ */

/*-
 * Copyright (c) 2011 Jared D. McNeill <jmcneill@invisible.ca>
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

#ifndef _XC3028VAR_H
#define _XC3028VAR_H

#include <dev/i2c/i2cvar.h>
#include <dev/dtv/dtvio.h>

typedef int (*xc3028_reset_cb)(void *);

enum xc3028_type {
	XC3028,
	XC3028L,
};

struct xc3028_fw {
	uint32_t	type;
	uint64_t	id;
	uint16_t	int_freq;
	uint8_t		*data;
	uint32_t	data_size;
};

struct xc3028 {
	device_t	parent;
	i2c_tag_t	i2c;
	i2c_addr_t	i2c_addr;

	xc3028_reset_cb reset;
	void		*reset_priv;

	enum xc3028_type type;

	struct xc3028_fw *fw;
	unsigned int	nfw;
};

struct xc3028 *	xc3028_open(device_t, i2c_tag_t, i2c_addr_t,
		    xc3028_reset_cb, void *, enum xc3028_type);
void		xc3028_close(struct xc3028 *);
int		xc3028_tune_dtv(struct xc3028 *,
		    const struct dvb_frontend_parameters *);

#endif /* !_XC3028VAR_H */
