/*	$NetBSD: ucycom.c,v 1.42 2015/03/07 20:20:55 mrg Exp $	*/

/*
 * Copyright (c) 2005 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Nick Hudson
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
 * This code is based on the ucom driver.
 */

/*
 * Device driver for Cypress CY7C637xx and CY7C640/1xx series USB to
 * RS232 bridges.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ucycom.c,v 1.42 2015/03/07 20:20:55 mrg Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/sysctl.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/vnode.h>
#include <sys/kauth.h>
#include <sys/lwp.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>

#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/uhidev.h>
#include <dev/usb/hid.h>

#include "ioconf.h"

#ifdef UCYCOM_DEBUG
#define DPRINTF(x)	if (ucycomdebug) printf x
#define DPRINTFN(n, x)	if (ucycomdebug > (n)) printf x
int	ucycomdebug = 20;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif


#define	UCYCOMCALLUNIT_MASK	TTCALLUNIT_MASK
#define	UCYCOMUNIT_MASK		TTUNIT_MASK
#define	UCYCOMDIALOUT_MASK	TTDIALOUT_MASK

#define	UCYCOMCALLUNIT(x)	TTCALLUNIT(x)
#define	UCYCOMUNIT(x)		TTUNIT(x)
#define	UCYCOMDIALOUT(x)	TTDIALOUT(x)

/* Configuration Byte */
#define UCYCOM_RESET		0x80
#define UCYCOM_PARITY_TYPE_MASK	0x20
#define  UCYCOM_PARITY_ODD	 0x20
#define  UCYCOM_PARITY_EVEN	 0x00
#define UCYCOM_PARITY_MASK	0x10
#define  UCYCOM_PARITY_ON	 0x10
#define  UCYCOM_PARITY_OFF	 0x00
#define UCYCOM_STOP_MASK	0x08
#define  UCYCOM_STOP_BITS_2	 0x08
#define  UCYCOM_STOP_BITS_1	 0x00
#define UCYCOM_DATA_MASK	0x03
#define  UCYCOM_DATA_BITS_8	 0x03
#define  UCYCOM_DATA_BITS_7	 0x02
#define  UCYCOM_DATA_BITS_6	 0x01
#define  UCYCOM_DATA_BITS_5	 0x00

/* Modem (Input) status byte */
#define UCYCOM_RI	0x80
#define UCYCOM_DCD	0x40
#define UCYCOM_DSR	0x20
#define UCYCOM_CTS	0x10
#define UCYCOM_ERROR	0x08
#define UCYCOM_LMASK	0x07

/* Modem (Output) control byte */
#define UCYCOM_DTR	0x20
#define UCYCOM_RTS	0x10
#define UCYCOM_ORESET	0x08

struct ucycom_softc {
	struct uhidev		sc_hdev;

	struct tty		*sc_tty;

	kmutex_t sc_lock;	/* protects refcnt, others */

	/* uhidev parameters */
	size_t			sc_flen; /* feature report length */
	size_t			sc_ilen; /* input report length */
	size_t			sc_olen; /* output report length */

	uint8_t			*sc_obuf;
	int			sc_wlen;

	/* settings */
	uint32_t		sc_baud;
	uint8_t			sc_cfg;	/* Data format */
	uint8_t			sc_mcr;	/* Modem control */
	uint8_t			sc_msr;	/* Modem status */
	int			sc_swflags;

	/* flags */
	char			sc_dying;
};

dev_type_open(ucycomopen);
dev_type_close(ucycomclose);
dev_type_read(ucycomread);
dev_type_write(ucycomwrite);
dev_type_ioctl(ucycomioctl);
dev_type_stop(ucycomstop);
dev_type_tty(ucycomtty);
dev_type_poll(ucycompoll);

const struct cdevsw ucycom_cdevsw = {
	.d_open = ucycomopen,
	.d_close = ucycomclose,
	.d_read = ucycomread,
	.d_write = ucycomwrite,
	.d_ioctl = ucycomioctl,
	.d_stop = ucycomstop,
	.d_tty = ucycomtty,
	.d_poll = ucycompoll,
	.d_mmap = nommap,
	.d_kqfilter = ttykqfilter,
	.d_discard = nodiscard,
	.d_flag = D_TTY
};

