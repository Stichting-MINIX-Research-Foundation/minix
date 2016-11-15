/*	$NetBSD: at24cxx.c,v 1.20 2015/09/27 13:02:21 phx Exp $	*/

/*
 * Copyright (c) 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Steve C. Woodford and Jason R. Thorpe for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: at24cxx.c,v 1.20 2015/09/27 13:02:21 phx Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/fcntl.h>
#include <sys/uio.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/event.h>

#include <sys/bus.h>

#include <dev/i2c/i2cvar.h>
#include <dev/i2c/at24cxxvar.h>

/*
 * AT24Cxx EEPROM I2C address:
 *	101 0xxx
 * (and others depending on the exact model)  The bigger 8-bit parts
 * decode multiple addresses.  The bigger 16-bit parts do too (those
 * larger than 512kb).  Be sure to check the datasheet of your EEPROM
 * because there's much variation between models.
 */
#define	AT24CXX_ADDRMASK	0x3f8
#define	AT24CXX_ADDR		0x50

#define	AT24CXX_WRITE_CYCLE_MS	10
#define	AT24CXX_ADDR_HI(a)	(((a) >> 8) & 0x1f)
#define	AT24CXX_ADDR_LO(a)	((a) & 0xff)

#include "seeprom.h"

#if NSEEPROM > 0

struct seeprom_softc {
	device_t sc_dev;
	i2c_tag_t sc_tag;
	int sc_address;
	int sc_size;
	int sc_cmdlen;
	int sc_open;
};

static int  seeprom_match(device_t, cfdata_t, void *);
static void seeprom_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(seeprom, sizeof(struct seeprom_softc),
	seeprom_match, seeprom_attach, NULL, NULL);
extern struct cfdriver seeprom_cd;

dev_type_open(seeprom_open);
dev_type_close(seeprom_close);
dev_type_read(seeprom_read);
dev_type_write(seeprom_write);

const struct cdevsw seeprom_cdevsw = {
	.d_open = seeprom_open,
	.d_close = seeprom_close,
	.d_read = seeprom_read,
	.d_write = seeprom_write,
	.d_ioctl = noioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER
};

static int seeprom_wait_idle(struct seeprom_softc *);

static const char * seeprom_compats[] = {
	"i2c-at24c64",
	"i2c-at34c02",
	NULL
};

static int
seeprom_match(device_t parent, cfdata_t cf, void *aux)
{
	struct i2c_attach_args *ia = aux;

	if (ia->ia_name) {
		if (ia->ia_ncompat > 0) {
			if (iic_compat_match(ia, seeprom_compats))
				return (1);
		} else {
			if (strcmp(ia->ia_name, "seeprom") == 0)
				return (1);
		}
	} else {
		if ((ia->ia_addr & AT24CXX_ADDRMASK) == AT24CXX_ADDR)
			return (1);
	}

	return (0);
}

