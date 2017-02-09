/*	$NetBSD: nor.c,v 1.5 2014/02/25 18:30:10 pooka Exp $	*/

/*-
 * Copyright (c) 2011 Department of Software Engineering,
 *		      University of Szeged, Hungary
 * Copyright (c) 2011 Adam Hoka <ahoka@NetBSD.org>
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by the Department of Software Engineering, University of Szeged, Hungary
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* Common driver for NOR chips implementing the ONFI CFI specification */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: nor.c,v 1.5 2014/02/25 18:30:10 pooka Exp $");

#include "locators.h"
#include "opt_nor.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/device.h>
#include <sys/kmem.h>
#include <sys/sysctl.h>
#include <sys/atomic.h>

#include <dev/flash/flash.h>
#include <dev/flash/flash_io.h>
#include <dev/nor/nor.h>


static int nor_match(device_t, cfdata_t, void *);
static void nor_attach(device_t, device_t, void *);
static int nor_detach(device_t, int);
static bool nor_shutdown(device_t, int);
static int nor_print(void *, const char *);
static int nor_search(device_t, cfdata_t, const int *, void *);

/* flash interface implementation */
static int nor_flash_isbad(device_t, flash_off_t, bool *);
static int nor_flash_markbad(device_t, flash_off_t);
static int nor_flash_write(device_t, flash_off_t, size_t, size_t *,
	const u_char *);
static int nor_flash_read(device_t, flash_off_t, size_t, size_t *, uint8_t *);
static int nor_flash_erase_all(device_t);
static int nor_flash_erase(device_t, struct flash_erase_instruction *);
static int nor_flash_submit(device_t, buf_t *);

/* default functions for driver development */
static void nor_default_select(device_t, bool);
static int  nor_default_read_page(device_t, flash_off_t, uint8_t *);
static int  nor_default_program_page(device_t, flash_off_t, const uint8_t *);

static int nor_scan_media(device_t, struct nor_chip *);

CFATTACH_DECL_NEW(nor, sizeof(struct nor_softc),
    nor_match, nor_attach, nor_detach, NULL);

#ifdef NOR_DEBUG
int	nordebug = NOR_DEBUG;
#endif

int nor_cachesync_timeout = 1;
int nor_cachesync_nodenum;

struct flash_interface nor_flash_if = {
	.type = FLASH_TYPE_NOR,

	.read = nor_flash_read,
	.write = nor_flash_write,
	.erase = nor_flash_erase,
	.block_isbad = nor_flash_isbad,
	.block_markbad = nor_flash_markbad,

	.submit = nor_flash_submit
};

#ifdef NOR_VERBOSE
const struct nor_manufacturer nor_mfrs[] = {
	{ NOR_MFR_AMD,		"AMD" },
	{ NOR_MFR_FUJITSU,	"Fujitsu" },
	{ NOR_MFR_RENESAS,	"Renesas" },
	{ NOR_MFR_STMICRO,	"ST Micro" },
	{ NOR_MFR_MICRON,	"Micron" },
	{ NOR_MFR_NATIONAL,	"National" },
	{ NOR_MFR_TOSHIBA,	"Toshiba" },
	{ NOR_MFR_HYNIX,	"Hynix" },
	{ NOR_MFGR_MACRONIX,	"Macronix" },
	{ NOR_MFR_SAMSUNG,	"Samsung" },
	{ NOR_MFR_UNKNOWN,	"Unknown" }
};

static const char *
nor_midtoname(int id)
{
	int i;

	for (i = 0; nor_mfrs[i].id != 0; i++) {
		if (nor_mfrs[i].id == id)
			return nor_mfrs[i].name;
	}

	KASSERT(nor_mfrs[i].id == 0);

	return nor_mfrs[i].name;
}
#endif

/* ARGSUSED */
static int
nor_match(device_t parent, cfdata_t match, void *aux)
{
	/* pseudo device, always attaches */
	return 1;
}

