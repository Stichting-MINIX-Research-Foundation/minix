/* $NetBSD: radeonfb_bios.c,v 1.4 2010/11/03 00:49:02 macallan Exp $ */

/*-
 * Copyright (c) 2006 Itronix Inc.
 * All rights reserved.
 *
 * Written by Garrett D'Amore for Itronix Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Itronix Inc. may not be used to endorse
 *    or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ITRONIX INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL ITRONIX INC. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */ 

/*
 * ATI Technologies Inc. ("ATI") has not assisted in the creation of, and
 * does not endorse, this software.  ATI will not be responsible or liable
 * for any actual or alleged damage or loss caused by or in connection with
 * the use of or reliance on this software.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: radeonfb_bios.c,v 1.4 2010/11/03 00:49:02 macallan Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/bus.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/radeonfbreg.h>
#include <dev/pci/radeonfbvar.h>

#include "opt_radeonfb.h"

#ifdef RADEONFB_BIOS_INIT

/*
 * Globals for the entire BIOS.
 */
#define	ROM_HEADER_OFFSET		0x48
#define	MAX_REVISION			0x10
#define	SINGLE_TABLE_REVISION		0x09
#define	MIN_OFFSET			0x60

/*
 * Offsets of specific tables.
 */
#define	RAGE_REGS1_OFFSET		0x0c
#define	RAGE_REGS2_OFFSET		0x4e
#define	DYN_CLOCK_OFFSET		0x52
#define	PLL_INIT_OFFSET			0x46
#define	MEM_CONFIG_OFFSET		0x48

/*
 * Values related to generic intialization tables.
 */
#define	TABLE_ENTRY_FLAG_MASK		0xe000
#define	TABLE_ENTRY_INDEX_MASK		0x1fff
#define	TABLE_ENTRY_COMMAND_MASK	0x00ff

#define	TABLE_FLAG_WRITE_INDEXED	0x0000
#define	TABLE_FLAG_WRITE_DIRECT		0x2000
#define	TABLE_FLAG_MASK_INDEXED		0x4000
#define	TABLE_FLAG_MASK_DIRECT		0x6000
#define	TABLE_FLAG_DELAY		0x8000
#define	TABLE_FLAG_SCOMMAND		0xa000

#define	TABLE_SCOMMAND_WAIT_MC_BUSY_MASK	0x03
#define	TABLE_SCOMMAND_WAIT_MEM_PWRUP_COMPLETE	0x08

/*
 * PLL initialization block values.
 */
#define	PLL_FLAG_MASK			0xc0
#define	PLL_INDEX_MASK			0x3f

#define	PLL_FLAG_WRITE			0x00
#define	PLL_FLAG_MASK_BYTE		0x40
#define	PLL_FLAG_WAIT			0x80

#define	PLL_WAIT_150MKS				1
#define	PLL_WAIT_5MS				2
#define	PLL_WAIT_MC_BUSY_MASK			3
#define	PLL_WAIT_DLL_READY_MASK			4
#define	PLL_WAIT_CHK_SET_CLK_PWRMGT_CNTL24	5


#ifdef	RADEONFB_BIOS_DEBUG
#define	DPRINTF(x)	printf x
#else
#define	DPRINTF(x)
#endif

struct rb_table;

static void rb_validate(struct radeonfb_softc *, struct rb_table *);
static uint16_t rb_find_asic_table(struct radeonfb_softc *, struct rb_table *);
static uint16_t rb_find_mem_reset_table(struct radeonfb_softc *,
    struct rb_table *);
static uint16_t rb_find_short_mem_reset_table(struct radeonfb_softc *,
    struct rb_table *);
static int rb_load_init_block(struct radeonfb_softc *, struct rb_table *);
static int rb_load_pll_block(struct radeonfb_softc *, struct rb_table *);
static int rb_reset_sdram(struct radeonfb_softc *, struct rb_table *);

