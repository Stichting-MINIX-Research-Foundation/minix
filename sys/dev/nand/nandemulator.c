/*	$NetBSD: nandemulator.c,v 1.7 2015/08/20 14:40:18 christos Exp $	*/

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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: nandemulator.c,v 1.7 2015/08/20 14:40:18 christos Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/kmem.h>
#include <sys/kernel.h>

#include "nandemulator.h"

#include <dev/nand/nand.h>
#include <dev/nand/onfi.h>
#include <dev/nand/nand_crc.h>

#include "ioconf.h"

extern struct cfdriver nandemulator_cd;

static int nandemulator_match(device_t, cfdata_t, void *);
static void nandemulator_attach(device_t, device_t, void *);
static int nandemulator_detach(device_t, int);

static void nandemulator_device_reset(device_t);
static void nandemulator_command(device_t, uint8_t);
static void nandemulator_address(device_t, uint8_t);
static void nandemulator_busy(device_t);
static void nandemulator_read_1(device_t, uint8_t *);
static void nandemulator_write_1(device_t, uint8_t);
static void nandemulator_read_2(device_t, uint16_t *);
static void nandemulator_write_2(device_t, uint16_t);
static void nandemulator_read_buf_1(device_t, void *, size_t);
static void nandemulator_read_buf_2(device_t, void *, size_t);
static void nandemulator_write_buf_1(device_t, const void *, size_t);
static void nandemulator_write_buf_2(device_t, const void *, size_t);

static size_t nandemulator_address_to_page(device_t);
static size_t nandemulator_page_to_backend_offset(device_t, size_t);
static size_t nandemulator_column_address_to_subpage(device_t);
/*
#define NANDEMULATOR_DEBUG 1

#ifdef NANDEMULATOR_DEBUG
#warning debug enabled
#define DPRINTF(x)	if (nandemulatordebug) printf x
#define DPRINTFN(n,x)	if (nandemulatordebug>(n)) printf x
#else
#error no debug
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#ifdef NANDEMULATOR_DEBUG
int	nandemulatordebug = NANDEMULATOR_DEBUG;
#endif
*/

extern int nanddebug;

enum {
	NANDEMULATOR_8BIT,
	NANDEMULATOR_16BIT
};

struct nandemulator_softc {
	device_t		sc_dev;
	device_t		sc_nanddev;

	int			sc_buswidth;

	struct nand_interface	sc_nand_if;

	uint8_t			sc_command;
	size_t			sc_io_len;
	uint8_t			*sc_io_pointer;
	uint64_t		sc_address;

	uint8_t			*sc_backend;
	size_t			sc_backend_size;
	size_t			sc_device_size;
	bool			sc_register_writable;

	uint8_t			sc_status_register;
	uint8_t			sc_ids[2];
	uint8_t			sc_onfi[4];

	size_t			sc_page_size;
	size_t			sc_block_size;
	size_t			sc_spare_size;
	size_t			sc_lun_size;
	uint8_t			sc_row_cycles;
	uint8_t			sc_column_cycles;
	uint64_t		sc_row_mask;

	int			sc_address_counter;

	struct onfi_parameter_page	*sc_parameter_page;
};

CFATTACH_DECL_NEW(nandemulator, sizeof(struct nandemulator_softc),
    nandemulator_match, nandemulator_attach, nandemulator_detach, NULL);

void
nandemulatorattach(int n)
{
	int i, err;
	cfdata_t cf;

	aprint_debug("nandemulator: requested %d units\n", n);

	err = config_cfattach_attach(nandemulator_cd.cd_name,
	    &nandemulator_ca);
	if (err) {
		aprint_error("%s: couldn't register cfattach: %d\n",
		    nandemulator_cd.cd_name, err);
		config_cfdriver_detach(&nandemulator_cd);
		return;
	}
	for (i = 0; i < n; i++) {
		cf = kmem_alloc(sizeof(struct cfdata), KM_NOSLEEP);
		if (cf == NULL) {
			aprint_error("%s: couldn't allocate cfdata\n",
			    nandemulator_cd.cd_name);
			continue;
		}
		cf->cf_name = nandemulator_cd.cd_name;
		cf->cf_atname = nandemulator_cd.cd_name;
		cf->cf_unit = i;
		cf->cf_fstate = FSTATE_STAR;

		(void)config_attach_pseudo(cf);
	}
}