static void
nor_attach(device_t parent, device_t self, void *aux)
{
	struct nor_softc * const sc = device_private(self);
	struct nor_attach_args * const naa = aux;
	struct nor_chip * const chip = &sc->sc_chip;

	sc->sc_dev = self;
	sc->sc_controller_dev = parent;
	sc->sc_nor_if = naa->naa_nor_if;

	aprint_naive("\n");
	aprint_normal("\n");

	if (nor_scan_media(self, chip))
		return;

	sc->sc_flash_if = nor_flash_if;
	sc->sc_flash_if.erasesize = chip->nc_block_size;
	sc->sc_flash_if.page_size = chip->nc_page_size;
	sc->sc_flash_if.writesize = chip->nc_page_size;

	/* allocate cache */
#ifdef NOTYET
	chip->nc_oob_cache = kmem_alloc(chip->nc_spare_size, KM_SLEEP);
#endif
	chip->nc_page_cache = kmem_alloc(chip->nc_page_size, KM_SLEEP);

	mutex_init(&sc->sc_device_lock, MUTEX_DEFAULT, IPL_NONE);

	if (flash_sync_thread_init(&sc->sc_flash_io, self, &sc->sc_flash_if)) {
		goto error;
	}

	if (!pmf_device_register1(sc->sc_dev, NULL, NULL, nor_shutdown))
		aprint_error_dev(sc->sc_dev,
		    "couldn't establish power handler\n");

#ifdef NOR_BBT
	nor_bbt_init(self);
	nor_bbt_scan(self);
#endif

	/*
	 * Attach all our devices
	 */
	config_search_ia(nor_search, self, NULL, NULL);

	return;

error:
#ifdef NOTET
	kmem_free(chip->nc_oob_cache, chip->nc_spare_size);
#endif
	kmem_free(chip->nc_page_cache, chip->nc_page_size);
	mutex_destroy(&sc->sc_device_lock);
}

static int
nor_search(device_t parent, cfdata_t cf, const int *ldesc, void *aux)
{
	struct nor_softc * const sc = device_private(parent);
	struct nor_chip * const chip = &sc->sc_chip;
	struct flash_attach_args faa;

	faa.partinfo.part_offset = cf->cf_loc[FLASHBUSCF_OFFSET];

	if (cf->cf_loc[FLASHBUSCF_SIZE] == 0) {
		faa.partinfo.part_size =
		    chip->nc_size - faa.partinfo.part_offset;
	} else {
		faa.partinfo.part_size = cf->cf_loc[FLASHBUSCF_SIZE];
	}

	if (cf->cf_loc[FLASHBUSCF_READONLY])
		faa.partinfo.part_flags = FLASH_PART_READONLY;
	else
		faa.partinfo.part_flags = 0;

	faa.flash_if = &sc->sc_flash_if;

	if (config_match(parent, cf, &faa)) {
		if (config_attach(parent, cf, &faa, nor_print) != NULL) {
			return 0;
		} else {
			return 1;
		}
	}

	return 1;
}

static int
nor_detach(device_t self, int flags)
{
	struct nor_softc * const sc = device_private(self);
	struct nor_chip * const chip = &sc->sc_chip;
	int error = 0;

	error = config_detach_children(self, flags);
	if (error) {
		return error;
	}

	flash_sync_thread_destroy(&sc->sc_flash_io);
#ifdef NOR_BBT
	nor_bbt_detach(self);
#endif
#ifdef NOTET
	/* free oob cache */
	kmem_free(chip->nc_oob_cache, chip->nc_spare_size);
#endif
	kmem_free(chip->nc_page_cache, chip->nc_page_size);

	mutex_destroy(&sc->sc_device_lock);

	pmf_device_deregister(sc->sc_dev);

	return error;
}

static int
nor_print(void *aux, const char *pnp)
{
	if (pnp != NULL)
		aprint_normal("nor at %s\n", pnp);

	return UNCONF;
}

/* ask for a nor driver to attach to the controller */
device_t
nor_attach_mi(struct nor_interface * const nor_if, device_t parent)
{
	struct nor_attach_args arg;

	KASSERT(nor_if != NULL);

	if (nor_if->select == NULL)
		nor_if->select = &nor_default_select;
	if (nor_if->read_page == NULL)
		nor_if->read_page = &nor_default_read_page;
	if (nor_if->program_page == NULL)
		nor_if->program_page = &nor_default_program_page;

	arg.naa_nor_if = nor_if;

	device_t dev = config_found_ia(parent, "norbus", &arg, nor_print);

	return dev;
}

static void
nor_default_select(device_t self, bool n)
{
	/* do nothing */
	return;
}

static int
nor_flash_submit(device_t self, buf_t * const bp)
{
	struct nor_softc * const sc = device_private(self);

	return flash_io_submit(&sc->sc_flash_io, bp);
}


