/* $NetBSD: radio.c,v 1.27 2014/07/25 08:10:35 dholland Exp $ */
/* $OpenBSD: radio.c,v 1.2 2001/12/05 10:27:06 mickey Exp $ */
/* $RuOBSD: radio.c,v 1.7 2001/12/04 06:03:05 tm Exp $ */

/*
 * Copyright (c) 2001 Maxim Tsyplakov <tm@oganer.net>
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
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* This is the /dev/radio driver from OpenBSD */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: radio.c,v 1.27 2014/07/25 08:10:35 dholland Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/vnode.h>
#include <sys/radioio.h>
#include <sys/conf.h>

#include <dev/radio_if.h>

struct radio_softc {
	void		*hw_hdl;	/* hardware driver handle */
	device_t 	sc_dev;		/* hardware device struct */
	const struct radio_hw_if *hw_if; /* hardware interface */
};

static int	radioprobe(device_t, cfdata_t, void *);
static void	radioattach(device_t, device_t, void *);
static int	radioprint(void *, const char *);
static int	radiodetach(device_t, int);

CFATTACH_DECL_NEW(radio, sizeof(struct radio_softc),
    radioprobe, radioattach, radiodetach, NULL);

static dev_type_open(radioopen);
static dev_type_close(radioclose);
static dev_type_ioctl(radioioctl);

const struct cdevsw radio_cdevsw = {
	.d_open = radioopen,
	.d_close = radioclose,
	.d_read = noread,
	.d_write = nowrite,
	.d_ioctl = radioioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER,
};

extern struct cfdriver radio_cd;

static int
radioprobe(device_t parent, cfdata_t match, void *aux)
{
	return (1);
}

static void
radioattach(device_t parent, device_t self, void *aux)
{
	struct radio_softc *sc = device_private(self);
	struct radio_attach_args *sa = aux;
	const struct radio_hw_if *hwp = sa->hwif;
	void  *hdlp = sa->hdl;

	aprint_naive("\n");
	aprint_normal("\n");
	sc->hw_if = hwp;
	sc->hw_hdl = hdlp;
	sc->sc_dev = self;
}

static int
radioopen(dev_t dev, int flags, int fmt, struct lwp *l)
{
	int	unit;
	struct radio_softc *sc;

	unit = RADIOUNIT(dev);
	sc = device_lookup_private(&radio_cd, unit);
	if (sc == NULL || sc->hw_if == NULL)
		return (ENXIO);

	if (sc->hw_if->open != NULL)
		return (sc->hw_if->open(sc->hw_hdl, flags, fmt, l->l_proc));
	else
		return (0);
}

static int
radioclose(dev_t dev, int flags, int fmt, struct lwp *l)
{
	struct radio_softc *sc;

	sc = device_lookup_private(&radio_cd, RADIOUNIT(dev));

	if (sc->hw_if->close != NULL)
		return (sc->hw_if->close(sc->hw_hdl, flags, fmt, l->l_proc));
	else
		return (0);
}

static int
radioioctl(dev_t dev, u_long cmd, void *data, int flags,
    struct lwp *l)
{
	struct radio_softc *sc;
	int unit, error;

	unit = RADIOUNIT(dev);
	sc = device_lookup_private(&radio_cd, unit);
	if (sc == NULL || sc->hw_if == NULL)
		return (ENXIO);

	error = EOPNOTSUPP;
	switch (cmd) {
	case RIOCGINFO:
		if (sc->hw_if->get_info)
			error = (sc->hw_if->get_info)(sc->hw_hdl,
					(struct radio_info *)data);
			break;
	case RIOCSINFO:
		if (sc->hw_if->set_info)
			error = (sc->hw_if->set_info)(sc->hw_hdl,
				(struct radio_info *)data);
		break;
	case RIOCSSRCH:
		if (sc->hw_if->search)
			error = (sc->hw_if->search)(sc->hw_hdl,
					*(int *)data);
		break;
	default:
		error = EINVAL;
	}

	return (error);
}

/*
 * Called from hardware driver. This is where the MI radio driver gets
 * probed/attached to the hardware driver
 */
device_t
radio_attach_mi(const struct radio_hw_if *rhwp, void *hdlp, device_t dev)
{
	struct radio_attach_args arg;

	arg.hwif = rhwp;
	arg.hdl = hdlp;
	return (config_found(dev, &arg, radioprint));
}

static int
radioprint(void *aux, const char *pnp)
{
	if (pnp != NULL)
		aprint_normal("radio at %s", pnp);
	return (UNCONF);
}

static int
radiodetach(device_t self, int flags)
{
	int maj, mn;

	/* locate the major number */
	maj = cdevsw_lookup_major(&radio_cdevsw);

	/* Nuke the vnodes for any open instances (calls close). */
	mn = device_unit(self);
	vdevgone(maj, mn, mn, VCHR);

	return (0);
}
