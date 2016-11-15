/*	$NetBSD: cfi_0002.c,v 1.8 2015/06/09 21:42:21 matt Exp $	*/
/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Cliff Neighbors.
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

#include "opt_flash.h"

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: cfi_0002.c,v 1.8 2015/06/09 21:42:21 matt Exp $"); 

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/cdefs.h>
#include <sys/device.h>
#include <sys/endian.h>
#include <sys/sched.h>
#include <sys/time.h>

#include <sys/bus.h>
        
#include <dev/nor/nor.h>
#include <dev/nor/cfi.h>
#include <dev/nor/cfi_0002.h>


static void cfi_0002_version_init(struct cfi * const);
static int  cfi_0002_read_page(device_t, flash_off_t, uint8_t *);
static int  cfi_0002_program_page(device_t, flash_off_t, const uint8_t *);
static int  cfi_0002_erase_block(device_t, flash_off_t);
static int  cfi_0002_erase_all(device_t);
static int  cfi_0002_busy(device_t, flash_off_t, u_long);
static int  cfi_0002_busy_wait(struct cfi * const, flash_off_t, u_long);
static int  cfi_0002_busy_poll(struct cfi * const, flash_off_t, u_long);
static int  cfi_0002_busy_yield(struct cfi * const, flash_off_t, u_long);
static int  cfi_0002_busy_dq7(struct cfi * const , flash_off_t);
#ifdef NOTYET
static int  cfi_0002_busy_reg(struct cfi * const, flash_off_t);
#endif

#ifdef NOR_VERBOSE
static const char *page_mode_str[] = {
	"(not supported)",
	"4 word page",
	"8 word page",
	"16 word page",
};

static const char *wp_mode_str[] = {
	"Flash device without WP Protect (No Boot)",
	"Eight 8 kB Sectors at TOP and Bottom with WP (Dual Boot)",
	"Bottom Boot Device with WP Protect (Bottom Boot)",
	"Top Boot Device with WP Protect (Top Boot)",
	"Uniform, Bottom WP Protect (Uniform Bottom Boot)",
	"Uniform, Top WP Protect (Uniform Top Boot)",
	"WP Protect for all sectors",
	"Uniform, Top or Bottom WP Protect",
};

static inline const char *
cfi_0002_page_mode_str(uint8_t mode)
{
	if (mode >= __arraycount(page_mode_str))
		panic("%s: mode %d out of range", __func__, mode);
	return page_mode_str[mode];
}

static inline const char *
cfi_0002_wp_mode_str(uint8_t mode)
{
	if (mode >= __arraycount(wp_mode_str))
		panic("%s: mode %d out of range", __func__, mode);
	return wp_mode_str[mode];
}
#endif

/*
 * cfi_0002_time_write_nbyte - maximum usec delay waiting for write buffer
 */
static inline u_long
cfi_0002_time_write_nbyte(struct cfi *cfi)
{
	u_int shft = cfi->cfi_qry_data.write_nbyte_time_typ;
	shft += cfi->cfi_qry_data.write_nbyte_time_max;
	u_long usec = 1UL << shft;
	return usec;
}

/*
 * cfi_0002_time_erase_blk - maximum usec delay waiting for erase block
 */
static inline u_long
cfi_0002_time_erase_blk(struct cfi *cfi)
{
	u_int shft = cfi->cfi_qry_data.erase_blk_time_typ;
	shft += cfi->cfi_qry_data.erase_blk_time_max;
	u_long usec = 1000UL << shft;
	return usec;
}

/*
 * cfi_0002_time_erase_all - maximum usec delay waiting for erase chip
 */
static inline u_long
cfi_0002_time_erase_all(struct cfi *cfi)
{
	u_int shft = cfi->cfi_qry_data.erase_chip_time_typ;
	shft += cfi->cfi_qry_data.erase_chip_time_max;
	u_long usec = 1000UL << shft;
	return usec;
}