/* default everything to reasonable values, to ease future api changes */
void
nor_init_interface(struct nor_interface * const nor_if)
{
	nor_if->select = &nor_default_select;
	nor_if->read_1 = NULL;
	nor_if->read_2 = NULL;
	nor_if->read_4 = NULL;
	nor_if->read_buf_1 = NULL;
	nor_if->read_buf_2 = NULL;
	nor_if->read_buf_4 = NULL;
	nor_if->write_1 = NULL;
	nor_if->write_2 = NULL;
	nor_if->write_4 = NULL;
	nor_if->write_buf_1 = NULL;
	nor_if->write_buf_2 = NULL;
	nor_if->write_buf_4 = NULL;
	nor_if->busy = NULL;
}

#ifdef NOTYET
/* handle quirks here */
static void
nor_quirks(device_t self, struct nor_chip * const chip)
{
	/* this is an example only! */
	switch (chip->nc_manf_id) {
	case NOR_MFR_SAMSUNG:
		if (chip->nc_dev_id == 0x00) {
			/* do something only samsung chips need */
			/* or */
			/* chip->nc_quirks |= NC_QUIRK_NO_READ_START */
		}
	}

	return;
}
#endif

/**
 * scan media to determine the chip's properties
 * this function resets the device
 */
static int
nor_scan_media(device_t self, struct nor_chip * const chip)
{
	struct nor_softc * const sc = device_private(self);
	char pbuf[3][sizeof("XXXX MB")];

	KASSERT(sc->sc_nor_if != NULL);
	KASSERT(sc->sc_nor_if->scan_media != NULL);
	int error = sc->sc_nor_if->scan_media(self, chip);
	if (error != 0)
		return error;

#ifdef NOR_VERBOSE
	aprint_normal_dev(self,
	    "manufacturer id: 0x%.4x (%s), device id: 0x%.4x\n",
	    chip->nc_manf_id,
	    nor_midtoname(chip->nc_manf_id),
	    chip->nc_dev_id);
#endif

	format_bytes(pbuf[0], sizeof(pbuf[0]), chip->nc_page_size);
	format_bytes(pbuf[1], sizeof(pbuf[1]), chip->nc_spare_size);
	format_bytes(pbuf[2], sizeof(pbuf[2]), chip->nc_block_size);
	aprint_normal_dev(self,
	    "page size: %s, spare size: %s, block size: %s\n",
	    pbuf[0], pbuf[1], pbuf[2]);

	format_bytes(pbuf[0], sizeof(pbuf[0]), chip->nc_size);
	aprint_normal_dev(self,
	    "LUN size: %" PRIu32 " blocks, LUNs: %" PRIu8
	    ", total storage size: %s\n",
	    chip->nc_lun_blocks, chip->nc_num_luns, pbuf[0]);

#ifdef NOTYET
	/* XXX does this apply to nor? */
	/*
	 * calculate badblock marker offset in oob
	 * we try to be compatible with linux here
	 */
	if (chip->nc_page_size > 512)
		chip->nc_badmarker_offs = 0;
	else
		chip->nc_badmarker_offs = 5;
#endif

	/* Calculate page shift and mask */
	chip->nc_page_shift = ffs(chip->nc_page_size) - 1;
	chip->nc_page_mask = ~(chip->nc_page_size - 1);
	/* same for block */
	chip->nc_block_shift = ffs(chip->nc_block_size) - 1;
	chip->nc_block_mask = ~(chip->nc_block_size - 1);

#ifdef NOTYET
	/* look for quirks here if needed in future */
	nor_quirks(self, chip);
#endif

	return 0;
}

/* ARGSUSED */
static bool
nor_shutdown(device_t self, int howto)
{
	return true;
}

/* implementation of the block device API */

