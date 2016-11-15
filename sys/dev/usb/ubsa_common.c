/*	$NetBSD: ubsa_common.c,v 1.9 2012/12/11 09:17:31 msaitoh Exp $	*/
/*-
 * Copyright (c) 2002, Alexander Kabaev <kan.FreeBSD.org>.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Ichiro FUKUHARA (ichiro@ichiro.org).
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
__KERNEL_RCSID(0, "$NetBSD: ubsa_common.c,v 1.9 2012/12/11 09:17:31 msaitoh Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/ioccom.h>
#include <sys/fcntl.h>
#include <sys/conf.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/poll.h>
#include <sys/sysctl.h>
#include <sys/bus.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>

#include <dev/usb/usbcdc.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/usb_quirks.h>
#include <dev/usb/ucomvar.h>
#include <dev/usb/ubsavar.h>

#ifdef UBSA_DEBUG
extern	int	ubsadebug;
#define	DPRINTFN(n, x)	do { \
				if (ubsadebug > (n)) \
					printf x; \
			} while (0)
#else
#define	DPRINTFN(n, x)
#endif
#define	DPRINTF(x) DPRINTFN(0, x)

int
ubsa_request(struct ubsa_softc *sc, int portno, u_int8_t request, u_int16_t value)
{
	usb_device_request_t req;
	usbd_status err;

	if (sc->sc_quadumts)
		req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	else
		req.bmRequestType = UT_WRITE_VENDOR_DEVICE;

	if (portno >= UBSA_MAXCONN) {
		printf("%s: ubsa_request: invalid port(%d)#\n",
			device_xname(sc->sc_dev), portno);
		return USBD_INVAL; 
	}

	req.bRequest = request;
	USETW(req.wValue, value);
	USETW(req.wIndex, sc->sc_iface_number[portno]);
	USETW(req.wLength, 0);

	err = usbd_do_request(sc->sc_udev, &req, 0);
	if (err)
		printf("%s: ubsa_request: %s\n",
		    device_xname(sc->sc_dev), usbd_errstr(err));
	return (err);
}

void
ubsa_dtr(struct ubsa_softc *sc, int portno, int onoff)
{

	DPRINTF(("ubsa_dtr: onoff = %d\n", onoff));

	if (sc->sc_dtr == onoff)
		return;
	sc->sc_dtr = onoff;

	ubsa_request(sc, portno, UBSA_SET_DTR, onoff ? 1 : 0);
}

void
ubsa_rts(struct ubsa_softc *sc, int portno, int onoff)
{

	DPRINTF(("ubsa_rts: onoff = %d\n", onoff));

	if (sc->sc_rts == onoff)
		return;
	sc->sc_rts = onoff;

	ubsa_request(sc, portno, UBSA_SET_RTS, onoff ? 1 : 0);
}

void
ubsa_quadumts_dtr(struct ubsa_softc *sc, int portno, int onoff)
{

	DPRINTF(("ubsa_dtr: onoff = %d\n", onoff));

	if (sc->sc_dtr == onoff)
		return;
	sc->sc_dtr = onoff;

	ubsa_request(sc, portno, UBSA_QUADUMTS_SET_PIN,
		 (sc->sc_rts ? 2 : 0)+(sc->sc_dtr ? 1 : 0));
}

void
ubsa_quadumts_rts(struct ubsa_softc *sc, int portno, int onoff)
{

	DPRINTF(("ubsa_rts: onoff = %d\n", onoff));

	if (sc->sc_rts == onoff)
		return;
	sc->sc_rts = onoff;

	ubsa_request(sc, portno, UBSA_QUADUMTS_SET_PIN,
		 (sc->sc_rts ? 2 : 0)+(sc->sc_dtr ? 1 : 0));
}

void
ubsa_break(struct ubsa_softc *sc, int portno, int onoff)
{
	DPRINTF(("ubsa_rts: onoff = %d\n", onoff));

	ubsa_request(sc, portno, UBSA_SET_BREAK, onoff ? 1 : 0);
}

void
ubsa_set(void *addr, int portno, int reg, int onoff)
{
	struct ubsa_softc *sc;

	sc = addr;
	switch (reg) {
	case UCOM_SET_DTR:
		if (sc->sc_quadumts)
			ubsa_quadumts_dtr(sc, portno, onoff);
		else
			ubsa_dtr(sc, portno, onoff);
		break;
	case UCOM_SET_RTS:
		if (sc->sc_quadumts)
			ubsa_quadumts_rts(sc, portno, onoff);
		else
			ubsa_rts(sc, portno, onoff);
		break;
	case UCOM_SET_BREAK:
		if (!sc->sc_quadumts)
			ubsa_break(sc, portno, onoff);
		break;
	default:
		break;
	}
}

void
ubsa_baudrate(struct ubsa_softc *sc, int portno, speed_t speed)
{
	u_int16_t value = 0;

	DPRINTF(("ubsa_baudrate: speed = %d\n", speed));

	switch(speed) {
	case B0:
		break;
	case B300:
	case B600:
	case B1200:
	case B2400:
	case B4800:
	case B9600:
	case B19200:
	case B38400:
	case B57600:
	case B115200:
	case B230400:
		value = B230400 / speed;
		break;
	default:
		printf("%s: ubsa_param: unsupported baudrate, "
		    "forcing default of 9600\n",
		    device_xname(sc->sc_dev));
		value = B230400 / B9600;
		break;
	};

	if (speed == B0) {
		ubsa_flow(sc, portno, 0, 0);
		ubsa_dtr(sc, portno, 0);
		ubsa_rts(sc, portno, 0);
	} else
		ubsa_request(sc, portno, UBSA_SET_BAUDRATE, value);
}

void
ubsa_parity(struct ubsa_softc *sc, int portno, tcflag_t cflag)
{
	int value;

	DPRINTF(("ubsa_parity: cflag = 0x%x\n", cflag));

	if (cflag & PARENB)
		value = (cflag & PARODD) ? UBSA_PARITY_ODD : UBSA_PARITY_EVEN;
	else
		value = UBSA_PARITY_NONE;

	ubsa_request(sc, portno, UBSA_SET_PARITY, value);
}

void
ubsa_databits(struct ubsa_softc *sc, int portno, tcflag_t cflag)
{
	int value;

	DPRINTF(("ubsa_databits: cflag = 0x%x\n", cflag));

	switch (cflag & CSIZE) {
	case CS5: value = 0; break;
	case CS6: value = 1; break;
	case CS7: value = 2; break;
	case CS8: value = 3; break;
	default:
		printf("%s: ubsa_param: unsupported databits requested, "
		    "forcing default of 8\n",
		    device_xname(sc->sc_dev));
		value = 3;
	}

	ubsa_request(sc, portno, UBSA_SET_DATA_BITS, value);
}

void
ubsa_stopbits(struct ubsa_softc *sc, int portno, tcflag_t cflag)
{
	int value;

	DPRINTF(("ubsa_stopbits: cflag = 0x%x\n", cflag));

	value = (cflag & CSTOPB) ? 1 : 0;

	ubsa_request(sc, portno, UBSA_SET_STOP_BITS, value);
}

void
ubsa_flow(struct ubsa_softc *sc, int portno, tcflag_t cflag, tcflag_t iflag)
{
	int value;

	DPRINTF(("ubsa_flow: cflag = 0x%x, iflag = 0x%x\n", cflag, iflag));

	value = 0;
	if (cflag & CRTSCTS)
		value |= UBSA_FLOW_OCTS | UBSA_FLOW_IRTS;
	if (iflag & (IXON|IXOFF))
		value |= UBSA_FLOW_OXON | UBSA_FLOW_IXON;

	ubsa_request(sc, portno, UBSA_SET_FLOW_CTRL, value);
}

int
ubsa_param(void *addr, int portno, struct termios *ti)
{
	struct ubsa_softc *sc = addr;

	DPRINTF(("ubsa_param: sc = %p\n", sc));

	if (!sc->sc_quadumts) {
		ubsa_baudrate(sc, portno, ti->c_ospeed);
		ubsa_parity(sc, portno, ti->c_cflag);
		ubsa_databits(sc, portno, ti->c_cflag);
		ubsa_stopbits(sc, portno, ti->c_cflag);
		ubsa_flow(sc, portno, ti->c_cflag, ti->c_iflag);
	}

	return (0);
}

int
ubsa_open(void *addr, int portno)
{
	struct ubsa_softc *sc = addr;
	int err;

	if (sc->sc_dying)
		return (ENXIO);

	if (sc->sc_intr_number != -1 && sc->sc_intr_pipe == NULL) {
		sc->sc_intr_buf = malloc(sc->sc_isize, M_USBDEV, M_WAITOK);
		/* XXX only iface# = 0 has intr line */
		/* XXX E220 specific? need to check */
		err = usbd_open_pipe_intr(sc->sc_iface[0],
		    sc->sc_intr_number,
		    USBD_SHORT_XFER_OK,
		    &sc->sc_intr_pipe,
		    sc,
		    sc->sc_intr_buf,
		    sc->sc_isize,
		    ubsa_intr,
		    UBSA_INTR_INTERVAL);
		if (err) {
			printf("%s: cannot open interrupt pipe (addr %d)\n",
			    device_xname(sc->sc_dev),
			    sc->sc_intr_number);
			return (EIO);
		}
	}

	return (0);
}