static void rb_wait_mc_busy_mask(struct radeonfb_softc *, uint16_t);
static void rb_wait_mem_pwrup_complete(struct radeonfb_softc *, uint16_t);
static void rb_wait_dll_ready_mask(struct radeonfb_softc *, uint16_t);
static void rb_wait_chk_set_clk_pwrmgt_cntl24(struct radeonfb_softc *);

/*
 * Generic structure describing the tables.
 */
struct rb_table {
	const unsigned char	*name;
	uint16_t		offset;
	struct rb_table 	*parent;

	/* validate that the table looks sane */
	void	(*validate)(struct radeonfb_softc *, struct rb_table *);

	/* find looks for the table relative to its "parent" */
	uint16_t	(*find)(struct radeonfb_softc *, struct rb_table *);
};

/*
 * Instances of specific tables.
 */
static struct rb_table rb_rage_regs1_table = {
	"rage_regs_1",			/* name */
	RAGE_REGS1_OFFSET,		/* offset */
	NULL,				/* parent */
	rb_validate,			/* validate */
	NULL,				/* find */
};

static struct rb_table rb_rage_regs2_table = {
	"rage_regs_2",			/* name */
	RAGE_REGS2_OFFSET,		/* offset */
	NULL,				/* parent */
	rb_validate,			/* validate */
	NULL,				/* find */
};

static struct rb_table rb_dyn_clock_table = {
	"dyn_clock",			/* name */
	DYN_CLOCK_OFFSET,		/* offset */
	NULL,				/* parent */
	rb_validate,			/* validate */
	NULL,				/* find */
};

static struct rb_table rb_pll_init_table = {
	"pll_init",			/* name */
	PLL_INIT_OFFSET,		/* offset */
	NULL,				/* parent */
	rb_validate,			/* validate */
	NULL,				/* find */
};

static struct rb_table rb_mem_config_table = {
	"mem_config",			/* name */
	MEM_CONFIG_OFFSET,		/* offset */
	NULL,				/* parent */
	rb_validate,			/* validate */
	NULL,				/* find */
};

static struct rb_table rb_mem_reset_table = {
	"mem_reset",			/* name */
	0,				/* offset */
	&rb_mem_config_table,		/* parent */
	NULL,				/* validate */
	rb_find_mem_reset_table,	/* find */
};

static struct rb_table rb_short_mem_reset_table = {
	"short_mem_reset",		/* name */
	0,				/* offset */
	&rb_mem_config_table,		/* parent */
	NULL,				/* validate */
	rb_find_short_mem_reset_table,	/* find */
};

static struct rb_table rb_rage_regs3_table = {
	"rage_regs_3",			/* name */
	0,				/* offset */
	&rb_rage_regs2_table,		/* parent */
	NULL,				/* validate */
	rb_find_asic_table,		/* find */
};

static struct rb_table rb_rage_regs4_table = {
	"rage_regs_4",			/* name */
	0,				/* offset */
	&rb_rage_regs3_table,		/* parent */
	NULL,				/* validate */
	rb_find_asic_table,		/* find */
};

static struct rb_table *rb_tables[] = {
	&rb_rage_regs1_table,
	&rb_rage_regs2_table,
	&rb_dyn_clock_table,
	&rb_pll_init_table,
	&rb_mem_config_table,
	&rb_mem_reset_table,
	&rb_short_mem_reset_table,
	&rb_rage_regs3_table,
	&rb_rage_regs4_table,
	NULL
};

void
rb_validate(struct radeonfb_softc *sc, struct rb_table *tp)
{
	uint8_t	rev;

	rev = GETBIOS8(sc, tp->offset - 1);

	if (rev > MAX_REVISION) {
		DPRINTF(("%s: bad rev %x of %s\n", XNAME(sc), rev, tp->name));
		tp->offset = 0;
		return;
	}

	if (tp->offset < MIN_OFFSET) {
		DPRINTF(("%s: wrong pointer to %s!\n", XNAME(sc), tp->name));
		tp->offset = 0;
		return;
	}
}

