/*	$NetBSD: cfi.c,v 1.8 2015/09/18 21:30:02 phx Exp $	*/
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
#include "opt_nor.h"
#include "opt_cfi.h"

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: cfi.c,v 1.8 2015/09/18 21:30:02 phx Exp $"); 

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/cdefs.h>
#include <sys/device.h>
#include <sys/endian.h>

#include <sys/bus.h>
        
#include <dev/nor/nor.h>
#include <dev/nor/cfi.h>
#include <dev/nor/cfi_0002.h>


static int  cfi_scan_media(device_t self, struct nor_chip *chip);
static void cfi_init(device_t);
static void cfi_select(device_t, bool);
static void cfi_read_1(device_t, flash_off_t, uint8_t *);
static void cfi_read_2(device_t, flash_off_t, uint16_t *);
static void cfi_read_4(device_t, flash_off_t, uint32_t *);
static void cfi_read_buf_1(device_t, flash_off_t, uint8_t *, size_t);
static void cfi_read_buf_2(device_t, flash_off_t, uint16_t *, size_t);
static void cfi_read_buf_4(device_t, flash_off_t, uint32_t *, size_t);
static void cfi_write_1(device_t, flash_off_t, uint8_t);
static void cfi_write_2(device_t, flash_off_t, uint16_t);
static void cfi_write_4(device_t, flash_off_t, uint32_t);
static void cfi_write_buf_1(device_t, flash_off_t, const uint8_t *, size_t);
static void cfi_write_buf_2(device_t, flash_off_t, const uint16_t *, size_t);
static void cfi_write_buf_4(device_t, flash_off_t, const uint32_t *, size_t);
static uint8_t cfi_read_qry(struct cfi * const, bus_size_t);
static bool cfi_jedec_id(struct cfi * const);
static bool cfi_emulate(struct cfi * const);
static const struct cfi_jedec_tab * cfi_jedec_search(struct cfi *);
static void cfi_jedec_fill(struct cfi * const,
	const struct cfi_jedec_tab *);
#if defined(CFI_DEBUG_JEDEC) || defined(CFI_DEBUG_QRY)
static void cfi_hexdump(flash_off_t, void * const, u_int, u_int);
#endif

#define LOG2_64K	16
#define LOG2_128K	17
#define LOG2_256K	18
#define LOG2_512K	19
#define LOG2_1M		20
#define LOG2_2M		21
#define LOG2_4M		22
#define LOG2_8M		23
#define LOG2_16M	24
#define LOG2_32M	25
#define LOG2_64M	26
#define LOG2_128M	27
#define LOG2_256M	28
#define LOG2_512M	29
#define LOG2_1G		30
#define LOG2_2G		31
const struct cfi_jedec_tab cfi_jedec_tab[] = {
	{
		.jt_name = "Pm39LV512",
		.jt_mid = 0x9d,
		.jt_did = 0x1b,
		.jt_id_pri = 0,				/* XXX */
		.jt_id_alt = 0,				/* XXX */
		.jt_device_size = LOG2_64K,
		.jt_interface_code_desc = CFI_IFCODE_X8,
		.jt_erase_blk_regions = 1,
		.jt_erase_blk_info = {
			{ 4096/256, (64/4)-1 },
		},
		.jt_write_word_time_typ = 40,
		.jt_write_nbyte_time_typ = 0,
		.jt_erase_blk_time_typ = 55,
		.jt_erase_chip_time_typ = 55,
		.jt_write_word_time_max = 1,
		.jt_write_nbyte_time_max = 0,
		.jt_erase_blk_time_max = 1,
		.jt_erase_chip_time_max = 1,
	},
	{
		.jt_name = "Pm39LV010",
		.jt_mid = 0x9d,
		.jt_did = 0x1c,
		.jt_id_pri = 0,				/* XXX */
		.jt_id_alt = 0,				/* XXX */
		.jt_device_size = LOG2_128K,
		.jt_interface_code_desc = CFI_IFCODE_X8,
		.jt_erase_blk_regions = 1,
		.jt_erase_blk_info = {
			{ 4096/256, (128/4)-1 },
		},
		.jt_write_word_time_typ = 40,
		.jt_write_nbyte_time_typ = 0,
		.jt_erase_blk_time_typ = 55,
		.jt_erase_chip_time_typ = 55,
		.jt_write_word_time_max = 1,
		.jt_write_nbyte_time_max = 0,
		.jt_erase_blk_time_max = 1,
		.jt_erase_chip_time_max = 1,
	},
};