/*
 * cfi_0002_time_dflt - maximum usec delay to use waiting for ready
 *
 * use the maximum delay for chip erase function
 * that should be the worst non-sick case
 */
static inline u_long
cfi_0002_time_dflt(struct cfi *cfi)
{
	return cfi_0002_time_erase_all(cfi);
}

void
cfi_0002_init(struct nor_softc * const sc, struct cfi * const cfi,
    struct nor_chip * const chip)
{
	CFI_0002_STATS_INIT(sc->sc_dev, cfi);

	cfi_0002_version_init(cfi);

	cfi->cfi_ops.cfi_reset = cfi_reset_std;
	cfi->cfi_yield_time = 500;		/* 500 usec */

	/* page size for buffered write */
	chip->nc_page_size =
		1 << cfi->cfi_qry_data.write_nbyte_size_max;

	/* these are unused */
	chip->nc_spare_size = 0;
	chip->nc_badmarker_offs = 0;

	/* establish command-set-specific interface ops */
	sc->sc_nor_if->read_page = cfi_0002_read_page;
	sc->sc_nor_if->program_page = cfi_0002_program_page;
	sc->sc_nor_if->erase_block = cfi_0002_erase_block;
	sc->sc_nor_if->erase_all = cfi_0002_erase_all;
	sc->sc_nor_if->busy = cfi_0002_busy;

}

/*
 * cfi_0002_version_init - command set version-specific initialization
 *
 * see "Programmer's Guide for the Spansion 65 nm GL-S MirrorBit EclipseTM
 * Flash Non-Volatile Memory Family Architecture" section 5.
 */
static void
cfi_0002_version_init(struct cfi * const cfi)
{
	const uint8_t major = cfi->cfi_qry_data.pri.cmd_0002.version_maj;
	const uint8_t minor = cfi->cfi_qry_data.pri.cmd_0002.version_min;

	if ((minor == '3') && (major == '1')) {
		/* cmdset version 1.3 */
		cfi->cfi_ops.cfi_busy = cfi_0002_busy_dq7;
#ifdef NOTYET
		cfi->cfi_ops.cfi_erase_sector = cfi_0002_erase_sector_q;
		cfi->cfi_ops.cfi_program_word = cfi_0002_program_word_ub;
	} else if ((minor >= '5') && (major == '1')) {
		/* cmdset version 1.5 or later */
		cfi->cfi_ops.cfi_busy = cfi_0002_busy_reg;
		cfi->cfi_ops.cfi_erase_sector = cfi_0002_erase_sector_1;
		cfi->cfi_ops.cfi_program_word = cfi_0002_program_word_no_ub;
#endif
	} else {
		/* XXX this is excessive */
		panic("%s: unknown cmdset version %c.%c\n",
			__func__, major, minor);
	}

}

void
cfi_0002_print(device_t self, struct cfi * const cfi)
{
#ifdef NOR_VERBOSE
	struct cmdset_0002_query_data *pri = &cfi->cfi_qry_data.pri.cmd_0002;

	aprint_normal_dev(self, "AMD/Fujitsu cmdset (0x0002) version=%c.%c\n",
		pri->version_maj, pri->version_min);
	aprint_normal_dev(self, "page mode type: %s\n",
		cfi_0002_page_mode_str(pri->page_mode_type));
	aprint_normal_dev(self, "wp protection: %s\n",
		cfi_0002_wp_mode_str(pri->wp_prot));
	aprint_normal_dev(self, "program suspend %ssupported\n",
		(pri->prog_susp == 0) ? "not " : "");
	aprint_normal_dev(self, "unlock bypass %ssupported\n",
		(pri->unlock_bypass == 0) ? "not " : "");
	aprint_normal_dev(self, "secure silicon sector size %#x\n",
		1 << pri->sss_size);
	aprint_normal_dev(self, "SW features %#x\n", pri->soft_feat);
	aprint_normal_dev(self, "page size %d\n", 1 << pri->page_size);
#endif
}

