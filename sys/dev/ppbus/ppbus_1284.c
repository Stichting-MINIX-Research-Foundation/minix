/* $NetBSD: ppbus_1284.c,v 1.13 2014/07/13 17:12:23 dholland Exp $ */

/*-
 * Copyright (c) 1997 Nicolas Souchu
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
 *
 * FreeBSD: src/sys/dev/ppbus/ppb_1284.c,v 1.11 2000/01/14 08:03:14 nsouch Exp
 *
 */

/* General purpose routines for the IEEE1284-1994 Standard */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ppbus_1284.c,v 1.13 2014/07/13 17:12:23 dholland Exp $");

#include "opt_ppbus_1284.h"

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#include <dev/ppbus/ppbus_conf.h>
#include <dev/ppbus/ppbus_base.h>
#include <dev/ppbus/ppbus_1284.h>
#include <dev/ppbus/ppbus_io.h>
#include <dev/ppbus/ppbus_var.h>


/* Wait for the peripheral up to 40ms */
static int
do_1284_wait(struct ppbus_softc * bus, char mask, char status)
{
	return (ppbus_poll_bus(bus->sc_dev, 4, mask, status,
		PPBUS_NOINTR | PPBUS_POLL));
}

/* Wait for the host up to 1 second (peripheral side) */
static int
do_peripheral_wait(struct ppbus_softc * bus, char mask, char status)
{
	return (ppbus_poll_bus(bus->sc_dev, 100, mask, status,
		PPBUS_NOINTR | PPBUS_POLL));
}


/* Unconditionaly reset the error field */
static int
ppbus_1284_reset_error(struct ppbus_softc * bus, int state)
{
	bus->sc_1284_error = PPBUS_NO_ERROR;
	bus->sc_1284_state = state;
	return 0;
}


/* Get IEEE1284 state */
int
ppbus_1284_get_state(device_t dev)
{
	struct ppbus_softc *sc = device_private(dev);

	return sc->sc_1284_state;
}


/* Set IEEE1284 state if no error occurred */
int
ppbus_1284_set_state(device_t dev, int state)
{
	struct ppbus_softc * bus = device_private(dev);

	/* call ppbus_1284_reset_error() if you absolutly want to change
	 * the state from PPBUS_ERROR to another */
	if ((bus->sc_1284_state != PPBUS_ERROR) &&
			(bus->sc_1284_error == PPBUS_NO_ERROR)) {
		bus->sc_1284_state = state;
		bus->sc_1284_error = PPBUS_NO_ERROR;
	}

	return 0;
}


/* Set the IEEE1284 error field */
static int
ppbus_1284_set_error(struct ppbus_softc * bus, int error, int event)
{
	/* do not accumulate errors */
	if ((bus->sc_1284_error == PPBUS_NO_ERROR) &&
			(bus->sc_1284_state != PPBUS_ERROR)) {
		bus->sc_1284_error = error;
		bus->sc_1284_state = PPBUS_ERROR;
	}

#ifdef DEBUG_1284
	printf("%s<1284>: error=%d status=0x%x event=%d\n",
		device_xname(bus->sc_dev), error, ppbus_rstr(bus->sc_dev),
		event);

#endif

	return 0;
}


/* Converts mode+options into ext. value */
static int
ppbus_request_mode(int mode, int options)
{
	int request_mode = 0;

	if (options & PPBUS_EXTENSIBILITY_LINK) {
		request_mode = EXT_LINK_1284_NORMAL;

	}
	else {
		switch (mode) {
		case PPBUS_NIBBLE:
			request_mode = (options & PPBUS_REQUEST_ID) ?
					NIBBLE_1284_REQUEST_ID :
					NIBBLE_1284_NORMAL;
			break;
		case PPBUS_PS2:
			request_mode = (options & PPBUS_REQUEST_ID) ?
					BYTE_1284_REQUEST_ID :
					BYTE_1284_NORMAL;
			break;
		case PPBUS_ECP:
			if (options & PPBUS_USE_RLE)
				request_mode = (options & PPBUS_REQUEST_ID) ?
					ECP_1284_RLE_REQUEST_ID :
					ECP_1284_RLE;
			else
				request_mode = (options & PPBUS_REQUEST_ID) ?
					ECP_1284_REQUEST_ID :
					ECP_1284_NORMAL;
			break;
		case PPBUS_EPP:
			request_mode = EPP_1284_NORMAL;
			break;
		default:
			panic("%s: unsupported mode %d\n", __func__, mode);
		}
	}

	return (request_mode);
}