const struct nor_interface nor_interface_cfi = {
	.scan_media = cfi_scan_media,
	.init = cfi_init,
	.select = cfi_select,
	.read_1 = cfi_read_1,
	.read_2 = cfi_read_2,
	.read_4 = cfi_read_4,
	.read_buf_1 = cfi_read_buf_1,
	.read_buf_2 = cfi_read_buf_2,
	.read_buf_4 = cfi_read_buf_4,
	.write_1 = cfi_write_1,
	.write_2 = cfi_write_2,
	.write_4 = cfi_write_4,
	.write_buf_1 = cfi_write_buf_1,
	.write_buf_2 = cfi_write_buf_2,
	.write_buf_4 = cfi_write_buf_4,
	.read_page = NULL,			/* cmdset */
	.program_page = NULL,			/* cmdset */
	.busy = NULL,
	.private = NULL,
	.access_width = -1,
	.part_info = NULL,
	.part_num = -1,
};


/* only data[7..0] are used regardless of chip width */
#define cfi_unpack_1(n)			((n) & 0xff)

/* construct uint16_t */
#define cfi_unpack_2(b0, b1)						\
	((cfi_unpack_1(b1) << 8) | cfi_unpack_1(b0))

/* construct uint32_t */
#define cfi_unpack_4(b0, b1, b2, b3)					\
	((cfi_unpack_1(b3) << 24) |					\
	 (cfi_unpack_1(b2) << 16) |					\
	 (cfi_unpack_1(b1) <<  8) |					\
	 (cfi_unpack_1(b0)))

#define cfi_unpack_qry(qryp, data)					\
    do {								\
	(qryp)->qry[0] = cfi_unpack_1(data[0x10]);			\
	(qryp)->qry[1] = cfi_unpack_1(data[0x11]);			\
	(qryp)->qry[2] = cfi_unpack_1(data[0x12]);			\
	(qryp)->id_pri = cfi_unpack_2(data[0x13], data[0x14]);		\
	(qryp)->addr_pri = cfi_unpack_2(data[0x15], data[0x16]);	\
	(qryp)->id_alt = cfi_unpack_2(data[0x17], data[0x18]);		\
	(qryp)->addr_alt = cfi_unpack_2(data[0x19], data[0x1a]);	\
	(qryp)->vcc_min = cfi_unpack_1(data[0x1b]);			\
	(qryp)->vcc_max = cfi_unpack_1(data[0x1c]);			\
	(qryp)->vpp_min = cfi_unpack_1(data[0x1d]);			\
	(qryp)->vpp_max = cfi_unpack_1(data[0x1e]);			\
	(qryp)->write_word_time_typ = cfi_unpack_1(data[0x1f]);		\
	(qryp)->write_nbyte_time_typ = cfi_unpack_1(data[0x20]);	\
	(qryp)->erase_blk_time_typ = cfi_unpack_1(data[0x21]);		\
	(qryp)->erase_chip_time_typ = cfi_unpack_1(data[0x22]);		\
	(qryp)->write_word_time_max = cfi_unpack_1(data[0x23]);		\
	(qryp)->write_nbyte_time_max = cfi_unpack_1(data[0x24]);	\
	(qryp)->erase_blk_time_max = cfi_unpack_1(data[0x25]);		\
	(qryp)->erase_chip_time_max = cfi_unpack_1(data[0x26]);		\
	(qryp)->device_size = cfi_unpack_1(data[0x27]);			\
	(qryp)->interface_code_desc =					\
		cfi_unpack_2(data[0x28], data[0x29]);			\
	(qryp)->write_nbyte_size_max = 					\
		cfi_unpack_2(data[0x2a], data[0x2b]);			\
	(qryp)->erase_blk_regions = cfi_unpack_1(data[0x2c]);		\
	u_int _i = 0x2d;						\
	const u_int _n = (qryp)->erase_blk_regions;			\
	KASSERT(_n <= 4);						\
	for (u_int _r = 0; _r < _n; _r++, _i+=4) {			\
		(qryp)->erase_blk_info[_r].y =				\
			cfi_unpack_2(data[_i+0], data[_i+1]);		\
		(qryp)->erase_blk_info[_r].z =				\
			cfi_unpack_2(data[_i+2], data[_i+3]);		\
	}								\
    } while (0)

