/*	$NetBSD: mpu.c,v 1.19 2012/01/21 16:49:26 chs Exp $	*/

/*
 * Copyright (c) 1998, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (augustss@NetBSD.org) and by Andrew Doran.
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
__KERNEL_RCSID(0, "$NetBSD: mpu.c,v 1.19 2012/01/21 16:49:26 chs Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/buf.h>

#include <sys/cpu.h>
#include <sys/intr.h>
#include <sys/bus.h>

#include <dev/midi_if.h>

#include <dev/ic/mpuvar.h>

#ifdef AUDIO_DEBUG
#define DPRINTF(x)	if (mpudebug) printf x
#define DPRINTFN(n,x)	if (mpudebug >= (n)) printf x
int	mpudebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define MPU_DATA		0
#define MPU_COMMAND	1
#define  MPU_RESET	0xff
#define  MPU_UART_MODE	0x3f
#define  MPU_ACK		0xfe
#define MPU_STATUS	1
#define  MPU_OUTPUT_BUSY	0x40
#define  MPU_INPUT_EMPTY	0x80

#define MPU_MAXWAIT	10000	/* usec/10 to wait */

#define MPU_GETSTATUS(iot, ioh) (bus_space_read_1(iot, ioh, MPU_STATUS))

static int 		mpu_reset(struct mpu_softc *);
static	inline int mpu_waitready(struct mpu_softc *);
static void		mpu_readinput(struct mpu_softc *);

static int	mpu_open(void *, int,
			 void (*iintr)(void *, int),
			 void (*ointr)(void *), void *arg);
static void	mpu_close(void *);
static int	mpu_output(void *, int);
static void	mpu_getinfo(void *, struct midi_info *);
static void	mpu_get_locks(void *, kmutex_t **, kmutex_t **);

const struct midi_hw_if mpu_midi_hw_if = {
	mpu_open,
	mpu_close,
	mpu_output,
	mpu_getinfo,
	0,			/* ioctl */
	mpu_get_locks,
};

int
mpu_find(struct mpu_softc *sc)
{
	if (MPU_GETSTATUS(sc->iot, sc->ioh) == 0xff) {
		DPRINTF(("%s: No status\n", __func__));
		goto bad;
	}
	sc->open = 0;
	sc->intr = 0;
	if (mpu_reset(sc) == 0)
		return 1;
bad:
	return 0;
}

void
mpu_attach(struct mpu_softc *sc)
{

	if (sc->lock == NULL) {
		panic("mpu_attach: no lock");
	}

	midi_attach_mi(&mpu_midi_hw_if, sc, sc->sc_dev);
}

static inline int
mpu_waitready(struct mpu_softc *sc)
{
	int i;

	KASSERT(sc->lock == NULL || mutex_owned(sc->lock));

	for (i = 0; i < MPU_MAXWAIT; i++) {
		if (!(MPU_GETSTATUS(sc->iot, sc->ioh) & MPU_OUTPUT_BUSY))
			return 0;
		delay(10);
	}
	return 1;
}

static int
mpu_reset(struct mpu_softc *sc)
{
	bus_space_tag_t iot = sc->iot;
	bus_space_handle_t ioh = sc->ioh;
	int i;

	KASSERT(sc->lock == NULL || mutex_owned(sc->lock));

	if (mpu_waitready(sc)) {
		DPRINTF(("%s: not ready\n", __func__));
		return EIO;
	}
	bus_space_write_1(iot, ioh, MPU_COMMAND, MPU_RESET);
	for (i = 0; i < 2*MPU_MAXWAIT; i++) {
		if (!(MPU_GETSTATUS(iot, ioh) & MPU_INPUT_EMPTY) &&
		    bus_space_read_1(iot, ioh, MPU_DATA) == MPU_ACK) {
			return 0;
		}
	}
	DPRINTF(("%s: No ACK\n", __func__));
	return EIO;
}

static int
mpu_open(void *addr, int flags, void (*iintr)(void *, int),
    void (*ointr)(void *), void *arg)
{
	struct mpu_softc *sc = addr;

        DPRINTF(("%s: sc=%p\n", __func__, sc));

	KASSERT(mutex_owned(sc->lock));

	if (sc->open)
		return EBUSY;
#ifndef AUDIO_NO_POWER_CTL
	if (sc->powerctl)
		sc->powerctl(sc->powerarg, 1);
#endif
	if (mpu_reset(sc) != 0) {
#ifndef AUDIO_NO_POWER_CTL
		if (sc->powerctl)
			sc->powerctl(sc->powerarg, 0);
#endif
		return EIO;
	}

	bus_space_write_1(sc->iot, sc->ioh, MPU_COMMAND, MPU_UART_MODE);
	sc->open = 1;
	sc->intr = iintr;
	sc->arg = arg;
	return 0;
}

static void
mpu_close(void *addr)
{
	struct mpu_softc *sc = addr;

        DPRINTF(("%s: sc=%p\n", __func__, sc));

	KASSERT(mutex_owned(sc->lock));

	sc->open = 0;
	sc->intr = 0;
	mpu_reset(sc); /* exit UART mode */

#ifndef AUDIO_NO_POWER_CTL
	if (sc->powerctl)
		sc->powerctl(sc->powerarg, 0);
#endif
}

static void
mpu_readinput(struct mpu_softc *sc)
{
	bus_space_tag_t iot = sc->iot;
	bus_space_handle_t ioh = sc->ioh;
	int data;

	KASSERT(mutex_owned(sc->lock));

	while(!(MPU_GETSTATUS(iot, ioh) & MPU_INPUT_EMPTY)) {
		data = bus_space_read_1(iot, ioh, MPU_DATA);
		DPRINTFN(3, ("%s: sc=%p 0x%02x\n", __func__, sc, data));
		if (sc->intr)
			sc->intr(sc->arg, data);
	}
}

static int
mpu_output(void *addr, int d)
{
	struct mpu_softc *sc = addr;

	KASSERT(mutex_owned(sc->lock));

	DPRINTFN(3, ("%s: sc=%p 0x%02x\n", __func__, sc, d));
	if (!(MPU_GETSTATUS(sc->iot, sc->ioh) & MPU_INPUT_EMPTY)) {
		mpu_readinput(sc);
	}
	if (mpu_waitready(sc)) {
		DPRINTF(("%s:: not ready\n", __func__));
		return EIO;
	}
	bus_space_write_1(sc->iot, sc->ioh, MPU_DATA, d);
	return 0;
}

static void
mpu_getinfo(void *addr, struct midi_info *mi)
{
	struct mpu_softc *sc = addr;

	KASSERT(mutex_owned(sc->lock));

	mi->name = sc->model;
	mi->props = 0;
}

static void
mpu_get_locks(void *addr, kmutex_t **intr, kmutex_t **proc)
{
	struct mpu_softc *sc = addr;

	*intr = sc->lock;
	*proc = NULL;
}

int
mpu_intr(void *addr)
{
	struct mpu_softc *sc = addr;

	KASSERT(mutex_owned(sc->lock));

	if (MPU_GETSTATUS(sc->iot, sc->ioh) & MPU_INPUT_EMPTY) {
		DPRINTF(("%s: no data\n", __func__));
		return 0;
	} else {
		mpu_readinput(sc);
		return 1;
	}
}
