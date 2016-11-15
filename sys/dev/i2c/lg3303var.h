/* $NetBSD: lg3303var.h,v 1.3 2011/07/15 20:28:38 jmcneill Exp $ */

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

#ifndef _LG3303VAR_H
#define _LG3303VAR_H

#include <dev/i2c/i2cvar.h>
#include <dev/dtv/dtvio.h>

#define	LG3303_CFG_SERIAL_INPUT	0x01

struct lg3303 {
	device_t	parent;
	i2c_tag_t	i2c;
	i2c_addr_t	i2c_addr;

	fe_modulation_t	current_modulation;

	int		flags;
};

struct lg3303 *	lg3303_open(device_t, i2c_tag_t, i2c_addr_t, int);
void		lg3303_close(struct lg3303 *);
int		lg3303_set_modulation(struct lg3303 *, fe_modulation_t);
fe_status_t	lg3303_get_dtv_status(struct lg3303 *);
uint16_t	lg3303_get_snr(struct lg3303 *);
uint16_t	lg3303_get_signal_strength(struct lg3303 *);
uint32_t	lg3303_get_ucblocks(struct lg3303 *);

#endif /* !_LG3303VAR_H */
