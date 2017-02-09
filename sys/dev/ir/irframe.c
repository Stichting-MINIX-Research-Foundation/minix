/*	$NetBSD: irframe.c,v 1.46 2014/07/25 08:10:37 dholland Exp $	*/

/*
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net).
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: irframe.c,v 1.46 2014/07/25 08:10:37 dholland Exp $");

#include "irframe.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <sys/vnode.h>

#include <dev/ir/ir.h>
#include <dev/ir/irdaio.h>
#include <dev/ir/irframevar.h>

#ifdef IRFRAME_DEBUG
#define DPRINTF(x)	if (irframedebug) printf x
#define Static
int irframedebug = 0;
#else
#define DPRINTF(x)
#define Static static
#endif

dev_type_open(irframeopen);
dev_type_close(irframeclose);
dev_type_read(irframeread);
dev_type_write(irframewrite);
dev_type_ioctl(irframeioctl);
dev_type_poll(irframepoll);
dev_type_kqfilter(irframekqfilter);

const struct cdevsw irframe_cdevsw = {
	.d_open = irframeopen,
	.d_close = irframeclose,
	.d_read = irframeread,
	.d_write = irframewrite,
	.d_ioctl = irframeioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = irframepoll,
	.d_mmap = nommap,
	.d_kqfilter = irframekqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER
};

int irframe_match(device_t parent, cfdata_t match, void *aux);

Static int irf_set_params(struct irframe_softc *sc, struct irda_params *p);
Static int irf_reset_params(struct irframe_softc *sc);

#if NIRFRAME == 0
/* In case we just have tty attachment. */
CFDRIVER_DECL(irframe, DV_DULL, NULL);
#endif

CFATTACH_DECL_NEW(irframe, sizeof(struct irframe_softc),
    irframe_match, irframe_attach, irframe_detach, NULL);

extern struct cfdriver irframe_cd;

#define IRFRAMEUNIT(dev) (minor(dev))

int
irframe_match(device_t parent, cfdata_t match, void *aux)
{
	struct ir_attach_args *ia = aux;

	return (ia->ia_type == IR_TYPE_IRFRAME);
}

void
irframe_attach(device_t parent, device_t self, void *aux)
{
	struct irframe_softc *sc = device_private(self);
	struct ir_attach_args *ia = aux;
	const char *delim;
	int speeds = 0;

	sc->sc_dev = self;
	sc->sc_methods = ia->ia_methods;
	sc->sc_handle = ia->ia_handle;

#ifdef DIAGNOSTIC
	if (sc->sc_methods->im_read == NULL ||
	    sc->sc_methods->im_write == NULL ||
	    sc->sc_methods->im_poll == NULL ||
	    sc->sc_methods->im_kqfilter == NULL ||
	    sc->sc_methods->im_set_params == NULL ||
	    sc->sc_methods->im_get_speeds == NULL ||
	    sc->sc_methods->im_get_turnarounds == NULL)
		panic("%s: missing methods", device_xname(self));
#endif

	(void)sc->sc_methods->im_get_speeds(sc->sc_handle, &speeds);
	sc->sc_speedmask = speeds;
	delim = ":";
	if (speeds & IRDA_SPEEDS_SIR) {
		printf("%s SIR", delim);
		delim = ",";
	}
	if (speeds & IRDA_SPEEDS_MIR) {
		printf("%s MIR", delim);
		delim = ",";
	}
	if (speeds & IRDA_SPEEDS_FIR) {
		printf("%s FIR", delim);
		delim = ",";
	}
	if (speeds & IRDA_SPEEDS_VFIR) {
		printf("%s VFIR", delim);
		delim = ",";
	}
	printf("\n");

	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");
}

int
irframe_detach(device_t self, int flags)
{
	/*struct irframe_softc *sc = device_private(self);*/
	int maj, mn;

	pmf_device_deregister(self);

	/* XXX needs reference count */

	/* locate the major number */
	maj = cdevsw_lookup_major(&irframe_cdevsw);

	/* Nuke the vnodes for any open instances (calls close). */
	mn = device_unit(self);
	vdevgone(maj, mn, mn, VCHR);

	return (0);
}

int
irframeopen(dev_t dev, int flag, int mode, struct lwp *l)
{
	struct irframe_softc *sc;
	int error;

	sc = device_lookup_private(&irframe_cd, IRFRAMEUNIT(dev));
	if (sc == NULL)
		return (ENXIO);
	if (!device_is_active(sc->sc_dev))
		return (EIO);
	if (sc->sc_open)
		return (EBUSY);
	if (sc->sc_methods->im_open != NULL) {
		error = sc->sc_methods->im_open(sc->sc_handle, flag, mode, l);
		if (error)
			return (error);
	}
	sc->sc_open = 1;
#ifdef DIAGNOSTIC
	sc->sc_speed = IRDA_DEFAULT_SPEED;
#endif
	(void)irf_reset_params(sc);
	return (0);
}

int
irframeclose(dev_t dev, int flag, int mode, struct lwp *l)
{
	struct irframe_softc *sc;
	int error;

	sc = device_lookup_private(&irframe_cd, IRFRAMEUNIT(dev));
	if (sc == NULL)
		return (ENXIO);
	sc->sc_open = 0;
	if (sc->sc_methods->im_close != NULL)
		error = sc->sc_methods->im_close(sc->sc_handle, flag, mode, l);
	else
		error = 0;
	return (error);
}

