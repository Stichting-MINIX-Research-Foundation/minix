/* $NetBSD: motoi2c.c,v 1.4 2011/04/17 15:14:59 phx Exp $ */

/*-
 * Copyright (c) 2007, 2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas.
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
__KERNEL_RCSID(0, "$NetBSD: motoi2c.c,v 1.4 2011/04/17 15:14:59 phx Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/mutex.h>
#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/i2c/i2cvar.h>
#include <dev/i2c/motoi2creg.h>
#include <dev/i2c/motoi2cvar.h>

#ifdef DEBUG
int motoi2c_debug = 0;
#define	DPRINTF(x)	if (motoi2c_debug) printf x
#else
#define	DPRINTF(x)
#endif

static int  motoi2c_acquire_bus(void *, int);
static void motoi2c_release_bus(void *, int);
static int  motoi2c_exec(void *, i2c_op_t, i2c_addr_t, const void *, size_t,
		void *, size_t, int);
static int  motoi2c_busy_wait(struct motoi2c_softc *, uint8_t);

static const struct i2c_controller motoi2c = {
	.ic_acquire_bus = motoi2c_acquire_bus,
	.ic_release_bus = motoi2c_release_bus,
	.ic_exec	= motoi2c_exec,
};

static const struct motoi2c_settings motoi2c_default_settings = {
	.i2c_adr	= MOTOI2C_ADR_DEFAULT,
	.i2c_fdr	= MOTOI2C_FDR_DEFAULT,
	.i2c_dfsrr	= MOTOI2C_DFSRR_DEFAULT,
};

#define	I2C_READ(r)	((*sc->sc_iord)(sc, (r)))
#define	I2C_WRITE(r,v)	((*sc->sc_iowr)(sc, (r), (v)))
#define I2C_SETCLR(r, s, c) \
	((*sc->sc_iowr)(sc, (r), ((*sc->sc_iord)(sc, (r)) | (s)) & ~(c)))

static uint8_t
motoi2c_iord1(struct motoi2c_softc *sc, bus_size_t off)
{
	return bus_space_read_1(sc->sc_iot, sc->sc_ioh, off);
}

static void
motoi2c_iowr1(struct motoi2c_softc *sc, bus_size_t off, uint8_t data)
{
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, off, data);
}

void
motoi2c_attach_common(device_t self, struct motoi2c_softc *sc,
	const struct motoi2c_settings *i2c)
{
	struct i2cbus_attach_args iba;

	mutex_init(&sc->sc_buslock, MUTEX_DEFAULT, IPL_NONE);

	if (i2c == NULL)
		i2c = &motoi2c_default_settings;

	sc->sc_i2c = motoi2c;
	sc->sc_i2c.ic_cookie = sc;
	if (sc->sc_iord == NULL)
		sc->sc_iord = motoi2c_iord1;
	if (sc->sc_iowr == NULL)
		sc->sc_iowr = motoi2c_iowr1;
	memset(&iba, 0, sizeof(iba));
	iba.iba_tag = &sc->sc_i2c;

	I2C_WRITE(I2CCR, 0);		/* reset before changing anything */
	I2C_WRITE(I2CDFSRR, i2c->i2c_dfsrr);	/* sampling units */
	I2C_WRITE(I2CFDR, i2c->i2c_fdr);	/* divider 3072 (0x31) */
	I2C_WRITE(I2CADR, i2c->i2c_adr);	/* our slave address is 0x7f */
	I2C_WRITE(I2CSR, 0);		/* clear status flags */

	config_found_ia(self, "i2cbus", &iba, iicbus_print);
}

static int
motoi2c_acquire_bus(void *v, int flags)
{
	struct motoi2c_softc * const sc = v;

	mutex_enter(&sc->sc_buslock);
	I2C_WRITE(I2CCR, CR_MEN);	/* enable the I2C module */

	return 0;
}

static void
motoi2c_release_bus(void *v, int flags)
{
	struct motoi2c_softc * const sc = v;

	I2C_WRITE(I2CCR, 0);		/* reset before changing anything */
	mutex_exit(&sc->sc_buslock);
}