/* ARGSUSED */
static int
nandemulator_match(device_t parent, cfdata_t match, void *aux)
{
	/* pseudo device, always attaches */
	return 1;
}

static void
nandemulator_attach(device_t parent, device_t self, void *aux)
{
	struct nandemulator_softc *sc = device_private(self);
	int i;

	aprint_normal_dev(self, "NAND emulator\n");

	sc->sc_dev = self;

	nand_init_interface(&sc->sc_nand_if);

	sc->sc_nand_if.command = &nandemulator_command;
	sc->sc_nand_if.address = &nandemulator_address;
	sc->sc_nand_if.read_buf_1 = &nandemulator_read_buf_1;
	sc->sc_nand_if.read_buf_2 = &nandemulator_read_buf_2;
	sc->sc_nand_if.read_1 = &nandemulator_read_1;
	sc->sc_nand_if.read_2 = &nandemulator_read_2;
	sc->sc_nand_if.write_buf_1 = &nandemulator_write_buf_1;
	sc->sc_nand_if.write_buf_2 = &nandemulator_write_buf_2;
	sc->sc_nand_if.write_1 = &nandemulator_write_1;
	sc->sc_nand_if.write_2 = &nandemulator_write_2;
	sc->sc_nand_if.busy = &nandemulator_busy;

	sc->sc_nand_if.ecc.necc_code_size = 3;
	sc->sc_nand_if.ecc.necc_block_size = 256;

	if (!pmf_device_register1(sc->sc_dev, NULL, NULL, NULL))
		aprint_error_dev(sc->sc_dev,
		    "couldn't establish power handler\n");

	sc->sc_buswidth = NANDEMULATOR_16BIT;	/* 16bit for now */

	/* hardcode these now, make it configurable later */
	sc->sc_device_size = 32 * 1024 * 1024; /* 32MB */
	sc->sc_page_size = 2048;
	sc->sc_block_size = 64;
	sc->sc_lun_size =
	    sc->sc_device_size / (sc->sc_page_size * sc->sc_block_size);
	KASSERT(sc->sc_device_size %
	    (sc->sc_page_size * sc->sc_block_size) == 0);
	sc->sc_spare_size = 64;

	sc->sc_column_cycles = 2;
	sc->sc_row_cycles = 3;

	/* init the emulator data structures */
	sc->sc_backend_size =
	    sc->sc_device_size +
	    sc->sc_device_size / sc->sc_page_size * sc->sc_spare_size;

	sc->sc_backend = kmem_alloc(sc->sc_backend_size, KM_SLEEP);
	memset(sc->sc_backend, 0xff, sc->sc_backend_size);

	sc->sc_parameter_page =
	    kmem_zalloc(sizeof(struct onfi_parameter_page) * 4, KM_SLEEP);

	struct onfi_parameter_page *opp;
	uint8_t sig[4] = { 'O', 'N', 'F', 'I' };

	for (i = 0; i < 4; i++) {
		opp = &sc->sc_parameter_page[i];

		opp->param_signature = htole32(*(uint32_t *)sig);
		opp->param_pagesize = htole32(sc->sc_page_size);
		opp->param_blocksize = htole32(sc->sc_block_size);
		opp->param_sparesize = htole16(sc->sc_spare_size);
		opp->param_lunsize = htole32(sc->sc_lun_size);
		opp->param_numluns = 1;

		opp->param_manufacturer_id = 0x00;
		memcpy(opp->param_manufacturer,
		    "NETBSD", strlen("NETBSD"));
		memcpy(opp->param_model,
		    "NANDEMULATOR", strlen("NANDEMULATOR"));

		uint16_t features = ONFI_FEATURE_16BIT;
		opp->param_features = htole16(features);

		/* the lower 4 bits contain the row address cycles
		 * the upper 4 bits contain the column address cycles
		 */
		opp->param_addr_cycles = sc->sc_row_cycles;
		opp->param_addr_cycles |= (sc->sc_column_cycles << 4);

		opp->param_integrity_crc = nand_crc16((uint8_t *)opp, 254);
	}

	sc->sc_ids[0] = 0x00;
	sc->sc_ids[1] = 0x00;

	sc->sc_onfi[0] = 'O';
	sc->sc_onfi[1] = 'N';
	sc->sc_onfi[2] = 'F';
	sc->sc_onfi[3] = 'I';

	sc->sc_row_mask = 0x00;
	for (i = 0; i < sc->sc_row_cycles; i++) {
		sc->sc_row_mask <<= 8;
		sc->sc_row_mask |= 0xff;
	}

	nandemulator_device_reset(self);

	sc->sc_nanddev = nand_attach_mi(&sc->sc_nand_if, sc->sc_dev);
}