static int
cfi_0002_read_page(device_t self, flash_off_t offset, uint8_t *datap)
{
	struct nor_softc * const sc = device_private(self);
	KASSERT(sc != NULL);
	KASSERT(sc->sc_nor_if != NULL);
	struct cfi *cfi = (struct cfi * const)sc->sc_nor_if->private;
	KASSERT(cfi != NULL);
	struct nor_chip * const chip = &sc->sc_chip;
	KASSERT(chip != NULL);
	KASSERT(chip->nc_page_mask != 0);
	KASSERT((offset & ~chip->nc_page_mask) == 0);
	KASSERT (chip->nc_page_size != 0);
	KASSERT((chip->nc_page_size & ((1 << cfi->cfi_portwidth) - 1)) == 0);

	CFI_0002_STATS_INC(cfi, read_page);

	bus_size_t count = chip->nc_page_size >> cfi->cfi_portwidth;
							/* #words/page */

	int error = cfi_0002_busy_wait(cfi, offset, cfi_0002_time_dflt(cfi));
	if (error != 0)
		return error;

	switch(cfi->cfi_portwidth) {
	case 0:
		bus_space_read_region_1(cfi->cfi_bst, cfi->cfi_bsh, offset,
			(uint8_t *)datap, count);
		break;
	case 1:
		bus_space_read_region_2(cfi->cfi_bst, cfi->cfi_bsh, offset,
			(uint16_t *)datap, count);
		break;
	case 2:
		bus_space_read_region_4(cfi->cfi_bst, cfi->cfi_bsh, offset,
			(uint32_t *)datap, count);
		break;
	default:
		panic("%s: bad port width %d\n", __func__, cfi->cfi_portwidth);
	};

	return 0;
}

static int
cfi_0002_program_page(device_t self, flash_off_t offset, const uint8_t *datap)
{
	struct nor_softc * const sc = device_private(self);
	KASSERT(sc != NULL);
	KASSERT(sc->sc_nor_if != NULL);
	struct cfi *cfi = (struct cfi * const)sc->sc_nor_if->private;
	KASSERT(cfi != NULL);
	struct nor_chip * const chip = &sc->sc_chip;
	KASSERT(chip != NULL);
	KASSERT(chip->nc_page_mask != 0);
	KASSERT((offset & ~chip->nc_page_mask) == 0);
	KASSERT (chip->nc_page_size != 0);
	KASSERT((chip->nc_page_size & ((1 << cfi->cfi_portwidth) - 1)) == 0);

	CFI_0002_STATS_INC(cfi, program_page);

	bus_size_t count = chip->nc_page_size >> cfi->cfi_portwidth;
							/* #words/page */
	bus_size_t sa = offset << (3 - cfi->cfi_portwidth);
							/* sector addr */
	uint32_t wc = count - 1;			/* #words - 1 */

	int error = cfi_0002_busy_wait(cfi, offset, cfi_0002_time_dflt(cfi));
	if (error != 0)
		return ETIMEDOUT;

	cfi_cmd(cfi, cfi->cfi_unlock_addr1, 0xaa);
	cfi_cmd(cfi, cfi->cfi_unlock_addr2, 0x55);
	cfi_cmd(cfi, sa,                    0x25); /* Write To Buffer */
	cfi_cmd(cfi, sa,                    wc);

	switch(cfi->cfi_portwidth) {
	case 0:
		bus_space_write_region_1(cfi->cfi_bst, cfi->cfi_bsh, offset,
			(const uint8_t *)datap, count);
		break;
	case 1:
		bus_space_write_region_2(cfi->cfi_bst, cfi->cfi_bsh, offset,
			(const uint16_t *)datap, count);
		break;
	case 2:
		bus_space_write_region_4(cfi->cfi_bst, cfi->cfi_bsh, offset,
			(const uint32_t *)datap, count);
		break;
	default:
		panic("%s: bad port width %d\n", __func__, cfi->cfi_portwidth);
	};

	cfi_cmd(cfi, sa, 0x29);	/*  Write Buffer Program Confirm */

	error = cfi_0002_busy_wait(cfi, offset, cfi_0002_time_write_nbyte(cfi));

	return error;
}