/* busy waiting for byte data transfer completion */
static int
motoi2c_busy_wait(struct motoi2c_softc *sc, uint8_t cr)
{
	uint8_t sr;
	u_int timo;
	int error = 0;

	timo = 1000;
	while (((sr = I2C_READ(I2CSR)) & SR_MIF) == 0 && --timo)
		DELAY(10);

	if (timo == 0) {
		DPRINTF(("%s: timeout (sr=%#x, cr=%#x)\n",
		    __func__, sr, I2C_READ(I2CCR)));
		error = ETIMEDOUT;
	}
	/*
	 * RXAK is only valid when transmitting.
	 */
	if ((cr & CR_MTX) && (sr & SR_RXAK)) {
		DPRINTF(("%s: missing rx ack (%#x): spin=%u\n",
		    __func__, sr, 1000 - timo));
		error = EIO;
	}
	I2C_WRITE(I2CSR, 0);
	return error;
}

int
motoi2c_intr(void *v)
{
	struct motoi2c_softc * const sc = v;

	panic("%s(%p)", __func__, sc);

	return 0;
}

int
motoi2c_exec(void *v, i2c_op_t op, i2c_addr_t addr,
	const void *cmdbuf, size_t cmdlen,
	void *databuf, size_t datalen,
	int flags)
{
	struct motoi2c_softc * const sc = v;
	uint8_t sr;
	uint8_t cr;
	int error;

	sr = I2C_READ(I2CSR);
	cr = I2C_READ(I2CCR);

#if 0
	DPRINTF(("%s(%#x,%#x,%p,%zu,%p,%zu,%#x): sr=%#x cr=%#x\n",
	    __func__, op, addr, cmdbuf, cmdlen, databuf, datalen, flags,
	    sr, cr));
#endif

	if ((cr & CR_MSTA) == 0 && (sr & SR_MBB) != 0) {
		/* wait for bus becoming available */
		u_int timo = 100;
		do {
			DELAY(10);
		} while (--timo > 0 && ((sr = I2C_READ(I2CSR)) & SR_MBB) != 0);

		if (timo == 0) {
			DPRINTF(("%s: bus is busy (%#x)\n", __func__, sr));
			return ETIMEDOUT;
		}
	}

	/* reset interrupt and arbitration-lost flags (all others are RO) */
	I2C_WRITE(I2CSR, 0);
	sr = I2C_READ(I2CSR);

	/*
	 * Generate start (or restart) condition
	 */
	/* CR_RTSA is write-only and transitory */
	uint8_t rsta = (cr & CR_MSTA ? CR_RSTA : 0);
	cr = CR_MEN | CR_MTX | CR_MSTA;
	I2C_WRITE(I2CCR, cr | rsta);

	DPRINTF(("%s: started: sr=%#x cr=%#x/%#x\n",
	    __func__, I2C_READ(I2CSR), cr, I2C_READ(I2CCR)));

	sr = I2C_READ(I2CSR);
	if (sr & SR_MAL) {
		DPRINTF(("%s: lost bus: sr=%#x cr=%#x/%#x\n",
		    __func__, I2C_READ(I2CSR), cr, I2C_READ(I2CCR)));
		I2C_WRITE(I2CCR, 0);
		DELAY(10);
		I2C_WRITE(I2CCR, CR_MEN | CR_MTX | CR_MSTA);
		DELAY(10);
		sr = I2C_READ(I2CSR);
		if (sr & SR_MAL) {
			error = EBUSY;
			goto out;
		}
		DPRINTF(("%s: reacquired bus: sr=%#x cr=%#x/%#x\n",
		    __func__, I2C_READ(I2CSR), cr, I2C_READ(I2CCR)));
	}

	/* send target address and transfer direction */
	uint8_t addr_byte = (addr << 1)
	    | (cmdlen == 0 && I2C_OP_READ_P(op) ? 1 : 0);
	I2C_WRITE(I2CDR, addr_byte);

	error = motoi2c_busy_wait(sc, cr);
	if (error) {
		DPRINTF(("%s: error sending address: %d\n", __func__, error));
		if (error == EIO)
			error = ENXIO;
		goto out;
	}

	const uint8_t *cmdptr = cmdbuf;
	for (size_t i = 0; i < cmdlen; i++) {
		I2C_WRITE(I2CDR, *cmdptr++);

		error = motoi2c_busy_wait(sc, cr);
		if (error) {
			DPRINTF(("%s: error sending cmd byte %zu (cr=%#x/%#x):"
			    " %d\n", __func__, i, I2C_READ(I2CCR), cr, error));
			goto out;
		}
	}

	if (cmdlen > 0 && I2C_OP_READ_P(op)) {
		KASSERT(cr & CR_MTX);
		KASSERT((cr & CR_TXAK) == 0);
		I2C_WRITE(I2CCR, cr | CR_RSTA);
#if 0
		DPRINTF(("%s: restarted(read): sr=%#x cr=%#x(%#x)\n",
		    __func__, I2C_READ(I2CSR), cr | CR_RSTA, I2C_READ(I2CCR)));
#endif

		/* send target address and read transfer direction */
		addr_byte |= 1;
		I2C_WRITE(I2CDR, addr_byte);

		error = motoi2c_busy_wait(sc, cr);
		if (error) {
			if (error == EIO)
				error = ENXIO;
			goto out;
		}
	}

	if (I2C_OP_READ_P(op)) {
		uint8_t *dataptr = databuf;
		cr &= ~CR_MTX;		/* clear transmit flags */
		if (datalen <= 1)
			cr |= CR_TXAK;
		I2C_WRITE(I2CCR, cr);
		DELAY(10);
		(void)I2C_READ(I2CDR);		/* dummy read */
		for (size_t i = 0; i < datalen; i++) {
			/*
			 * If a master receiver wants to terminate a data
			 * transfer, it must inform the slave transmitter by
			 * not acknowledging the last byte of data (by setting
			 * the transmit acknowledge bit (I2CCR[TXAK])) before
			 * reading the next-to-last byte of data. 
			 */
			error = motoi2c_busy_wait(sc, cr);
			if (error) {
				DPRINTF(("%s: error reading byte %zu: %d\n",
				    __func__, i, error));
				goto out;
			}
			if (i == datalen - 2) {
				cr |= CR_TXAK;
				I2C_WRITE(I2CCR, cr);
			} else if (i == datalen - 1 && I2C_OP_STOP_P(op)) {
				cr = CR_MEN;
				I2C_WRITE(I2CCR, cr);
			}
			*dataptr++ = I2C_READ(I2CDR);
		}
		if (datalen == 0) {
			if (I2C_OP_STOP_P(op)) {
				cr = CR_MEN;
				I2C_WRITE(I2CCR, cr);
			}
			(void)I2C_READ(I2CDR);	/* dummy read */
			error = motoi2c_busy_wait(sc, cr);
			if (error) {
				DPRINTF(("%s: error reading dummy last byte:"
				    "%d\n", __func__, error));
				goto out;
			}
		}
	} else {
		const uint8_t *dataptr = databuf;
		for (size_t i = 0; i < datalen; i++) {
			I2C_WRITE(I2CDR, *dataptr++);
			error = motoi2c_busy_wait(sc, cr);
			if (error) {
				DPRINTF(("%s: error sending data byte %zu:"
				    " %d\n", __func__, i, error));
				goto out;
			}
		}
	}

 out:
	/*
	 * If we encountered an error condition or caller wants a STOP,
	 * send a STOP.
	 */
	if (error || (cr & CR_TXAK) || ((cr & CR_MSTA) && I2C_OP_STOP_P(op))) {
		cr = CR_MEN;
		I2C_WRITE(I2CCR, cr);
		DPRINTF(("%s: stopping: cr=%#x/%#x\n", __func__,
		    cr, I2C_READ(I2CCR)));
	}

	DPRINTF(("%s: exit sr=%#x cr=%#x: %d\n", __func__,
	    I2C_READ(I2CSR), I2C_READ(I2CCR), error));

	return error;
}