#define cfi_unpack_pri_0002(qryp, data)					\
    do {								\
	(qryp)->pri.cmd_0002.pri[0] = cfi_unpack_1(data[0x00]);		\
	(qryp)->pri.cmd_0002.pri[1] = cfi_unpack_1(data[0x01]);		\
	(qryp)->pri.cmd_0002.pri[2] = cfi_unpack_1(data[0x02]);		\
	(qryp)->pri.cmd_0002.version_maj = cfi_unpack_1(data[0x03]);	\
	(qryp)->pri.cmd_0002.version_min = cfi_unpack_1(data[0x04]);	\
	(qryp)->pri.cmd_0002.asupt = cfi_unpack_1(data[0x05]);		\
	(qryp)->pri.cmd_0002.erase_susp = cfi_unpack_1(data[0x06]);	\
	(qryp)->pri.cmd_0002.sector_prot = cfi_unpack_1(data[0x07]);	\
	(qryp)->pri.cmd_0002.tmp_sector_unprot =			\
		cfi_unpack_1(data[0x08]);				\
	(qryp)->pri.cmd_0002.sector_prot_scheme =			\
		cfi_unpack_1(data[0x09]);				\
	(qryp)->pri.cmd_0002.simul_op = cfi_unpack_1(data[0x0a]);	\
	(qryp)->pri.cmd_0002.burst_mode_type = cfi_unpack_1(data[0x0b]);\
	(qryp)->pri.cmd_0002.page_mode_type = cfi_unpack_1(data[0x0c]);	\
	(qryp)->pri.cmd_0002.acc_min = cfi_unpack_1(data[0x0d]);	\
	(qryp)->pri.cmd_0002.acc_max = cfi_unpack_1(data[0x0e]);	\
	(qryp)->pri.cmd_0002.wp_prot = cfi_unpack_1(data[0x0f]);	\
	/* XXX 1.3 stops here */					\
	(qryp)->pri.cmd_0002.prog_susp = cfi_unpack_1(data[0x10]);	\
	(qryp)->pri.cmd_0002.unlock_bypass = cfi_unpack_1(data[0x11]);	\
	(qryp)->pri.cmd_0002.sss_size = cfi_unpack_1(data[0x12]);	\
	(qryp)->pri.cmd_0002.soft_feat = cfi_unpack_1(data[0x13]);	\
	(qryp)->pri.cmd_0002.page_size = cfi_unpack_1(data[0x14]);	\
	(qryp)->pri.cmd_0002.erase_susp_time_max =			\
		cfi_unpack_1(data[0x15]);				\
	(qryp)->pri.cmd_0002.prog_susp_time_max =			\
		cfi_unpack_1(data[0x16]);				\
	(qryp)->pri.cmd_0002.embhwrst_time_max =			\
		cfi_unpack_1(data[0x38]);				\
	(qryp)->pri.cmd_0002.hwrst_time_max =				\
		cfi_unpack_1(data[0x39]);				\
    } while (0)

#define CFI_QRY_UNPACK_COMMON(cfi, data, type)				\
    do {								\
	struct cfi_query_data * const qryp = &cfi->cfi_qry_data;	\
									\
	memset(qryp, 0, sizeof(*qryp));					\
	cfi_unpack_qry(qryp, data);					\
									\
	switch (qryp->id_pri) {						\
	case 0x0002:							\
		if ((cfi_unpack_1(data[qryp->addr_pri + 0]) == 'P') &&	\
		    (cfi_unpack_1(data[qryp->addr_pri + 1]) == 'R') &&	\
		    (cfi_unpack_1(data[qryp->addr_pri + 2]) == 'I')) {	\
			type *pri_data = &data[qryp->addr_pri];		\
			cfi_unpack_pri_0002(qryp, pri_data);		\
			break;						\
		}							\
	}								\
    } while (0)