static int
cfi_0002_erase_all(device_t self)
{
	struct nor_softc * const sc = device_private(self);
	KASSERT(sc != NULL);
	KASSERT(sc->sc_nor_if != NULL);
	struct cfi *cfi = (struct cfi * const)sc->sc_nor_if->private;
	KASSERT(cfi != NULL);

	CFI_0002_STATS_INC(cfi, erase_all);

	int error = cfi_0002_busy_wait(cfi, 0, cfi_0002_time_dflt(cfi));
	if (error != 0)
		return ETIMEDOUT;

	cfi_cmd(cfi, cfi->cfi_unlock_addr1, 0xaa);
	cfi_cmd(cfi, cfi->cfi_unlock_addr2, 0x55);
	cfi_cmd(cfi, cfi->cfi_unlock_addr1, 0x80); /* erase start */
	cfi_cmd(cfi, cfi->cfi_unlock_addr1, 0xaa);
	cfi_cmd(cfi, cfi->cfi_unlock_addr2, 0x55);
	cfi_cmd(cfi, cfi->cfi_unlock_addr1, 0x10); /* erase chip */

	error = cfi_0002_busy_wait(cfi, 0, cfi_0002_time_erase_all(cfi));

	return error;
}

static int
cfi_0002_erase_block(device_t self, flash_off_t offset)
{
	struct nor_softc * const sc = device_private(self);
	KASSERT(sc != NULL);
	KASSERT(sc->sc_nor_if != NULL);
	struct cfi *cfi = (struct cfi * const)sc->sc_nor_if->private;
	KASSERT(cfi != NULL);

	CFI_0002_STATS_INC(cfi, erase_block);

	bus_size_t sa = offset << (3 - cfi->cfi_portwidth);

	int error = cfi_0002_busy_wait(cfi, offset, cfi_0002_time_dflt(cfi));
	if (error != 0)
		return ETIMEDOUT;

	cfi_cmd(cfi, cfi->cfi_unlock_addr1, 0xaa);
	cfi_cmd(cfi, cfi->cfi_unlock_addr2, 0x55);
	cfi_cmd(cfi, cfi->cfi_unlock_addr1, 0x80); /* erase start */
	cfi_cmd(cfi, cfi->cfi_unlock_addr1, 0xaa);
	cfi_cmd(cfi, cfi->cfi_unlock_addr2, 0x55);
	cfi_cmd(cfi, sa,                    0x30); /* erase sector */

	error = cfi_0002_busy_wait(cfi, offset, cfi_0002_time_erase_blk(cfi));

	return error;
}

/*
 * cfi_0002_busy - nor_interface busy op
 */
static int
cfi_0002_busy(device_t self, flash_off_t offset, u_long usec)
{
	struct nor_softc *sc = device_private(self);
	KASSERT(sc != NULL);
	KASSERT(sc->sc_nor_if != NULL);
	struct cfi * const cfi = (struct cfi * const)sc->sc_nor_if->private;

	CFI_0002_STATS_INC(cfi, busy);

	return cfi_0002_busy_wait(cfi, offset, usec);
}

/*
 * cfi_0002_busy_wait - wait until device is not busy
 */
