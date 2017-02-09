/* $NetBSD: elantechvar.h,v 1.2 2012/01/07 10:27:58 jmcneill Exp $ */

/*-
 * Copyright (c) 2008 Jared D. McNeill <jmcneill@invisible.ca>
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

#ifndef _DEV_PCKBCPORT_ELANTECHVAR_H
#define _DEV_PCKBCPORT_ELANTECHVAR_H

struct elantech_softc {
	uint32_t	version;

	bool		initializing;
	int16_t		last_x, last_y, last_z;
	int		last_nfingers;
};

/* minimum versions supported by this driver */
#define ELANTECH_MIN_VERSION	0x0230

#define ELANTECH_MAGIC(magic)	\
	((magic)[0] == 0x3c && (magic)[1] == 0x03 && (magic)[2] == 0xc8)

int	pms_elantech_probe_init(void *);
void	pms_elantech_enable(void *);
void	pms_elantech_resume(void *);

#endif /* !_DEV_PCKBCPORT_ELANTECHVAR_H */