#ifdef CFI_DEBUG_QRY
# define CFI_DUMP_QRY(off, p, sz, stride)				\
    do {								\
	printf("%s: QRY data\n", __func__);				\
	cfi_hexdump(off, p, sz, stride);				\
    } while (0)
#else
# define CFI_DUMP_QRY(off, p, sz, stride)
#endif

#ifdef CFI_DEBUG_JEDEC
# define CFI_DUMP_JEDEC(off, p, sz, stride)				\
    do {								\
	printf("%s: JEDEC data\n", __func__);				\
	cfi_hexdump(off, p, sz, stride);				\
    } while (0)
#else
# define CFI_DUMP_JEDEC(off, p, sz, stride)
#endif


static void
cfi_chip_query_1(struct cfi * const cfi)
{
	uint8_t data[0x80];

	bus_space_read_region_1(cfi->cfi_bst, cfi->cfi_bsh, 0, data,
	    __arraycount(data));
	CFI_DUMP_QRY(0, data, sizeof(data), 1);
	CFI_QRY_UNPACK_COMMON(cfi, data, uint8_t);
}

static void
cfi_chip_query_2(struct cfi * const cfi)
{
	uint16_t data[0x80];

	bus_space_read_region_2(cfi->cfi_bst, cfi->cfi_bsh, 0, data,
	    __arraycount(data));
	CFI_DUMP_QRY(0, data, sizeof(data), 2);
	CFI_QRY_UNPACK_COMMON(cfi, data, uint16_t);
}

static void
cfi_chip_query_4(struct cfi * const cfi)
{
	uint32_t data[0x80];

	bus_space_read_region_4(cfi->cfi_bst, cfi->cfi_bsh, 0, data,
	    __arraycount(data));
	CFI_DUMP_QRY(0, data, sizeof(data), 4);
	CFI_QRY_UNPACK_COMMON(cfi, data, uint32_t);
}

static void
cfi_chip_query_8(struct cfi * const cfi)
{
#ifdef NOTYET
	uint64_t data[0x80];

	bus_space_read_region_8(cfi->cfi_bst, cfi->cfi_bsh, 0, data,
	    __arraycount(data));
	CFI_DUMP_QRY(0, data, sizeof(data), 8);
	CFI_QRY_UNPACK_COMMON(cfi, data, uint64_t);
#endif
}

/*
 * cfi_chip_query - detect a CFI chip
 *
 * fill in the struct cfi as we discover what's there
 */
static bool
cfi_chip_query(struct cfi * const cfi)
{
	const bus_size_t cfi_query_offset[] = {
		CFI_QUERY_MODE_ADDR,
		CFI_QUERY_MODE_ALT_ADDR
	};

	KASSERT(cfi != NULL);
	KASSERT(cfi->cfi_bst != NULL);

	for (int j=0; j < __arraycount(cfi_query_offset); j++) {

		cfi_reset_default(cfi);
		cfi_cmd(cfi, cfi_query_offset[j], CFI_QUERY_DATA);

		if (cfi_read_qry(cfi, 0x10) == 'Q' &&
		    cfi_read_qry(cfi, 0x11) == 'R' &&
		    cfi_read_qry(cfi, 0x12) == 'Y') {
			switch(cfi->cfi_portwidth) {
			case 0:
				cfi_chip_query_1(cfi);
				break;
			case 1:
				cfi_chip_query_2(cfi);
				break;
			case 2:
				cfi_chip_query_4(cfi);
				break;
			case 3:
				cfi_chip_query_8(cfi);
				break;
			default:
				panic("%s: bad portwidth %d\n",
				    __func__, cfi->cfi_portwidth);
			}

			switch (cfi->cfi_qry_data.id_pri) {
			case 0x0002:
				cfi->cfi_unlock_addr1 = CFI_AMD_UNLOCK_ADDR1;
				cfi->cfi_unlock_addr2 = CFI_AMD_UNLOCK_ADDR2;
				break;
			default:
				DPRINTF(("%s: unsupported CFI cmdset %#04x\n",
				    __func__, cfi->cfi_qry_data.id_pri));
				return false;
			}

			cfi->cfi_emulated = false;
			return true;
		}
	}

	return false;
}