static int
cfi_0002_busy_wait(struct cfi * const cfi, flash_off_t offset, u_long usec)
{
	int error;

#ifdef CFI_0002_STATS
	struct timeval start;
	struct timeval now;
	struct timeval delta;

	if (usec > cfi->cfi_0002_stats.busy_usec_max)
		cfi->cfi_0002_stats.busy_usec_max = usec;
	if (usec < cfi->cfi_0002_stats.busy_usec_min)
		cfi->cfi_0002_stats.busy_usec_min = usec;
	microtime(&start);
#endif
	if (usec > cfi->cfi_yield_time) {
		error = cfi_0002_busy_yield(cfi, offset, usec);
#ifdef CFI_0002_STATS
		microtime(&now);
		cfi->cfi_0002_stats.busy_yield++;
		timersub(&now, &start, &delta);
		timeradd(&delta,
			&cfi->cfi_0002_stats.busy_yield_tv,
			&cfi->cfi_0002_stats.busy_yield_tv);
#endif
	} else {
		error = cfi_0002_busy_poll(cfi, offset, usec);
#ifdef CFI_0002_STATS
		microtime(&now);
		cfi->cfi_0002_stats.busy_poll++;
		timersub(&now, &start, &delta);
		timeradd(&delta,
			&cfi->cfi_0002_stats.busy_poll_tv,
			&cfi->cfi_0002_stats.busy_poll_tv);
#endif
	}
	return error;
}

/*
 * cfi_0002_busy_poll - poll until device is not busy
 */
static int
cfi_0002_busy_poll(struct cfi * const cfi, flash_off_t offset, u_long usec)
{
	u_long count = usec >> 3;
	if (count == 0)
		count = 1;	/* enforce minimum */
	do {
		if (! cfi->cfi_ops.cfi_busy(cfi, offset))
			return 0;	/* not busy */
		DELAY(8);
	} while (count-- != 0);

	return ETIMEDOUT;		/* busy */
}

/*
 * cfi_0002_busy_yield - yield until device is not busy
 */
static int
cfi_0002_busy_yield(struct cfi * const cfi, flash_off_t offset, u_long usec)
{
	struct timeval start;
	struct timeval delta;
	struct timeval limit;
	struct timeval now;

	microtime(&start);

	/* try optimism */
	if (! cfi->cfi_ops.cfi_busy(cfi, offset)) {
		CFI_0002_STATS_INC(cfi, busy_yield_hit);
		return 0;		/* not busy */
	}
	CFI_0002_STATS_INC(cfi, busy_yield_miss);

	delta.tv_sec = usec / 1000000;
	delta.tv_usec = usec % 1000000;
	timeradd(&start, &delta, &limit);
	do {
		yield();
		microtime(&now);
		if (! cfi->cfi_ops.cfi_busy(cfi, offset))
			return 0;	/* not busy */
	} while (timercmp(&now, &limit, <));

	CFI_0002_STATS_INC(cfi, busy_yield_timo);

	return ETIMEDOUT;		/* busy */
}

/*
 * cfi_0002_busy_dq7 - DQ7 "toggle" method to check busy
 *
 * Check busy during/after erase, program, protect operation.
 *
 * NOTE:
 *	Chip manufacturers (Spansion) plan to deprecate this method.
 */
static int
cfi_0002_busy_dq7(struct cfi * const cfi, flash_off_t offset)
{
	bus_space_tag_t bst = cfi->cfi_bst;
	bus_space_handle_t bsh = cfi->cfi_bsh;
	bool busy;

	switch(cfi->cfi_portwidth) {
	case 0: {
		uint8_t r0 = bus_space_read_1(bst, bsh, 0) & __BIT(7);
		uint8_t r1 = bus_space_read_1(bst, bsh, 0) & __BIT(7);
		busy = (r0 != r1);
		break;
	}
	case 1: {
		uint16_t r0 = bus_space_read_2(bst, bsh, 0);
		uint16_t r1 = bus_space_read_2(bst, bsh, 0);
		busy = (r0 != r1);
		break;
	}
	case 2: {
		uint32_t r0 = bus_space_read_4(bst, bsh, 0);
		uint32_t r1 = bus_space_read_4(bst, bsh, 0);
		busy = (r0 != r1);
		break;
	}
	default:
		busy = true;	/* appeas gcc */
		panic("%s: bad port width %d\n",
			__func__, cfi->cfi_portwidth);
	}
	return busy;
}

