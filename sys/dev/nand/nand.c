/*	$NetBSD: nand.c,v 1.23 2013/10/20 17:13:18 christos Exp $	*/

/*-
 * Copyright (c) 2010 Department of Software Engineering,
 *		      University of Szeged, Hungary
 * Copyright (c) 2010 Adam Hoka <ahoka@NetBSD.org>
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

/* Common driver for NAND chips implementing the ONFI 2.2 specification */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: nand.c,v 1.23 2013/10/20 17:13:18 christos Exp $");

#include "locators.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/device.h>
#include <sys/kmem.h>
#include <sys/atomic.h>

#include <dev/flash/flash.h>
#include <dev/flash/flash_io.h>
#include <dev/nand/nand.h>
#include <dev/nand/onfi.h>
#include <dev/nand/hamming.h>
#include <dev/nand/nand_bbt.h>
#include <dev/nand/nand_crc.h>

#include "opt_nand.h"

int nand_match(device_t, cfdata_t, void *);
void nand_attach(device_t, device_t, void *);
int nand_detach(device_t, int);
bool nand_shutdown(device_t, int);

int nand_print(void *, const char *);

static int nand_search(device_t, cfdata_t, const int *, void *);
static void nand_address_row(device_t, size_t);
static void nand_address_column(device_t, size_t, size_t);
static int nand_fill_chip_structure(device_t, struct nand_chip *);
static int nand_scan_media(device_t, struct nand_chip *);
static bool nand_check_wp(device_t);

CFATTACH_DECL_NEW(nand, sizeof(struct nand_softc),
    nand_match, nand_attach, nand_detach, NULL);

#ifdef NAND_DEBUG
int	nanddebug = NAND_DEBUG;
#endif

struct flash_interface nand_flash_if = {
	.type = FLASH_TYPE_NAND,

	.read = nand_flash_read,
	.write = nand_flash_write,
	.erase = nand_flash_erase,
	.block_isbad = nand_flash_isbad,
	.block_markbad = nand_flash_markbad,

	.submit = nand_flash_submit
};

#ifdef NAND_VERBOSE
const struct nand_manufacturer nand_mfrs[] = {
	{ NAND_MFR_AMD,		"AMD" },
	{ NAND_MFR_FUJITSU,	"Fujitsu" },
	{ NAND_MFR_RENESAS,	"Renesas" },
	{ NAND_MFR_STMICRO,	"ST Micro" },
	{ NAND_MFR_MICRON,	"Micron" },
	{ NAND_MFR_NATIONAL,	"National" },
	{ NAND_MFR_TOSHIBA,	"Toshiba" },
	{ NAND_MFR_HYNIX,	"Hynix" },
	{ NAND_MFR_SAMSUNG,	"Samsung" },
	{ NAND_MFR_UNKNOWN,	"Unknown" }
};

static const char *
nand_midtoname(int id)
{
	int i;

	for (i = 0; nand_mfrs[i].id != 0; i++) {
		if (nand_mfrs[i].id == id)
			return nand_mfrs[i].name;
	}

	KASSERT(nand_mfrs[i].id == 0);

	return nand_mfrs[i].name;
}
#endif

/* ARGSUSED */
int
nand_match(device_t parent, cfdata_t match, void *aux)
{
	/* pseudo device, always attaches */
	return 1;
}

void
nand_attach(device_t parent, device_t self, void *aux)
{
	struct nand_softc *sc = device_private(self);
	struct nand_attach_args *naa = aux;
	struct nand_chip *chip = &sc->sc_chip;

	sc->sc_dev = self;
	sc->controller_dev = parent;
	sc->nand_if = naa->naa_nand_if;

	aprint_naive("\n");

	if (nand_check_wp(self)) {
		aprint_error("NAND chip is write protected!\n");
		return;
	}

	if (nand_scan_media(self, chip)) {
		return;
	}

	nand_flash_if.erasesize = chip->nc_block_size;
	nand_flash_if.page_size = chip->nc_page_size;
	nand_flash_if.writesize = chip->nc_page_size;

	/* allocate cache */
	chip->nc_oob_cache = kmem_alloc(chip->nc_spare_size, KM_SLEEP);
	chip->nc_page_cache = kmem_alloc(chip->nc_page_size, KM_SLEEP);

	mutex_init(&sc->sc_device_lock, MUTEX_DEFAULT, IPL_NONE);

	if (flash_sync_thread_init(&sc->sc_flash_io, self, &nand_flash_if)) {
		goto error;
	}

	if (!pmf_device_register1(sc->sc_dev, NULL, NULL, nand_shutdown))
		aprint_error_dev(sc->sc_dev,
		    "couldn't establish power handler\n");

#ifdef NAND_BBT
	nand_bbt_init(self);
	nand_bbt_scan(self);
#endif

	/*
	 * Attach all our devices
	 */
	config_search_ia(nand_search, self, NULL, NULL);

	return;
error:
	kmem_free(chip->nc_oob_cache, chip->nc_spare_size);
	kmem_free(chip->nc_page_cache, chip->nc_page_size);
	mutex_destroy(&sc->sc_device_lock);
}