uint16_t
rb_find_asic_table(struct radeonfb_softc *sc, struct rb_table *tp)
{
	uint16_t		offset;
	uint8_t			c;

	if ((offset = tp->offset) != 0) {
		while ((c = GETBIOS8(sc, offset + 1)) != 0) {
			if (c & 0x40)
				offset += 10;
			else if (c & 0x80)
				offset += 4;
			else
				offset += 6;
		}
		return offset + 2;
	}
	return 0;
}

uint16_t
rb_find_mem_reset_table(struct radeonfb_softc *sc, struct rb_table *tp)
{
	uint16_t		offset;

	if ((offset = tp->offset) != 0) {
		while (GETBIOS8(sc, offset))
			offset++;
		offset++;
		return offset + 2;	/* skip table revision and mask */
	}
	return 0;
}

uint16_t
rb_find_short_mem_reset_table(struct radeonfb_softc *sc, struct rb_table *tp)
{

	if ((tp->offset != 0) && (GETBIOS8(sc, tp->offset - 2) <= 64))
		return (tp->offset + GETBIOS8(sc, tp->offset - 3));

	return 0;
}

/* helper commands */
void
rb_wait_mc_busy_mask(struct radeonfb_softc *sc, uint16_t count)
{
	DPRINTF(("WAIT_MC_BUSY_MASK: %d ", count));
	while (count--) {
		if (!(radeonfb_getpll(sc, RADEON_CLK_PWRMGT_CNTL) &
			RADEON_MC_BUSY_MASK))
			break;
	}
	DPRINTF(("%d\n", count));
}

void
rb_wait_mem_pwrup_complete(struct radeonfb_softc *sc, uint16_t count)
{
	DPRINTF(("WAIT_MEM_PWRUP_COMPLETE: %d ", count));
	while (count--) {
		if ((radeonfb_getindex(sc, RADEON_MEM_STR_CNTL) &
			RADEON_MEM_PWRUP_COMPLETE) ==
		    RADEON_MEM_PWRUP_COMPLETE)
			break;
	}
	DPRINTF(("%d\n", count));
}

void
rb_wait_dll_ready_mask(struct radeonfb_softc *sc, uint16_t count)
{
	DPRINTF(("WAIT_DLL_READY_MASK: %d ", count));
	while (count--) {
		if (radeonfb_getpll(sc, RADEON_CLK_PWRMGT_CNTL) &
		    RADEON_DLL_READY_MASK)
			break;
	}
	DPRINTF(("%d\n", count));
}

void
rb_wait_chk_set_clk_pwrmgt_cntl24(struct radeonfb_softc *sc)
{
	uint32_t	pmc;
	DPRINTF(("WAIT CHK_SET_CLK_PWRMGT_CNTL24\n"));
	pmc = radeonfb_getpll(sc, RADEON_CLK_PWRMGT_CNTL);

	if (pmc & RADEON_CLK_PWRMGT_CNTL24) {
		radeonfb_maskpll(sc, RADEON_MCLK_CNTL, 0xFFFF0000,
		    RADEON_SET_ALL_SRCS_TO_PCI);
		delay(10000);
		radeonfb_putpll(sc, RADEON_CLK_PWRMGT_CNTL,
		    pmc & ~RADEON_CLK_PWRMGT_CNTL24);
		delay(10000);
	}
}

/*
 * Block initialization routines.  These take action based on data in
 * the tables.
 */
