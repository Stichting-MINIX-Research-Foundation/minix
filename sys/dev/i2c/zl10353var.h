/*	$NetBSD: zl10353var.h,v 1.2 2011/08/29 11:16:36 jruoho Exp $ */

/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jukka Ruohonen.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#ifndef _DEV_I2C_ZL10353VAR_H_
#define _DEV_I2C_ZL10353VAR_H_

struct zl10353 {
	device_t	 zl_parent;
	const char	*zl_name;
	i2c_tag_t	 zl_i2c;
	i2c_addr_t	 zl_i2c_addr;
	uint32_t	 zl_clock;
	uint16_t	 zl_freq;
	uint8_t		 zl_bw;
};

struct zl10353	*zl10353_open(device_t, i2c_tag_t, i2c_addr_t);
void		 zl10353_close(struct zl10353 *);
void		 zl10353_get_devinfo(struct dvb_frontend_info *);
int		 zl10353_set_params(struct zl10353 *,
			const struct dvb_frontend_parameters *);
int		 zl10353_set_fsm(struct zl10353 *);
int		 zl10353_set_gate(void *, bool);
fe_status_t	 zl10353_get_status(struct zl10353 *);
uint16_t	 zl10353_get_signal_strength(struct zl10353 *);
uint16_t	 zl10353_get_snr(struct zl10353 *);

#endif	/* !_DEV_I2C_ZL10353VAR_H */