static int
nand_search(device_t parent, cfdata_t cf, const int *ldesc, void *aux)
{
	struct nand_softc *sc = device_private(parent);
	struct nand_chip *chip = &sc->sc_chip;
	struct flash_attach_args faa;

	faa.flash_if = &nand_flash_if;

	faa.partinfo.part_offset = cf->cf_loc[FLASHBUSCF_OFFSET];

	if (cf->cf_loc[FLASHBUSCF_SIZE] == 0) {
		faa.partinfo.part_size = chip->nc_size -
		    faa.partinfo.part_offset;
	} else {
		faa.partinfo.part_size = cf->cf_loc[FLASHBUSCF_SIZE];
	}

	if (cf->cf_loc[FLASHBUSCF_READONLY])
		faa.partinfo.part_flags = FLASH_PART_READONLY;
	else
		faa.partinfo.part_flags = 0;

	if (config_match(parent, cf, &faa)) {
		if (config_attach(parent, cf, &faa, nand_print) != NULL) {
			return 0;
		} else {
			return 1;
		}
	}

	return 1;
}

int
nand_detach(device_t self, int flags)
{
	struct nand_softc *sc = device_private(self);
	struct nand_chip *chip = &sc->sc_chip;
	int error = 0;

	error = config_detach_children(self, flags);
	if (error) {
		return error;
	}

	flash_sync_thread_destroy(&sc->sc_flash_io);
#ifdef NAND_BBT
	nand_bbt_detach(self);
#endif
	/* free oob cache */
	kmem_free(chip->nc_oob_cache, chip->nc_spare_size);
	kmem_free(chip->nc_page_cache, chip->nc_page_size);
	kmem_free(chip->nc_ecc_cache, chip->nc_ecc->necc_size);

	mutex_destroy(&sc->sc_device_lock);

	pmf_device_deregister(sc->sc_dev);

	return error;
}

int
nand_print(void *aux, const char *pnp)
{
	if (pnp != NULL)
		aprint_normal("nand at %s\n", pnp);

	return UNCONF;
}

/* ask for a nand driver to attach to the controller */
device_t
nand_attach_mi(struct nand_interface *nand_if, device_t parent)
{
	struct nand_attach_args arg;

	KASSERT(nand_if != NULL);

	/* fill the defaults if we have null pointers */
	if (nand_if->program_page == NULL) {
		nand_if->program_page = &nand_default_program_page;
	}

	if (nand_if->read_page == NULL) {
		nand_if->read_page = &nand_default_read_page;
	}

	arg.naa_nand_if = nand_if;
	return config_found_ia(parent, "nandbus", &arg, nand_print);
}

/* default everything to reasonable values, to ease future api changes */
void
nand_init_interface(struct nand_interface *interface)
{
	interface->select = &nand_default_select;
	interface->command = NULL;
	interface->address = NULL;
	interface->read_buf_1 = NULL;
	interface->read_buf_2 = NULL;
	interface->read_1 = NULL;
	interface->read_2 = NULL;
	interface->write_buf_1 = NULL;
	interface->write_buf_2 = NULL;
	interface->write_1 = NULL;
	interface->write_2 = NULL;
	interface->busy = NULL;

	/*-
	 * most drivers dont want to change this, but some implement
	 * read/program in one step
	 */
	interface->program_page = &nand_default_program_page;
	interface->read_page = &nand_default_read_page;

	/* default to soft ecc, that should work everywhere */
	interface->ecc_compute = &nand_default_ecc_compute;
	interface->ecc_correct = &nand_default_ecc_correct;
	interface->ecc_prepare = NULL;
	interface->ecc.necc_code_size = 3;
	interface->ecc.necc_block_size = 256;
	interface->ecc.necc_type = NAND_ECC_TYPE_SW;
}

#if 0
/* handle quirks here */
static void
nand_quirks(device_t self, struct nand_chip *chip)
{
	/* this is an example only! */
	switch (chip->nc_manf_id) {
	case NAND_MFR_SAMSUNG:
		if (chip->nc_dev_id == 0x00) {
			/* do something only samsung chips need */
			/* or */
			/* chip->nc_quirks |= NC_QUIRK_NO_READ_START */
		}
	}

	return;
}
#endif

static int
nand_fill_chip_structure_legacy(device_t self, struct nand_chip *chip)
{
	switch (chip->nc_manf_id) {
	case NAND_MFR_MICRON:
		return nand_read_parameters_micron(self, chip);
	case NAND_MFR_SAMSUNG:
		return nand_read_parameters_samsung(self, chip);
	default:
		return 1;
	}

	return 0;
}

/**
 * scan media to determine the chip's properties
 * this function resets the device
 */