/*
 * cfi_probe - search for a CFI NOR trying various port & chip widths
 *
 * - gather CFI QRY and PRI data
 * - gather JEDEC ID data
 * - if cfi_chip_query() fails, emulate CFI using table data if possible,
 *   otherwise fail.
 *
 * NOTE:
 *   striped NOR chips design not supported yet
 */
bool
cfi_probe(struct cfi * const cfi)
{
	bool found;

	KASSERT(cfi != NULL);

	/* XXX set default unlock address for cfi_jedec_id() */
	cfi->cfi_unlock_addr1 = CFI_AMD_UNLOCK_ADDR1;
	cfi->cfi_unlock_addr2 = CFI_AMD_UNLOCK_ADDR2;

	for (u_int pw = 0; pw < 3; pw++) {
		for (u_int cw = 0; cw <= pw; cw++) {
			cfi->cfi_portwidth = pw;
			cfi->cfi_chipwidth = cw;
			found = cfi_chip_query(cfi);
			cfi_jedec_id(cfi);
			if (! found)
				found = cfi_emulate(cfi);
			if (found)
				goto exit_qry;
		}
	}

    exit_qry:
	cfi_reset_default(cfi);		/* exit QRY mode */
	return found;
}

bool
cfi_identify(struct cfi * const cfi)
{
	const bus_space_tag_t bst = cfi->cfi_bst;
	const bus_space_handle_t bsh = cfi->cfi_bsh;

	KASSERT(cfi != NULL);
	KASSERT(bst != NULL);

	memset(cfi, 0, sizeof(struct cfi));	/* XXX clean slate */
	cfi->cfi_bst = bst;		/* restore bus space */
	cfi->cfi_bsh = bsh;		/*  "       "   "    */

	return cfi_probe(cfi);
}

static int
cfi_scan_media(device_t self, struct nor_chip *chip)
{
	struct nor_softc *sc = device_private(self);
	KASSERT(sc != NULL);
	KASSERT(sc->sc_nor_if != NULL);
	struct cfi * const cfi = (struct cfi * const)sc->sc_nor_if->private;
	KASSERT(cfi != NULL);

	sc->sc_nor_if->access_width = cfi->cfi_portwidth;

	chip->nc_manf_id = cfi->cfi_id_data.id_mid;
	chip->nc_dev_id = cfi->cfi_id_data.id_did[0]; /* XXX 3 words */
	chip->nc_size = 1 << cfi->cfi_qry_data.device_size;

	/* size of line for Read Buf command */
	chip->nc_line_size = 1 << cfi->cfi_qry_data.pri.cmd_0002.page_size;

	/*
	 * size of erase block
	 * XXX depends on erase region
	 */
	chip->nc_num_luns = 1;
	chip->nc_lun_blocks = cfi->cfi_qry_data.erase_blk_info[0].y + 1;
	chip->nc_block_size = cfi->cfi_qry_data.erase_blk_info[0].z ?
	    cfi->cfi_qry_data.erase_blk_info[0].z * 256 : 128;

	switch (cfi->cfi_qry_data.id_pri) {
	case 0x0002:
		cfi_0002_init(sc, cfi, chip);
		break;
	}

	return 0;
}

void
cfi_init(device_t self)
{
	/* nothing */
}

static void
cfi_select(device_t self, bool select)
{
	/* nothing */
}

static void
cfi_read_1(device_t self, flash_off_t offset, uint8_t *datap)
{
}

static void
cfi_read_2(device_t self, flash_off_t offset, uint16_t *datap)
{
}

static void
cfi_read_4(device_t self, flash_off_t offset, uint32_t *datap)
{
}

static void
cfi_read_buf_1(device_t self, flash_off_t offset, uint8_t *datap, size_t size)
{
}

static void
cfi_read_buf_2(device_t self, flash_off_t offset, uint16_t *datap, size_t size)
{
}

static void
cfi_read_buf_4(device_t self, flash_off_t offset, uint32_t *datap, size_t size)
{
}

static void
cfi_write_1(device_t self, flash_off_t offset, uint8_t data)
{
}

static void
cfi_write_2(device_t self, flash_off_t offset, uint16_t data)
{
}

static void
cfi_write_4(device_t self, flash_off_t offset, uint32_t data)
{
}