static void
seeprom_attach(device_t parent, device_t self, void *aux)
{
	struct seeprom_softc *sc = device_private(self);
	struct i2c_attach_args *ia = aux;

	sc->sc_tag = ia->ia_tag;
	sc->sc_address = ia->ia_addr;
	sc->sc_dev = self;

	if (ia->ia_name != NULL) {
		aprint_naive(": %s", ia->ia_name);
		aprint_normal(": %s", ia->ia_name);
	} else {
		aprint_naive(": EEPROM");
		aprint_normal(": AT24Cxx or compatible EEPROM");
	}

	/*
	 * The AT24C01A/02/04/08/16 EEPROMs use a 1 byte command
	 * word to select the offset into the EEPROM page.  The
	 * AT24C04/08/16 decode fewer of the i2c address bits,
	 * using the bottom 1, 2, or 3 to select the 256-byte
	 * super-page.
	 *
	 * The AT24C32/64/128/256/512 EEPROMs use a 2 byte command
	 * word and decode all of the i2c address bits.
	 *
	 * The AT24C1024 EEPROMs use a 2 byte command and also do bank
	 * switching to select the proper super-page.  This isn't
	 * supported by this driver.
	 */
	if (device_cfdata(self)->cf_flags)
		sc->sc_size = (device_cfdata(self)->cf_flags << 7);
	else
		sc->sc_size = ia->ia_size;
	switch (sc->sc_size) {
	case 128:		/* 1Kbit */
	case 256:		/* 2Kbit */
	case 512:		/* 4Kbit */
	case 1024:		/* 8Kbit */
	case 2048:		/* 16Kbit */
		sc->sc_cmdlen = 1;
		aprint_normal(": size %d\n", sc->sc_size);
		break;

	case 4096:		/* 32Kbit */
	case 8192:		/* 64Kbit */
	case 16384:		/* 128Kbit */
	case 32768:		/* 256Kbit */
	case 65536:		/* 512Kbit */
		sc->sc_cmdlen = 2;
		aprint_normal(": size %d\n", sc->sc_size);
		break;

	default:
		/*
		 * Default to 2KB.  If we happen to have a 2KB
		 * EEPROM this will allow us to access it.  If we
		 * have a smaller one, the worst that can happen
		 * is that we end up trying to read a different
		 * EEPROM on the bus when accessing it.
		 *
		 * Obviously this will not work for 4KB or 8KB
		 * EEPROMs, but them's the breaks.
		 */
		aprint_normal("\n");
		aprint_error_dev(self, "invalid size specified; "
		    "assuming 2KB (16Kb)\n");
		sc->sc_size = 2048;
		sc->sc_cmdlen = 1;
	}

	sc->sc_open = 0;
}

/*ARGSUSED*/
int
seeprom_open(dev_t dev, int flag, int fmt, struct lwp *l)
{
	struct seeprom_softc *sc;

	if ((sc = device_lookup_private(&seeprom_cd, minor(dev))) == NULL)
		return (ENXIO);

	/* XXX: Locking */

	if (sc->sc_open)
		return (EBUSY);

	sc->sc_open = 1;
	return (0);
}

/*ARGSUSED*/
int
seeprom_close(dev_t dev, int flag, int fmt, struct lwp *l)
{
	struct seeprom_softc *sc;

	if ((sc = device_lookup_private(&seeprom_cd, minor(dev))) == NULL)
		return (ENXIO);

	sc->sc_open = 0;
	return (0);
}

/*ARGSUSED*/
int
seeprom_read(dev_t dev, struct uio *uio, int flags)
{
	struct seeprom_softc *sc;
	i2c_addr_t addr;
	u_int8_t ch, cmdbuf[2];
	int a, error;

	if ((sc = device_lookup_private(&seeprom_cd, minor(dev))) == NULL)
		return (ENXIO);

	if (uio->uio_offset >= sc->sc_size)
		return (EINVAL);

	/*
	 * Even though the AT24Cxx EEPROMs support sequential
	 * reads within a page, some I2C controllers do not
	 * support anything other than single-byte transfers,
	 * so we're stuck with this lowest-common-denominator.
	 */

	while (uio->uio_resid > 0 && uio->uio_offset < sc->sc_size) {
		a = (int)uio->uio_offset;
		if (sc->sc_cmdlen == 1) {
			addr = sc->sc_address + (a >> 8);
			cmdbuf[0] = a & 0xff;
		} else {
			addr = sc->sc_address;
			cmdbuf[0] = AT24CXX_ADDR_HI(a);
			cmdbuf[1] = AT24CXX_ADDR_LO(a);
		}

		if ((error = iic_acquire_bus(sc->sc_tag, 0)) != 0)
			return (error);
		if ((error = iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
				      addr, cmdbuf, sc->sc_cmdlen,
				      &ch, 1, 0)) != 0) {
			iic_release_bus(sc->sc_tag, 0);
			aprint_error_dev(sc->sc_dev,
			    "seeprom_read: byte read failed at 0x%x\n", a);
			return (error);
		}
		iic_release_bus(sc->sc_tag, 0);

		if ((error = uiomove(&ch, 1, uio)) != 0) {
			return (error);
		}
	}

	return (0);
}

