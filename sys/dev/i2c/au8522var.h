/* $NetBSD: au8522var.h,v 1.4 2011/07/10 00:47:34 jmcneill Exp $ */

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

#ifndef _AU8522VAR_H
#define _AU8522VAR_H

#include <dev/i2c/i2cvar.h>
#include <dev/dtv/dtvio.h>

struct au8522 {
	device_t	parent;
	i2c_tag_t	i2c;
	i2c_addr_t	i2c_addr;

	unsigned int	if_freq;

	fe_modulation_t	current_modulation;
};

typedef enum {
	AU8522_VINPUT_UNCONF = -1,
	AU8522_VINPUT_NONE = 0,
	AU8522_VINPUT_CVBS,
	AU8522_VINPUT_CVBS_TUNER,
	AU8522_VINPUT_SVIDEO,
} au8522_vinput_t;

typedef enum {
	AU8522_AINPUT_UNCONF = -1,
	AU8522_AINPUT_NONE = 0,
	AU8522_AINPUT_SIF,
} au8522_ainput_t;

struct au8522 *	au8522_open(device_t, i2c_tag_t, i2c_addr_t, unsigned int);
void		au8522_close(struct au8522 *);

void		au8522_enable(struct au8522 *, bool);
void		au8522_set_input(struct au8522 *,
				 au8522_vinput_t, au8522_ainput_t);
int		au8522_get_signal(struct au8522 *);
void		au8522_set_audio(struct au8522 *, bool);
int		au8522_set_modulation(struct au8522 *, fe_modulation_t);
void		au8522_set_gate(struct au8522 *, bool);
fe_status_t	au8522_get_dtv_status(struct au8522 *);
uint16_t	au8522_get_snr(struct au8522 *);

#endif /* !_AU8522VAR_H */
