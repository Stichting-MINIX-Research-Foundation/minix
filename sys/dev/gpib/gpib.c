/*	$NetBSD: gpib.c,v 1.22 2014/07/25 08:10:36 dholland Exp $	*/

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Gregory McGarry.
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
__KERNEL_RCSID(0, "$NetBSD: gpib.c,v 1.22 2014/07/25 08:10:36 dholland Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/proc.h>

#include <dev/gpib/gpibvar.h>

#include <dev/gpib/gpibio.h>		/* XXX */

#include "locators.h"

#ifndef DEBUG
#define DEBUG
#endif

#ifdef DEBUG
int gpibdebug = 0xff;
#define DBG_FOLLOW	0x01
#define DBG_INTR	0x02
#define DBG_FAIL	0x04
#define DPRINTF(mask, str)	if (gpibdebug & (mask)) printf str
#else
#define DPRINTF(mask, str)	/* nothing */
#endif

int	gpibmatch(device_t, cfdata_t, void *);
void	gpibattach(device_t, device_t, void *);

CFATTACH_DECL_NEW(gpib, sizeof(struct gpib_softc),
	gpibmatch, gpibattach, NULL, NULL);

static int	gpibsubmatch1(device_t, cfdata_t, const int *, void *);
static int	gpibsubmatch2(device_t, cfdata_t, const int *, void *);
static int	gpibprint(void *, const char *);

dev_type_open(gpibopen);
dev_type_close(gpibclose);
dev_type_read(gpibread);
dev_type_write(gpibwrite);
dev_type_ioctl(gpibioctl);
dev_type_poll(gpibpoll);

const struct cdevsw gpib_cdevsw = {
	.d_open = gpibopen,
	.d_close = gpibclose,
	.d_read = gpibread,
	.d_write = gpibwrite,
	.d_ioctl = gpibioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = gpibpoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER
};

extern struct cfdriver gpib_cd;

#define GPIBUNIT(dev)		(minor(dev) & 0x0f)

int gpibtimeout = 100000;	/* # of status tests before we give up */

int
gpibmatch(device_t parent, cfdata_t match, void *aux)
{

	return (1);
}

void
gpibattach(device_t parent, device_t self, void *aux)
{
	struct gpib_softc *sc = device_private(self);
	cfdata_t cf = device_cfdata(self);
	struct gpibdev_attach_args *gda = aux;
	struct gpib_attach_args ga;
	int address;

	sc->sc_dev = self;
	sc->sc_ic = gda->ga_ic;

	/*
	 * If the configuration file specified a host address, then
	 * use it in favour of registers/switches or the default (30).
	 */
	if (cf->cf_loc[GPIBDEVCF_ADDRESS] != GPIBDEVCF_ADDRESS_DEFAULT)
		sc->sc_myaddr = cf->cf_loc[GPIBDEVCF_ADDRESS];
	else if (gda->ga_address != GPIBDEVCF_ADDRESS_DEFAULT)
		sc->sc_myaddr = gda->ga_address;
	else
		sc->sc_myaddr = 30;

	printf(": host address %d\n", sc->sc_myaddr);

	/* record our softc pointer */
	sc->sc_ic->bus = sc;

	/* Initialize the slave request queue */
	TAILQ_INIT(&sc->sc_queue);

	/* attach addressed devices */
	for (address=0; address<GPIB_NDEVS; address++) {
		ga.ga_ic = sc->sc_ic;
		ga.ga_address = address;
		(void) config_search_ia(gpibsubmatch1, sc->sc_dev, "gpib", &ga);
	}

	/* attach the wild-carded devices - probably protocol busses */
	ga.ga_ic = sc->sc_ic;
	(void) config_search_ia(gpibsubmatch2, sc->sc_dev, "gpib", &ga);
}

int
gpibsubmatch1(device_t parent, cfdata_t cf, const int *ldesc, void *aux)
{
	struct gpib_softc *sc = device_private(parent);
	struct gpib_attach_args *ga = aux;

	if (cf->cf_loc[GPIBCF_ADDRESS] != ga->ga_address)
		return (0);

	if (cf->cf_loc[GPIBCF_ADDRESS] == sc->sc_myaddr)
		return (0);

	if (config_match(parent, cf, ga) > 0) {
		if (gpib_alloc(sc, ga->ga_address))
			return (0);
		config_attach(parent, cf, ga, gpibprint);
		return (0);
	}
	return (0);
}

int
gpibsubmatch2(device_t parent, cfdata_t cf, const int *ldesc, void *aux)
{
	struct gpib_attach_args *ga = aux;

	if (cf->cf_loc[GPIBCF_ADDRESS] != GPIBCF_ADDRESS_DEFAULT)
		return (0);

	ga->ga_address = GPIBCF_ADDRESS_DEFAULT;
	if (config_match(parent, cf, ga) > 0) {
		config_attach(parent, cf, ga, gpibdevprint);
		return (0);
	}
	return (0);
}

int
gpibprint(void *aux, const char *pnp)
{
	struct gpib_attach_args *ga = aux;

	if (ga->ga_address != GPIBCF_ADDRESS_DEFAULT)
		printf(" address %d", ga->ga_address);
	return (UNCONF);
}

int
gpibdevprint(void *aux, const char *pnp)
{

	if (pnp != NULL)
		printf("gpib at %s", pnp);
	return (UNCONF);
}

/*
 * Called by hardware driver, pass to device driver.
 */
int
gpibintr(void *v)
{
	struct gpib_softc *sc = v;
	gpib_handle_t hdl;

	DPRINTF(DBG_INTR, ("gpibintr: sc=%p\n", sc));

	hdl = TAILQ_FIRST(&sc->sc_queue);
	(hdl->hq_callback)(hdl->hq_softc, GPIBCBF_INTR);
	return (0);
}

/*
 * Create a callback handle.
 */
int
_gpibregister(struct gpib_softc *sc, int slave, gpib_callback_t callback, void *arg, gpib_handle_t *hdl)
{

	*hdl = malloc(sizeof(struct gpibqueue), M_DEVBUF, M_NOWAIT);
	if (*hdl == NULL) {
		DPRINTF(DBG_FAIL, ("_gpibregister: can't allocate queue\n"));
		return (1);
	}

	(*hdl)->hq_slave = slave;
	(*hdl)->hq_callback = callback;
	(*hdl)->hq_softc = arg;

	return (0);
}

/*
 * Request exclusive access to the GPIB bus.
 */
int
_gpibrequest(struct gpib_softc *sc, gpib_handle_t hdl)
{

	DPRINTF(DBG_FOLLOW, ("_gpibrequest: sc=%p hdl=%p\n", sc, hdl));

	TAILQ_INSERT_TAIL(&sc->sc_queue, hdl, hq_list);
	if (TAILQ_FIRST(&sc->sc_queue) == hdl)
		return (1);

	return (0);
}

/*
 * Release exclusive access to the GPIB bus.
 */
void
_gpibrelease(struct gpib_softc *sc, gpib_handle_t hdl)
{

	DPRINTF(DBG_FOLLOW, ("_gpibrelease: sc=%p hdl=%p\n", sc, hdl));

	TAILQ_REMOVE(&sc->sc_queue, hdl, hq_list);
	if ((hdl = TAILQ_FIRST(&sc->sc_queue)) != NULL)
		(*hdl->hq_callback)(hdl->hq_softc, GPIBCBF_START);
}


/*
 * Asynchronous wait.
 */
void
_gpibawait(struct gpib_softc *sc)
{
	int slave;

	DPRINTF(DBG_FOLLOW, ("_gpibawait: sc=%p\n", sc));

	slave = TAILQ_FIRST(&sc->sc_queue)->hq_slave;
	(*sc->sc_ic->ppwatch)(sc->sc_ic->cookie, slave);
}

/*
 * Synchronous (spin) wait.
 */
int
_gpibswait(struct gpib_softc *sc, int slave)
{
	int timo = gpibtimeout;
	int (*pptest)(void *, int);

	DPRINTF(DBG_FOLLOW, ("_gpibswait: sc=%p\n", sc));

	pptest = sc->sc_ic->pptest;
	while ((*pptest)(sc->sc_ic->cookie, slave) == 0) {
		if (--timo == 0) {
			aprint_error_dev(sc->sc_dev, "swait timeout\n");
			return(-1);
		}
	}
	return (0);
}

/*
 * Resource accounting: check if the address has already been
 * claimed and allocated.
 */
int
gpib_isalloc(struct gpib_softc *sc, u_int8_t address)
{

	DPRINTF(DBG_FOLLOW, ("gpib_isalloc: sc=%p address=%d\n", sc, address));

#ifdef DIAGNOSTIC
	if (address >= GPIB_NDEVS)
		panic("gpib_isalloc: device address out of range");
#endif

	return ((sc->sc_rmap & (1 << address)) != 0);
}

/*
 * Resource accounting: allocate the address.
 */
int
gpib_alloc(struct gpib_softc *sc, u_int8_t address)
{

	DPRINTF(DBG_FOLLOW, ("gpib_alloc: sc=%p address=%d\n", sc, address));

#ifdef DIAGNOSTIC
	if (address >= GPIB_NDEVS)
		panic("gpib_alloc: device address out of range");
#endif

	if (!gpib_isalloc(sc, address)) {
		sc->sc_rmap |= (1 << address);
		return (0);
	}
	return (1);
}

/*
 * Resource accounting: deallocate the address.
 */
void
gpib_dealloc(struct gpib_softc *sc, u_int8_t address)
{

	DPRINTF(DBG_FOLLOW, ("gpib_free: sc=%p address=%d\n", sc, address));

#ifdef DIAGNOSTIC
	if (address >= GPIB_NDEVS)
		panic("gpib_free: device address out of range");

	if (!gpib_isalloc(sc, address))
		panic("gpib_free: not allocated");
#endif

	sc->sc_rmap &= ~(1 << address);
}

int
_gpibsend(struct gpib_softc *sc, int slave, int sec, void *ptr, int origcnt)
{
	int rv;
	int cnt = 0;
	u_int8_t cmds[4];
	int i = 0;

	DPRINTF(DBG_FOLLOW,
	    ("_gpibsend: sc=%p slave %d sec=%d ptr=%p cnt=%d\n",
	    sc, slave, sec, ptr, origcnt));

	/*
	 * For compatibility, call the hardware driver directly.
	 */
	if (sc->sc_ic->send != NULL) {
		rv = (*sc->sc_ic->send)(sc->sc_ic->cookie,
			slave, sec, ptr, origcnt);
		return (rv);
	}

	if ((*sc->sc_ic->tc)(sc->sc_ic->cookie, 0))
		goto senderror;
	cmds[i++] = GPIBCMD_UNL;
	cmds[i++] = GPIBCMD_TAG | sc->sc_myaddr;
	cmds[i++] = GPIBCMD_LAG | slave;
	if (sec >= 0 || sec == -2) {
		if (sec == -2)		/* selected device clear KLUDGE */
			cmds[i++] = GPIBCMD_SDC;
		else
			cmds[i++] = GPIBCMD_SCG | sec;
	}
	if ((*sc->sc_ic->sendcmds)(sc->sc_ic->cookie, cmds, i) != i)
		goto senderror;
	if ((*sc->sc_ic->gts)(sc->sc_ic->cookie))
		goto senderror;
	if (origcnt) {
		cnt = (*sc->sc_ic->senddata)(sc->sc_ic->cookie, ptr, origcnt);
		if (cnt != origcnt)
			goto senderror;
		if ((*sc->sc_ic->tc)(sc->sc_ic->cookie, 0))
			goto senderror;
	}
	return (origcnt);

senderror:
	(*sc->sc_ic->ifc)(sc->sc_ic->cookie);
	DPRINTF(DBG_FAIL,
	    ("%s: _gpibsend failed: slave %d, sec %x, sent %d of %d bytes\n",
	    device_xname(sc->sc_dev), slave, sec, cnt, origcnt));
	return (cnt);
}

int
_gpibrecv(struct gpib_softc *sc, int slave, int sec, void *ptr, int origcnt)
{
	int rv;
	u_int8_t cmds[4];
	int cnt = 0;
	int i = 0;

	DPRINTF(DBG_FOLLOW,
	    ("_gpibrecv: sc=%p slave=%d sec=%d buf=%p cnt=%d\n",
	    sc, slave, sec, ptr, origcnt));

	/*
	 * For compatibility, call the hardware driver directly.
	 */
	if (sc->sc_ic->recv != NULL) {
		rv = (*sc->sc_ic->recv)(sc->sc_ic->cookie,
			slave, sec, ptr, origcnt);
		return (rv);
	}

	/*
	 * slave < 0 implies continuation of a previous receive
	 * that probably timed out.
	 */
	if (slave >= 0) {
		if ((*sc->sc_ic->tc)(sc->sc_ic->cookie, 0))
			goto recverror;
		cmds[i++] = GPIBCMD_UNL;
		cmds[i++] = GPIBCMD_LAG | sc->sc_myaddr;
		cmds[i++] = GPIBCMD_TAG | slave;
		if (sec >= 0)
			cmds[i++] = GPIBCMD_SCG | sec;
		if ((*sc->sc_ic->sendcmds)(sc->sc_ic->cookie, cmds, i) != i)
			goto recverror;
		if ((*sc->sc_ic->gts)(sc->sc_ic->cookie))
			goto recverror;
	}
	if (origcnt) {
		cnt = (*sc->sc_ic->recvdata)(sc->sc_ic->cookie, ptr, origcnt);
		if (cnt != origcnt)
			goto recverror;
		if ((sc->sc_ic->tc)(sc->sc_ic->cookie, 0))
			goto recverror;
		cmds[0] = (slave == GPIB_BROADCAST_ADDR) ?
		    GPIBCMD_UNA : GPIBCMD_UNT;
		if ((*sc->sc_ic->sendcmds)(sc->sc_ic->cookie, cmds, 1) != 1)
			goto recverror;
	}
	return (origcnt);

recverror:
	(*sc->sc_ic->ifc)(sc->sc_ic->cookie);
	DPRINTF(DBG_FAIL,
	    ("_gpibrecv: failed, sc=%p slave %d, sec %x, got %d of %d bytes\n",
	    sc, slave, sec, cnt, origcnt));
	return (cnt);
}

/*
 * /dev/gpib? interface
 */

int
gpibopen(dev_t dev, int flags, int mode, struct lwp *l)
{
	struct gpib_softc *sc;

	sc = device_lookup_private(&gpib_cd, GPIBUNIT(dev));
	if (sc == NULL)
		return (ENXIO);

	DPRINTF(DBG_FOLLOW, ("gpibopen: sc=%p\n", sc));

	if (sc->sc_flags & GPIBF_ACTIVE)
		return (EBUSY);
	sc->sc_flags |= GPIBF_ACTIVE;

	return (0);
}

int
gpibclose(dev_t dev, int flag, int mode, struct lwp *l)
{
	struct gpib_softc *sc;

	sc = device_lookup_private(&gpib_cd, GPIBUNIT(dev));
	if (sc == NULL)
		return (ENXIO);

	DPRINTF(DBG_FOLLOW, ("gpibclose: sc=%p\n", sc));

	sc->sc_flags &= ~GPIBF_ACTIVE;

	return (0);
}

int
gpibread(dev_t dev, struct uio *uio, int flags)
{
	struct gpib_softc *sc;

	sc = device_lookup_private(&gpib_cd, GPIBUNIT(dev));
	if (sc == NULL)
		return (ENXIO);

	DPRINTF(DBG_FOLLOW, ("gpibread: sc=%p\n", sc));

	return (EOPNOTSUPP);
}

int
gpibwrite(dev_t dev, struct uio *uio, int flags)
{
	struct gpib_softc *sc;

	sc = device_lookup_private(&gpib_cd, GPIBUNIT(dev));
	if (sc == NULL)
		return (ENXIO);

	DPRINTF(DBG_FOLLOW, ("gpibwrite: sc=%p\n", sc));

	return (EOPNOTSUPP);
}

int
gpibioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{
	struct gpib_softc *sc;

	sc = device_lookup_private(&gpib_cd, GPIBUNIT(dev));
	if (sc == NULL)
		return (ENXIO);

	DPRINTF(DBG_FOLLOW, ("gpibioctl(%lu, '%c',%lu): sc=%p\n",
	    IOCPARM_LEN(cmd), (char)IOCGROUP(cmd), cmd & 0xff, sc));

	switch (cmd) {
	case GPIB_INFO:
		(*(int *)data) = 0xa5a5a5a5;
		break;
	}

	return (EINVAL);
}

int
gpibpoll(dev_t dev, int events, struct lwp *l)
{
	struct gpib_softc *sc;

	sc = device_lookup_private(&gpib_cd, GPIBUNIT(dev));
	if (sc == NULL)
		return (ENXIO);

	DPRINTF(DBG_FOLLOW, ("gpibpoll: sc=%p\n", sc));

	return (0);
}
