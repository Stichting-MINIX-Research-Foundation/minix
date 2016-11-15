/*	$NetBSD: sunms.c,v 1.32 2013/09/15 14:13:19 martin Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)ms.c	8.1 (Berkeley) 6/11/93
 */

/*
 * Mouse driver (/dev/mouse)
 */

/*
 * Zilog Z8530 Dual UART driver (mouse interface)
 *
 * This is the "slave" driver that will be attached to
 * the "zsc" driver for a Sun mouse.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sunms.c,v 1.32 2013/09/15 14:13:19 martin Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/syslog.h>
#include <sys/fcntl.h>
#include <sys/tty.h>

#include <machine/vuid_event.h>

#include <dev/sun/event_var.h>
#include <dev/sun/msvar.h>
#include <dev/sun/kbd_ms_ttyvar.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsmousevar.h>

#include "ms.h"
#include "wsmouse.h"
#if NMS > 0

#ifdef SUN_MS_BPS
int	sunms_bps = SUN_MS_BPS;
#else
int	sunms_bps = MS_DEFAULT_BPS;
#endif

static int	sunms_match(device_t, cfdata_t, void *);
static void	sunms_attach(device_t, device_t, void *);
static int	sunmsiopen(device_t, int mode);
int	sunmsinput(int, struct tty *);

CFATTACH_DECL_NEW(ms_tty, sizeof(struct ms_softc),
    sunms_match, sunms_attach, NULL, NULL);

struct linesw sunms_disc = {
	.l_name = "sunms",
	.l_open = ttylopen,
	.l_close = ttylclose,
	.l_read = ttyerrio,
	.l_write = ttyerrio,
	.l_ioctl = ttynullioctl,
	.l_rint = sunmsinput,
	.l_start = ttstart,
	.l_modem = nullmodem,
	.l_poll = ttpoll
};

int	sunms_enable(void *);
int	sunms_ioctl(void *, u_long, void *, int, struct lwp *);
void	sunms_disable(void *);

const struct wsmouse_accessops	sunms_accessops = {
	sunms_enable,
	sunms_ioctl,
	sunms_disable,
};

/*
 * ms_match: how is this zs channel configured?
 */
int
sunms_match(device_t parent, cfdata_t cf, void *aux)
{
	struct kbd_ms_tty_attach_args *args = aux;

	if (sunms_bps == 0)
		return 0;

	if (strcmp(args->kmta_name, "mouse") == 0)
		return (1);

	return 0;
}

void
sunms_attach(device_t parent, device_t self, void *aux)
{
	struct ms_softc *ms = device_private(self);
	struct kbd_ms_tty_attach_args *args = aux;
	struct tty *tp = args->kmta_tp;
#if NWSMOUSE > 0
	struct wsmousedev_attach_args a;
#endif

	ms->ms_dev = self;
	tp->t_sc  = ms;
	tp->t_dev = args->kmta_dev;
	ms->ms_priv = tp;
	ms->ms_deviopen = sunmsiopen;
	ms->ms_deviclose = NULL;

	aprint_normal("\n");

	/* Initialize the speed, etc. */
	if (ttyldisc_attach(&sunms_disc) != 0)
		panic("sunms_attach: sunms_disc");
	ttyldisc_release(tp->t_linesw);
	tp->t_linesw = ttyldisc_lookup(sunms_disc.l_name);
	KASSERT(tp->t_linesw == &sunms_disc);
	tp->t_oflag &= ~OPOST;

	/* Initialize translator. */
	ms->ms_byteno = -1;

#if NWSMOUSE > 0
	/*
	 * attach wsmouse
	 */
	a.accessops = &sunms_accessops;
	a.accesscookie = ms;

	ms->ms_wsmousedev = config_found(self, &a, wsmousedevprint);
#endif
}

/*
 * Internal open routine.  This really should be inside com.c
 * But I'm putting it here until we have a generic internal open
 * mechanism.
 */
int
sunmsiopen(device_t dev, int flags)
{
	struct ms_softc *ms = device_private(dev);
	struct tty *tp = ms->ms_priv;
	struct lwp *l = curlwp ? curlwp : &lwp0;
	struct termios t;
	int error;

	/* Open the lower device */
	if ((error = cdev_open(tp->t_dev, O_NONBLOCK|flags,
				     0/* ignored? */, l)) != 0)
		return (error);

	/* Now configure it for the console. */
	tp->t_ospeed = 0;
	t.c_ispeed = sunms_bps;
	t.c_ospeed = sunms_bps;
	t.c_cflag =  CLOCAL|CS8;
	(*tp->t_param)(tp, &t);

	return (0);
}

int
sunmsinput(int c, struct tty *tp)
{
	struct ms_softc *ms = tp->t_sc;

	if (c & TTY_ERRORMASK) c = -1;
	else c &= TTY_CHARMASK;

	/* Pass this up to the "middle" layer. */
	ms_input(ms, c);
	return (0);
}

int
sunms_ioctl(void *v, u_long cmd, void *data, int flag, struct lwp *l)
{
/*	struct ms_softc *sc = v; */

	switch (cmd) {
	case WSMOUSEIO_GTYPE:
		*(u_int *)data = WSMOUSE_TYPE_PS2; /* XXX  */
		break;

	default:
		return (EPASSTHROUGH);
	}
	return (0);
}

int
sunms_enable(void *v)
{
	struct ms_softc *ms = v;
	int err;
	int s;

	if (ms->ms_ready)
		return EBUSY;

	err = sunmsiopen(ms->ms_dev, 0);
	if (err)
		return err;

	s = spltty();
	ms->ms_ready = 2;
	splx(s);

	return 0;
}

void
sunms_disable(void *v)
{
	struct ms_softc *ms = v;
	int s;

	s = spltty();
	ms->ms_ready = 0;
	splx(s);
}
#endif
