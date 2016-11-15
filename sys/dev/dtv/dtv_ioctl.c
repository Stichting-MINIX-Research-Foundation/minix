/* $NetBSD: dtv_ioctl.c,v 1.3 2011/07/13 22:43:04 jmcneill Exp $ */

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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: dtv_ioctl.c,v 1.3 2011/07/13 22:43:04 jmcneill Exp $");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/kmem.h>
#include <sys/device.h>
#include <sys/select.h>

#include <dev/dtv/dtvvar.h>

int
dtv_frontend_ioctl(struct dtv_softc *sc, u_long cmd, void *data, int flags)
{
	switch (cmd) {
	case FE_READ_STATUS:
		*(fe_status_t *)data = dtv_device_get_status(sc);
		return 0;
	case FE_READ_BER:
		*(uint32_t *)data = 0;	/* XXX TODO */
		return 0;
	case FE_READ_SNR:
		*(uint16_t *)data = dtv_device_get_snr(sc);
		return 0;
	case FE_READ_SIGNAL_STRENGTH:
		*(uint16_t *)data = dtv_device_get_signal_strength(sc);
		return 0;
	case FE_SET_FRONTEND:
		return dtv_device_set_tuner(sc, data);
	case FE_GET_INFO:
		dtv_device_get_devinfo(sc, data);
		return 0;
	default:
		return EINVAL;
	}
}