/* read a page, default implementation */
static int
nor_default_read_page(device_t self, flash_off_t offset, uint8_t * const data)
{
	struct nor_softc * const sc = device_private(self);
	struct nor_chip * const chip = &sc->sc_chip;

	/*
	 * access by specified access_width
	 * note: #bits == 1 << width
	 */
	switch(sc->sc_nor_if->access_width) {
	case 0:
		nor_read_buf_1(self, offset, data, chip->nc_page_size);
		break;
	case 1:
		nor_read_buf_2(self, offset, data, chip->nc_page_size);
		break;
	case 2:
		nor_read_buf_4(self, offset, data, chip->nc_page_size);
		break;
#ifdef NOTYET
	case 3:
		nor_read_buf_8(self, offset, data, chip->nc_page_size);
		break;
#endif
	default:
		panic("%s: bad width %d\n", __func__, sc->sc_nor_if->access_width);
	}

#if 0
	/* for debugging new drivers */
	nor_dump_data("page", data, chip->nc_page_size);
#endif

	return 0;
}

/* write a page, default implementation */
static int
nor_default_program_page(device_t self, flash_off_t offset,
    const uint8_t * const data)
{
	struct nor_softc * const sc = device_private(self);
	struct nor_chip * const chip = &sc->sc_chip;

	/*
	 * access by specified width
	 * #bits == 1 << access_width
	 */
	switch(sc->sc_nor_if->access_width) {
	case 0:
		nor_write_buf_1(self, offset, data, chip->nc_page_size);
		break;
	case 1:
		nor_write_buf_2(self, offset, data, chip->nc_page_size);
		break;
	case 2:
		nor_write_buf_4(self, offset, data, chip->nc_page_size);
		break;
#ifdef NOTYET
	case 3:
		nor_write_buf_8(self, offset, data, chip->nc_page_size);
		break;
#endif
	default:
		panic("%s: bad width %d\n", __func__,
			sc->sc_nor_if->access_width);
	}

#if 0
	/* for debugging new drivers */
	nor_dump_data("page", data, chip->nc_page_size);
#endif

	return 0;
}

/*
 * nor_flash_erase_all - erase the entire chip
 *
 * XXX a good way to brick your system
 */
static int
nor_flash_erase_all(device_t self)
{
	struct nor_softc * const sc = device_private(self);
	int error;

	mutex_enter(&sc->sc_device_lock);
	error = nor_erase_all(self);
	mutex_exit(&sc->sc_device_lock);

	return error;
}

static int
nor_flash_erase(device_t self, struct flash_erase_instruction * const ei)
{
	struct nor_softc * const sc = device_private(self);
	struct nor_chip * const chip = &sc->sc_chip;
	flash_off_t addr;
	int error = 0;

	if (ei->ei_addr < 0 || ei->ei_len < chip->nc_block_size)
		return EINVAL;

	if (ei->ei_addr + ei->ei_len > chip->nc_size) {
		DPRINTF(("%s: erase address is past the end"
			" of the device\n", __func__));
		return EINVAL;
	}

	if ((ei->ei_addr == 0) && (ei->ei_len == chip->nc_size)
	&&  (sc->sc_nor_if->erase_all != NULL)) {
		return nor_flash_erase_all(self);
	}

	if (ei->ei_addr % chip->nc_block_size != 0) {
		aprint_error_dev(self,
		    "nor_flash_erase: ei_addr (%ju) is not"
		    " a multiple of block size (%ju)\n",
		    (uintmax_t)ei->ei_addr,
		    (uintmax_t)chip->nc_block_size);
		return EINVAL;
	}

	if (ei->ei_len % chip->nc_block_size != 0) {
		aprint_error_dev(self,
		    "nor_flash_erase: ei_len (%ju) is not"
		    " a multiple of block size (%ju)",
		    (uintmax_t)ei->ei_len,
		    (uintmax_t)chip->nc_block_size);
		return EINVAL;
	}

	mutex_enter(&sc->sc_device_lock);
	addr = ei->ei_addr;
	while (addr < ei->ei_addr + ei->ei_len) {
#ifdef NOTYET
		if (nor_isbad(self, addr)) {
			aprint_error_dev(self, "bad block encountered\n");
			ei->ei_state = FLASH_ERASE_FAILED;
			error = EIO;
			goto out;
		}
#endif

		error = nor_erase_block(self, addr);
		if (error) {
			ei->ei_state = FLASH_ERASE_FAILED;
			goto out;
		}

		addr += chip->nc_block_size;
	}
	mutex_exit(&sc->sc_device_lock);

	ei->ei_state = FLASH_ERASE_DONE;
	if (ei->ei_callback != NULL) {
		ei->ei_callback(ei);
	}

	return 0;
out:
	mutex_exit(&sc->sc_device_lock);

	return error;
}

/*
 * handle (page) unaligned write to nor
 */