static int
nand_scan_media(device_t self, struct nand_chip *chip)
{
	struct nand_softc *sc = device_private(self);
	struct nand_ecc *ecc;
	uint8_t onfi_signature[4];

	nand_select(self, true);
	nand_command(self, ONFI_RESET);
	nand_select(self, false);

	/* check if the device implements the ONFI standard */
	nand_select(self, true);
	nand_command(self, ONFI_READ_ID);
	nand_address(self, 0x20);
	nand_read_1(self, &onfi_signature[0]);
	nand_read_1(self, &onfi_signature[1]);
	nand_read_1(self, &onfi_signature[2]);
	nand_read_1(self, &onfi_signature[3]);
	nand_select(self, false);

	if (onfi_signature[0] != 'O' || onfi_signature[1] != 'N' ||
	    onfi_signature[2] != 'F' || onfi_signature[3] != 'I') {
		chip->nc_isonfi = false;

		aprint_normal(": Legacy NAND Flash\n");

		nand_read_id(self, &chip->nc_manf_id, &chip->nc_dev_id);

		if (nand_fill_chip_structure_legacy(self, chip)) {
			aprint_error_dev(self,
			    "can't read device parameters for legacy chip\n");
			return 1;
		}
	} else {
		chip->nc_isonfi = true;

		aprint_normal(": ONFI NAND Flash\n");

		nand_read_id(self, &chip->nc_manf_id, &chip->nc_dev_id);

		if (nand_fill_chip_structure(self, chip)) {
			aprint_error_dev(self,
			    "can't read device parameters\n");
			return 1;
		}
	}

#ifdef NAND_VERBOSE
	aprint_normal_dev(self,
	    "manufacturer id: 0x%.2x (%s), device id: 0x%.2x\n",
	    chip->nc_manf_id,
	    nand_midtoname(chip->nc_manf_id),
	    chip->nc_dev_id);
#endif

	aprint_normal_dev(self,
	    "page size: %" PRIu32 " bytes, spare size: %" PRIu32 " bytes, "
	    "block size: %" PRIu32 " bytes\n",
	    chip->nc_page_size, chip->nc_spare_size, chip->nc_block_size);

	aprint_normal_dev(self,
	    "LUN size: %" PRIu32 " blocks, LUNs: %" PRIu8
	    ", total storage size: %" PRIu64 " MB\n",
	    chip->nc_lun_blocks, chip->nc_num_luns,
	    chip->nc_size / 1024 / 1024);

	aprint_normal_dev(self, "column cycles: %" PRIu8 ", row cycles: %"
	    PRIu8 ", width: %s\n",
	    chip->nc_addr_cycles_column, chip->nc_addr_cycles_row,
	    (chip->nc_flags & NC_BUSWIDTH_16) ? "x16" : "x8");

	ecc = chip->nc_ecc = &sc->nand_if->ecc;

	/*
	 * calculate the place of ecc data in oob
	 * we try to be compatible with Linux here
	 */
	switch (chip->nc_spare_size) {
	case 8:
		ecc->necc_offset = 0;
		break;
	case 16:
		ecc->necc_offset = 0;
		break;
	case 32:
		ecc->necc_offset = 0;
		break;
	case 64:
		ecc->necc_offset = 40;
		break;
	case 128:
		ecc->necc_offset = 80;
		break;
	default:
		panic("OOB size %" PRIu32 " is unexpected", chip->nc_spare_size);
	}

	ecc->necc_steps = chip->nc_page_size / ecc->necc_block_size;
	ecc->necc_size = ecc->necc_steps * ecc->necc_code_size;

	/* check if we fit in oob */
	if (ecc->necc_offset + ecc->necc_size > chip->nc_spare_size) {
		panic("NAND ECC bits dont fit in OOB");
	}

	/* TODO: mark free oob area available for file systems */

	chip->nc_ecc_cache = kmem_zalloc(ecc->necc_size, KM_SLEEP);

	/*
	 * calculate badblock marker offset in oob
	 * we try to be compatible with linux here
	 */
	if (chip->nc_page_size > 512)
		chip->nc_badmarker_offs = 0;
	else
		chip->nc_badmarker_offs = 5;

	/* Calculate page shift and mask */
	chip->nc_page_shift = ffs(chip->nc_page_size) - 1;
	chip->nc_page_mask = ~(chip->nc_page_size - 1);
	/* same for block */
	chip->nc_block_shift = ffs(chip->nc_block_size) - 1;
	chip->nc_block_mask = ~(chip->nc_block_size - 1);

	/* look for quirks here if needed in future */
	/* nand_quirks(self, chip); */

	return 0;
}

void
nand_read_id(device_t self, uint8_t *manf, uint8_t *dev)
{
	nand_select(self, true);
	nand_command(self, ONFI_READ_ID);
	nand_address(self, 0x00);

	nand_read_1(self, manf);
	nand_read_1(self, dev);

	nand_select(self, false);
}

int
nand_read_parameter_page(device_t self, struct onfi_parameter_page *params)
{
	uint8_t *bufp;
	uint16_t crc;
	int i;//, tries = 0;

	KASSERT(sizeof(*params) == 256);

//read_params:
//	tries++;

	nand_select(self, true);
	nand_command(self, ONFI_READ_PARAMETER_PAGE);
	nand_address(self, 0x00);

	nand_busy(self);

	/* TODO check the signature if it contains at least 2 letters */

	bufp = (uint8_t *)params;
	/* XXX why i am not using read_buf? */
	for (i = 0; i < 256; i++) {
		nand_read_1(self, &bufp[i]);
	}
	nand_select(self, false);

	/* validate the parameter page with the crc */
	crc = nand_crc16(bufp, 254);

	if (crc != params->param_integrity_crc) {
		aprint_error_dev(self, "parameter page crc check failed\n");
		/* TODO: we should read the next parameter page copy */
		return 1;
	}

	return 0;
}

