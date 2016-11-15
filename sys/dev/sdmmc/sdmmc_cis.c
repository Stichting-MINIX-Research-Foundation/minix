/*	$NetBSD: sdmmc_cis.c,v 1.4 2012/02/01 22:34:43 matt Exp $	*/
/*	$OpenBSD: sdmmc_cis.c,v 1.1 2006/06/01 21:53:41 uwe Exp $	*/

/*
 * Copyright (c) 2006 Uwe Stuehler <uwe@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* Routines to decode the Card Information Structure of SD I/O cards */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sdmmc_cis.c,v 1.4 2012/02/01 22:34:43 matt Exp $");

#ifdef _KERNEL_OPT
#include "opt_sdmmc.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>

#include <dev/sdmmc/sdmmc_ioreg.h>
#include <dev/sdmmc/sdmmcdevs.h>
#include <dev/sdmmc/sdmmcvar.h>

#include <dev/pcmcia/pcmciareg.h>

#ifdef SDMMCCISDEBUG
#define DPRINTF(s)	printf s
#else
#define DPRINTF(s)	/**/
#endif

static uint32_t sdmmc_cisptr(struct sdmmc_function *);
static void decode_funce_common(struct sdmmc_function *, struct sdmmc_cis *,
				int, uint32_t);
static void decode_funce_function(struct sdmmc_function *, struct sdmmc_cis *,
				  int, uint32_t);
static void decode_vers_1(struct sdmmc_function *, struct sdmmc_cis *, int,
			  uint32_t);

static uint32_t
sdmmc_cisptr(struct sdmmc_function *sf)
{
	uint32_t cisptr = 0;

	/* CIS pointer stored in little-endian format. */
	if (sf->number == 0) {
		cisptr |= sdmmc_io_read_1(sf, SD_IO_CCCR_CISPTR + 0) << 0;
		cisptr |= sdmmc_io_read_1(sf, SD_IO_CCCR_CISPTR + 1) << 8;
		cisptr |= sdmmc_io_read_1(sf, SD_IO_CCCR_CISPTR + 2) << 16;
	} else {
		struct sdmmc_function *sf0 = sf->sc->sc_fn0;
		int num = sf->number;

		cisptr |= sdmmc_io_read_1(sf0, SD_IO_FBR(num) + 9) << 0;
		cisptr |= sdmmc_io_read_1(sf0, SD_IO_FBR(num) + 10) << 8;
		cisptr |= sdmmc_io_read_1(sf0, SD_IO_FBR(num) + 11) << 16;
	}
	return cisptr;
}

static void
decode_funce_common(struct sdmmc_function *sf, struct sdmmc_cis *cis,
		    int tpllen, uint32_t reg)
{
	static const int speed_val[] =
	    { 0, 10, 12, 13, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 70, 80 };
	static const int speed_unit[] = { 10, 100, 1000, 10000, };
	struct sdmmc_function *sf0 = sf->sc->sc_fn0;
	device_t dev = sf->sc->sc_dev;
	int fn0_blk_size, max_tran_speed;

	if (sf->number != 0) {
		aprint_error_dev(dev,
		    "CISTPL_FUNCE(common) found in function\n");
		return;
	}
	if (tpllen < 4) {
		aprint_error_dev(dev, "CISTPL_FUNCE(common) too short\n");
		return;
	}

	fn0_blk_size = sdmmc_io_read_1(sf0, reg++);
	fn0_blk_size |= sdmmc_io_read_1(sf0, reg++) << 8;
	max_tran_speed = sdmmc_io_read_1(sf0, reg++);
	sf->csd.tran_speed =
	    speed_val[max_tran_speed >> 3] * speed_unit[max_tran_speed & 7];

	DPRINTF(
	    ("CISTPL_FUNCE: FN0_BLK_SIZE=0x%x, MAX_TRAN_SPEED=0x%x(%dkHz)\n",
	    fn0_blk_size, max_tran_speed, sf->csd.tran_speed));
}