#ifdef NOTYET
/*
 * cfi_0002_busy_reg - read and evaluate Read Status Register
 *
 * NOTE:
 *	Read Status Register not present on all chips
 *	use "toggle" method when Read Status Register not available.
 */
static bool
cfi_0002_busy_reg(struct cfi * const cfi, flash_off_t offset)
{
	bus_space_tag_t bst = cfi->cfi_bst;
	bus_space_handle_t bsh = cfi->cfi_bsh;
	uint32_t r;

	cfi_cmd(cfi, cfi->cfi_unlock_addr1, 0x70); /* Status Register Read  */

	switch(cfi->cfi_portwidth) {
	case 0:
		r = bus_space_read_1(bst, bsh, 0);
		break;
	case 1:
		r = bus_space_read_2(bst, bsh, 0);
		break;
	case 2:
		r = bus_space_read_4(bst, bsh, 0);
		break;
	default:
		panic("%s: bad port width %d\n",
			__func__, cfi->cfi_portwidth);
	}

	return ((r & __BIT(7)) == 0):
}
#endif	/* NOTYET */

#ifdef CFI_0002_STATS
void
cfi_0002_stats_reset(struct cfi *cfi)
{
	memset(&cfi->cfi_0002_stats, 0, sizeof(struct cfi_0002_stats));
        cfi->cfi_0002_stats.busy_usec_min = ~0;
}

void
cfi_0002_stats_print(struct cfi *cfi)
{
	printf("read_page %lu\n", cfi->cfi_0002_stats.read_page);
	printf("program_page %lu\n", cfi->cfi_0002_stats.program_page);
	printf("erase_all %lu\n", cfi->cfi_0002_stats.erase_all);
	printf("erase_block %lu\n", cfi->cfi_0002_stats.erase_block);
	printf("busy %lu\n", cfi->cfi_0002_stats.busy);

	printf("write_nbyte_time_typ %d\n",
		 cfi->cfi_qry_data.write_nbyte_time_typ);
	printf("write_nbyte_time_max %d\n",
		 cfi->cfi_qry_data.write_nbyte_time_max);

	printf("erase_blk_time_typ %d\n",
		 cfi->cfi_qry_data.erase_blk_time_typ);
	printf("erase_blk_time_max %d\n",
		 cfi->cfi_qry_data.erase_blk_time_max);

	printf("erase_chip_time_typ %d\n",
		 cfi->cfi_qry_data.erase_chip_time_typ);
	printf("erase_chip_time_max %d\n",
		 cfi->cfi_qry_data.erase_chip_time_max);

	printf("time_write_nbyte %lu\n", cfi_0002_time_write_nbyte(cfi));
	printf("time_erase_blk %lu\n", cfi_0002_time_erase_blk(cfi));
	printf("time_erase_all %lu\n", cfi_0002_time_erase_all(cfi));

	printf("busy_usec_min %lu\n", cfi->cfi_0002_stats.busy_usec_min);
	printf("busy_usec_max %lu\n", cfi->cfi_0002_stats.busy_usec_max);

	printf("busy_poll_tv %lld.%d\n",
		cfi->cfi_0002_stats.busy_poll_tv.tv_sec,
		cfi->cfi_0002_stats.busy_poll_tv.tv_usec);
	printf("busy_yield_tv %lld.%d\n",
		cfi->cfi_0002_stats.busy_yield_tv.tv_sec,
		cfi->cfi_0002_stats.busy_yield_tv.tv_usec);
	printf("busy_poll %lu\n", cfi->cfi_0002_stats.busy_poll);
	printf("busy_yield %lu\n", cfi->cfi_0002_stats.busy_yield);
	printf("busy_yield_hit %lu\n", cfi->cfi_0002_stats.busy_yield_hit);
	printf("busy_yield_miss %lu\n", cfi->cfi_0002_stats.busy_yield_miss);
	printf("busy_yield_timo %lu\n", cfi->cfi_0002_stats.busy_yield_timo);
}
#endif	/* CFI_0002_STATS */