static int
nand_fill_chip_structure(device_t self, struct nand_chip *chip)
{
	struct onfi_parameter_page params;
	uint8_t	vendor[13], model[21];
	int i;

	if (nand_read_parameter_page(self, &params)) {
		return 1;
	}

	/* strip manufacturer and model string */
	strlcpy(vendor, params.param_manufacturer, sizeof(vendor));
	for (i = 11; i > 0 && vendor[i] == ' '; i--)
		vendor[i] = 0;
	strlcpy(model, params.param_model, sizeof(model));
	for (i = 19; i > 0 && model[i] == ' '; i--)
		model[i] = 0;

	aprint_normal_dev(self, "vendor: %s, model: %s\n", vendor, model);

	chip->nc_page_size = le32toh(params.param_pagesize);
	chip->nc_block_size =
	    le32toh(params.param_blocksize) * chip->nc_page_size;
	chip->nc_spare_size = le16toh(params.param_sparesize);
	chip->nc_lun_blocks = le32toh(params.param_lunsize);
	chip->nc_num_luns = params.param_numluns;

	chip->nc_size =
	    chip->nc_block_size * chip->nc_lun_blocks * chip->nc_num_luns;

	/* the lower 4 bits contain the row address cycles */
	chip->nc_addr_cycles_row = params.param_addr_cycles & 0x07;
	/* the upper 4 bits contain the column address cycles */
	chip->nc_addr_cycles_column = (params.param_addr_cycles & ~0x07) >> 4;

	uint16_t features = le16toh(params.param_features);
	if (features & ONFI_FEATURE_16BIT) {
		chip->nc_flags |= NC_BUSWIDTH_16;
	}

	if (features & ONFI_FEATURE_EXTENDED_PARAM) {
		chip->nc_flags |= NC_EXTENDED_PARAM;
	}

	return 0;
}

/* ARGSUSED */
bool
nand_shutdown(device_t self, int howto)
{
	return true;
}

static void
nand_address_column(device_t self, size_t row, size_t column)
{
	struct nand_softc *sc = device_private(self);
	struct nand_chip *chip = &sc->sc_chip;
	uint8_t i;

	DPRINTF(("addressing row: 0x%jx column: %" PRIu32 "\n",
		(uintmax_t )row, column));

	/* XXX TODO */
	row >>= chip->nc_page_shift;

	/* Write the column (subpage) address */
	if (chip->nc_flags & NC_BUSWIDTH_16)
		column >>= 1;
	for (i = 0; i < chip->nc_addr_cycles_column; i++, column >>= 8)
		nand_address(self, column & 0xff);

	/* Write the row (page) address */
	for (i = 0; i < chip->nc_addr_cycles_row; i++, row >>= 8)
		nand_address(self, row & 0xff);
}

static void
nand_address_row(device_t self, size_t row)
{
	struct nand_softc *sc = device_private(self);
	struct nand_chip *chip = &sc->sc_chip;
	int i;

	/* XXX TODO */
	row >>= chip->nc_page_shift;

	/* Write the row (page) address */
	for (i = 0; i < chip->nc_addr_cycles_row; i++, row >>= 8)
		nand_address(self, row & 0xff);
}

static inline uint8_t
nand_get_status(device_t self)
{
	uint8_t status;

	nand_command(self, ONFI_READ_STATUS);
	nand_busy(self);
	nand_read_1(self, &status);

	return status;
}

static bool
nand_check_wp(device_t self)
{
	if (nand_get_status(self) & 0x80)
		return false;
	else
		return true;
}

static void
nand_prepare_read(device_t self, flash_off_t row, flash_off_t column)
{
	nand_command(self, ONFI_READ);
	nand_address_column(self, row, column);
	nand_command(self, ONFI_READ_START);

	nand_busy(self);
}