static int
nor_flash_write_unaligned(device_t self, flash_off_t offset, size_t len,
    size_t * const retlen, const uint8_t * const buf)
{
	struct nor_softc * const sc = device_private(self);
	struct nor_chip * const chip = &sc->sc_chip;
	flash_off_t first, last, firstoff;
	const uint8_t *bufp;
	flash_off_t addr;
	size_t left, count;
	int error = 0, i;

	first = offset & chip->nc_page_mask;
	firstoff = offset & ~chip->nc_page_mask;
	/* XXX check if this should be len - 1 */
	last = (offset + len) & chip->nc_page_mask;
	count = last - first + 1;

	addr = first;
	*retlen = 0;

	mutex_enter(&sc->sc_device_lock);
	if (count == 1) {
#ifdef NOTYET
		if (nor_isbad(self, addr)) {
			aprint_error_dev(self,
			    "nor_flash_write_unaligned: "
			    "bad block encountered\n");
			error = EIO;
			goto out;
		}
#endif

		error = nor_read_page(self, addr, chip->nc_page_cache);
		if (error) {
			goto out;
		}

		memcpy(chip->nc_page_cache + firstoff, buf, len);

		error = nor_program_page(self, addr, chip->nc_page_cache);
		if (error) {
			goto out;
		}

		*retlen = len;
		goto out;
	}

	bufp = buf;
	left = len;

	for (i = 0; i < count && left != 0; i++) {
#ifdef NOTYET
		if (nor_isbad(self, addr)) {
			aprint_error_dev(self,
			    "nor_flash_write_unaligned: "
			    "bad block encountered\n");
			error = EIO;
			goto out;
		}
#endif

		if (i == 0) {
			error = nor_read_page(self, addr, chip->nc_page_cache);
			if (error) {
				goto out;
			}

			memcpy(chip->nc_page_cache + firstoff,
			    bufp, chip->nc_page_size - firstoff);

			printf("write page: %s: %d\n", __FILE__, __LINE__);
			error = nor_program_page(self, addr,
				chip->nc_page_cache);
			if (error) {
				goto out;
			}

			bufp += chip->nc_page_size - firstoff;
			left -= chip->nc_page_size - firstoff;
			*retlen += chip->nc_page_size - firstoff;

		} else if (i == count - 1) {
			error = nor_read_page(self, addr, chip->nc_page_cache);
			if (error) {
				goto out;
			}

			memcpy(chip->nc_page_cache, bufp, left);

			error = nor_program_page(self, addr,
				chip->nc_page_cache);
			if (error) {
				goto out;
			}

			*retlen += left;
			KASSERT(left < chip->nc_page_size);

		} else {
			/* XXX debug */
			if (left > chip->nc_page_size) {
				printf("left: %zu, i: %d, count: %zu\n",
				    (size_t )left, i, count);
			}
			KASSERT(left > chip->nc_page_size);

			error = nor_program_page(self, addr, bufp);
			if (error) {
				goto out;
			}

			bufp += chip->nc_page_size;
			left -= chip->nc_page_size;
			*retlen += chip->nc_page_size;
		}

		addr += chip->nc_page_size;
	}

	KASSERT(*retlen == len);
out:
	mutex_exit(&sc->sc_device_lock);

	return error;
}

static int
nor_flash_write(device_t self, flash_off_t offset, size_t len,
    size_t * const retlen, const uint8_t * const buf)
{
	struct nor_softc * const sc = device_private(self);
	struct nor_chip * const chip = &sc->sc_chip;
	const uint8_t *bufp;
	size_t pages, page;
	daddr_t addr;
	int error = 0;

	if ((offset + len) > chip->nc_size) {
		DPRINTF(("%s: write (off: 0x%jx, len: %ju),"
			" exceeds device size (0x%jx)\n", __func__,
			(uintmax_t)offset, (uintmax_t)len,
			(uintmax_t)chip->nc_size));
		return EINVAL;
	}

	if (len % chip->nc_page_size != 0 ||
	    offset % chip->nc_page_size != 0) {
		return nor_flash_write_unaligned(self,
		    offset, len, retlen, buf);
	}

	pages = len / chip->nc_page_size;
	KASSERT(pages != 0);
	*retlen = 0;

	addr = offset;
	bufp = buf;

	mutex_enter(&sc->sc_device_lock);
	for (page = 0; page < pages; page++) {
#ifdef NOTYET
		/* do we need this check here? */
		if (nor_isbad(self, addr)) {
			aprint_error_dev(self,
			    "nor_flash_write: bad block encountered\n");

			error = EIO;
			goto out;
		}
#endif

		error = nor_program_page(self, addr, bufp);
		if (error) {
			goto out;
		}

		addr += chip->nc_page_size;
		bufp += chip->nc_page_size;
		*retlen += chip->nc_page_size;
	}
out:
	mutex_exit(&sc->sc_device_lock);
	DPRINTF(("%s: retlen: %zu, len: %zu\n", __func__, *retlen, len));

	return error;
}

