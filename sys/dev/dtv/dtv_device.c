/* $NetBSD: dtv_device.c,v 1.11 2014/08/09 13:34:10 jmcneill Exp $ */

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
__KERNEL_RCSID(0, "$NetBSD: dtv_device.c,v 1.11 2014/08/09 13:34:10 jmcneill Exp $");

#include <sys/types.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/atomic.h>
#include <sys/module.h>
#include <sys/poll.h>
#include <sys/select.h>

#include <dev/dtv/dtvvar.h>

MODULE(MODULE_CLASS_DRIVER, dtv, NULL);

static dev_type_open(dtvopen);
static dev_type_close(dtvclose);
static dev_type_read(dtvread);
static dev_type_ioctl(dtvioctl);
static dev_type_poll(dtvpoll);

const struct cdevsw dtv_cdevsw = {
	.d_open = dtvopen,
	.d_close = dtvclose,
	.d_read = dtvread,
	.d_write = nowrite,
	.d_ioctl = dtvioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = dtvpoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER | D_MPSAFE,
};

static int	dtv_match(device_t, cfdata_t, void *);
static void	dtv_attach(device_t, device_t, void *);
static int	dtv_detach(device_t, int);

CFATTACH_DECL_NEW(dtv,
    sizeof(struct dtv_softc),
    dtv_match,
    dtv_attach,
    dtv_detach,
    NULL
);

extern struct cfdriver dtv_cd;

static int
dtv_match(device_t parent, cfdata_t cfdata, void *aa)
{
	return 1;
}

static void
dtv_attach(device_t parent, device_t self, void *aa)
{
	struct dtv_attach_args *daa = aa;
	struct dtv_softc *sc = device_private(self);
	struct dtv_stream *ds = &sc->sc_stream;
	struct dvb_frontend_info info;

	sc->sc_dev = self;
	sc->sc_hw = daa->hw;
	sc->sc_priv = daa->priv;
	sc->sc_open = 0;
	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_NONE);

	ds->ds_nbufs = 0;
	ds->ds_buf = NULL;
	SIMPLEQ_INIT(&ds->ds_ingress);
	SIMPLEQ_INIT(&ds->ds_egress);
	mutex_init(&ds->ds_egress_lock, MUTEX_DEFAULT, IPL_SCHED);
	mutex_init(&ds->ds_ingress_lock, MUTEX_DEFAULT, IPL_SCHED);
	cv_init(&ds->ds_sample_cv, "dtv");
	selinit(&ds->ds_sel);
	dtv_scatter_buf_init(&ds->ds_data);
	if (dtv_buffer_realloc(sc, DTV_DEFAULT_BUFSIZE) != 0) {
		aprint_error(": no memory\n");
		sc->sc_dying = true;
		return;
	}

	mutex_init(&sc->sc_demux_lock, MUTEX_DEFAULT, IPL_SCHED);
	TAILQ_INIT(&sc->sc_demux_list);
	sc->sc_demux_runcnt = 0;

	dtv_device_get_devinfo(sc, &info);

	aprint_naive("\n");
	aprint_normal(": %s", info.name);
	switch (info.type) {
	case FE_QPSK:
		aprint_normal(" [QPSK]");
		break;
	case FE_QAM:
		aprint_normal(" [QAM]");
		break;
	case FE_OFDM:
		aprint_normal(" [OFDM]");
		break;
	case FE_ATSC:
		aprint_normal(" [ATSC]");
		break;
	}
	aprint_normal("\n");
}