/* Negotiate the peripheral side */
int
ppbus_peripheral_negotiate(device_t dev, int mode, int options)
{
	struct ppbus_softc * bus = device_private(dev);
	int spin, request_mode, error = 0;
	char r;

	ppbus_1284_terminate(dev);
	ppbus_1284_set_state(dev, PPBUS_PERIPHERAL_NEGOTIATION);

	/* compute ext. value */
	request_mode = ppbus_request_mode(mode, options);

	/* wait host */
	spin = 10;
	while (spin-- && (ppbus_rstr(dev) & nBUSY))
		DELAY(1);

	/* check termination */
	if (!(ppbus_rstr(dev) & SELECT) || !spin) {
		error = ENODEV;
		goto error;
	}

	/* Event 4 - read ext. value */
	r = ppbus_rdtr(dev);

	/* nibble mode is not supported */
	if ((r == (char)request_mode) ||
			(r == NIBBLE_1284_NORMAL)) {

		/* Event 5 - restore direction bit, no data avail */
		ppbus_wctr(dev, (STROBE | nINIT) & ~(SELECTIN));
		DELAY(1);

		/* Event 6 */
		ppbus_wctr(dev, (nINIT) & ~(SELECTIN | STROBE));

		if (r == NIBBLE_1284_NORMAL) {
#ifdef DEBUG_1284
			printf("R");
#endif
			ppbus_1284_set_error(bus, PPBUS_MODE_UNSUPPORTED, 4);
			error = EINVAL;
			goto error;
		}
		else {
			ppbus_1284_set_state(dev, PPBUS_PERIPHERAL_IDLE);
#ifdef DEBUG_1284
			printf("A");
#endif
			/* negotiation succeeds */
		}
	}
	else {
		/* Event 5 - mode not supported */
		ppbus_wctr(dev, SELECTIN);
		DELAY(1);

		/* Event 6 */
		ppbus_wctr(dev, (SELECTIN) & ~(STROBE | nINIT));
		ppbus_1284_set_error(bus, PPBUS_MODE_UNSUPPORTED, 4);

#ifdef DEBUG_1284
		printf("r");
#endif
		error = EINVAL;
		goto error;
	}

	return (0);

error:
	ppbus_peripheral_terminate(dev, PPBUS_WAIT);
	return (error);
}


/* Terminate peripheral transfer side. Always return 0 in compatible mode */
int
ppbus_peripheral_terminate(device_t dev, int how)
{
	struct ppbus_softc * bus = device_private(dev);
	int error = 0;

#ifdef DEBUG_1284
	printf("t");
#endif

	ppbus_1284_set_state(dev, PPBUS_PERIPHERAL_TERMINATION);

	/* Event 22 - wait up to host response time (1s) */
	if ((error = do_peripheral_wait(bus, SELECT | nBUSY, 0))) {
		ppbus_1284_set_error(bus, PPBUS_TIMEOUT, 22);
		goto error;
	}

	/* Event 24 */
        ppbus_wctr(dev, (nINIT | STROBE) & ~(AUTOFEED | SELECTIN));

	/* Event 25 - wait up to host response time (1s) */
	if ((error = do_peripheral_wait(bus, nBUSY, nBUSY))) {
		ppbus_1284_set_error(bus, PPBUS_TIMEOUT, 25);
		goto error;
	}

	/* Event 26 */
        ppbus_wctr(dev, (SELECTIN | nINIT | STROBE) & ~(AUTOFEED));
	DELAY(1);
	/* Event 27 */
        ppbus_wctr(dev, (SELECTIN | nINIT) & ~(STROBE | AUTOFEED));

	/* Event 28 - wait up to host response time (1s) */
	if ((error = do_peripheral_wait(bus, nBUSY, 0))) {
		ppbus_1284_set_error(bus, PPBUS_TIMEOUT, 28);
		goto error;
	}

error:
	ppbus_1284_terminate(dev);
	ppbus_1284_set_state(dev, PPBUS_FORWARD_IDLE);

	return (0);
}