static int
nandemulator_detach(device_t self, int flags)
{
	struct nandemulator_softc *sc = device_private(self);
	int ret = 0;

	aprint_normal_dev(sc->sc_dev, "detaching emulator\n");

	pmf_device_deregister(sc->sc_dev);

	if (sc->sc_nanddev != NULL)
		ret = config_detach(sc->sc_nanddev, flags);

	kmem_free(sc->sc_backend, sc->sc_backend_size);
	kmem_free(sc->sc_parameter_page,
	    sizeof(struct onfi_parameter_page) * 4);

	return ret;
}

/**
 * bring the emulated device to a known state
 */
static void
nandemulator_device_reset(device_t self)
{
	struct nandemulator_softc *sc = device_private(self);

	DPRINTF(("device reset\n"));

	sc->sc_command = 0;
	sc->sc_register_writable = false;
	sc->sc_io_len = 0;
	sc->sc_io_pointer = NULL;
	sc->sc_address = 0;
	sc->sc_address_counter = 0;

	sc->sc_status_register = ONFI_STATUS_RDY | ONFI_STATUS_WP;
}

static void
nandemulator_address_chip(device_t self)
{
	struct nandemulator_softc *sc = device_private(self);
	size_t page, offset;

	KASSERT(sc->sc_address_counter ==
	    sc->sc_column_cycles + sc->sc_row_cycles);

	if (sc->sc_address_counter !=
	    sc->sc_column_cycles + sc->sc_row_cycles) {
		aprint_error_dev(self, "incorrect number of address cycles\n");
		aprint_error_dev(self, "cc: %d, rc: %d, ac: %d\n",
		    sc->sc_column_cycles, sc->sc_row_cycles,
		    sc->sc_address_counter);
	}

	page = nandemulator_address_to_page(self);
	offset = sc->sc_page_size * page;

	DPRINTF(("READ/PROGRAM; page: 0x%jx (row addr: 0x%jx)\n",
		(uintmax_t )page,
		(uintmax_t )offset));

	KASSERT(offset < sc->sc_device_size);

	if (offset >= sc->sc_device_size) {
		aprint_error_dev(self, "address > device size!\n");
		sc->sc_io_len = 0;
	} else {
		size_t addr =
		    nandemulator_page_to_backend_offset(self, page);
		size_t pageoff =
		    nandemulator_column_address_to_subpage(self);

		DPRINTF(("subpage: 0x%jx\n", (uintmax_t )pageoff));

		KASSERT(pageoff <
		    sc->sc_page_size + sc->sc_spare_size);
		KASSERT(addr < sc->sc_backend_size);

		sc->sc_io_pointer = sc->sc_backend + addr + pageoff;
		sc->sc_io_len =
		    sc->sc_page_size + sc->sc_spare_size - pageoff;
	}
}