/*ARGSUSED*/
int
seeprom_write(dev_t dev, struct uio *uio, int flags)
{
	struct seeprom_softc *sc;
	i2c_addr_t addr;
	u_int8_t ch, cmdbuf[2];
	int a, error;

	if ((sc = device_lookup_private(&seeprom_cd, minor(dev))) == NULL)
		return (ENXIO);

	if (uio->uio_offset >= sc->sc_size)
		return (EINVAL);

	/*
	 * See seeprom_read() for why we don't use sequential
	 * writes within a page.
	 */

	while (uio->uio_resid > 0 && uio->uio_offset < sc->sc_size) {
		a = (int)uio->uio_offset;
		if (sc->sc_cmdlen == 1) {
			addr = sc->sc_address + (a >> 8);
			cmdbuf[0] = a & 0xff;
		} else {
			addr = sc->sc_address;
			cmdbuf[0] = AT24CXX_ADDR_HI(a);
			cmdbuf[1] = AT24CXX_ADDR_LO(a);
		}
		if ((error = uiomove(&ch, 1, uio)) != 0) {
			iic_release_bus(sc->sc_tag, 0);
			return (error);
		}

		if ((error = iic_acquire_bus(sc->sc_tag, 0)) != 0)
			return (error);
		if ((error = iic_exec(sc->sc_tag, I2C_OP_WRITE_WITH_STOP,
				      addr, cmdbuf, sc->sc_cmdlen,
				      &ch, 1, 0)) != 0) {
			iic_release_bus(sc->sc_tag, 0);
			aprint_error_dev(sc->sc_dev,
			    "seeprom_write: byte write failed at 0x%x\n", a);
			return (error);
		}
		iic_release_bus(sc->sc_tag, 0);

		/* Wait until the device commits the byte. */
		if ((error = seeprom_wait_idle(sc)) != 0) {
			return (error);
		}
	}

	return (0);
}

static int
seeprom_wait_idle(struct seeprom_softc *sc)
{
	uint8_t cmdbuf[2] = { 0, 0 };
	int rv, timeout;
	u_int8_t dummy;
	int error;

	timeout = (1000 / hz) / AT24CXX_WRITE_CYCLE_MS;
	if (timeout == 0)
		timeout = 1;

	delay(10);

	/*
	 * Read the byte at address 0.  This is just a dummy
	 * read to wait for the EEPROM's write cycle to complete.
	 */
	for (;;) {
		if ((error = iic_acquire_bus(sc->sc_tag, 0)) != 0)
			return error;
		error = iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
		    sc->sc_address, cmdbuf, sc->sc_cmdlen, &dummy, 1, 0);
		iic_release_bus(sc->sc_tag, 0);
		if (error == 0)
			break;

		rv = tsleep(sc, PRIBIO | PCATCH, "seepromwr", timeout);
		if (rv != EWOULDBLOCK)
			return (rv);
	}

	return (0);
}

#endif /* NSEEPROM > 0 */

int
seeprom_bootstrap_read(i2c_tag_t tag, int i2caddr, int offset, int devsize,
    u_int8_t *rvp, size_t len)
{
	i2c_addr_t addr;
	int cmdlen;
	uint8_t cmdbuf[2];

	if (len == 0)
		return (0);

	/* We are very forgiving about devsize during bootstrap. */
	cmdlen = (devsize >= 4096) ? 2 : 1;

	if (iic_acquire_bus(tag, I2C_F_POLL) != 0)
		return (-1);

	while (len) {
		if (cmdlen == 1) {
			addr = i2caddr + (offset >> 8);
			cmdbuf[0] = offset & 0xff;
		} else {
			addr = i2caddr;
			cmdbuf[0] = AT24CXX_ADDR_HI(offset);
			cmdbuf[1] = AT24CXX_ADDR_LO(offset);
		}

		/* Read a single byte. */
		if (iic_exec(tag, I2C_OP_READ_WITH_STOP, addr,
			     cmdbuf, cmdlen, rvp, 1, I2C_F_POLL)) {
			iic_release_bus(tag, I2C_F_POLL);
			return (-1);
		}

		len--;
		rvp++;
		offset++;
	}

	iic_release_bus(tag, I2C_F_POLL);
	return (0);
}