/* read a page with ecc correction, default implementation */
int
nand_default_read_page(device_t self, size_t offset, uint8_t *data)
{
	struct nand_softc *sc = device_private(self);
	struct nand_chip *chip = &sc->sc_chip;
	size_t b, bs, e, cs;
	uint8_t *ecc;
	int result;

	nand_prepare_read(self, offset, 0);

	bs = chip->nc_ecc->necc_block_size;
	cs = chip->nc_ecc->necc_code_size;

	/* decide if we access by 8 or 16 bits */
	if (chip->nc_flags & NC_BUSWIDTH_16) {
		for (b = 0, e = 0; b < chip->nc_page_size; b += bs, e += cs) {
			nand_ecc_prepare(self, NAND_ECC_READ);
			nand_read_buf_2(self, data + b, bs);
			nand_ecc_compute(self, data + b,
			    chip->nc_ecc_cache + e);
		}
	} else {
		for (b = 0, e = 0; b < chip->nc_page_size; b += bs, e += cs) {
			nand_ecc_prepare(self, NAND_ECC_READ);
			nand_read_buf_1(self, data + b, bs);
			nand_ecc_compute(self, data + b,
			    chip->nc_ecc_cache + e);
		}
	}

	/* for debugging new drivers */
#if 0
	nand_dump_data("page", data, chip->nc_page_size);
#endif

	nand_read_oob(self, offset, chip->nc_oob_cache);
	ecc = chip->nc_oob_cache + chip->nc_ecc->necc_offset;

	/* useful for debugging new ecc drivers */
#if 0
	printf("dumping ecc %d\n--------------\n", chip->nc_ecc->necc_steps);
	for (e = 0; e < chip->nc_ecc->necc_steps; e++) {
		printf("0x");
		for (b = 0; b < cs; b++) {
			printf("%.2hhx", ecc[e+b]);
		}
		printf(" 0x");
		for (b = 0; b < cs; b++) {
			printf("%.2hhx", chip->nc_ecc_cache[e+b]);
		}
		printf("\n");
	}
	printf("--------------\n");
#endif

	for (b = 0, e = 0; b < chip->nc_page_size; b += bs, e += cs) {
		result = nand_ecc_correct(self, data + b, ecc + e,
		    chip->nc_ecc_cache + e);

		switch (result) {
		case NAND_ECC_OK:
			break;
		case NAND_ECC_CORRECTED:
			aprint_error_dev(self,
			    "data corrected with ECC at page offset 0x%jx "
			    "block %zu\n", (uintmax_t)offset, b);
			break;
		case NAND_ECC_TWOBIT:
			aprint_error_dev(self,
			    "uncorrectable ECC error at page offset 0x%jx "
			    "block %zu\n", (uintmax_t)offset, b);
			return EIO;
			break;
		case NAND_ECC_INVALID:
			aprint_error_dev(self,
			    "invalid ECC in oob at page offset 0x%jx "
			    "block %zu\n", (uintmax_t)offset, b);
			return EIO;
			break;
		default:
			panic("invalid ECC correction errno");
		}
	}

	return 0;
}

int
nand_default_program_page(device_t self, size_t page, const uint8_t *data)
{
	struct nand_softc *sc = device_private(self);
	struct nand_chip *chip = &sc->sc_chip;
	size_t bs, cs, e, b;
	uint8_t status;
	uint8_t *ecc;

	nand_command(self, ONFI_PAGE_PROGRAM);
	nand_address_column(self, page, 0);

	nand_busy(self);

	bs = chip->nc_ecc->necc_block_size;
	cs = chip->nc_ecc->necc_code_size;
	ecc = chip->nc_oob_cache + chip->nc_ecc->necc_offset;

	/* XXX code duplication */
	/* decide if we access by 8 or 16 bits */
	if (chip->nc_flags & NC_BUSWIDTH_16) {
		for (b = 0, e = 0; b < chip->nc_page_size; b += bs, e += cs) {
			nand_ecc_prepare(self, NAND_ECC_WRITE);
			nand_write_buf_2(self, data + b, bs);
			nand_ecc_compute(self, data + b, ecc + e);
		}
		/* write oob with ecc correction code */
		nand_write_buf_2(self, chip->nc_oob_cache,
		    chip->nc_spare_size);
	} else {
		for (b = 0, e = 0; b < chip->nc_page_size; b += bs, e += cs) {
			nand_ecc_prepare(self, NAND_ECC_WRITE);
			nand_write_buf_1(self, data + b, bs);
			nand_ecc_compute(self, data + b, ecc + e);
		}
		/* write oob with ecc correction code */
		nand_write_buf_1(self, chip->nc_oob_cache,
		    chip->nc_spare_size);
	}

	nand_command(self, ONFI_PAGE_PROGRAM_START);

	nand_busy(self);

	/* for debugging ecc */
#if 0
	printf("dumping ecc %d\n--------------\n", chip->nc_ecc->necc_steps);
	for (e = 0; e < chip->nc_ecc->necc_steps; e++) {
		printf("0x");
		for (b = 0; b < cs; b++) {
			printf("%.2hhx", ecc[e+b]);
		}
		printf("\n");
	}
	printf("--------------\n");
#endif

	status = nand_get_status(self);
	KASSERT(status & ONFI_STATUS_RDY);
	if (status & ONFI_STATUS_FAIL) {
		aprint_error_dev(self, "page program failed!\n");
		return EIO;
	}

	return 0;
}

/* read the OOB of a page */
int
nand_read_oob(device_t self, size_t page, uint8_t *oob)
{
	struct nand_softc *sc = device_private(self);
	struct nand_chip *chip = &sc->sc_chip;

	nand_prepare_read(self, page, chip->nc_page_size);

	if (chip->nc_flags & NC_BUSWIDTH_16)
		nand_read_buf_2(self, oob, chip->nc_spare_size);
	else
		nand_read_buf_1(self, oob, chip->nc_spare_size);

	/* for debugging drivers */
#if 0
	nand_dump_data("oob", oob, chip->nc_spare_size);
#endif

	return 0;
}