static void
nandemulator_command(device_t self, uint8_t command)
{
	struct nandemulator_softc *sc = device_private(self);
	size_t offset, page;

	sc->sc_command = command;
	sc->sc_register_writable = false;

	DPRINTF(("nandemulator command: 0x%hhx\n", command));

	switch (command) {
	case ONFI_READ_STATUS:
		sc->sc_io_pointer = &sc->sc_status_register;
		sc->sc_io_len = 1;
		break;
	case ONFI_RESET:
		nandemulator_device_reset(self);
		break;
	case ONFI_PAGE_PROGRAM:
		sc->sc_register_writable = true;
	case ONFI_READ:
	case ONFI_BLOCK_ERASE:
		sc->sc_address_counter = 0;
	case ONFI_READ_ID:
	case ONFI_READ_PARAMETER_PAGE:
		sc->sc_io_len = 0;
		sc->sc_address = 0;
		break;
	case ONFI_PAGE_PROGRAM_START:
		/* XXX the program should only happen here */
		break;
	case ONFI_READ_START:
		nandemulator_address_chip(self);
		break;
	case ONFI_BLOCK_ERASE_START:
		page = nandemulator_address_to_page(self);
		offset = sc->sc_page_size * page;

		KASSERT(offset %
		    (sc->sc_block_size * sc->sc_page_size) == 0);

		KASSERT(offset < sc->sc_device_size);

		if (offset >= sc->sc_device_size) {
			aprint_error_dev(self, "address > device size!\n");
		} else {
			size_t addr =
			    nandemulator_page_to_backend_offset(self, page);

			size_t blocklen =
			    sc->sc_block_size *
			    (sc->sc_page_size + sc->sc_spare_size);

			KASSERT(addr < sc->sc_backend_size);
			uint8_t *block = sc->sc_backend + addr;

			DPRINTF(("erasing block at 0x%jx\n",
				(uintmax_t )offset));

			memset(block, 0xff, blocklen);
		}
		sc->sc_io_len = 0;
		break;
	default:
		aprint_error_dev(self,
		    "invalid nand command (0x%hhx)\n", command);
		KASSERT(false);
		sc->sc_io_len = 0;
	}
};

static void
nandemulator_address(device_t self, uint8_t address)
{
	struct nandemulator_softc *sc = device_private(self);

	DPRINTF(("nandemulator_address: %hhx\n", address));

	/**
	 * we have to handle read id/parameter page here,
	 * as we can read right after giving the address.
	 */
	switch (sc->sc_command) {
	case ONFI_READ_ID:
		if (address == 0x00) {
			sc->sc_io_len = 2;
			sc->sc_io_pointer = sc->sc_ids;
		} else if (address == 0x20) {
			sc->sc_io_len = 4;
			sc->sc_io_pointer = sc->sc_onfi;
		} else {
			sc->sc_io_len = 0;
		}
		break;
	case ONFI_READ_PARAMETER_PAGE:
		if (address == 0x00) {
			sc->sc_io_len = sizeof(struct onfi_parameter_page) * 4;
			sc->sc_io_pointer = (uint8_t *)sc->sc_parameter_page;
		} else {
			sc->sc_io_len = 0;
		}
		break;
	case ONFI_PAGE_PROGRAM:
		sc->sc_address <<= 8;
		sc->sc_address |= address;
		sc->sc_address_counter++;

		if (sc->sc_address_counter ==
		    sc->sc_column_cycles + sc->sc_row_cycles) {
			nandemulator_address_chip(self);
		}
		break;
	default:
		sc->sc_address <<= 8;
		sc->sc_address |= address;
		sc->sc_address_counter++;
	}
};

static void
nandemulator_busy(device_t self)
{
#ifdef NANDEMULATOR_DELAYS
	struct nandemulator_softc *sc = device_private(self);

	/* do some delay depending on command */
	switch (sc->sc_command) {
	case ONFI_PAGE_PROGRAM_START:
	case ONFI_BLOCK_ERASE_START:
		DELAY(10);
		break;
	case ONFI_READ_START:
	default:
		DELAY(1);
	}
#endif
}