/* Write 1 byte to host in BYTE mode (peripheral side) */
static int
byte_peripheral_outbyte(device_t dev, char *buffer, int last)
{
	struct ppbus_softc * bus = device_private(dev);
	int error = 0;

	/* Event 7 */
	if ((error = do_1284_wait(bus, nBUSY, nBUSY))) {
		ppbus_1284_set_error(bus, PPBUS_TIMEOUT, 7);
		goto error;
	}

	/* check termination */
	if (!(ppbus_rstr(dev) & SELECT)) {
		ppbus_peripheral_terminate(dev, PPBUS_WAIT);
		goto error;
	}

	/* Event 15 - put byte on data lines */
#ifdef DEBUG_1284
	printf("B");
#endif
	ppbus_wdtr(dev, *buffer);

	/* Event 9 */
	ppbus_wctr(dev, (AUTOFEED | STROBE) & ~(nINIT | SELECTIN));

	/* Event 10 - wait data read */
	if ((error = do_peripheral_wait(bus, nBUSY, 0))) {
		ppbus_1284_set_error(bus, PPBUS_TIMEOUT, 16);
		goto error;
	}

	/* Event 11 */
	if (!last) {
		ppbus_wctr(dev, (AUTOFEED) & ~(nINIT | STROBE | SELECTIN));
	} else {
		ppbus_wctr(dev, (nINIT) & ~(STROBE | SELECTIN | AUTOFEED));
	}

#if 0
	/* Event 16 - wait strobe */
	if ((error = do_peripheral_wait(bus, nACK | nBUSY, 0))) {
		ppbus_1284_set_error(bus, PPBUS_TIMEOUT, 16);
		goto error;
	}
#endif

	/* check termination */
	if (!(ppbus_rstr(dev) & SELECT)) {
		ppbus_peripheral_terminate(dev, PPBUS_WAIT);
		goto error;
	}

error:
	return (error);
}


/* Write n bytes to host in BYTE mode (peripheral side) */
int
byte_peripheral_write(device_t dev, char *buffer, int len,
	int *sent)
{
	int error = 0, i;
	char r;

	ppbus_1284_set_state(dev, PPBUS_PERIPHERAL_TRANSFER);

	/* wait forever, the remote host is master and should initiate
	 * termination
	 */
	for(i = 0; i < len; i++) {
		/* force remote nFAULT low to release the remote waiting
		 * process, if any
		 */
		r = ppbus_rctr(dev);
		ppbus_wctr(dev, r & ~nINIT);

#ifdef DEBUG_1284
		printf("y");
#endif
		/* Event 7 */
		error = ppbus_poll_bus(dev, PPBUS_FOREVER, nBUSY, nBUSY,
					PPBUS_INTR);

		if (error && error != EWOULDBLOCK)
			goto error;

#ifdef DEBUG_1284
		printf("b");
#endif
		if ((error = byte_peripheral_outbyte(dev, buffer+i, (i == len-1))))
			goto error;
	}
error:
	if (!error)
		ppbus_1284_set_state(dev, PPBUS_PERIPHERAL_IDLE);

	*sent = i;
	return (error);
}


/* Read the device ID using the specified mode */
int
ppbus_1284_read_id(device_t dev, int mode, char ** buffer,
		size_t * size, size_t * read)
{
	u_int16_t msg_sz;
	u_int8_t length_field;
	u_int8_t old_mode;
	int error;
	int old_ivar;
	int new_ivar = 1;

	error = ppbus_read_ivar(dev, PPBUS_IVAR_IEEE, &old_ivar);
	if(error) {
		printf("%s(%s): error reading PPBUS_IVAR_IEEE.\n", __func__,
			device_xname(dev));
		return error;
	}
	if(old_ivar == 0) {
		error = ppbus_write_ivar(dev, PPBUS_IVAR_IEEE, &new_ivar);
		if(error) {
			printf("%s(%s): error enabling IEEE usage.\n", __func__,
				device_xname(dev));
			return error;
		}
	}

	old_mode = ppbus_get_mode(dev);
	switch (mode) {
	case PPBUS_NIBBLE:
	case PPBUS_ECP:
	case PPBUS_BYTE:
		error = ppbus_set_mode(dev, mode, PPBUS_REQUEST_ID);
		if(error) {
			printf("%s(%s): error setting bus mode.\n", __func__,
				device_xname(dev));
			goto end_read_id;
		}
		break;
	default:
		printf("%s(%s): mode does not support returning device ID.\n",
			__func__, device_xname(dev));
		error = ENODEV;
		goto end_read_id;
	}

	error = ppbus_read(dev, &length_field, 1, 0, read);
	if(error) {
		printf("%s(%s): error reading first byte.\n", __func__,
			device_xname(dev));
		goto end_read_id;
	}
	msg_sz = length_field;
	error = ppbus_read(dev, &length_field, 1, 0, read);
	if(error) {
		printf("%s(%s): error reading second byte.\n",
			__func__, device_xname(dev));
		goto end_read_id;
	}
	msg_sz <<= 8;
	msg_sz |= length_field;
	msg_sz -= 2;
	if(msg_sz <= 0) {
		printf("%s(%s): device ID length <= 0.\n", __func__,
			device_xname(dev));
		goto end_read_id;
	}
	*buffer = malloc(msg_sz, M_DEVBUF, M_WAITOK);
	*size = msg_sz;
	error = ppbus_read(dev, *buffer, msg_sz, 0, read);

end_read_id:
	ppbus_set_mode(dev, old_mode, 0);
	if(old_ivar == 0) {
		if(ppbus_write_ivar(dev, PPBUS_IVAR_IEEE, &old_ivar)) {
			printf("%s(%s): error restoring PPBUS_IVAR_IEEE.\n",
				__func__, device_xname(dev));
		}
	}
	return (error);
}