int
rb_load_init_block(struct radeonfb_softc *sc, struct rb_table *tp)
{
	uint16_t	offset;
	uint16_t	value;

	if ((tp == NULL) || ((offset = tp->offset) == 0))
		return 1;

	DPRINTF(("%s: load_init_block processing %s\n", XNAME(sc), tp->name));
	while ((value = GETBIOS16(sc, offset)) != 0) {
		uint16_t	flag = value & TABLE_ENTRY_FLAG_MASK;
		uint16_t	index = value & TABLE_ENTRY_INDEX_MASK;
		uint8_t		command = value & TABLE_ENTRY_COMMAND_MASK;
		uint32_t	ormask;
		uint32_t	andmask;
		uint16_t	count;

		offset += 2;

		switch (flag) {
		case TABLE_FLAG_WRITE_INDEXED:
			DPRINTF(("WRITE INDEXED: %x %x\n",
				    index, (uint32_t)GETBIOS32(sc, offset)));
			radeonfb_putindex(sc, index, GETBIOS32(sc, offset));
			offset += 4;
			break;

		case TABLE_FLAG_WRITE_DIRECT:
			DPRINTF(("WRITE DIRECT: %x %x\n",
				    index, (uint32_t)GETBIOS32(sc, offset)));
			radeonfb_put32(sc, index, GETBIOS32(sc, offset));
			offset += 4;
			break;

		case TABLE_FLAG_MASK_INDEXED:
			andmask = GETBIOS32(sc, offset);
			offset += 4;
			ormask = GETBIOS32(sc, offset);
			offset += 4;
			DPRINTF(("MASK INDEXED: %x %x %x\n",
				    index, andmask, ormask));
			radeonfb_maskindex(sc, index, andmask, ormask);
			break;

		case TABLE_FLAG_MASK_DIRECT:
			andmask = GETBIOS32(sc, offset);
			offset += 4;
			ormask = GETBIOS32(sc, offset);
			offset += 4;
			DPRINTF(("MASK DIRECT: %x %x %x\n",
				    index, andmask, ormask));
			radeonfb_mask32(sc, index,  andmask, ormask);
			break;

		case TABLE_FLAG_DELAY:
			/* in the worst case, this would be 16msec */
			count = GETBIOS16(sc, offset);
			DPRINTF(("DELAY: %d\n", count));
			delay(count);
			offset += 2;
			break;

		case TABLE_FLAG_SCOMMAND:
			DPRINTF(("SCOMMAND %x\n", command)); 
			switch (command) {

			case TABLE_SCOMMAND_WAIT_MC_BUSY_MASK:
				count = GETBIOS16(sc, offset);
				rb_wait_mc_busy_mask(sc, count);
				break;

			case TABLE_SCOMMAND_WAIT_MEM_PWRUP_COMPLETE:
				count = GETBIOS16(sc, offset);
				rb_wait_mem_pwrup_complete(sc, count);
				break;

			}
			offset += 2;
			break;
		}
	}
	return 0;
}

int
rb_load_pll_block(struct radeonfb_softc *sc, struct rb_table *tp)
{
	uint16_t	offset;
	uint8_t		index;
	uint8_t		shift;
	uint32_t	andmask;
	uint32_t	ormask;

	if ((tp == NULL) || ((offset = tp->offset) == 0))
		return 1;

	DPRINTF(("%s: load_pll_block processing %s\n", XNAME(sc), tp->name));
	while ((index = GETBIOS8(sc, offset)) != 0) {
		offset++;

		switch (index & PLL_FLAG_MASK) {
		case PLL_FLAG_WAIT:
			switch (index & PLL_INDEX_MASK) {
			case PLL_WAIT_150MKS:
				delay(150);
				break;
			case PLL_WAIT_5MS:
				/* perhaps this should be tsleep? */
				delay(5000);
				break;

			case PLL_WAIT_MC_BUSY_MASK:
				rb_wait_mc_busy_mask(sc, 1000);
				break;

			case PLL_WAIT_DLL_READY_MASK:
				rb_wait_dll_ready_mask(sc, 1000);
				break;

			case PLL_WAIT_CHK_SET_CLK_PWRMGT_CNTL24:
				rb_wait_chk_set_clk_pwrmgt_cntl24(sc);
				break;
			}
			break;
			
		case PLL_FLAG_MASK_BYTE:
			shift = GETBIOS8(sc, offset) * 8;
			offset++;

			andmask =
			    (((uint32_t)GETBIOS8(sc, offset)) << shift) |
			    ~((uint32_t)0xff << shift);
			offset++;

			ormask = ((uint32_t)GETBIOS8(sc, offset)) << shift;
			offset++;

			DPRINTF(("PLL_MASK_BYTE %u %u %x %x\n", index, 
				    shift, andmask, ormask));
			radeonfb_maskpll(sc, index, andmask, ormask);
			break;

		case PLL_FLAG_WRITE:
			DPRINTF(("PLL_WRITE %u %x\n", index,
				    GETBIOS32(sc, offset)));
			radeonfb_putpll(sc, index, GETBIOS32(sc, offset));
			offset += 4;
			break;
		}
	}

	return 0;
}