static void
cfi_write_buf_1(device_t self, flash_off_t offset, const uint8_t *datap,
    size_t size)
{
}

static void
cfi_write_buf_2(device_t self, flash_off_t offset, const uint16_t *datap,
    size_t size)
{
}

static void
cfi_write_buf_4(device_t self, flash_off_t offset, const uint32_t *datap,
    size_t size)
{
}

/*
 * cfi_cmd - write a CFI command word.
 *
 * The offset 'off' is given for 64-bit port width and will be scaled
 * down to the actual port width of the chip.
 * The command word will be constructed out of 'val' regarding port- and
 * chip width.
 */
void
cfi_cmd(struct cfi * const cfi, bus_size_t off, uint32_t val)
{
	const bus_space_tag_t bst = cfi->cfi_bst;
	bus_space_handle_t bsh = cfi->cfi_bsh;
	uint64_t cmd;
	int cw, pw;

	off >>= 3 - cfi->cfi_portwidth;

	pw = 1 << cfi->cfi_portwidth;
	cw = 1 << cfi->cfi_chipwidth;
	cmd = 0;
	while (pw > 0) {
		cmd <<= cw << 3;
		cmd += val;
		pw -= cw;
	}

	DPRINTF(("%s: %p %x %x %" PRIx64 "\n", __func__, bst, bsh, off, cmd));

	switch (cfi->cfi_portwidth) {
	case 0:
		bus_space_write_1(bst, bsh, off, cmd);
		break;
	case 1:
		bus_space_write_2(bst, bsh, off, cmd);
		break;
	case 2:
		bus_space_write_4(bst, bsh, off, cmd);
		break;
#ifdef NOTYET
	case 3:
		bus_space_write_8(bst, bsh, off, cmd);
		break;
#endif
	default:
		panic("%s: bad portwidth %d bytes\n",
			__func__, 1 << cfi->cfi_portwidth);
	}
}

static uint8_t
cfi_read_qry(struct cfi * const cfi, bus_size_t off)
{
	const bus_space_tag_t bst = cfi->cfi_bst;
	bus_space_handle_t bsh = cfi->cfi_bsh;
	uint8_t data;

	off <<= cfi->cfi_portwidth;

	switch (cfi->cfi_portwidth) {
	case 0:
		data = bus_space_read_1(bst, bsh, off);
		break;
	case 1:
		data = bus_space_read_2(bst, bsh, off);
		break;
	case 2:
		data = bus_space_read_4(bst, bsh, off);
		break;
	case 3:
		data = bus_space_read_8(bst, bsh, off);
		break;
	default:
		data = ~0;
		break;
	}
	return data;
}

/*
 * cfi_reset_default - when we don't know which command will work, use both
 */
void
cfi_reset_default(struct cfi * const cfi)
{

	cfi_cmd(cfi, CFI_ADDR_ANY, CFI_RESET_DATA);
	cfi_cmd(cfi, CFI_ADDR_ANY, CFI_ALT_RESET_DATA);
}

/*
 * cfi_reset_std - use standard reset command
 */
void
cfi_reset_std(struct cfi * const cfi)
{

	cfi_cmd(cfi, CFI_ADDR_ANY, CFI_RESET_DATA);
}

/*
 * cfi_reset_alt - use "alternate" reset command
 */
void
cfi_reset_alt(struct cfi * const cfi)
{

	cfi_cmd(cfi, CFI_ADDR_ANY, CFI_ALT_RESET_DATA);
}

static void
cfi_jedec_id_1(struct cfi * const cfi)
{
	struct cfi_jedec_id_data *idp = &cfi->cfi_id_data;
	uint8_t data[0x10];

	bus_space_read_region_1(cfi->cfi_bst, cfi->cfi_bsh, 0, data,
		__arraycount(data));

	CFI_DUMP_JEDEC(0, data, sizeof(data), 1);

	idp->id_mid = (uint16_t)data[0];
	idp->id_did[0] = (uint16_t)data[1];
	idp->id_did[1] = (uint16_t)data[0xe];
	idp->id_did[2] = (uint16_t)data[0xf];
	idp->id_prot_state = (uint16_t)data[2];
	idp->id_indicators = (uint16_t)data[3];

	/* software bits, upper and lower */
	idp->id_swb_lo = data[0xc];
	idp->id_swb_hi = data[0xd];

}

