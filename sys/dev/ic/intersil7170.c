/*	$NetBSD: intersil7170.c,v 1.12 2008/04/28 20:23:50 martin Exp $ */
/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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
 * Intersil 7170 time-of-day chip subroutines.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: intersil7170.c,v 1.12 2008/04/28 20:23:50 martin Exp $");

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/errno.h>

#include <sys/bus.h>
#include <dev/clock_subr.h>
#include <dev/ic/intersil7170reg.h>
#include <dev/ic/intersil7170var.h>

int intersil7170_gettime_ymdhms(todr_chip_handle_t, struct clock_ymdhms *);
int intersil7170_settime_ymdhms(todr_chip_handle_t, struct clock_ymdhms *);

void
intersil7170_attach(struct intersil7170_softc *sc)
{
	todr_chip_handle_t handle;

	aprint_normal(": intersil7170");

	handle = &sc->sc_handle;

	handle->cookie = sc;
	handle->todr_gettime = NULL;
	handle->todr_settime = NULL;
	handle->todr_gettime_ymdhms = intersil7170_gettime_ymdhms;
	handle->todr_settime_ymdhms = intersil7170_settime_ymdhms;
	handle->todr_setwen = NULL;

	todr_attach(handle);
}

/*
 * Set up the system's time, given a `reasonable' time value.
 */
int
intersil7170_gettime_ymdhms(todr_chip_handle_t handle, struct clock_ymdhms *dt)
{
	struct intersil7170_softc *sc = handle->cookie;
	bus_space_tag_t bt = sc->sc_bst;
	bus_space_handle_t bh = sc->sc_bsh;
	uint8_t cmd;
	int year;
	int s;

	/* No interrupts while we're fiddling with the chip */
	s = splhigh();

	/* Enable read (stop time) */
	cmd = INTERSIL_COMMAND(INTERSIL_CMD_STOP, INTERSIL_CMD_IENABLE);
	bus_space_write_1(bt, bh, INTERSIL_ICMD, cmd);

	/* The order of reading out the clock elements is important */
	(void)bus_space_read_1(bt, bh, INTERSIL_ICSEC);	/* not used */
	dt->dt_hour = bus_space_read_1(bt, bh, INTERSIL_IHOUR);
	dt->dt_min = bus_space_read_1(bt, bh, INTERSIL_IMIN);
	dt->dt_sec = bus_space_read_1(bt, bh, INTERSIL_ISEC);
	dt->dt_mon = bus_space_read_1(bt, bh, INTERSIL_IMON);
	dt->dt_day = bus_space_read_1(bt, bh, INTERSIL_IDAY);
	year = bus_space_read_1(bt, bh, INTERSIL_IYEAR);
	dt->dt_wday = bus_space_read_1(bt, bh, INTERSIL_IDOW);

	/* Done writing (time wears on) */
	cmd = INTERSIL_COMMAND(INTERSIL_CMD_RUN, INTERSIL_CMD_IENABLE);
	bus_space_write_1(bt, bh, INTERSIL_ICMD, cmd);
	splx(s);

	year += sc->sc_year0;
	if (year < POSIX_BASE_YEAR &&
	    (sc->sc_flag & INTERSIL7170_NO_CENT_ADJUST) == 0)
		year += 100;

	dt->dt_year = year;

	return 0;
}

/*
 * Reset the clock based on the current time.
 */
int
intersil7170_settime_ymdhms(todr_chip_handle_t handle, struct clock_ymdhms *dt)
{
	struct intersil7170_softc *sc = handle->cookie;
	bus_space_tag_t bt = sc->sc_bst;
	bus_space_handle_t bh = sc->sc_bsh;
	uint8_t cmd;
	int year;
	int s;

	year = dt->dt_year - sc->sc_year0;
	if (year > 99 && (sc->sc_flag & INTERSIL7170_NO_CENT_ADJUST) == 0)
		year -= 100;

	/* No interrupts while we're fiddling with the chip */
	s = splhigh();

	/* Enable write (stop time) */
	cmd = INTERSIL_COMMAND(INTERSIL_CMD_STOP, INTERSIL_CMD_IENABLE);
	bus_space_write_1(bt, bh, INTERSIL_ICMD, cmd);

	/* The order of reading writing the clock elements is important */
	bus_space_write_1(bt, bh, INTERSIL_ICSEC, 0);
	bus_space_write_1(bt, bh, INTERSIL_IHOUR, dt->dt_hour);
	bus_space_write_1(bt, bh, INTERSIL_IMIN, dt->dt_min);
	bus_space_write_1(bt, bh, INTERSIL_ISEC, dt->dt_sec);
	bus_space_write_1(bt, bh, INTERSIL_IMON, dt->dt_mon);
	bus_space_write_1(bt, bh, INTERSIL_IDAY, dt->dt_day);
	bus_space_write_1(bt, bh, INTERSIL_IYEAR, year);
	bus_space_write_1(bt, bh, INTERSIL_IDOW, dt->dt_wday);

	/* Done writing (time wears on) */
	cmd = INTERSIL_COMMAND(INTERSIL_CMD_RUN, INTERSIL_CMD_IENABLE);
	bus_space_write_1(bt, bh, INTERSIL_ICMD, cmd);
	splx(s);

	return 0;
}
