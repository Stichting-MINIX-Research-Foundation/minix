/* $NetBSD: dtvif.h,v 1.3 2011/08/09 01:42:24 jmcneill Exp $ */

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by Jared D. McNeill.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

#ifndef _DEV_DTV_DTVIF_H
#define _DEV_DTV_DTVIF_H

#include <sys/device.h>

#include <dev/dtv/dtvio.h>

#define	DTV_DEVICE_FRONTEND	0
#define	DTV_DEVICE_DEMUX	1
#define	DTV_DEVICE_DVR		2

#define	DTV_NUM_DEVICES		3

#define	DTVUNIT(x)		(minor(x) & 0x0f)
#define	DTVDEV(x)		((minor(x) & 0xf0) >> 4)

#define	ISDTVFRONTEND(x)	(DTVDEV((x)) == DTV_DEVICE_FRONTEND)
#define	ISDTVDEMUX(x)		(DTVDEV((x)) == DTV_DEVICE_DEMUX)
#define	ISDTVDVR(x)		(DTVDEV((x)) == DTV_DEVICE_DVR)

struct dtv_payload;

struct dtv_hw_if {
	void		(*get_devinfo)(void *, struct dvb_frontend_info *);

	int		(*open)(void *, int);
	void		(*close)(void *);
	int		(*set_tuner)(void *, const struct dvb_frontend_parameters *);
	fe_status_t	(*get_status)(void *);
	uint16_t	(*get_signal_strength)(void *);
	uint16_t	(*get_snr)(void *);
	int		(*start_transfer)(void *,
			    void (*)(void *, const struct dtv_payload *),
			    void *);
	int		(*stop_transfer)(void *);
};

struct dtv_attach_args {
	const struct dtv_hw_if *hw;
	void *priv;
};

struct dtv_payload {
	const uint8_t	*data;
	size_t		size;
};

static inline int
dtv_print(void *priv, const char *pnp)
{
	if (pnp)
		aprint_normal("dtv at %s", pnp);
	return UNCONF;
}

#endif /* !_DEV_DTV_DTVIF_H */
