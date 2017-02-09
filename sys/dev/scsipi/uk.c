/*	$NetBSD: uk.c,v 1.62 2014/07/25 08:10:39 dholland Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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

/*
 * Dummy driver for a device we can't identify.
 * Originally by Julian Elischer (julian@tfs.com)
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uk.c,v 1.62 2014/07/25 08:10:39 dholland Exp $");

#include <sys/param.h>
#include <sys/systm.h>

#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/vnode.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

struct uk_softc {
	device_t sc_dev;

	struct scsipi_periph *sc_periph; /* all the inter level info */
};

static int	ukmatch(device_t, cfdata_t, void *);
static void	ukattach(device_t, device_t, void *);
static int	ukdetach(device_t, int);

CFATTACH_DECL_NEW(
    uk,
    sizeof(struct uk_softc),
    ukmatch,
    ukattach,
    ukdetach,
    NULL
);

extern struct cfdriver uk_cd;

static dev_type_open(ukopen);
static dev_type_close(ukclose);
static dev_type_ioctl(ukioctl);

const struct cdevsw uk_cdevsw = {
	.d_open = ukopen,
	.d_close = ukclose,
	.d_read = noread,
	.d_write = nowrite,
	.d_ioctl = ukioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER
};

static int
ukmatch(device_t parent, cfdata_t match, void *aux)
{
	return 1;
}

/*
 * The routine called by the low level scsi routine when it discovers
 * a device suitable for this driver.
 */
static void
ukattach(device_t parent, device_t self, void *aux)
{
	struct uk_softc *uk = device_private(self);
	struct scsipibus_attach_args *sa = aux;
	struct scsipi_periph *periph = sa->sa_periph;

	SC_DEBUG(periph, SCSIPI_DB2, ("ukattach: "));
	uk->sc_dev = self;

	/* Store information needed to contact our base driver */
	uk->sc_periph = periph;
	periph->periph_dev = uk->sc_dev;

	printf("\n");
}

static int
ukdetach(device_t self, int flags)
{
	int cmaj, mn;

	/* locate the major number */
	cmaj = cdevsw_lookup_major(&uk_cdevsw);

	/* Nuke the vnodes for any open instances */
	mn = device_unit(self);
	vdevgone(cmaj, mn, mn, VCHR);

	return 0;
}

static int
ukopen(dev_t dev, int flag, int fmt, struct lwp *l)
{
	int unit, error;
	struct uk_softc *uk;
	struct scsipi_periph *periph;
	struct scsipi_adapter *adapt;

	unit = minor(dev);
	uk = device_lookup_private(&uk_cd, unit);
	if (uk == NULL)
		return ENXIO;

	periph = uk->sc_periph;
	adapt = periph->periph_channel->chan_adapter;

	SC_DEBUG(periph, SCSIPI_DB1,
	    ("ukopen: dev=0x%"PRIx64" (unit %d (of %d))\n", dev, unit,
		uk_cd.cd_ndevs));

	/* Only allow one at a time */
	if (periph->periph_flags & PERIPH_OPEN) {
		aprint_error_dev(uk->sc_dev, "already open\n");
		return EBUSY;
	}

	if ((error = scsipi_adapter_addref(adapt)) != 0)
		return error;
	periph->periph_flags |= PERIPH_OPEN;

	SC_DEBUG(periph, SCSIPI_DB3, ("open complete\n"));
	return 0;
}

static int
ukclose(dev_t dev, int flag, int fmt, struct lwp *l)
{
	struct uk_softc *uk = device_lookup_private(&uk_cd, minor(dev));
	struct scsipi_periph *periph = uk->sc_periph;
	struct scsipi_adapter *adapt = periph->periph_channel->chan_adapter;

	SC_DEBUG(uk->sc_periph, SCSIPI_DB1, ("closing\n"));

	scsipi_wait_drain(periph);

	scsipi_adapter_delref(adapt);
	periph->periph_flags &= ~PERIPH_OPEN;

	return 0;
}

/*
 * Perform special action on behalf of the user.
 * Only does generic scsi ioctls.
 */
static int
ukioctl(dev_t dev, u_long cmd, void *addr, int flag, struct lwp *l)
{
	struct uk_softc *uk = device_lookup_private(&uk_cd, minor(dev));

	return scsipi_do_ioctl(uk->sc_periph, dev, cmd, addr, flag, l);
}
