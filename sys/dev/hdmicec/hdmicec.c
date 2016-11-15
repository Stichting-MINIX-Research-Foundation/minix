/* $NetBSD: hdmicec.c,v 1.1 2015/08/01 21:19:24 jmcneill Exp $ */

/*-
 * Copyright (c) 2015 Jared D. McNeill <jmcneill@invisible.ca>
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: hdmicec.c,v 1.1 2015/08/01 21:19:24 jmcneill Exp $");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/atomic.h>

#include <dev/hdmicec/hdmicec_if.h>
#include <dev/hdmicec/hdmicecio.h>

#define CEC_MAX_FRAMESIZE	16

struct hdmicec_softc {
	device_t	sc_dev;

	void *		sc_priv;
	const struct hdmicec_hw_if *sc_hwif;

	u_int		sc_busy;
};

static dev_type_open(hdmicec_open);
static dev_type_close(hdmicec_close);
static dev_type_read(hdmicec_read);
static dev_type_write(hdmicec_write);
static dev_type_ioctl(hdmicec_ioctl);
static dev_type_poll(hdmicec_poll);

static int	hdmicec_match(device_t, cfdata_t, void *);
static void	hdmicec_attach(device_t, device_t, void *);

const struct cdevsw hdmicec_cdevsw = {
	.d_open = hdmicec_open,
	.d_close = hdmicec_close,
	.d_read = hdmicec_read,
	.d_write = hdmicec_write,
	.d_ioctl = hdmicec_ioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = hdmicec_poll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_flag = D_MPSAFE
};

extern struct cfdriver hdmicec_cd;

CFATTACH_DECL_NEW(hdmicec, sizeof(struct hdmicec_softc), hdmicec_match,
    hdmicec_attach, NULL, NULL);

static int
hdmicec_match(device_t parent, cfdata_t match, void *opaque)
{
	return 1;
}

static void
hdmicec_attach(device_t parent, device_t self, void *opaque)
{
	struct hdmicec_softc *sc = device_private(self);
	struct hdmicec_attach_args *caa = opaque;

	sc->sc_dev = self;
	sc->sc_priv = caa->priv;
	sc->sc_hwif = caa->hwif;

	aprint_naive("\n");
	aprint_normal("\n");
}

static int
hdmicec_open(dev_t dev, int flag, int fmt, lwp_t *l)
{
	struct hdmicec_softc *sc =
	    device_lookup_private(&hdmicec_cd, minor(dev));
	int error;

	if (sc == NULL)
		return ENXIO;

	if (atomic_cas_uint(&sc->sc_busy, 0, 1) != 1)
		return EBUSY;

	if (sc->sc_hwif->open != NULL) {
		error = sc->sc_hwif->open(sc->sc_priv, flag);
		if (error) {
			atomic_swap_uint(&sc->sc_busy, 0);
			return error;
		}
	}

	return 0;
}

static int
hdmicec_close(dev_t dev, int flag, int fmt, lwp_t *l)
{
	struct hdmicec_softc *sc =
	    device_lookup_private(&hdmicec_cd, minor(dev));

	if (sc->sc_hwif->close)
		sc->sc_hwif->close(sc->sc_priv);

	atomic_swap_uint(&sc->sc_busy, 0);
	return 0;
}

static int
hdmicec_read(dev_t dev, struct uio *uio, int flags)
{
	struct hdmicec_softc *sc =
	    device_lookup_private(&hdmicec_cd, minor(dev));
	uint8_t data[CEC_MAX_FRAMESIZE];
	ssize_t len;
	int error;

	if (uio->uio_resid < CEC_MAX_FRAMESIZE)
		return EINVAL;

	len = sc->sc_hwif->recv(sc->sc_priv, data, sizeof(data));
	if (len < 0)
		return EIO;

	error = uiomove(data, len, uio);
	if (error)
		return error;

	return 0;
}

static int
hdmicec_write(dev_t dev, struct uio *uio, int flags)
{
	struct hdmicec_softc *sc =
	    device_lookup_private(&hdmicec_cd, minor(dev));
	uint8_t data[CEC_MAX_FRAMESIZE];
	size_t len = uio->uio_resid;
	int error;

	if (len > CEC_MAX_FRAMESIZE)
		return EINVAL;

	error = uiomove(data, len, uio);
	if (error)
		return error;

	return sc->sc_hwif->send(sc->sc_priv, data, len);
}

static int
hdmicec_ioctl(dev_t dev, u_long cmd, void *data, int flag, lwp_t *l)
{
	struct hdmicec_softc *sc =
	    device_lookup_private(&hdmicec_cd, minor(dev));

	return sc->sc_hwif->ioctl(sc->sc_priv, cmd, data, flag, l);
}

static int
hdmicec_poll(dev_t dev, int events, lwp_t *l)
{
	struct hdmicec_softc *sc =
	    device_lookup_private(&hdmicec_cd, minor(dev));

	return sc->sc_hwif->poll(sc->sc_priv, events, l);
}