static void
cfi_jedec_id_2(struct cfi * const cfi)
{
	struct cfi_jedec_id_data *idp = &cfi->cfi_id_data;
	uint16_t data[0x10];

	bus_space_read_region_2(cfi->cfi_bst, cfi->cfi_bsh, 0, data,
		__arraycount(data));

	CFI_DUMP_JEDEC(0, data, sizeof(data), 1);

	idp->id_mid = data[0];
	idp->id_did[0] = data[1];
	idp->id_did[1] = data[0xe];
	idp->id_did[2] = data[0xf];
	idp->id_prot_state = data[2];
	idp->id_indicators = data[3];

	/* software bits, upper and lower
	 * - undefined on S29GL-P
	 * - defined   on S29GL-S
	 */
	idp->id_swb_lo = data[0xc];
	idp->id_swb_hi = data[0xd];

}

static void
cfi_jedec_id_4(struct cfi * const cfi)
{
	struct cfi_jedec_id_data *idp = &cfi->cfi_id_data;
	uint32_t data[0x10];

	bus_space_read_region_4(cfi->cfi_bst, cfi->cfi_bsh, 0, data,
		__arraycount(data));

	CFI_DUMP_JEDEC(0, data, sizeof(data), 1);

	idp->id_mid = data[0] & 0xffff;
	idp->id_did[0] = data[1] & 0xffff;
	idp->id_did[1] = data[0xe] & 0xffff;
	idp->id_did[2] = data[0xf] & 0xffff;
	idp->id_prot_state = data[2] & 0xffff;
	idp->id_indicators = data[3] & 0xffff;

	/* software bits, upper and lower
	 * - undefined on S29GL-P
	 * - defined   on S29GL-S
	 */
	idp->id_swb_lo = data[0xc] & 0xffff;
	idp->id_swb_hi = data[0xd] & 0xffff;

}

/*
 * cfi_jedec_id - get JEDEC ID info
 */
static bool
cfi_jedec_id(struct cfi * const cfi)
{

	DPRINTF(("%s\n", __func__));

	cfi_reset_default(cfi);
	cfi_cmd(cfi, cfi->cfi_unlock_addr1, 0xaa);
	cfi_cmd(cfi, cfi->cfi_unlock_addr2, 0x55);
	cfi_cmd(cfi, cfi->cfi_unlock_addr1, 0x90);

	switch(cfi->cfi_portwidth) {
	case 0:
		cfi_jedec_id_1(cfi);
		break;
	case 1:
		cfi_jedec_id_2(cfi);
		break;
	case 2:
		cfi_jedec_id_4(cfi);
		break;
#ifdef NOTYET
	case 3:
		cfi_jedec_id_8(cfi);
		break;
#endif
	default:
		panic("%s: bad portwidth %d bytes\n",
			__func__, 1 << cfi->cfi_portwidth);
	}

	return true;
}

static bool
cfi_emulate(struct cfi * const cfi)
{
	bool found = false;
	const struct cfi_jedec_tab *jt = cfi_jedec_search(cfi);
	if (jt != NULL) {
		found = true;
		cfi->cfi_emulated = true;
		cfi_jedec_fill(cfi, jt);
	}
	return found;
}

/*
 * cfi_jedec_search - search cfi_jedec_tab[] for entry matching given JEDEC IDs
 */
static const struct cfi_jedec_tab *
cfi_jedec_search(struct cfi *cfi)
{
	struct cfi_jedec_id_data *idp = &cfi->cfi_id_data;

	for (u_int i=0; i < __arraycount(cfi_jedec_tab); i++) {
		const struct cfi_jedec_tab *jt = &cfi_jedec_tab[i];
		if ((jt->jt_mid == idp->id_mid) &&
		    (jt->jt_did == idp->id_did[0])) {
			return jt;
		}
	}
	return NULL;
}

/*
 * cfi_jedec_fill - fill in cfi with info from table entry
 */