static void
nandemulator_read_1(device_t self, uint8_t *data)
{
	struct nandemulator_softc *sc = device_private(self);

	KASSERT(sc->sc_io_len > 0);

	if (sc->sc_io_len > 0) {
		*data = *sc->sc_io_pointer;

		sc->sc_io_pointer++;
		sc->sc_io_len--;
	} else {
		aprint_error_dev(self, "reading byte from invalid location\n");
		*data = 0xff;
	}
}

static void
nandemulator_write_1(device_t self, uint8_t data)
{
	struct nandemulator_softc *sc = device_private(self);

	KASSERT(sc->sc_register_writable);

	if (!sc->sc_register_writable) {
		aprint_error_dev(self,
		    "trying to write read only location without effect\n");
		return;
	}

	KASSERT(sc->sc_io_len > 0);

	if (sc->sc_io_len > 0) {
		*sc->sc_io_pointer = data;

		sc->sc_io_pointer++;
		sc->sc_io_len--;
	} else {
		aprint_error_dev(self, "write to invalid location\n");
	}
}

static void
nandemulator_read_2(device_t self, uint16_t *data)
{
	struct nandemulator_softc *sc = device_private(self);

	KASSERT(sc->sc_buswidth == NANDEMULATOR_16BIT);

	if (sc->sc_buswidth != NANDEMULATOR_16BIT) {
		aprint_error_dev(self,
		    "trying to read a word on an 8bit chip\n");
		return;
	}

	KASSERT(sc->sc_io_len > 1);

	if (sc->sc_io_len > 1) {
		*data = *(uint16_t *)sc->sc_io_pointer;

		sc->sc_io_pointer += 2;
		sc->sc_io_len -= 2;
	} else {
		aprint_error_dev(self, "reading word from invalid location\n");
		*data = 0xffff;
	}
}

static void
nandemulator_write_2(device_t self, uint16_t data)
{
	struct nandemulator_softc *sc = device_private(self);

	KASSERT(sc->sc_register_writable);

	if (!sc->sc_register_writable) {
		aprint_error_dev(self,
		    "trying to write read only location without effect\n");
		return;
	}

	KASSERT(sc->sc_buswidth == NANDEMULATOR_16BIT);

	if (sc->sc_buswidth != NANDEMULATOR_16BIT) {
		aprint_error_dev(self,
		    "trying to write a word to an 8bit chip");
		return;
	}

	KASSERT(sc->sc_io_len > 1);

	if (sc->sc_io_len > 1) {
		*(uint16_t *)sc->sc_io_pointer = data;

		sc->sc_io_pointer += 2;
		sc->sc_io_len -= 2;
	} else {
		aprint_error_dev(self, "writing to invalid location");
	}
}

static void
nandemulator_read_buf_1(device_t self, void *buf, size_t len)
{
	uint8_t *addr;

	KASSERT(buf != NULL);
	KASSERT(len >= 1);

	addr = buf;
	while (len > 0) {
		nandemulator_read_1(self, addr);
		addr++, len--;
	}
}

static void
nandemulator_read_buf_2(device_t self, void *buf, size_t len)
{
	uint16_t *addr;

	KASSERT(buf != NULL);
	KASSERT(len >= 2);
	KASSERT(!(len & 0x01));

	addr = buf;
	len /= 2;
	while (len > 0) {
		nandemulator_read_2(self, addr);
		addr++, len--;
	}
}

static void
nandemulator_write_buf_1(device_t self, const void *buf, size_t len)
{
	const uint8_t *addr;

	KASSERT(buf != NULL);
	KASSERT(len >= 1);

	addr = buf;
	while (len > 0) {
		nandemulator_write_1(self, *addr);
		addr++, len--;
	}
}