/*
 * IEEE1284 negotiation phase: after negotiation, nFAULT is low if data is
 * available for reverse modes.
 */
int
ppbus_1284_negotiate(device_t dev, int mode, int options)
{
	struct ppbus_softc * bus = device_private(dev);
	int error;
	int request_mode;

#ifdef DEBUG_1284
	printf("n");
#endif

	if (ppbus_1284_get_state(dev) >= PPBUS_PERIPHERAL_NEGOTIATION)
		ppbus_peripheral_terminate(dev, PPBUS_WAIT);

#ifdef DEBUG_1284
	printf("%d", mode);
#endif

	/* ensure the host is in compatible mode */
	ppbus_1284_terminate(dev);

	/* reset error to catch the actual negotiation error */
	ppbus_1284_reset_error(bus, PPBUS_FORWARD_IDLE);

	/* calculate ext. value */
	request_mode = ppbus_request_mode(mode, options);

	/* default state */
	ppbus_wctr(dev, (nINIT | SELECTIN) & ~(STROBE | AUTOFEED));
	DELAY(1);

	/* enter negotiation phase */
	ppbus_1284_set_state(dev, PPBUS_NEGOTIATION);

	/* Event 0 - put the exten. value on the data lines */
	ppbus_wdtr(dev, request_mode);

#ifdef PERIPH_1284
	/* request remote host attention */
        ppbus_wctr(dev, (nINIT | STROBE) & ~(AUTOFEED | SELECTIN));
        DELAY(1);
        ppbus_wctr(dev, (nINIT) & ~(STROBE | AUTOFEED | SELECTIN));
#else
	DELAY(1);

#endif /* !PERIPH_1284 */

	/* Event 1 - enter IEEE1284 mode */
	ppbus_wctr(dev, (nINIT | AUTOFEED) & ~(STROBE | SELECTIN));

#ifdef PERIPH_1284
	/* ignore the PError line, wait a bit more, remote host's
	 * interrupts don't respond fast enough */
	if (ppbus_poll_bus(bus, 40, nACK | SELECT | nFAULT,
				SELECT | nFAULT, PPBUS_NOINTR | PPBUS_POLL)) {
                ppbus_1284_set_error(bus, PPBUS_NOT_IEEE1284, 2);
                error = ENODEV;
                goto error;
        }
#else
	/* Event 2 - trying IEEE1284 dialog */
	if (do_1284_wait(bus, nACK | PERROR | SELECT | nFAULT,
			PERROR  | SELECT | nFAULT)) {
		ppbus_1284_set_error(bus, PPBUS_NOT_IEEE1284, 2);
		error = ENODEV;
		goto error;
	}
#endif /* !PERIPH_1284 */

	/* Event 3 - latch the ext. value to the peripheral */
	ppbus_wctr(dev, (nINIT | STROBE | AUTOFEED) & ~SELECTIN);
	DELAY(1);

	/* Event 4 - IEEE1284 device recognized */
	ppbus_wctr(dev, nINIT & ~(SELECTIN | AUTOFEED | STROBE));

	/* Event 6 - waiting for status lines */
	if (do_1284_wait(bus, nACK, nACK)) {
		ppbus_1284_set_error(bus, PPBUS_TIMEOUT, 6);
		error = EBUSY;
		goto error;
	}

	/* Event 7 - quering result consider nACK not to misunderstand
	 * a remote computer terminate sequence */
	if (options & PPBUS_EXTENSIBILITY_LINK) {
		/* XXX not fully supported yet */
		ppbus_1284_terminate(dev);
		error = ENODEV;
		goto error;
		/* return (0); */
	}
	if (request_mode == NIBBLE_1284_NORMAL) {
		if (do_1284_wait(bus, nACK | SELECT, nACK)) {
			ppbus_1284_set_error(bus, PPBUS_MODE_UNSUPPORTED, 7);
			error = ENODEV;
			goto error;
		}
	} else {
		if (do_1284_wait(bus, nACK | SELECT, SELECT | nACK)) {
			ppbus_1284_set_error(bus, PPBUS_MODE_UNSUPPORTED, 7);
			error = ENODEV;
			goto error;
		}
	}

	switch (mode) {
	case PPBUS_NIBBLE:
	case PPBUS_PS2:
		/* enter reverse idle phase */
		ppbus_1284_set_state(dev, PPBUS_REVERSE_IDLE);
		break;
	case PPBUS_ECP:
		/* negotiation ok, now setup the communication */
		ppbus_1284_set_state(dev, PPBUS_SETUP);
		ppbus_wctr(dev, (nINIT | AUTOFEED) & ~(SELECTIN | STROBE));

#ifdef PERIPH_1284
		/* ignore PError line */
		if (do_1284_wait(bus, nACK | SELECT | nBUSY,
                                        nACK | SELECT | nBUSY)) {
                        ppbus_1284_set_error(bus, PPBUS_TIMEOUT, 30);
                        error = ENODEV;
                        goto error;
                }
#else
		if (do_1284_wait(bus, nACK | SELECT | PERROR | nBUSY,
					nACK | SELECT | PERROR | nBUSY)) {
			ppbus_1284_set_error(bus, PPBUS_TIMEOUT, 30);
			error = ENODEV;
			goto error;
		}
#endif /* !PERIPH_1284 */

		/* ok, the host enters the ForwardIdle state */
		ppbus_1284_set_state(dev, PPBUS_ECP_FORWARD_IDLE);
		break;
	case PPBUS_EPP:
		ppbus_1284_set_state(dev, PPBUS_EPP_IDLE);
		break;
	default:
		panic("%s: unknown mode (%d)!", __func__, mode);
	}

	return 0;

error:
	ppbus_1284_terminate(dev);
	return error;
}