static void
cfi_jedec_fill(struct cfi *cfi, const struct cfi_jedec_tab *jt)
{

	cfi->cfi_name = jt->jt_name;

	struct cfi_query_data *qryp = &cfi->cfi_qry_data;
	memset(&qryp, 0, sizeof(*qryp));
	qryp->id_pri = jt->jt_id_pri;
	qryp->id_alt = jt->jt_id_alt;
	qryp->interface_code_desc = jt->jt_interface_code_desc;
	qryp->write_word_time_typ = jt->jt_write_word_time_typ;
	qryp->write_nbyte_time_typ = jt->jt_write_nbyte_time_typ;
	qryp->erase_blk_time_typ = jt->jt_erase_blk_time_typ;
	qryp->erase_chip_time_typ = jt->jt_erase_chip_time_typ;
	qryp->write_word_time_max = jt->jt_write_word_time_max;
	qryp->write_nbyte_time_max = jt->jt_write_nbyte_time_max;
	qryp->erase_blk_time_max = jt->jt_erase_blk_time_max;
	qryp->erase_chip_time_max = jt->jt_erase_chip_time_max;
	qryp->device_size = jt->jt_device_size;
	qryp->interface_code_desc = jt->jt_interface_code_desc;
	qryp->write_nbyte_size_max = jt->jt_write_nbyte_size_max;
	qryp->erase_blk_regions = jt->jt_erase_blk_regions;
	for (u_int i=0; i < 4; i++)
		qryp->erase_blk_info[i] = jt->jt_erase_blk_info[i];

}

void
cfi_print(device_t self, struct cfi * const cfi)
{
	char pbuf[sizeof("XXXX MB")];
	struct cfi_query_data * const qryp = &cfi->cfi_qry_data;

	format_bytes(pbuf, sizeof(pbuf), 1 << qryp->device_size);
	if (cfi->cfi_emulated) {
		aprint_normal_dev(self, "%s NOR flash %s %s\n",
			cfi->cfi_name, pbuf,
			cfi_interface_desc_str(qryp->interface_code_desc));
	} else {
		aprint_normal_dev(self, "CFI NOR flash %s %s\n", pbuf,
			cfi_interface_desc_str(qryp->interface_code_desc));
	}
#ifdef NOR_VERBOSE
	aprint_normal_dev(self, "manufacturer id %#x, device id %#x %#x %#x\n",
		cfi->cfi_id_data.id_mid,
		cfi->cfi_id_data.id_did[0],
		cfi->cfi_id_data.id_did[1],
		cfi->cfi_id_data.id_did[2]);
	aprint_normal_dev(self, "x%u device operating in %u-bit mode\n",
		8 << cfi->cfi_portwidth, 8 << cfi->cfi_chipwidth);
	aprint_normal_dev(self, "sw bits lo=%#x hi=%#x\n",
		cfi->cfi_id_data.id_swb_lo,
		cfi->cfi_id_data.id_swb_hi);
	aprint_normal_dev(self, "max multibyte write size %d\n",
		1 << qryp->write_nbyte_size_max);
	aprint_normal_dev(self, "%d Erase Block Region(s)\n",
		qryp->erase_blk_regions);
	for (u_int r=0; r < qryp->erase_blk_regions; r++) {
		size_t sz = qryp->erase_blk_info[r].z ?
		    qryp->erase_blk_info[r].z * 256 : 128;
		format_bytes(pbuf, sizeof(pbuf), sz);
		aprint_normal("    %d: %d blocks, size %s\n", r,
			qryp->erase_blk_info[r].y + 1, pbuf);
	}
#endif

	switch (cfi->cfi_qry_data.id_pri) {
	case 0x0002:
		cfi_0002_print(self, cfi);
		break;
	}
}

#if defined(CFI_DEBUG_JEDEC) || defined(CFI_DEBUG_QRY)
void
cfi_hexdump(flash_off_t offset, void * const v, u_int count, u_int stride)
{
	uint8_t * const data = v;
	for(int n=0; n < count; n+=16) {
		int i;
		printf("%08llx: ", (offset + n) / stride);
		for(i=n; i < n+16; i++)
			printf("%02x ", data[i]);
		printf("\t");
		for(i=n; i < n+16; i++) {
			u_int c = (int)data[i];
			if (c >= 0x20 && c < 0x7f)
				printf("%c", c);
			else
				printf("%c", '.');
		}
		printf("\n");
	}
}
#endif