static void
nandemulator_write_buf_2(device_t self, const void *buf, size_t len)
{
	const uint16_t *addr;

	KASSERT(buf != NULL);
	KASSERT(len >= 2);
	KASSERT(!(len & 0x01));

	addr = buf;
	len /= 2;
	while (len > 0) {
		nandemulator_write_2(self, *addr);
		addr++, len--;
	}
}

static size_t
nandemulator_address_to_page(device_t self)
{
	struct nandemulator_softc *sc = device_private(self);
	uint64_t address, offset;
	int i;

	address = htole64(sc->sc_address);
	address &= sc->sc_row_mask;

	offset = 0;
	for (i = 0; i < sc->sc_row_cycles; i++) {
		offset <<= 8;
		offset |= (address & 0xff);
		address >>= 8;
	}

	return le64toh(offset);
}

static size_t
nandemulator_column_address_to_subpage(device_t self)
{
	struct nandemulator_softc *sc = device_private(self);
	uint64_t address, offset;
	int i;

	address = htole64(sc->sc_address);
	address >>= (8 * sc->sc_row_cycles);

	offset = 0;
	for (i = 0; i < sc->sc_column_cycles; i++) {
		offset <<= 8;
		offset |= (address & 0xff);
		address >>= 8;
	}

	if (sc->sc_buswidth == NANDEMULATOR_16BIT)
		return (size_t )le64toh(offset << 1);
	else
		return (size_t )le64toh(offset);
}

static size_t
nandemulator_page_to_backend_offset(device_t self, size_t page)
{
	struct nandemulator_softc *sc = device_private(self);

	return (sc->sc_page_size + sc->sc_spare_size) * page;
}

#ifdef _MODULE

MODULE(MODULE_CLASS_DRIVER, nandemulator, "nand");

static const struct cfiattrdata nandbuscf_iattrdata = {
	"nandbus", 0, { { NULL, NULL, 0 }, }
};
static const struct cfiattrdata * const nandemulator_attrs[] = {
	&nandbuscf_iattrdata, NULL
};

CFDRIVER_DECL(nandemulator, DV_DULL, nandemulator_attrs);
extern struct cfattach nandemulator_ca;
static int nandemulatorloc[] = { -1, -1 };

static struct cfdata nandemulator_cfdata[] = {
	{
		.cf_name = "nandemulator",
		.cf_atname = "nandemulator",
		.cf_unit = 0,
		.cf_fstate = FSTATE_STAR,
		.cf_loc = nandemulatorloc,
		.cf_flags = 0,
		.cf_pspec = NULL,
	},
	{ NULL, NULL, 0, 0, NULL, 0, NULL }
};

static int
nandemulator_modcmd(modcmd_t cmd, void *arg)
{
	int error;

	switch (cmd) {
	case MODULE_CMD_INIT:
		error = config_cfdriver_attach(&nandemulator_cd);
		if (error) {
			return error;
		}

		error = config_cfattach_attach(nandemulator_cd.cd_name,
		    &nandemulator_ca);
		if (error) {
			config_cfdriver_detach(&nandemulator_cd);
			aprint_error("%s: unable to register cfattach\n",
				nandemulator_cd.cd_name);

			return error;
		}

		error = config_cfdata_attach(nandemulator_cfdata, 1);
		if (error) {
			config_cfattach_detach(nandemulator_cd.cd_name,
			    &nandemulator_ca);
			config_cfdriver_detach(&nandemulator_cd);
			aprint_error("%s: unable to register cfdata\n",
				nandemulator_cd.cd_name);

			return error;
		}

		(void)config_attach_pseudo(nandemulator_cfdata);

		return 0;

	case MODULE_CMD_FINI:
		error = config_cfdata_detach(nandemulator_cfdata);
		if (error) {
			return error;
		}

		config_cfattach_detach(nandemulator_cd.cd_name,
		    &nandemulator_ca);
		config_cfdriver_detach(&nandemulator_cd);

		return 0;

	case MODULE_CMD_AUTOUNLOAD:
		/* prevent auto-unload */
		return EBUSY;

	default:
		return ENOTTY;
	}
}

#endif