/*
 * IEEE1284 termination phase, return code should ignored since the host
 * is _always_ in compatible mode after ppbus_1284_terminate()
 */
int
ppbus_1284_terminate(device_t dev)
{
	struct ppbus_softc * bus = device_private(dev);

#ifdef DEBUG_1284
	printf("T");
#endif

	/* do not reset error here to keep the error that
	 * may occurred before the ppbus_1284_terminate() call */
	ppbus_1284_set_state(dev, PPBUS_TERMINATION);

#ifdef PERIPH_1284
	/* request remote host attention */
        ppbus_wctr(dev, (nINIT | STROBE | SELECTIN) & ~(AUTOFEED));
        DELAY(1);
#endif /* PERIPH_1284 */

	/* Event 22 - set nSelectin low and nAutoFeed high */
	ppbus_wctr(dev, (nINIT | SELECTIN) & ~(STROBE | AUTOFEED));

	/* Event 24 - waiting for peripheral, Xflag ignored */
	if (do_1284_wait(bus, nACK | nBUSY | nFAULT, nFAULT)) {
		ppbus_1284_set_error(bus, PPBUS_TIMEOUT, 24);
		goto error;
	}

	/* Event 25 - set nAutoFd low */
	ppbus_wctr(dev, (nINIT | SELECTIN | AUTOFEED) & ~STROBE);

	/* Event 26 - compatible mode status is set */

	/* Event 27 - peripheral set nAck high */
	if (do_1284_wait(bus, nACK, nACK)) {
		ppbus_1284_set_error(bus, PPBUS_TIMEOUT, 27);
	}

	/* Event 28 - end termination, return to idle phase */
	ppbus_wctr(dev, (nINIT | SELECTIN) & ~(STROBE | AUTOFEED));

error:
	ppbus_1284_set_state(dev, PPBUS_FORWARD_IDLE);

	return (0);
}