static void
decode_funce_function(struct sdmmc_function *sf, struct sdmmc_cis *cis,
		      int tpllen, uint32_t reg)
{
	struct sdmmc_function *sf0 = sf->sc->sc_fn0;
	device_t dev = sf->sc->sc_dev;
	int sdiox_cccrx, sdiox, max_blk_size;

	sdiox_cccrx = sdmmc_io_read_1(sf0, SD_IO_CCCR_CCCR_SDIO_REV);
	sdiox = SD_IO_CCCR_SDIO_REV(sdiox_cccrx);

	if (sf->number == 0) {
		aprint_error_dev(dev,
		    "CISTPL_FUNCE(function) found in common\n");
		return;
	}
	if (sdiox == CCCR_SDIO_REV_1_00 && tpllen < 0x1c) {
		aprint_error_dev(dev,
		    "CISTPL_FUNCE(function) too short (v1.00)\n");
		return;
	} else if (sdiox != CCCR_SDIO_REV_1_00 && tpllen < 0x2a) {
		aprint_error_dev(dev, "CISTPL_FUNCE(function) too short\n");
		return;
	}

	max_blk_size = sdmmc_io_read_1(sf0, reg + 11);
	max_blk_size |= sdmmc_io_read_1(sf0, reg + 12) << 8;

	DPRINTF(("CISTPL_FUNCE: MAX_BLK_SIZE=0x%x\n", max_blk_size));
}

static void
decode_vers_1(struct sdmmc_function *sf, struct sdmmc_cis *cis, int tpllen,
	      uint32_t reg)
{
	struct sdmmc_function *sf0 = sf->sc->sc_fn0;
	device_t dev = sf->sc->sc_dev;
	int start, ch, count, i;

	if (tpllen < 2) {
		aprint_error_dev(dev, "CISTPL_VERS_1 too short\n");
		return;
	}

	cis->cis1_major = sdmmc_io_read_1(sf0, reg++);
	cis->cis1_minor = sdmmc_io_read_1(sf0, reg++);

	for (count = 0, start = 0, i = 0; (count < 4) && ((i + 4) < 256); i++) {
		ch = sdmmc_io_read_1(sf0, reg + i);
		if (ch == 0xff)
			break;
		cis->cis1_info_buf[i] = ch;
		if (ch == 0) {
			cis->cis1_info[count] = cis->cis1_info_buf + start;
			start = i + 1;
			count++;
		}
	}

	DPRINTF(("CISTPL_VERS_1\n"));
}