static int
nand_write_oob(device_t self, size_t offset, const void *oob)
{
	struct nand_softc *sc = device_private(self);
	struct nand_chip *chip = &sc->sc_chip;
	uint8_t status;

	nand_command(self, ONFI_PAGE_PROGRAM);
	nand_address_column(self, offset, chip->nc_page_size);
	nand_command(self, ONFI_PAGE_PROGRAM_START);

	nand_busy(self);

	if (chip->nc_flags & NC_BUSWIDTH_16)
		nand_write_buf_2(self, oob, chip->nc_spare_size);
	else
		nand_write_buf_1(self, oob, chip->nc_spare_size);

	status = nand_get_status(self);
	KASSERT(status & ONFI_STATUS_RDY);
	if (status & ONFI_STATUS_FAIL)
		return EIO;
	else
		return 0;
}

void
nand_markbad(device_t self, size_t offset)
{
	struct nand_softc *sc = device_private(self);
	struct nand_chip *chip = &sc->sc_chip;
	flash_off_t blockoffset;
#ifdef NAND_BBT
	flash_off_t block;

	block = offset / chip->nc_block_size;

	nand_bbt_block_markbad(self, block);
#endif
	blockoffset = offset & chip->nc_block_mask;

	/* check if it is already marked bad */
	if (nand_isbad(self, blockoffset))
		return;

	nand_read_oob(self, blockoffset, chip->nc_oob_cache);

	chip->nc_oob_cache[chip->nc_badmarker_offs] = 0x00;
	chip->nc_oob_cache[chip->nc_badmarker_offs + 1] = 0x00;

	nand_write_oob(self, blockoffset, chip->nc_oob_cache);
}

bool
nand_isfactorybad(device_t self, flash_off_t offset)
{
	struct nand_softc *sc = device_private(self);
	struct nand_chip *chip = &sc->sc_chip;
	flash_off_t block, first_page, last_page, page;
	int i;

	/* Check for factory bad blocks first
	 * Factory bad blocks are marked in the first or last
	 * page of the blocks, see: ONFI 2.2, 3.2.2.
	 */
	block = offset / chip->nc_block_size;
	first_page = block * chip->nc_block_size;
	last_page = (block + 1) * chip->nc_block_size
	    - chip->nc_page_size;

	for (i = 0, page = first_page; i < 2; i++, page = last_page) {
		/* address OOB */
		nand_prepare_read(self, page, chip->nc_page_size);

		if (chip->nc_flags & NC_BUSWIDTH_16) {
			uint16_t word;
			nand_read_2(self, &word);
			if (word == 0x0000)
				return true;
		} else {
			uint8_t byte;
			nand_read_1(self, &byte);
			if (byte == 0x00)
				return true;
		}
	}

	return false;
}

bool
nand_iswornoutbad(device_t self, flash_off_t offset)
{
	struct nand_softc *sc = device_private(self);
	struct nand_chip *chip = &sc->sc_chip;
	flash_off_t block;

	/* we inspect the first page of the block */
	block = offset & chip->nc_block_mask;

	/* Linux/u-boot compatible badblock handling */
	if (chip->nc_flags & NC_BUSWIDTH_16) {
		uint16_t word, mark;

		nand_prepare_read(self, block,
		    chip->nc_page_size + (chip->nc_badmarker_offs & 0xfe));

		nand_read_2(self, &word);
		mark = htole16(word);
		if (chip->nc_badmarker_offs & 0x01)
			mark >>= 8;
		if ((mark & 0xff) != 0xff)
			return true;
	} else {
		uint8_t byte;

		nand_prepare_read(self, block,
		    chip->nc_page_size + chip->nc_badmarker_offs);

		nand_read_1(self, &byte);
		if (byte != 0xff)
			return true;
	}

	return false;
}

bool
nand_isbad(device_t self, flash_off_t offset)
{
#ifdef NAND_BBT
	struct nand_softc *sc = device_private(self);
	struct nand_chip *chip = &sc->sc_chip;
	flash_off_t block;

	block = offset / chip->nc_block_size;

	return nand_bbt_block_isbad(self, block);
#else
	/* ONFI host requirement */
	if (nand_isfactorybad(self, offset))
		return true;

	/* Look for Linux/U-Boot compatible bad marker */
	if (nand_iswornoutbad(self, offset))
		return true;

	return false;
#endif
}

int
nand_erase_block(device_t self, size_t offset)
{
	uint8_t status;

	/* xxx calculate first page of block for address? */

	nand_command(self, ONFI_BLOCK_ERASE);
	nand_address_row(self, offset);
	nand_command(self, ONFI_BLOCK_ERASE_START);

	nand_busy(self);

	status = nand_get_status(self);
	KASSERT(status & ONFI_STATUS_RDY);
	if (status & ONFI_STATUS_FAIL) {
		aprint_error_dev(self, "block erase failed!\n");
		nand_markbad(self, offset);
		return EIO;
	} else {
		return 0;
	}
}

/* default functions for driver development */

/* default ECC using hamming code of 256 byte chunks */
int
nand_default_ecc_compute(device_t self, const uint8_t *data, uint8_t *code)
{
	hamming_compute_256(data, code);

	return 0;
}

int
nand_default_ecc_correct(device_t self, uint8_t *data, const uint8_t *origcode,
	const uint8_t *compcode)
{
	return hamming_correct_256(data, origcode, compcode);
}

void
nand_default_select(device_t self, bool enable)
{
	/* do nothing */
	return;
}