/*
 * handle (page) unaligned read from nor
 */
static int
nor_flash_read_unaligned(device_t self, flash_off_t offset, size_t len,
    size_t * const retlen, uint8_t * const buf)
{
	struct nor_softc * const sc = device_private(self);
	struct nor_chip * const chip = &sc->sc_chip;
	daddr_t first, last, count, firstoff;
	uint8_t *bufp;
	daddr_t addr;
	size_t left;
	int error = 0, i;

	first = offset & chip->nc_page_mask;
	firstoff = offset & ~chip->nc_page_mask;
	last = (offset + len) & chip->nc_page_mask;
	count = (last - first) / chip->nc_page_size + 1;

	addr = first;
	bufp = buf;
	left = len;
	*retlen = 0;

	mutex_enter(&sc->sc_device_lock);
	if (count == 1) {
		error = nor_read_page(self, addr, chip->nc_page_cache);
		if (error) {
			goto out;
		}

		memcpy(bufp, chip->nc_page_cache + firstoff, len);

		*retlen = len;
		goto out;
	}

	for (i = 0; i < count && left != 0; i++) {
		/* XXX Why use the page cache here ? */
		error = nor_read_page(self, addr, chip->nc_page_cache);
		if (error) {
			goto out;
		}

		if (i == 0) {
			memcpy(bufp, chip->nc_page_cache + firstoff,
			    chip->nc_page_size - firstoff);

			bufp += chip->nc_page_size - firstoff;
			left -= chip->nc_page_size - firstoff;
			*retlen += chip->nc_page_size - firstoff;

		} else if (i == count - 1) {
			memcpy(bufp, chip->nc_page_cache, left);
			*retlen += left;
			KASSERT(left < chip->nc_page_size);

		} else {
			memcpy(bufp, chip->nc_page_cache, chip->nc_page_size);

			bufp += chip->nc_page_size;
			left -= chip->nc_page_size;
			*retlen += chip->nc_page_size;
		}

		addr += chip->nc_page_size;
	}
	KASSERT(*retlen == len);
out:
	mutex_exit(&sc->sc_device_lock);

	return error;
}

static int
nor_flash_read(device_t self, flash_off_t offset, size_t len,
    size_t * const retlen, uint8_t * const buf)
{
	struct nor_softc * const sc = device_private(self);
	struct nor_chip * const chip = &sc->sc_chip;
	uint8_t *bufp;
	size_t addr;
	size_t i, pages;
	int error = 0;

	*retlen = 0;

	DPRINTF(("%s: off: 0x%jx, len: %zu\n",
		__func__, (uintmax_t)offset, len));

	if (__predict_false((offset + len) > chip->nc_size)) {
		DPRINTF(("%s: read (off: 0x%jx, len: %zu),"
			" exceeds device size (%ju)\n", __func__,
			(uintmax_t)offset, len, (uintmax_t)chip->nc_size));
		return EINVAL;
	}

	/* Handle unaligned access, shouldnt be needed when using the
	 * block device, as strategy handles it, so only low level
	 * accesses will use this path
	 */
	/* XXX^2 */
#if 0
	if (len < chip->nc_page_size)
		panic("TODO page size is larger than read size");
#endif

	if (len % chip->nc_page_size != 0 ||
	    offset % chip->nc_page_size != 0) {
		return nor_flash_read_unaligned(self,
		    offset, len, retlen, buf);
	}

	bufp = buf;
	addr = offset;
	pages = len / chip->nc_page_size;

	mutex_enter(&sc->sc_device_lock);
	for (i = 0; i < pages; i++) {
#ifdef NOTYET
		/* XXX do we need this check here? */
		if (nor_isbad(self, addr)) {
			aprint_error_dev(self, "bad block encountered\n");
			error = EIO;
			goto out;
		}
#endif
		error = nor_read_page(self, addr, bufp);
		if (error)
			goto out;

		bufp += chip->nc_page_size;
		addr += chip->nc_page_size;
		*retlen += chip->nc_page_size;
	}
out:
	mutex_exit(&sc->sc_device_lock);

	return error;
}

