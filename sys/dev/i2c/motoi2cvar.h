/* $NetBSD: motoi2cvar.h,v 1.4 2011/04/17 15:14:59 phx Exp $ */

/*-
 * Copyright (c) 2007, 2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas.
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

#ifndef _DEV_I2C_MOTOI2CVAR_H_
#define _DEV_I2C_MOTOI2CVAR_H_

#include <dev/i2c/i2cvar.h>

struct motoi2c_softc;
typedef uint8_t (*motoi2c_iord_t)(struct motoi2c_softc *, bus_size_t);
typedef void (*motoi2c_iowr_t)(struct motoi2c_softc *, bus_size_t, uint8_t);

struct motoi2c_settings {
	uint8_t			i2c_adr;
	uint8_t			i2c_fdr;
	uint8_t			i2c_dfsrr;
};

struct motoi2c_softc {
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	struct i2c_controller	sc_i2c;
	kmutex_t		sc_buslock;
	motoi2c_iord_t		sc_iord;
	motoi2c_iowr_t		sc_iowr;
};

#define	MOTOI2C_ADR_DEFAULT	(0x7e << 1)
#define	MOTOI2C_FDR_DEFAULT	0x31	/* 3072 */
#define	MOTOI2C_DFSRR_DEFAULT	0x10

#ifdef _KERNEL
void	motoi2c_attach_common(device_t, struct motoi2c_softc *,
	    const struct motoi2c_settings *);
int	motoi2c_intr(void *);
#endif

#endif /* !_DEV_I2C_MOTOI2CVAR_H_ */