Static int ucycomparam(struct tty *, struct termios *);
Static void ucycomstart(struct tty *);
Static void ucycomwritecb(usbd_xfer_handle, usbd_private_handle, usbd_status);
Static void ucycom_intr(struct uhidev *, void *, u_int);
Static int ucycom_configure(struct ucycom_softc *, uint32_t, uint8_t);
Static void tiocm_to_ucycom(struct ucycom_softc *, u_long, int);
Static int ucycom_to_tiocm(struct ucycom_softc *);
Static void ucycom_set_status(struct ucycom_softc *);
Static void ucycom_dtr(struct ucycom_softc *, int);
#if 0
Static void ucycom_rts(struct ucycom_softc *, int);
#endif
Static void ucycom_cleanup(struct ucycom_softc *sc);

#ifdef UCYCOM_DEBUG
Static void ucycom_get_cfg(struct ucycom_softc *);
#endif

Static const struct usb_devno ucycom_devs[] = {
	{ USB_VENDOR_CYPRESS, USB_PRODUCT_CYPRESS_USBRS232 },
	{ USB_VENDOR_DELORME, USB_PRODUCT_DELORME_EARTHMATE },
};
#define ucycom_lookup(v, p) usb_lookup(ucycom_devs, v, p)

int             ucycom_match(device_t, cfdata_t, void *);
void            ucycom_attach(device_t, device_t, void *);
int             ucycom_detach(device_t, int);
int             ucycom_activate(device_t, enum devact);
extern struct cfdriver ucycom_cd;
CFATTACH_DECL_NEW(ucycom, sizeof(struct ucycom_softc), ucycom_match, ucycom_attach, ucycom_detach, ucycom_activate);

int
ucycom_match(device_t parent, cfdata_t match, void *aux)
{
	struct uhidev_attach_arg *uha = aux;

	return (ucycom_lookup(uha->uaa->vendor, uha->uaa->product) != NULL ?
	    UMATCH_VENDOR_PRODUCT : UMATCH_NONE);
}

void
ucycom_attach(device_t parent, device_t self, void *aux)
{
	struct ucycom_softc *sc = device_private(self);
	struct uhidev_attach_arg *uha = aux;
	int size, repid;
	void *desc;

	sc->sc_hdev.sc_dev = self;
	sc->sc_hdev.sc_intr = ucycom_intr;
	sc->sc_hdev.sc_parent = uha->parent;
	sc->sc_hdev.sc_report_id = uha->reportid;

	uhidev_get_report_desc(uha->parent, &desc, &size);
	repid = uha->reportid;
	sc->sc_ilen = hid_report_size(desc, size, hid_input, repid);
	sc->sc_olen = hid_report_size(desc, size, hid_output, repid);
	sc->sc_flen = hid_report_size(desc, size, hid_feature, repid);

	sc->sc_msr = sc->sc_mcr = 0;

	/* set up tty */
	sc->sc_tty = tty_alloc();
	sc->sc_tty->t_sc = sc;
	sc->sc_tty->t_oproc = ucycomstart;
	sc->sc_tty->t_param = ucycomparam;

	tty_attach(sc->sc_tty);

	/* Nothing interesting to report */
	aprint_normal("\n");
}


int
ucycom_detach(device_t self, int flags)
{
	struct ucycom_softc *sc = device_private(self);
	struct tty *tp = sc->sc_tty;
	int maj, mn;
	int s;

	DPRINTF(("ucycom_detach: sc=%p flags=%d tp=%p\n", sc, flags, tp));

	sc->sc_dying = 1;

	s = splusb();
	if (tp != NULL) {
		mutex_spin_enter(&tty_lock);
		CLR(tp->t_state, TS_CARR_ON);
		CLR(tp->t_cflag, CLOCAL | MDMBUF);
		ttyflush(tp, FREAD|FWRITE);
		mutex_spin_exit(&tty_lock);
	}
	/* Wait for processes to go away. */
	usb_detach_waitold(sc->sc_hdev.sc_dev);
	splx(s);

	/* locate the major number */
	maj = cdevsw_lookup_major(&ucycom_cdevsw);

	/* Nuke the vnodes for any open instances. */
	mn = device_unit(self);

	DPRINTFN(2, ("ucycom_detach: maj=%d mn=%d\n", maj, mn));
	vdevgone(maj, mn, mn, VCHR);
	vdevgone(maj, mn | UCYCOMDIALOUT_MASK, mn | UCYCOMDIALOUT_MASK, VCHR);
	vdevgone(maj, mn | UCYCOMCALLUNIT_MASK, mn | UCYCOMCALLUNIT_MASK, VCHR);

	/* Detach and free the tty. */
	if (tp != NULL) {
		DPRINTF(("ucycom_detach: tty_detach %p\n", tp));
		tty_detach(tp);
		tty_free(tp);
		sc->sc_tty = NULL;
	}

	return 0;
}

