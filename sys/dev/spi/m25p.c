/* $NetBSD: m25p.c,v 1.4 2013/10/26 15:18:21 rkujawa Exp $ */

/*-
 * Copyright (c) 2006 Urbana-Champaign Independent Media Center.
 * Copyright (c) 2006 Garrett D'Amore.
 * All rights reserved.
 *
 * Portions of this code were written by Garrett D'Amore for the
 * Champaign-Urbana Community Wireless Network Project.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgements:
 *      This product includes software developed by the Urbana-Champaign
 *      Independent Media Center.
 *	This product includes software developed by Garrett D'Amore.
 * 4. Urbana-Champaign Independent Media Center's name and Garrett
 *    D'Amore's name may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE URBANA-CHAMPAIGN INDEPENDENT
 * MEDIA CENTER AND GARRETT D'AMORE ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE URBANA-CHAMPAIGN INDEPENDENT
 * MEDIA CENTER OR GARRETT D'AMORE BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: m25p.c,v 1.4 2013/10/26 15:18:21 rkujawa Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>

#include <dev/sysmon/sysmonvar.h>

#include <dev/spi/spivar.h>
#include <dev/spi/spiflash.h>

/*
 * Device driver for STMicroelectronics M25P Family SPI Flash Devices
 */

static int m25p_match(device_t , cfdata_t , void *);
static void m25p_attach(device_t , device_t , void *);
static const char *m25p_getname(void *);
static struct spi_handle *m25p_gethandle(void *);
static int m25p_getflags(void *);
static int m25p_getsize(void *, int);

struct m25p_softc {
	struct spi_handle	*sc_sh;
	const char		*sc_name;
	int			sc_sizes[SPIFLASH_SIZE_COUNT];
	int			sc_flags;
};

CFATTACH_DECL_NEW(m25p, sizeof(struct m25p_softc),
    m25p_match, m25p_attach, NULL, NULL);

static const struct spiflash_hw_if m25p_hw_if = {
	.sf_getname = m25p_getname,
	.sf_gethandle = m25p_gethandle,
	.sf_getsize = m25p_getsize,
	.sf_getflags = m25p_getflags,
};

static const struct m25p_info {
	uint8_t		sig;
	uint8_t		mfgid;
	uint16_t	devid;
	const char	*name;
	uint16_t	size;	/* in KB */
	uint16_t	sector;	/* in KB */
	uint16_t	mhz;
} m25p_infos[] = {
	{ 0x16, 0x20, 0x2017, "STMicro M25P64", 8192, 64 },	/* 64Mbit */
	{ 0x14, 0x20, 0x2015, "STMicro M25P16", 2048, 64 },	/* 16Mbit */
	{ 0x12,	0x20, 0x2013, "STMicro M25P40", 512, 64 },	/* 4Mbit */
	{ 0xc0, 0x20, 0x7117, "STMicro M25PX64", 8192, 64 },	/* 64Mbit */
	{ 0x0, 0x20, 0xBB18, "Numonyx N25Q128", 16384, 64 },	/* 128Mbit */
	{ 0 }
};

static int
m25p_match(device_t parent, cfdata_t cf, void *aux)
{
	struct spi_attach_args *sa = aux;

	/* configure for 20MHz, which is the max for normal reads */
	if (spi_configure(sa->sa_handle, SPI_MODE_0, 20000000))
		return 0;

	return 1;
}

static void
m25p_attach(device_t parent, device_t self, void *aux)
{
	struct m25p_softc *sc = device_private(self);
	struct spi_attach_args *sa = aux;
	const struct m25p_info *info;
	uint8_t	 buf[4];
	uint8_t	cmd;
	uint8_t mfgid;
	uint8_t sig;
	uint16_t devid;

	sc->sc_sh = sa->sa_handle;

	/* first we try JEDEC ID read */
	
	cmd = SPIFLASH_CMD_RDJI;
	if (spi_send_recv(sa->sa_handle, 1, &cmd, 3, buf)) {
		aprint_error(": failed to get JEDEC identification\n");	
		return;
	}
	mfgid = buf[0];
	devid = ((uint16_t)buf[1] << 8) | buf[2];

	if ((mfgid == 0xff) || (mfgid == 0)) {
		cmd = SPIFLASH_CMD_RDID;
		if (spi_send_recv(sa->sa_handle, 1, &cmd, 4, buf)) {
			aprint_error(": failed to get legacy signature\n");
			return;
		}
		sig = buf[3];
	} else {
		sig = 0;
	}

	/* okay did it match */
	for (info = m25p_infos; info->name; info++) {
		if ((info->mfgid == mfgid) && (info->devid == devid))
			break;
		if (sig == info->sig)
			break;
	}

	if (info->name == NULL) {
		aprint_error(": unknown or unsupported device\n");
		return;
	}

	sc->sc_name = info->name;
	sc->sc_sh = sa->sa_handle;
	sc->sc_sizes[SPIFLASH_SIZE_DEVICE] = info->size * 1024;
	sc->sc_sizes[SPIFLASH_SIZE_ERASE] = info->sector * 1024;
	sc->sc_sizes[SPIFLASH_SIZE_WRITE] = 256;
	sc->sc_sizes[SPIFLASH_SIZE_READ] = -1;

	sc->sc_flags = SPIFLASH_FLAG_FAST_READ;

	aprint_normal("\n");

	spiflash_attach_mi(&m25p_hw_if, sc, self);
}

const char *
m25p_getname(void *cookie)
{
	struct m25p_softc *sc = cookie;

	return (sc->sc_name);
}

struct spi_handle *
m25p_gethandle(void *cookie)
{
	struct m25p_softc *sc = cookie;

	return (sc->sc_sh);
}

int
m25p_getflags(void *cookie)
{
	struct m25p_softc *sc = cookie;

	return (sc->sc_flags);
}

int
m25p_getsize(void *cookie, int idx)
{
	struct m25p_softc *sc = cookie;

	if ((idx < 0) || (idx >= SPIFLASH_SIZE_COUNT))
		return -1;
	return (sc->sc_sizes[idx]);
}