void
ubsa_close(void *addr, int portno)
{
	struct ubsa_softc *sc = addr;
	int err;

	if (sc->sc_dying)
		return;

	DPRINTF(("ubsa_close: close\n"));

	if (sc->sc_intr_pipe != NULL) {
		err = usbd_abort_pipe(sc->sc_intr_pipe);
		if (err)
			printf("%s: abort interrupt pipe failed: %s\n",
			    device_xname(sc->sc_dev),
			    usbd_errstr(err));
		err = usbd_close_pipe(sc->sc_intr_pipe);
		if (err)
			printf("%s: close interrupt pipe failed: %s\n",
			    device_xname(sc->sc_dev),
			    usbd_errstr(err));
		free(sc->sc_intr_buf, M_USBDEV);
		sc->sc_intr_pipe = NULL;
	}
}

void
ubsa_intr(usbd_xfer_handle xfer, usbd_private_handle priv,
    usbd_status status)
{
	struct ubsa_softc *sc = priv;
	u_char *buf;
	int i;

	buf = sc->sc_intr_buf;
	if (sc->sc_dying)
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;

		DPRINTF(("%s: ubsa_intr: abnormal status: %s\n",
		    device_xname(sc->sc_dev), usbd_errstr(status)));
		usbd_clear_endpoint_stall_async(sc->sc_intr_pipe);
		return;
	}

	/* incidentally, Belkin adapter status bits match UART 16550 bits */
	sc->sc_lsr = buf[2];
	sc->sc_msr = buf[3];

	DPRINTF(("%s: ubsa lsr = 0x%02x, msr = 0x%02x\n",
	    device_xname(sc->sc_dev), sc->sc_lsr, sc->sc_msr));

	for (i = 0; i < sc->sc_numif; i++) {
		ucom_status_change(device_private(sc->sc_subdevs[i]));
	}
}

void
ubsa_get_status(void *addr, int portno, u_char *lsr, u_char *msr)
{
	struct ubsa_softc *sc = addr;

	DPRINTF(("ubsa_get_status\n"));

	if (lsr != NULL)
		*lsr = sc->sc_lsr;
	if (msr != NULL)
		*msr = sc->sc_msr;
}