int
irframeread(dev_t dev, struct uio *uio, int flag)
{
	struct irframe_softc *sc;

	sc = device_lookup_private(&irframe_cd, IRFRAMEUNIT(dev));
	if (sc == NULL)
		return (ENXIO);
	if (!device_is_active(sc->sc_dev) || !sc->sc_open)
		return (EIO);
	if (uio->uio_resid < sc->sc_params.maxsize) {
#ifdef DIAGNOSTIC
		printf("irframeread: short read %ld < %d\n",
		       (long)uio->uio_resid, sc->sc_params.maxsize);
#endif
		return (EINVAL);
	}
	return (sc->sc_methods->im_read(sc->sc_handle, uio, flag));
}

int
irframewrite(dev_t dev, struct uio *uio, int flag)
{
	struct irframe_softc *sc;

	sc = device_lookup_private(&irframe_cd, IRFRAMEUNIT(dev));
	if (sc == NULL)
		return (ENXIO);
	if (!device_is_active(sc->sc_dev) || !sc->sc_open)
		return (EIO);
	if (uio->uio_resid > sc->sc_params.maxsize) {
#ifdef DIAGNOSTIC
		printf("irframeread: long write %ld > %d\n",
		       (long)uio->uio_resid, sc->sc_params.maxsize);
#endif
		return (EINVAL);
	}
	return (sc->sc_methods->im_write(sc->sc_handle, uio, flag));
}

int
irf_set_params(struct irframe_softc *sc, struct irda_params *p)
{
	int error;

	DPRINTF(("irf_set_params: set params speed=%u ebofs=%u maxsize=%u "
		 "speedmask=0x%x\n", p->speed, p->ebofs, p->maxsize,
		 sc->sc_speedmask));

	if (p->maxsize > IRDA_MAX_FRAME_SIZE) {
#ifdef IRFRAME_DEBUG
		printf("irf_set_params: bad maxsize=%u\n", p->maxsize);
#endif
		return (EINVAL);
	}

	if (p->ebofs > IRDA_MAX_EBOFS) {
#ifdef IRFRAME_DEBUG
		printf("irf_set_params: bad maxsize=%u\n", p->maxsize);
#endif
		return (EINVAL);
	}

#define CONC(x,y) x##y
#define CASE(s) case s: if (!(sc->sc_speedmask & CONC(IRDA_SPEED_,s))) return (EINVAL); break
	switch (p->speed) {
	CASE(2400);
	CASE(9600);
	CASE(19200);
	CASE(38400);
	CASE(57600);
	CASE(115200);
	CASE(576000);
	CASE(1152000);
	CASE(4000000);
	CASE(16000000);
	default: return (EINVAL);
	}
#undef CONC
#undef CASE

	error = sc->sc_methods->im_set_params(sc->sc_handle, p);
	if (!error) {
		sc->sc_params = *p;
		DPRINTF(("irf_set_params: ok\n"));
#ifdef DIAGNOSTIC
		if (p->speed != sc->sc_speed) {
			sc->sc_speed = p->speed;
			aprint_verbose_dev(sc->sc_dev, "set speed %u\n",
			       sc->sc_speed);
		}
#endif
	} else {
#ifdef IRFRAME_DEBUG
		printf("irf_set_params: error=%d\n", error);
#endif
	}
	return (error);
}

int
irf_reset_params(struct irframe_softc *sc)
{
	struct irda_params params;

	params.speed = IRDA_DEFAULT_SPEED;
	params.ebofs = IRDA_DEFAULT_EBOFS;
	params.maxsize = IRDA_DEFAULT_SIZE;
	return (irf_set_params(sc, &params));
}

int
irframeioctl(dev_t dev, u_long cmd, void *addr, int flag,
    struct lwp *l)
{
	struct irframe_softc *sc;
	void *vaddr = addr;
	int error;

	sc = device_lookup_private(&irframe_cd, IRFRAMEUNIT(dev));
	if (sc == NULL)
		return (ENXIO);
	if (!device_is_active(sc->sc_dev) || !sc->sc_open)
		return (EIO);

	switch (cmd) {
	case FIONBIO:
		/* All handled in the upper FS layer. */
		error = 0;
		break;

	case IRDA_SET_PARAMS:
		error = irf_set_params(sc, vaddr);
		break;

	case IRDA_RESET_PARAMS:
		error = irf_reset_params(sc);
		break;

	case IRDA_GET_SPEEDMASK:
		error = sc->sc_methods->im_get_speeds(sc->sc_handle, vaddr);
		break;

	case IRDA_GET_TURNAROUNDMASK:
		error = sc->sc_methods->im_get_turnarounds(sc->sc_handle,vaddr);
		break;

	default:
		error = EINVAL;
		break;
	}
	return (error);
}

int
irframepoll(dev_t dev, int events, struct lwp *l)
{
	struct irframe_softc *sc;

	sc = device_lookup_private(&irframe_cd, IRFRAMEUNIT(dev));
	if (sc == NULL)
		return (POLLHUP);
	if (!device_is_active(sc->sc_dev) || !sc->sc_open)
		return (POLLHUP);

	return (sc->sc_methods->im_poll(sc->sc_handle, events, l));
}

int
irframekqfilter(dev_t dev, struct knote *kn)
{
	struct irframe_softc *sc;

	sc = device_lookup_private(&irframe_cd, IRFRAMEUNIT(dev));
	if (!device_is_active(sc->sc_dev) || !sc->sc_open)
		return (1);

	return (sc->sc_methods->im_kqfilter(sc->sc_handle, kn));
}