int
rb_reset_sdram(struct radeonfb_softc *sc, struct rb_table *tp)
{
	uint16_t offset;
	uint8_t	index;

	if ((tp == NULL) || ((offset = tp->offset) == 0))
		return 1;

	DPRINTF(("%s: reset_sdram processing %s\n", XNAME(sc), tp->name));

	while ((index = GETBIOS8(sc, offset)) != 0xff) {
		offset++;
		if (index == 0x0f) {
			rb_wait_mem_pwrup_complete(sc, 20000);
		} else {
			uint32_t	ormask;

			ormask = GETBIOS16(sc, offset);
			offset += 2;

			DPRINTF(("INDEX reg RADEON_MEM_SDRAM_MODE_REG %x %x\n",
				    RADEON_SDRAM_MODE_MASK, ormask));
			radeonfb_maskindex(sc, RADEON_MEM_SDRAM_MODE_REG,
			    RADEON_SDRAM_MODE_MASK, ormask);

			ormask = (uint32_t)index << 24;
			DPRINTF(("INDEX reg RADEON_MEM_SDRAM_MODE_REG %x %x\n",
				    RADEON_B3MEM_RESET_MASK, ormask));
			radeonfb_maskindex(sc, RADEON_MEM_SDRAM_MODE_REG,
			    RADEON_B3MEM_RESET_MASK, ormask);
		}
	}
	return 0;
}

/*
 * Master entry point to parse and act on table data.
 */
int
radeonfb_bios_init(struct radeonfb_softc *sc)
{
	uint16_t		revision;
	uint16_t		scratch;
	int			i;
	struct rb_table		*tp;

	if (!sc->sc_biossz)
		return 1;

	scratch = GETBIOS16(sc, ROM_HEADER_OFFSET);
	revision = GETBIOS8(sc, scratch);
	DPRINTF(("%s: Bios Rev: %d\n", XNAME(sc), revision));


	/* First parse pass -- locate tables  */
	for (i = 0; (tp = rb_tables[i]) != NULL; i++) {

		DPRINTF(("%s: parsing table %s\n", XNAME(sc), tp->name));

		if (tp->offset != 0) {
			uint16_t	temp, offset;

			temp = GETBIOS16(sc, ROM_HEADER_OFFSET);
			offset = GETBIOS16(sc, temp + tp->offset);
			if (offset)
				tp->offset = offset;
			    
		} else {
			tp->offset = tp->find(sc, tp->parent);
		}

		if (tp->validate)
			tp->validate(sc, tp);

		if (revision > SINGLE_TABLE_REVISION)
			break;
	}

	if (rb_rage_regs3_table.offset + 1 == rb_pll_init_table.offset) {
		rb_rage_regs3_table.offset = 0;
		rb_rage_regs4_table.offset = 0;
	}

	if (rb_rage_regs1_table.offset)
		rb_load_init_block(sc, &rb_rage_regs1_table);

	if (revision < SINGLE_TABLE_REVISION) {
		if (rb_pll_init_table.offset)
			rb_load_pll_block(sc, &rb_pll_init_table);
		if (rb_rage_regs2_table.offset)
			rb_load_init_block(sc, &rb_rage_regs2_table);
		if (rb_rage_regs4_table.offset)
			rb_load_init_block(sc, &rb_rage_regs4_table);
		if (rb_mem_reset_table.offset)
			rb_reset_sdram(sc, &rb_mem_reset_table);
		if (rb_rage_regs3_table.offset)
			rb_load_init_block(sc, &rb_rage_regs3_table);
		if (rb_dyn_clock_table.offset)
			rb_load_pll_block(sc, &rb_dyn_clock_table);
	}

	DPRINTF(("%s: BIOS parse done\n", XNAME(sc)));
	return 0;
}

#endif
