/* $NetBSD: altmem.c,v 1.5 2015/04/26 15:15:20 mlelstv Exp $ */

/*-
 * Copyright (c) 2009 Jared D. McNeill <jmcneill@invisible.ca>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
__KERNEL_RCSID(0, "$NetBSD: altmem.c,v 1.5 2015/04/26 15:15:20 mlelstv Exp $");
#include <sys/param.h>
#include <sys/types.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/disklabel.h>
#include <sys/disk.h>

#include <dev/altmem/altmemvar.h>

struct altmem_softc {
	device_t	sc_dev;

	struct disk	sc_dkdev;

	void		*sc_cookie;
	const struct altmem_memops *sc_memops;

	size_t		sc_size;
};

static dev_type_open(altmemopen);
static dev_type_close(altmemclose);
static dev_type_read(altmemread);
static dev_type_write(altmemwrite);
static dev_type_ioctl(altmemioctl);
static dev_type_strategy(altmemstrategy);
static dev_type_size(altmemsize);

static int	altmem_match(device_t, cfdata_t, void *);
static void	altmem_attach(device_t, device_t, void *);

const struct bdevsw altmem_bdevsw = {
        .d_open = altmemopen,
	.d_close = altmemclose,
	.d_strategy = altmemstrategy,
	.d_ioctl = altmemioctl,
	.d_dump = nodump,
	.d_psize = altmemsize,
	.d_discard = nodiscard,
	.d_flag = D_DISK
};
const struct cdevsw altmem_cdevsw = {
	.d_open = altmemopen,
	.d_close = altmemclose,
	.d_read = altmemread,
	.d_write = altmemwrite,
	.d_ioctl = altmemioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_DISK
};
static struct dkdriver altmemdkdriver = {
	.d_strategy = altmemstrategy,
	.d_minphys = minphys
};
extern struct cfdriver altmem_cd;

CFATTACH_DECL_NEW(altmem, sizeof(struct altmem_softc), altmem_match,
    altmem_attach, NULL, NULL);

static int
altmem_match(device_t parent, cfdata_t match, void *opaque)
{
	return 1;
}

static void
altmem_attach(device_t parent, device_t self, void *opaque)
{
	struct altmem_softc *sc = device_private(self);
	struct altmem_attach_args *aaa = opaque;
	char pbuf[9];

	sc->sc_dev = self;
	sc->sc_cookie = aaa->cookie;
	sc->sc_memops = aaa->memops;
	sc->sc_size = sc->sc_memops->getsize(sc->sc_cookie);

	format_bytes(pbuf, sizeof(pbuf), sc->sc_size);

	aprint_naive("\n");
	aprint_normal(": %s\n", pbuf);

	disk_init(&sc->sc_dkdev, device_xname(self), &altmemdkdriver);
	disk_attach(&sc->sc_dkdev);
}

static int
altmemsize(dev_t dev)
{
	struct altmem_softc *sc = device_lookup_private(&altmem_cd, DISKUNIT(dev));
	if (sc == NULL)
		return 0;
	return sc->sc_size >> DEV_BSHIFT;
}

static int
altmemopen(dev_t dev, int flag, int fmt, struct lwp *l)
{
	struct altmem_softc *sc = device_lookup_private(&altmem_cd, DISKUNIT(dev));
	if (sc == NULL)
		return ENXIO;
	return 0;
}

static int
altmemclose(dev_t dev, int flag, int fmt, struct lwp *l)
{
	return 0;
}

static int
altmemread(dev_t dev, struct uio *uio, int flags)
{
	if (device_lookup_private(&altmem_cd, DISKUNIT(dev)) == NULL)
		return ENXIO;
	return physio(altmemstrategy, NULL, dev, B_READ, minphys, uio);
}

static int
altmemwrite(dev_t dev, struct uio *uio, int flags)
{
	if (device_lookup_private(&altmem_cd, DISKUNIT(dev)) == NULL)
		return ENXIO;
	return physio(altmemstrategy, NULL, dev, B_WRITE, minphys, uio);
}

static void
altmemstrategy(struct buf *bp)
{
	struct altmem_softc *sc = device_lookup_private(&altmem_cd, DISKUNIT(bp->b_dev));

	if (sc == NULL) {
		bp->b_error = ENXIO;
		biodone(bp);
		return;
	}
	if (bp->b_bcount == 0) {
		biodone(bp);
		return;
	}

	sc->sc_memops->strategy(sc->sc_cookie, bp);
	biodone(bp);
}

static int
altmemioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{
	struct altmem_softc *sc = device_lookup_private(&altmem_cd, DISKUNIT(dev));
	struct dkwedge_info *dkw;

	switch (cmd) {
	case DIOCGWEDGEINFO:
		dkw = (void *)data;
		strlcpy(dkw->dkw_devname, device_xname(sc->sc_dev),
		    sizeof(dkw->dkw_devname));
		strlcpy(dkw->dkw_wname, "altmem", sizeof(dkw->dkw_wname));
		dkw->dkw_parent[0] = '\0';
		dkw->dkw_offset = 0;
		dkw->dkw_size = sc->sc_size >> DEV_BSHIFT;
		strcpy(dkw->dkw_ptype, DKW_PTYPE_UNUSED);
		break;
	default:
		return ENOTTY;
	}
	return 0;
}