int
sdmmc_read_cis(struct sdmmc_function *sf, struct sdmmc_cis *cis)
{
	struct sdmmc_function *sf0 = sf->sc->sc_fn0;
	device_t dev = sf->sc->sc_dev;
	uint32_t reg;
	uint8_t tplcode, tpllen;

	memset(cis, 0, sizeof *cis);

	reg = sdmmc_cisptr(sf);
	if (reg < SD_IO_CIS_START ||
	    reg >= (SD_IO_CIS_START + SD_IO_CIS_SIZE - 16)) {
		aprint_error_dev(dev, "bad CIS ptr %#x\n", reg);
		return 1;
	}

	for (;;) {
		tplcode = sdmmc_io_read_1(sf0, reg++);

		if (tplcode == PCMCIA_CISTPL_NULL) {
			DPRINTF((" 00\nCISTPL_NONE\n"));
			continue;
		}

		tpllen = sdmmc_io_read_1(sf0, reg++);
		if (tplcode == PCMCIA_CISTPL_END || tpllen == 0) {
			if (tplcode != 0xff)
				aprint_error_dev(dev, "CIS parse error at %d, "
				    "tuple code %#x, length %d\n",
				    reg, tplcode, tpllen);
			else {
				DPRINTF((" ff\nCISTPL_END\n"));
			}
			break;
		}

#ifdef SDMMCCISDEBUG
		{ 
			int i;

			/* print the tuple */
			DPRINTF((" %02x %02x", tplcode, tpllen));

			for (i = 0; i < tpllen; i++) {
				DPRINTF((" %02x",
				    sdmmc_io_read_1(sf0, reg + i)));
				if ((i % 16) == 13)
					DPRINTF(("\n"));
			}
			if ((i % 16) != 14)
				DPRINTF(("\n"));
		}
#endif

		switch (tplcode) {
		case PCMCIA_CISTPL_FUNCE:
			if (sdmmc_io_read_1(sf0, reg++) == 0)
				decode_funce_common(sf, cis, tpllen, reg);
			else
				decode_funce_function(sf, cis, tpllen, reg);
			reg += (tpllen - 1);
			break;

		case PCMCIA_CISTPL_FUNCID:
			if (tpllen < 2) {
				aprint_error_dev(dev,
				    "bad CISTPL_FUNCID length\n");
				reg += tpllen;
				break;
			}
			cis->function = sdmmc_io_read_1(sf0, reg);
			DPRINTF(("CISTPL_FUNCID\n"));
			reg += tpllen;
			break;

		case PCMCIA_CISTPL_MANFID:
			if (tpllen < 4) {
				aprint_error_dev(dev,
				    "bad CISTPL_MANFID length\n");
				reg += tpllen;
				break;
			}
			cis->manufacturer = sdmmc_io_read_1(sf0, reg++);
			cis->manufacturer |= sdmmc_io_read_1(sf0, reg++) << 8;
			cis->product = sdmmc_io_read_1(sf0, reg++);
			cis->product |= sdmmc_io_read_1(sf0, reg++) << 8;
			DPRINTF(("CISTPL_MANFID\n"));
			break;

		case PCMCIA_CISTPL_VERS_1:
			decode_vers_1(sf, cis, tpllen, reg);
			reg += tpllen;
			break;

		default:
			aprint_error_dev(dev,
			    "unknown tuple code %#x, length %d\n",
			    tplcode, tpllen);
			reg += tpllen;
			break;
		}
	}

	return 0;
}

void
sdmmc_print_cis(struct sdmmc_function *sf)
{
	device_t dev = sf->sc->sc_dev;
	struct sdmmc_cis *cis = &sf->cis;
	int i;

	printf("%s: CIS version %u.%u\n", device_xname(dev), cis->cis1_major,
	    cis->cis1_minor);

	printf("%s: CIS info: ", device_xname(dev));
	for (i = 0; i < 4; i++) {
		if (cis->cis1_info[i] == NULL)
			break;
		if (i != 0)
			aprint_verbose(", ");
		printf("%s", cis->cis1_info[i]);
	}
	printf("\n");

	printf("%s: Manufacturer code 0x%x, product 0x%x\n", device_xname(dev),
	    cis->manufacturer, cis->product);

	printf("%s: function %d: ", device_xname(dev), sf->number);
	printf("\n");
}

void
sdmmc_check_cis_quirks(struct sdmmc_function *sf)
{
	char *p;
	int i;

	if (sf->cis.manufacturer == SDMMC_VENDOR_SPECTEC &&
	    sf->cis.product == SDMMC_PRODUCT_SPECTEC_SDW820) {
		/* This card lacks the VERS_1 tuple. */
		static const char cis1_info[] = 
		    "Spectec\0SDIO WLAN Card\0SDW-820\0\0";

		sf->cis.cis1_major = 0x01;
		sf->cis.cis1_minor = 0x00;

		p = sf->cis.cis1_info_buf;
		strlcpy(p, cis1_info, sizeof(sf->cis.cis1_info_buf));
		for (i = 0; i < 4; i++) {
			sf->cis.cis1_info[i] = p;
			p += strlen(p) + 1;
		}
	}
}