static int
dtv_detach(device_t self, int flags)
{
	struct dtv_softc *sc = device_private(self);
	struct dtv_stream *ds = &sc->sc_stream;

	cv_destroy(&ds->ds_sample_cv);
	mutex_destroy(&ds->ds_ingress_lock);
	mutex_destroy(&ds->ds_egress_lock);
	seldestroy(&ds->ds_sel);
	dtv_buffer_realloc(sc, 0);
	dtv_scatter_buf_destroy(&ds->ds_data);

	mutex_destroy(&sc->sc_demux_lock);
	mutex_destroy(&sc->sc_lock);

	return 0;
}

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
dtv_modcmd(modcmd_t cmd, void *arg)
{
#ifdef _MODULE
	int error, bmaj = -1, cmaj = -1;
#endif

	switch (cmd) {
	case MODULE_CMD_INIT:
#ifdef _MODULE
		error = config_init_component(cfdriver_ioconf_dtv,
		    cfattach_ioconf_dtv, cfdata_ioconf_dtv);
		if (error)
			return error;
		error = devsw_attach("dtv", NULL, &bmaj, &dtv_cdevsw, &cmaj);
		if (error)
			config_fini_component(cfdriver_ioconf_dtv,
			    cfattach_ioconf_dtv, cfdata_ioconf_dtv);
		return error;
#else
		return 0;
#endif
	case MODULE_CMD_FINI:
#ifdef _MODULE
		devsw_detach(NULL, &dtv_cdevsw);
		return config_fini_component(cfdriver_ioconf_dtv,
		    cfattach_ioconf_dtv, cfdata_ioconf_dtv);
#else
		return 0;
#endif
	default:
		return ENOTTY;
	}
}

static int
dtvopen(dev_t dev, int flags, int mode, lwp_t *l)
{
	struct dtv_softc *sc;
	struct dtv_ts *ts;
	int error;

	if ((sc = device_lookup_private(&dtv_cd, DTVUNIT(dev))) == NULL)
		return ENXIO;
	if (sc->sc_dying == true)
		return ENODEV;
	ts = &sc->sc_ts;

	mutex_enter(&sc->sc_lock);
	if (sc->sc_open == 0) {
		error = dtv_device_open(sc, flags);
		if (error)
			return error;
		sc->sc_bufsize = DTV_DEFAULT_BUFSIZE;
		sc->sc_bufsize_chg = true;
		memset(ts->ts_pidfilter, 0, sizeof(ts->ts_pidfilter));
	}
	++sc->sc_open;
	mutex_exit(&sc->sc_lock);

	if (ISDTVDEMUX(dev))
		return dtv_demux_open(sc, flags, mode, l);

	return 0;
}

static int
dtvclose(dev_t dev, int flags, int mode, lwp_t *l)
{
	struct dtv_softc *sc;

	if ((sc = device_lookup_private(&dtv_cd, DTVUNIT(dev))) == NULL)
		return ENXIO;

	dtv_common_close(sc);

	return 0;
}

static int
dtvread(dev_t dev, struct uio *uio, int flags)
{
	struct dtv_softc *sc;

	if ((sc = device_lookup_private(&dtv_cd, DTVUNIT(dev))) == NULL)
		return ENXIO;

	if (ISDTVDVR(dev))
		return dtv_buffer_read(sc, uio, flags);

	return ENXIO;
}

static int
dtvioctl(dev_t dev, u_long cmd, void *ptr, int flags, lwp_t *l)
{
	struct dtv_softc *sc;

	if ((sc = device_lookup_private(&dtv_cd, DTVUNIT(dev))) == NULL)
		return ENXIO;

	if (ISDTVFRONTEND(dev)) {
		return dtv_frontend_ioctl(sc, cmd, ptr, flags);
	}

	return EINVAL;
}

static int
dtvpoll(dev_t dev, int events, lwp_t *l)
{
	struct dtv_softc *sc;

	if ((sc = device_lookup_private(&dtv_cd, DTVUNIT(dev))) == NULL)
		return POLLERR;

	if (ISDTVFRONTEND(dev)) {
		return POLLPRI|POLLIN; /* XXX event */
	} else if (ISDTVDVR(dev)) {
		return dtv_buffer_poll(sc, events, l);
	}

	return POLLERR;
}

void
dtv_common_close(struct dtv_softc *sc)
{
	mutex_enter(&sc->sc_lock);
	KASSERT(sc->sc_open > 0);
	--sc->sc_open;
	if (sc->sc_open == 0) {
		dtv_device_close(sc);
		dtv_buffer_destroy(sc);
	}
	mutex_exit(&sc->sc_lock);
}