/* implementation of the block device API */

int
nand_flash_submit(device_t self, struct buf * const bp)
{
	struct nand_softc *sc = device_private(self);

	return flash_io_submit(&sc->sc_flash_io, bp);
}

/*
 * handle (page) unaligned write to nand
 */
static int
nand_flash_write_unaligned(device_t self, flash_off_t offset, size_t len,
    size_t *retlen, const uint8_t *buf)
{
	struct nand_softc *sc = device_private(self);
	struct nand_chip *chip = &sc->sc_chip;
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
		if (nand_isbad(self, addr)) {
			aprint_error_dev(self,
			    "nand_flash_write_unaligned: "
			    "bad block encountered\n");
			error = EIO;
			goto out;
		}

		error = nand_read_page(self, addr, chip->nc_page_cache);
		if (error) {
			goto out;
		}

		memcpy(chip->nc_page_cache + firstoff, buf, len);

		error = nand_program_page(self, addr, chip->nc_page_cache);
		if (error) {
			goto out;
		}

		*retlen = len;
		goto out;
	}

	bufp = buf;
	left = len;

	for (i = 0; i < count && left != 0; i++) {
		if (nand_isbad(self, addr)) {
			aprint_error_dev(self,
			    "nand_flash_write_unaligned: "
			    "bad block encountered\n");
			error = EIO;
			goto out;
		}

		if (i == 0) {
			error = nand_read_page(self,
			    addr, chip->nc_page_cache);
			if (error) {
				goto out;
			}

			memcpy(chip->nc_page_cache + firstoff,
			    bufp, chip->nc_page_size - firstoff);

			printf("program page: %s: %d\n", __FILE__, __LINE__);
			error = nand_program_page(self,
			    addr, chip->nc_page_cache);
			if (error) {
				goto out;
			}

			bufp += chip->nc_page_size - firstoff;
			left -= chip->nc_page_size - firstoff;
			*retlen += chip->nc_page_size - firstoff;

		} else if (i == count - 1) {
			error = nand_read_page(self,
			    addr, chip->nc_page_cache);
			if (error) {
				goto out;
			}

			memcpy(chip->nc_page_cache, bufp, left);

			error = nand_program_page(self,
			    addr, chip->nc_page_cache);
			if (error) {
				goto out;
			}

			*retlen += left;
			KASSERT(left < chip->nc_page_size);

		} else {
			/* XXX debug */
			if (left > chip->nc_page_size) {
				printf("left: %zu, i: %d, count: %zu\n",
				    left, i, count);
			}
			KASSERT(left > chip->nc_page_size);

			error = nand_program_page(self, addr, bufp);
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

int
nand_flash_write(device_t self, flash_off_t offset, size_t len, size_t *retlen,
    const uint8_t *buf)
{
	struct nand_softc *sc = device_private(self);
	struct nand_chip *chip = &sc->sc_chip;
	const uint8_t *bufp;
	size_t pages, page;
	daddr_t addr;
	int error = 0;

	if ((offset + len) > chip->nc_size) {
		DPRINTF(("nand_flash_write: write (off: 0x%jx, len: %ju),"
			" is over device size (0x%jx)\n",
			(uintmax_t)offset, (uintmax_t)len,
			(uintmax_t)chip->nc_size));
		return EINVAL;
	}

	if (len % chip->nc_page_size != 0 ||
	    offset % chip->nc_page_size != 0) {
		return nand_flash_write_unaligned(self,
		    offset, len, retlen, buf);
	}

	pages = len / chip->nc_page_size;
	KASSERT(pages != 0);
	*retlen = 0;

	addr = offset;
	bufp = buf;

	mutex_enter(&sc->sc_device_lock);
	for (page = 0; page < pages; page++) {
		/* do we need this check here? */
		if (nand_isbad(self, addr)) {
			aprint_error_dev(self,
			    "nand_flash_write: bad block encountered\n");

			error = EIO;
			goto out;
		}

		error = nand_program_page(self, addr, bufp);
		if (error) {
			goto out;
		}

		addr += chip->nc_page_size;
		bufp += chip->nc_page_size;
		*retlen += chip->nc_page_size;
	}
out:
	mutex_exit(&sc->sc_device_lock);
	DPRINTF(("page programming: retlen: %" PRIu32 ", len: %" PRIu32 "\n", *retlen, len));

	return error;
}

/*
 * handle (page) unaligned read from nand
 */
static int
nand_flash_read_unaligned(device_t self, size_t offset,
    size_t len, size_t *retlen, uint8_t *buf)
{
	struct nand_softc *sc = device_private(self);
	struct nand_chip *chip = &sc->sc_chip;
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
		error = nand_read_page(self, addr, chip->nc_page_cache);
		if (error) {
			goto out;
		}

		memcpy(bufp, chip->nc_page_cache + firstoff, len);

		*retlen = len;
		goto out;
	}

	for (i = 0; i < count && left != 0; i++) {
		error = nand_read_page(self, addr, chip->nc_page_cache);
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

int
nand_flash_read(device_t self, flash_off_t offset, size_t len, size_t *retlen,
    uint8_t *buf)
{
	struct nand_softc *sc = device_private(self);
	struct nand_chip *chip = &sc->sc_chip;
	uint8_t *bufp;
	size_t addr;
	size_t i, pages;
	int error = 0;

	*retlen = 0;

	DPRINTF(("nand_flash_read: off: 0x%jx, len: %" PRIu32 "\n",
		(uintmax_t)offset, len));

	if (__predict_false((offset + len) > chip->nc_size)) {
		DPRINTF(("nand_flash_read: read (off: 0x%jx, len: %" PRIu32 "),"
			" is over device size (%ju)\n", (uintmax_t)offset,
			len, (uintmax_t)chip->nc_size));
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
		return nand_flash_read_unaligned(self,
		    offset, len, retlen, buf);
	}

	bufp = buf;
	addr = offset;
	pages = len / chip->nc_page_size;

	mutex_enter(&sc->sc_device_lock);
	for (i = 0; i < pages; i++) {
		/* XXX do we need this check here? */
		if (nand_isbad(self, addr)) {
			aprint_error_dev(self, "bad block encountered\n");
			error = EIO;
			goto out;
		}
		error = nand_read_page(self, addr, bufp);
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

int
nand_flash_isbad(device_t self, flash_off_t ofs, bool *is_bad)
{
	struct nand_softc *sc = device_private(self);
	struct nand_chip *chip = &sc->sc_chip;
	bool result;

	if (ofs > chip->nc_size) {
		DPRINTF(("nand_flash_isbad: offset 0x%jx is larger than"
			" device size (0x%jx)\n", (uintmax_t)ofs,
			(uintmax_t)chip->nc_size));
		return EINVAL;
	}

	if (ofs % chip->nc_block_size != 0) {
		DPRINTF(("offset (0x%jx) is not a multiple of block size "
			"(%ju)",
			(uintmax_t)ofs, (uintmax_t)chip->nc_block_size));
		return EINVAL;
	}

	mutex_enter(&sc->sc_device_lock);
	result = nand_isbad(self, ofs);
	mutex_exit(&sc->sc_device_lock);

	*is_bad = result;

	return 0;
}

int
nand_flash_markbad(device_t self, flash_off_t ofs)
{
	struct nand_softc *sc = device_private(self);
	struct nand_chip *chip = &sc->sc_chip;

	if (ofs > chip->nc_size) {
		DPRINTF(("nand_flash_markbad: offset 0x%jx is larger than"
			" device size (0x%jx)\n", ofs,
			(uintmax_t)chip->nc_size));
		return EINVAL;
	}

	if (ofs % chip->nc_block_size != 0) {
		panic("offset (%ju) is not a multiple of block size (%ju)",
		    (uintmax_t)ofs, (uintmax_t)chip->nc_block_size);
	}

	mutex_enter(&sc->sc_device_lock);
	nand_markbad(self, ofs);
	mutex_exit(&sc->sc_device_lock);

	return 0;
}

int
nand_flash_erase(device_t self,
    struct flash_erase_instruction *ei)
{
	struct nand_softc *sc = device_private(self);
	struct nand_chip *chip = &sc->sc_chip;
	flash_off_t addr;
	int error = 0;

	if (ei->ei_addr < 0 || ei->ei_len < chip->nc_block_size)
		return EINVAL;

	if (ei->ei_addr + ei->ei_len > chip->nc_size) {
		DPRINTF(("nand_flash_erase: erase address is over the end"
			" of the device\n"));
		return EINVAL;
	}

	if (ei->ei_addr % chip->nc_block_size != 0) {
		aprint_error_dev(self,
		    "nand_flash_erase: ei_addr (%ju) is not"
		    " a multiple of block size (%ju)",
		    (uintmax_t)ei->ei_addr,
		    (uintmax_t)chip->nc_block_size);
		return EINVAL;
	}

	if (ei->ei_len % chip->nc_block_size != 0) {
		aprint_error_dev(self,
		    "nand_flash_erase: ei_len (%ju) is not"
		    " a multiple of block size (%ju)",
		    (uintmax_t)ei->ei_len,
		    (uintmax_t)chip->nc_block_size);
		return EINVAL;
	}

	mutex_enter(&sc->sc_device_lock);
	addr = ei->ei_addr;
	while (addr < ei->ei_addr + ei->ei_len) {
		if (nand_isbad(self, addr)) {
			aprint_error_dev(self, "bad block encountered\n");
			ei->ei_state = FLASH_ERASE_FAILED;
			error = EIO;
			goto out;
		}

		error = nand_erase_block(self, addr);
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

MODULE(MODULE_CLASS_DRIVER, nand, "flash");

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
nand_modcmd(modcmd_t cmd, void *opaque)
{
	switch (cmd) {
	case MODULE_CMD_INIT:
#ifdef _MODULE
		return config_init_component(cfdriver_ioconf_nand,
		    cfattach_ioconf_nand, cfdata_ioconf_nand);
#else
		return 0;
#endif
	case MODULE_CMD_FINI:
#ifdef _MODULE
		return config_fini_component(cfdriver_ioconf_nand,
		    cfattach_ioconf_nand, cfdata_ioconf_nand);
#else
		return 0;
#endif
	default:
		return ENOTTY;
	}
}