int
ucycom_activate(device_t self, enum devact act)
{
	struct ucycom_softc *sc = device_private(self);

	DPRINTFN(5,("ucycom_activate: %d\n", act));

	switch (act) {
	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

#if 0
void
ucycom_shutdown(struct ucycom_softc *sc)
{
	struct tty *tp = sc->sc_tty;

	DPRINTF(("ucycom_shutdown\n"));
	/*
	 * Hang up if necessary.  Wait a bit, so the other side has time to
	 * notice even if we immediately open the port again.
	 */
	if (ISSET(tp->t_cflag, HUPCL)) {
		ucycom_dtr(sc, 0);
		(void)tsleep(sc, TTIPRI, ttclos, hz);
	}
}
#endif

int
ucycomopen(dev_t dev, int flag, int mode, struct lwp *l)
{
	struct ucycom_softc *sc =
	    device_lookup_private(&ucycom_cd, UCYCOMUNIT(dev));
	struct tty *tp;
	int s, err;

	DPRINTF(("ucycomopen: unit=%d\n", UCYCOMUNIT(dev)));
	DPRINTF(("ucycomopen: sc=%p\n", sc));

	if (sc == NULL)
		return (ENXIO);

	if (sc->sc_dying)
		return (EIO);

	if (!device_is_active(sc->sc_hdev.sc_dev))
		return (ENXIO);

	tp = sc->sc_tty;

	DPRINTF(("ucycomopen: tp=%p\n", tp));

	if (kauth_authorize_device_tty(l->l_cred, KAUTH_DEVICE_TTY_OPEN, tp))
		return (EBUSY);

	s = spltty();

	if (!ISSET(tp->t_state, TS_ISOPEN) && tp->t_wopen == 0) {
		struct termios t;

		tp->t_dev = dev;

		err = uhidev_open(&sc->sc_hdev);
		if (err) {
			/* Any cleanup? */
			splx(s);
			return (err);
		}

		/*
		 * Initialize the termios status to the defaults.  Add in the
		 * sticky bits from TIOCSFLAGS.
		 */
		t.c_ispeed = 0;
		t.c_ospeed = TTYDEF_SPEED;
		t.c_cflag = TTYDEF_CFLAG;
		if (ISSET(sc->sc_swflags, TIOCFLAG_CLOCAL))
			SET(t.c_cflag, CLOCAL);
		if (ISSET(sc->sc_swflags, TIOCFLAG_CRTSCTS))
			SET(t.c_cflag, CRTSCTS);
		if (ISSET(sc->sc_swflags, TIOCFLAG_MDMBUF))
			SET(t.c_cflag, MDMBUF);

		tp->t_ospeed = 0;
		(void) ucycomparam(tp, &t);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		ttychars(tp);
		ttsetwater(tp);

		/* Allocate an output report buffer */
		sc->sc_obuf = malloc(sc->sc_olen, M_USBDEV, M_WAITOK);

		DPRINTF(("ucycomopen: sc->sc_obuf=%p\n", sc->sc_obuf));

#if 0
		/* XXX Don't do this as for some reason trying to do an
		 * XXX interrupt out transfer at this point means everything
		 * XXX gets stuck!?!
		 */
		/*
		 * Turn on DTR.  We must always do this, even if carrier is not
		 * present, because otherwise we'd have to use TIOCSDTR
		 * immediately after setting CLOCAL, which applications do not
		 * expect.  We always assert DTR while the device is open
		 * unless explicitly requested to deassert it.
		 */
		ucycom_dtr(sc, 1);
#endif

#if 0
		/* XXX CLR(sc->sc_rx_flags, RX_ANY_BLOCK);*/
		ucycom_hwiflow(sc);
#endif

	}
	splx(s);

	err = ttyopen(tp, UCYCOMDIALOUT(dev), ISSET(flag, O_NONBLOCK));
	if (err)
		goto bad;

	err = (*tp->t_linesw->l_open)(dev, tp);
	if (err)
		goto bad;

	return (0);

bad:
	if (!ISSET(tp->t_state, TS_ISOPEN) && tp->t_wopen == 0) {
		/*
		 * We failed to open the device, and nobody else had it opened.
		 * Clean up the state as appropriate.
		 */
		ucycom_cleanup(sc);
	}

	return (err);

}


int
ucycomclose(dev_t dev, int flag, int mode, struct lwp *l)
{
	struct ucycom_softc *sc =
	    device_lookup_private(&ucycom_cd, UCYCOMUNIT(dev));
	struct tty *tp = sc->sc_tty;

	DPRINTF(("ucycomclose: unit=%d\n", UCYCOMUNIT(dev)));
	if (!ISSET(tp->t_state, TS_ISOPEN))
		return (0);

	(*tp->t_linesw->l_close)(tp, flag);
	ttyclose(tp);

	if (!ISSET(tp->t_state, TS_ISOPEN) && tp->t_wopen == 0) {
		/*
		 * Although we got a last close, the device may still be in
		 * use; e.g. if this was the dialout node, and there are still
		 * processes waiting for carrier on the non-dialout node.
		 */
		ucycom_cleanup(sc);
	}

	return (0);
}

Static void
ucycomstart(struct tty *tp)
{
	struct ucycom_softc *sc =
	    device_lookup_private(&ucycom_cd, UCYCOMUNIT(tp->t_dev));
	usbd_status err __unused;
	u_char *data;
	int cnt, len, s;

	if (sc->sc_dying)
		return;

	s = spltty();
	if (ISSET(tp->t_state, TS_BUSY | TS_TIMEOUT | TS_TTSTOP)) {
		DPRINTFN(4,("ucycomstart: no go, state=0x%x\n", tp->t_state));
		goto out;
	}

#if 0
	/* HW FLOW CTL */
	if (sc->sc_tx_stopped)
		goto out;
#endif

	if (ttypull(tp) == 0)
		goto out;

	/* Grab the first contiguous region of buffer space. */
	data = tp->t_outq.c_cf;
	cnt = ndqb(&tp->t_outq, 0);

	if (cnt == 0) {
		DPRINTF(("ucycomstart: cnt == 0\n"));
		goto out;
	}

	SET(tp->t_state, TS_BUSY);

	/*
	 * The 8 byte output report uses byte 0 for control and byte
	 * count.
	 *
	 * The 32 byte output report uses byte 0 for control. Byte 1
	 * is used for byte count.
	 */
	memset(sc->sc_obuf, 0, sc->sc_olen);
	len = cnt;
	switch (sc->sc_olen) {
	case 8:
		if (cnt > sc->sc_olen - 1) {
			DPRINTF(("ucycomstart(8): big buffer %d chars\n", len));
			len = sc->sc_olen - 1;
		}

		memcpy(sc->sc_obuf + 1, data, len);
		sc->sc_obuf[0] = len | sc->sc_mcr;

		DPRINTF(("ucycomstart(8): sc->sc_obuf[0] = %d | %d = %d\n", len,
		    sc->sc_mcr, sc->sc_obuf[0]));
#ifdef UCYCOM_DEBUG
		if (ucycomdebug > 10) {
			u_int32_t i;
			u_int8_t *d = data;

			DPRINTF(("ucycomstart(8): data ="));
			for (i = 0; i < len; i++)
				DPRINTF((" %02x", d[i]));
			DPRINTF(("\n"));
		}
#endif
		break;

	case 32:
		if (cnt > sc->sc_olen - 2) {
			DPRINTF(("ucycomstart(32): big buffer %d chars\n",
			    len));
			len = sc->sc_olen - 2;
		}

		memcpy(sc->sc_obuf + 2, data, len);
		sc->sc_obuf[0] = sc->sc_mcr;
		sc->sc_obuf[1] = len;
		DPRINTF(("ucycomstart(32): sc->sc_obuf[0] = %d\n"
		    "sc->sc_obuf[1] = %d\n", sc->sc_obuf[0], sc->sc_obuf[1]));
#ifdef UCYCOM_DEBUG
		if (ucycomdebug > 10) {
			u_int32_t i;
			u_int8_t *d = data;

			DPRINTF(("ucycomstart(32): data ="));
			for (i = 0; i < len; i++)
				DPRINTF((" %02x", d[i]));
			DPRINTF(("\n"));
		}
#endif
		break;

	default:
		DPRINTFN(2,("ucycomstart: unknown output report size (%zd)\n",
		    sc->sc_olen));
		goto out;
	}
	splx(s);
	sc->sc_wlen = len;

#ifdef UCYCOM_DEBUG
	if (ucycomdebug > 5) {
		int i;

		if (len != 0) {
			DPRINTF(("ucycomstart: sc->sc_obuf[0..%zd) =",
			    sc->sc_olen));
			for (i = 0; i < sc->sc_olen; i++)
				DPRINTF((" %02x", sc->sc_obuf[i]));
			DPRINTF(("\n"));
		}
	}
#endif
	DPRINTFN(4,("ucycomstart: %d chars\n", len));
	usbd_setup_xfer(sc->sc_hdev.sc_parent->sc_oxfer,
	    sc->sc_hdev.sc_parent->sc_opipe, (usbd_private_handle)sc,
	    sc->sc_obuf, sc->sc_olen, 0 /* USBD_NO_COPY */, USBD_NO_TIMEOUT,
	    ucycomwritecb);

	/* What can we do on error? */
	err = usbd_transfer(sc->sc_hdev.sc_parent->sc_oxfer);

#ifdef UCYCOM_DEBUG
	if (err != USBD_IN_PROGRESS)
		DPRINTF(("ucycomstart: err=%s\n", usbd_errstr(err)));
#endif
	return;

out:
	splx(s);
}

Static void
ucycomwritecb(usbd_xfer_handle xfer, usbd_private_handle p, usbd_status status)
{
	struct ucycom_softc *sc = (struct ucycom_softc *)p;
	struct tty *tp = sc->sc_tty;
	usbd_status stat;
	int len, s;

	if (status == USBD_CANCELLED || sc->sc_dying)
		goto error;

	if (status) {
		DPRINTF(("ucycomwritecb: status=%d\n", status));
		usbd_clear_endpoint_stall(sc->sc_hdev.sc_parent->sc_opipe);
		/* XXX we should restart after some delay. */
		goto error;
	}

	usbd_get_xfer_status(xfer, NULL, NULL, &len, &stat);

	if (status != USBD_NORMAL_COMPLETION) {
		DPRINTFN(4,("ucycomwritecb: status = %d\n", status));
		goto error;
	}

	DPRINTFN(4,("ucycomwritecb: did %d/%d chars\n", sc->sc_wlen, len));

	s = spltty();
	CLR(tp->t_state, TS_BUSY);
	if (ISSET(tp->t_state, TS_FLUSH))
		CLR(tp->t_state, TS_FLUSH);
	else
		ndflush(&tp->t_outq, sc->sc_wlen);
	(*tp->t_linesw->l_start)(tp);
	splx(s);
	return;

error:
	s = spltty();
	CLR(tp->t_state, TS_BUSY);
	splx(s);
}

Static int
ucycomparam(struct tty *tp, struct termios *t)
{
	struct ucycom_softc *sc = tp->t_sc;
	uint32_t baud;
	uint8_t cfg;
	int err;

	if (t->c_ospeed < 0) {
		DPRINTF(("ucycomparam: c_ospeed < 0\n"));
		return (EINVAL);
	}

	/* Check requested parameters. */
	if (t->c_ispeed && t->c_ispeed != t->c_ospeed)
		return (EINVAL);

	/*
	 * For the console, always force CLOCAL and !HUPCL, so that the port
	 * is always active.
	 */
	if (ISSET(sc->sc_swflags, TIOCFLAG_SOFTCAR)) {
		SET(t->c_cflag, CLOCAL);
		CLR(t->c_cflag, HUPCL);
	}

	/*
	 * If there were no changes, don't do anything.  This avoids dropping
	 * input and improves performance when all we did was frob things like
	 * VMIN and VTIME.
	 */
	if (tp->t_ospeed == t->c_ospeed &&
	    tp->t_cflag == t->c_cflag)
		return (0);

	/* XXX lcr = ISSET(sc->sc_lcr, LCR_SBREAK) | cflag2lcr(t->c_cflag); */

	/* And copy to tty. */
	tp->t_ispeed = 0;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = t->c_cflag;

	baud = t->c_ispeed;
	DPRINTF(("ucycomparam: baud=%d\n", baud));

	if (t->c_cflag & CIGNORE) {
		cfg = sc->sc_cfg;
	} else {
		cfg = 0;
		switch (t->c_cflag & CSIZE) {
		case CS8:
			cfg |= UCYCOM_DATA_BITS_8;
			break;
		case CS7:
			cfg |= UCYCOM_DATA_BITS_7;
			break;
		case CS6:
			cfg |= UCYCOM_DATA_BITS_6;
			break;
		case CS5:
			cfg |= UCYCOM_DATA_BITS_5;
			break;
		default:
			return (EINVAL);
		}
		cfg |= ISSET(t->c_cflag, CSTOPB) ?
		    UCYCOM_STOP_BITS_2 : UCYCOM_STOP_BITS_1;
		cfg |= ISSET(t->c_cflag, PARENB) ?
		    UCYCOM_PARITY_ON : UCYCOM_PARITY_OFF;
		cfg |= ISSET(t->c_cflag, PARODD) ?
		    UCYCOM_PARITY_ODD : UCYCOM_PARITY_EVEN;
	}

	/*
	 * Update the tty layer's idea of the carrier bit, in case we changed
	 * CLOCAL or MDMBUF.  We don't hang up here; we only do that by
	 * explicit request.
	 */
	DPRINTF(("ucycomparam: l_modem\n"));
	(void) (*tp->t_linesw->l_modem)(tp, 1 /* XXX carrier */ );

	err = ucycom_configure(sc, baud, cfg);
	return (err);
}

void
ucycomstop(struct tty *tp, int flag)
{
	DPRINTF(("ucycomstop: flag=%d\n", flag));
}

int
ucycomread(dev_t dev, struct uio *uio, int flag)
{
	struct ucycom_softc *sc =
	    device_lookup_private(&ucycom_cd, UCYCOMUNIT(dev));
	struct tty *tp = sc->sc_tty;
	int err;

	DPRINTF(("ucycomread: sc=%p, tp=%p, uio=%p, flag=%d\n", sc, tp, uio,
	    flag));
	if (sc->sc_dying)
		return (EIO);

	err = ((*tp->t_linesw->l_read)(tp, uio, flag));
	return (err);
}


int
ucycomwrite(dev_t dev, struct uio *uio, int flag)
{
	struct ucycom_softc *sc =
	    device_lookup_private(&ucycom_cd, UCYCOMUNIT(dev));
	struct tty *tp = sc->sc_tty;
	int err;

	DPRINTF(("ucycomwrite: sc=%p, tp=%p, uio=%p, flag=%d\n", sc, tp, uio,
	    flag));
	if (sc->sc_dying)
		return (EIO);

	err = ((*tp->t_linesw->l_write)(tp, uio, flag));
	return (err);
}

struct tty *
ucycomtty(dev_t dev)
{
	struct ucycom_softc *sc =
	    device_lookup_private(&ucycom_cd, UCYCOMUNIT(dev));
	struct tty *tp = sc->sc_tty;

	DPRINTF(("ucycomtty: sc=%p, tp=%p\n", sc, tp));

	return (tp);
}

int
ucycomioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{
	struct ucycom_softc *sc =
	    device_lookup_private(&ucycom_cd, UCYCOMUNIT(dev));
	struct tty *tp = sc->sc_tty;
	int err;
	int s;

	if (sc->sc_dying)
		return (EIO);

	DPRINTF(("ucycomioctl: sc=%p, tp=%p, data=%p\n", sc, tp, data));

	err = (*tp->t_linesw->l_ioctl)(tp, cmd, data, flag, l);
	if (err != EPASSTHROUGH)
		return (err);

	err = ttioctl(tp, cmd, data, flag, l);
	if (err != EPASSTHROUGH)
		return (err);

	err = 0;

	DPRINTF(("ucycomioctl: our cmd=0x%08lx\n", cmd));
	s = spltty();

	switch (cmd) {
/*	case TIOCSBRK:
		ucycom_break(sc, 1);
		break;

	case TIOCCBRK:
		ucycom_break(sc, 0);
		break;
*/
	case TIOCSDTR:
		ucycom_dtr(sc, 1);
		break;

	case TIOCCDTR:
		ucycom_dtr(sc, 0);
		break;

	case TIOCGFLAGS:
		*(int *)data = sc->sc_swflags;
		break;

	case TIOCSFLAGS:
		err = kauth_authorize_device_tty(l->l_cred,
		    KAUTH_DEVICE_TTY_PRIVSET, tp);
		if (err)
			break;
		sc->sc_swflags = *(int *)data;
		break;

	case TIOCMSET:
	case TIOCMBIS:
	case TIOCMBIC:
		tiocm_to_ucycom(sc, cmd, *(int *)data);
		break;

	case TIOCMGET:
		*(int *)data = ucycom_to_tiocm(sc);
		break;

	default:
		err = EPASSTHROUGH;
		break;
	}

	splx(s);

	return (err);
}

int
ucycompoll(dev_t dev, int events, struct lwp *l)
{
	struct ucycom_softc *sc =
	    device_lookup_private(&ucycom_cd, UCYCOMUNIT(dev));
	struct tty *tp = sc->sc_tty;
	int err;

	DPRINTF(("ucycompoll: sc=%p, tp=%p, events=%d, lwp=%p\n", sc, tp,
	    events, l));

	if (sc->sc_dying)
		return (EIO);

	err = ((*tp->t_linesw->l_poll)(tp, events, l));
	return (err);
}

Static int
ucycom_configure(struct ucycom_softc *sc, uint32_t baud, uint8_t cfg)
{
	uint8_t report[5];
	int err;

	switch (baud) {
	case 600:
	case 1200:
	case 2400:
	case 4800:
	case 9600:
	case 19200:
	case 38400:
	case 57600:
#if 0
	/*
	 * Stock chips only support standard baud rates in the 600 - 57600
	 * range, but higher rates can be achieved using custom firmware.
	 */
	case 115200:
	case 153600:
	case 192000:
#endif
		break;
	default:
		return (EINVAL);
	}

	DPRINTF(("ucycom_configure: setting %d baud, %d-%c-%d (%d)\n", baud,
	    5 + (cfg & UCYCOM_DATA_MASK),
	    (cfg & UCYCOM_PARITY_MASK) ?
		((cfg & UCYCOM_PARITY_TYPE_MASK) ? 'O' : 'E') : 'N',
	    (cfg & UCYCOM_STOP_MASK) ? 2 : 1, cfg));

	report[0] = baud & 0xff;
	report[1] = (baud >> 8) & 0xff;
	report[2] = (baud >> 16) & 0xff;
	report[3] = (baud >> 24) & 0xff;
	report[4] = cfg;
	err = uhidev_set_report(&sc->sc_hdev, UHID_FEATURE_REPORT,
	    report, sc->sc_flen);
	if (err != 0) {
		DPRINTF(("%s\n", usbd_errstr(err)));
		return EIO;
	}
	sc->sc_baud = baud;
	sc->sc_cfg = cfg;

#ifdef UCYCOM_DEBUG
	ucycom_get_cfg(sc);
#endif

	return 0;
}

Static void
ucycom_intr(struct uhidev *addr, void *ibuf, u_int len)
{
	struct ucycom_softc *sc = (struct ucycom_softc *)addr;
	struct tty *tp = sc->sc_tty;
	int (*rint)(int , struct tty *) = tp->t_linesw->l_rint;
	uint8_t *cp = ibuf;
	int s, n, st, chg;

	/* We understand 8 byte and 32 byte input records */
	switch (len) {
	case 8:
		n = cp[0] & UCYCOM_LMASK;
		st = cp[0] & ~UCYCOM_LMASK;
		cp++;
		break;

	case 32:
		st = cp[0];
		n = cp[1];
		cp += 2;
		break;

	default:
		DPRINTFN(3,("ucycom_intr: Unknown input report length\n"));
		return;
	}

#ifdef UCYCOM_DEBUG
	if (ucycomdebug > 5) {
		u_int32_t i;

		if (n != 0) {
			DPRINTF(("ucycom_intr: ibuf[0..%d) =", n));
			for (i = 0; i < n; i++)
				DPRINTF((" %02x", cp[i]));
			DPRINTF(("\n"));
		}
	}
#endif

	/* Give characters to tty layer. */
	s = spltty();
	while (n-- > 0) {
		DPRINTFN(7,("ucycom_intr: char=0x%02x\n", *cp));
		if ((*rint)(*cp++, tp) == -1) {
			/* XXX what should we do? */
			aprint_error_dev(sc->sc_hdev.sc_dev,
			    "lost a character\n");
			break;
		}
	}
	splx(s);
	chg = st ^ sc->sc_msr;
	sc->sc_msr = st;
	if (ISSET(chg, UCYCOM_DCD))
		(*tp->t_linesw->l_modem)(tp,
		    ISSET(sc->sc_msr, UCYCOM_DCD));

}

Static void
tiocm_to_ucycom(struct ucycom_softc *sc, u_long how, int ttybits)
{
	u_char combits;
	u_char before = sc->sc_mcr;

	combits = 0;
	if (ISSET(ttybits, TIOCM_DTR))
		SET(combits, UCYCOM_DTR);
	if (ISSET(ttybits, TIOCM_RTS))
		SET(combits, UCYCOM_RTS);

	switch (how) {
	case TIOCMBIC:
		CLR(sc->sc_mcr, combits);
		break;

	case TIOCMBIS:
		SET(sc->sc_mcr, combits);
		break;

	case TIOCMSET:
		CLR(sc->sc_mcr, UCYCOM_DTR | UCYCOM_RTS);
		SET(sc->sc_mcr, combits);
		break;
	}
	if (before ^ sc->sc_mcr) {
		DPRINTF(("tiocm_to_ucycom: something has changed\n"));
		ucycom_set_status(sc);
	}
}

Static int
ucycom_to_tiocm(struct ucycom_softc *sc)
{
	u_char combits;
	int ttybits = 0;

	combits = sc->sc_mcr;
	if (ISSET(combits, UCYCOM_DTR))
		SET(ttybits, TIOCM_DTR);
	if (ISSET(combits, UCYCOM_RTS))
		SET(ttybits, TIOCM_RTS);

	combits = sc->sc_msr;
	if (ISSET(combits, UCYCOM_DCD))
		SET(ttybits, TIOCM_CD);
	if (ISSET(combits, UCYCOM_CTS))
		SET(ttybits, TIOCM_CTS);
	if (ISSET(combits, UCYCOM_DSR))
		SET(ttybits, TIOCM_DSR);
	if (ISSET(combits, UCYCOM_RI))
		SET(ttybits, TIOCM_RI);

	return (ttybits);
}

Static void
ucycom_dtr(struct ucycom_softc *sc, int set)
{
	uint8_t old;

	old = sc->sc_mcr;
	if (set)
		SET(sc->sc_mcr, UCYCOM_DTR);
	else
		CLR(sc->sc_mcr, UCYCOM_DTR);

	if (old ^ sc->sc_mcr)
		ucycom_set_status(sc);
}

#if 0
Static void
ucycom_rts(struct ucycom_softc *sc, int set)
{
	uint8_t old;

	old = sc->sc_msr;
	if (set)
		SET(sc->sc_mcr, UCYCOM_RTS);
	else
		CLR(sc->sc_mcr, UCYCOM_RTS);

	if (old ^ sc->sc_mcr)
		ucycom_set_status(sc);
}
#endif

Static void
ucycom_set_status(struct ucycom_softc *sc)
{
	int err;

	if (sc->sc_olen != 8 && sc->sc_olen != 32) {
		DPRINTFN(2,("ucycom_set_status: unknown output report "
		    "size (%zd)\n", sc->sc_olen));
		return;
	}

	DPRINTF(("ucycom_set_status: %d\n", sc->sc_mcr));

	memset(sc->sc_obuf, 0, sc->sc_olen);
	sc->sc_obuf[0] = sc->sc_mcr;

	err = uhidev_write(sc->sc_hdev.sc_parent, sc->sc_obuf, sc->sc_olen);
	if (err) {
		DPRINTF(("ucycom_set_status: err=%d\n", err));
	}
}

#ifdef UCYCOM_DEBUG
Static void
ucycom_get_cfg(struct ucycom_softc *sc)
{
	int err, cfg, baud;
	uint8_t report[5];

	err = uhidev_get_report(&sc->sc_hdev, UHID_FEATURE_REPORT,
	    report, sc->sc_flen);
	if (err) {
		DPRINTF(("%s: failed\n", __func__));
		return;
	}
	cfg = report[4];
	baud = (report[3] << 24) + (report[2] << 16) + (report[1] << 8) +
	    report[0];
	DPRINTF(("ucycom_get_cfg: device reports %d baud, %d-%c-%d (%d)\n",
	    baud, 5 + (cfg & UCYCOM_DATA_MASK),
	    (cfg & UCYCOM_PARITY_MASK) ?
		((cfg & UCYCOM_PARITY_TYPE_MASK) ? 'O' : 'E') : 'N',
	    (cfg & UCYCOM_STOP_MASK) ? 2 : 1, cfg));
}
#endif

Static void
ucycom_cleanup(struct ucycom_softc *sc)
{
	uint8_t	*obuf;

	DPRINTF(("ucycom_cleanup: closing uhidev\n"));

	obuf = sc->sc_obuf;
	sc->sc_obuf = NULL;
	uhidev_close(&sc->sc_hdev);

	if (obuf != NULL)
		free (obuf, M_USBDEV);
}