static int
nor_flash_isbad(device_t self, flash_off_t ofs, bool * const isbad)
{
	struct nor_softc * const sc = device_private(self);
	struct nor_chip * const chip = &sc->sc_chip;
#ifdef NOTYET
	bool result;
#endif

	if (ofs > chip->nc_size) {
		DPRINTF(("%s: offset 0x%jx is larger than"
			" device size (0x%jx)\n", __func__,
			(uintmax_t)ofs, (uintmax_t)chip->nc_size));
		return EINVAL;
	}

	if (ofs % chip->nc_block_size != 0) {
		DPRINTF(("offset (0x%jx) is not the multiple of block size "
			"(%ju)",
			(uintmax_t)ofs, (uintmax_t)chip->nc_block_size));
		return EINVAL;
	}

#ifdef NOTYET
	mutex_enter(&sc->sc_device_lock);
	result = nor_isbad(self, ofs);
	mutex_exit(&sc->sc_device_lock);

	*isbad = result;
#else
	*isbad = false;
#endif

	return 0;
}

static int
nor_flash_markbad(device_t self, flash_off_t ofs)
{
	struct nor_softc * const sc = device_private(self);
	struct nor_chip * const chip = &sc->sc_chip;

	if (ofs > chip->nc_size) {
		DPRINTF(("%s: offset 0x%jx is larger than"
			" device size (0x%jx)\n", __func__,
			ofs, (uintmax_t)chip->nc_size));
		return EINVAL;
	}

	if (ofs % chip->nc_block_size != 0) {
		panic("offset (%ju) is not the multiple of block size (%ju)",
		    (uintmax_t)ofs, (uintmax_t)chip->nc_block_size);
	}

	/* TODO: implement this */

	return 0;
}

static int
sysctl_nor_verify(SYSCTLFN_ARGS)
{
	int error, t;
	struct sysctlnode node;

	node = *rnode;
	t = *(int *)rnode->sysctl_data;
	node.sysctl_data = &t;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	if (node.sysctl_num == nor_cachesync_nodenum) {
		if (t <= 0 || t > 60)
			return EINVAL;
	} else {
		return EINVAL;
	}

	*(int *)rnode->sysctl_data = t;

	return 0;
}

SYSCTL_SETUP(sysctl_nor, "sysctl nor subtree setup")
{
	int rc, nor_root_num;
	const struct sysctlnode *node;

	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT, CTLTYPE_NODE, "nor",
	    SYSCTL_DESCR("NOR driver controls"),
	    NULL, 0, NULL, 0, CTL_HW, CTL_CREATE, CTL_EOL)) != 0) {
		goto error;
	}

	nor_root_num = node->sysctl_num;

	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
	    CTLTYPE_INT, "cache_sync_timeout",
	    SYSCTL_DESCR("NOR write cache sync timeout in seconds"),
	    sysctl_nor_verify, 0, &nor_cachesync_timeout,
	    0, CTL_HW, nor_root_num, CTL_CREATE,
	    CTL_EOL)) != 0) {
		goto error;
	}

	nor_cachesync_nodenum = node->sysctl_num;

	return;

error:
	aprint_error("%s: sysctl_createv failed (rc = %d)\n", __func__, rc);
}

MODULE(MODULE_CLASS_DRIVER, nor, "flash");

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
nor_modcmd(modcmd_t cmd, void *opaque)
{
	switch (cmd) {
	case MODULE_CMD_INIT:
#ifdef _MODULE
		return config_init_component(cfdriver_ioconf_nor,
		    cfattach_ioconf_nor, cfdata_ioconf_nor);
#else
		return 0;
#endif
	case MODULE_CMD_FINI:
#ifdef _MODULE
		return config_fini_component(cfdriver_ioconf_nor,
		    cfattach_ioconf_nor, cfdata_ioconf_nor);
#else
		return 0;
#endif
	default:
		return ENOTTY;
	}
}
