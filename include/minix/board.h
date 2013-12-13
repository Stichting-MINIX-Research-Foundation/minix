#ifndef __BOARD_H__
#define __BOARD_H__
#include <string.h>
/* 
  Utility functions to access/parse the board_id defined in the machine
  struct in include/minix/type.h.

   Identifier for the board 
     [31:28] Architecture.  (MINIX_BOARD_ARCH)
     [27:24] Architecture variant (MINIX_BOARD_ARCH_VARIANT) VERSION e.g. ARMV7 
     [23:16] Vendor/Soc (EG TI )  (MINIX_BOARD_VENDOR) 
     [15:8]  Board      (EG Beagle bone , beagle board ) (MINIX_BOARD)
     [7:0]   Board variant (EG BealgeBone white v.s. BeagleBone black ) (MINIX_BOARD_VARIANT)
*/

#define MINIX_BOARD_ARCH_SHIFT         (28)
#define MINIX_BOARD_ARCH_VARIANT_SHIFT (24)
#define MINIX_BOARD_VENDOR_SHIFT       (16)
#define MINIX_BOARD_SHIFT              (8)
#define MINIX_BOARD_VARIANT_SHIFT      (0)

/* 4 bits */
#define MINIX_BOARD_ARCH_MASK \
	(0XF << MINIX_BOARD_ARCH_SHIFT)
/* 4 bits */
#define MINIX_BOARD_ARCH_VARIANT_MASK \
	(0XF << MINIX_BOARD_ARCH_VARIANT_SHIFT)
/* 8 bits */
#define MINIX_BOARD_VENDOR_MASK \
	(0XFF << MINIX_BOARD_VENDOR_SHIFT)
/* 8 bits */
#define MINIX_BOARD_MASK \
	(0XFF << MINIX_BOARD_SHIFT)
/* 8 bits */
#define MINIX_BOARD_VARIANT_MASK \
	(0XFF << MINIX_BOARD_VARIANT_SHIFT)

#define MINIX_MK_BOARD_ARCH(v) \
	((v << MINIX_BOARD_ARCH_SHIFT ) & MINIX_BOARD_ARCH_MASK)
#define MINIX_MK_BOARD_ARCH_VARIANT(v) \
	(( v << MINIX_BOARD_ARCH_VARIANT_SHIFT) & MINIX_BOARD_ARCH_VARIANT_MASK )
#define MINIX_MK_BOARD_VENDOR(v) \
	(( v << MINIX_BOARD_VENDOR_SHIFT) & MINIX_BOARD_VENDOR_MASK )
#define MINIX_MK_BOARD(v) \
	(( v << MINIX_BOARD_SHIFT) & MINIX_BOARD_MASK )
#define MINIX_MK_BOARD_VARIANT(v) \
	(( v << MINIX_BOARD_VARIANT_SHIFT) & MINIX_BOARD_VARIANT_MASK )

#define MINIX_BOARD_ARCH(v) \
	((v & MINIX_BOARD_ARCH_MASK) >> MINIX_BOARD_ARCH_SHIFT )
#define MINIX_BOARD_ARCH_VARIANT(v) \
	(( v & MINIX_BOARD_ARCH_VARIANT_MASK) >> MINIX_BOARD_ARCH_VARIANT_SHIFT)
#define MINIX_BOARD_VENDOR(v) \
	(( v & MINIX_BOARD_VENDOR_MASK) >> MINIX_BOARD_VENDOR_SHIFT)
#define MINIX_BOARD(v) \
	(( v & MINIX_BOARD_MASK) >> MINIX_BOARD_SHIFT)
#define MINIX_BOARD_VARIANT(v) \
	(( v & MINIX_BOARD_VARIANT_MASK) >> MINIX_BOARD_VARIANT_SHIFT)

/* We want to make it possible to use masks and therefore only try to use bits */
#define MINIX_BOARD_ARCH_X86 MINIX_MK_BOARD_ARCH(1 << 0)
#define MINIX_BOARD_ARCH_ARM MINIX_MK_BOARD_ARCH(1 << 1)

#define MINIX_BOARD_ARCH_VARIANT_X86_GENERIC MINIX_MK_BOARD_ARCH_VARIANT(1<<0)
#define MINIX_BOARD_ARCH_VARIANT_ARM_ARMV6 MINIX_MK_BOARD_ARCH_VARIANT(1<<1)
#define MINIX_BOARD_ARCH_VARIANT_ARM_ARMV7 MINIX_MK_BOARD_ARCH_VARIANT(1<<2)

#define MINIX_BOARD_VENDOR_INTEL MINIX_MK_BOARD_VENDOR(1<<0)
#define MINIX_BOARD_VENDOR_TI MINIX_MK_BOARD_VENDOR(1<<1)

#define MINIX_BOARD_GENERIC MINIX_MK_BOARD(1<<0)
/* BeagleBoard XM */
#define MINIX_BOARD_BBXM MINIX_MK_BOARD(1<<1)
/* BeagleBone (Black and* white) */
#define MINIX_BOARD_BB MINIX_MK_BOARD(1<<2)

/* Only  one  of a kind */
#define MINIX_BOARD_VARIANT_GENERIC MINIX_MK_BOARD_VARIANT(1<<0)
/* BeagleBone White */
#define MINIX_BOARD_VARIANT_BBW MINIX_MK_BOARD_VARIANT(1<<1)
/* BeagleBone Black */
#define MINIX_BOARD_VARIANT_BBB MINIX_MK_BOARD_VARIANT(1<<2)

#define BOARD_ID_INTEL \
	( MINIX_BOARD_ARCH_X86 \
	| MINIX_BOARD_ARCH_VARIANT_X86_GENERIC \
	| MINIX_BOARD_VENDOR_INTEL \
	| MINIX_BOARD_GENERIC \
	| MINIX_BOARD_VARIANT_GENERIC\
	)

#define BOARD_ID_BBXM \
	( MINIX_BOARD_ARCH_ARM \
	| MINIX_BOARD_ARCH_VARIANT_ARM_ARMV7 \
	| MINIX_BOARD_VENDOR_TI \
	| MINIX_BOARD_BBXM \
	| MINIX_BOARD_VARIANT_GENERIC\
	)

#define BOARD_ID_BBW \
	( MINIX_BOARD_ARCH_ARM \
	| MINIX_BOARD_ARCH_VARIANT_ARM_ARMV7 \
	| MINIX_BOARD_VENDOR_TI \
	| MINIX_BOARD_BB \
	| MINIX_BOARD_VARIANT_BBW\
	)

#define BOARD_ID_BBB \
	( MINIX_BOARD_ARCH_ARM \
	| MINIX_BOARD_ARCH_VARIANT_ARM_ARMV7 \
	| MINIX_BOARD_VENDOR_TI \
	| MINIX_BOARD_BB \
	| MINIX_BOARD_VARIANT_BBB\
	)

#define BOARD_IS_BBXM(v) \
		( (BOARD_ID_BBXM & ~MINIX_BOARD_VARIANT_MASK) == (v & ~MINIX_BOARD_VARIANT_MASK))
/* Either one of the known BeagleBones */
#define BOARD_IS_BB(v)   \
		( (BOARD_ID_BBW & ~MINIX_BOARD_VARIANT_MASK) == (v & ~MINIX_BOARD_VARIANT_MASK))
#define BOARD_IS_BBW(v)  ( v == BOARD_ID_BBW)
#define BOARD_IS_BBB(v)  ( v == BOARD_ID_BBB)

#define BOARD_FILTER_BBXM_VALUE (BOARD_ID_BBXM)
#define BOARD_FILTER_BBXM_MASK  \
		(MINIX_BOARD_ARCH_MASK \
		| MINIX_BOARD_ARCH_VARIANT_MASK \
		| MINIX_BOARD_VENDOR_MASK \
		| MINIX_BOARD_MASK \
		| MINIX_BOARD_VARIANT_MASK)

#define BOARD_FILTER_BB_VALUE   (BOARD_ID_BBW & ~MINIX_BOARD_VARIANT_MASK)
#define BOARD_FILTER_BB_MASK    \
		(MINIX_BOARD_ARCH_MASK \
		| MINIX_BOARD_ARCH_VARIANT_MASK \
		| MINIX_BOARD_VENDOR_MASK \
		| MINIX_BOARD_MASK )

struct shortname2id
{
	const char name[15];
	unsigned int id;
};


/* mapping from fields given by the bootloader to board id's */
static struct shortname2id shortname2id[] = {
	{.name = "BBXM",.id = BOARD_ID_BBXM},
	{.name = "A335BONE",.id = BOARD_ID_BBW},
	{.name = "A335BNLT",.id = BOARD_ID_BBB},
};

struct board_id2name
{
	unsigned int id;
	const char name[40];
};

/* how to convert a BOARD id to a board name */
static struct board_id2name board_id2name[] = {
	{.id = BOARD_ID_INTEL,.name = "X86-I586-GENERIC-GENERIC-GENERIC"},
	{.id = BOARD_ID_BBXM,.name = "ARM-ARMV7-TI-BBXM-GENERIC"},
	{.id = BOARD_ID_BBW,.name = "ARM-ARMV7-TI-BB-WHITE"},
	{.id = BOARD_ID_BBB,.name = "ARM-ARMV7-TI-BB-BLACK"},
};

struct board_arch2arch
{
	unsigned int board_arch;
	const char arch[40];
};
/* Mapping from board_arch to arch */
static struct board_arch2arch board_arch2arch[] = {
	{.board_arch = MINIX_BOARD_ARCH_ARM ,.arch = "earm"},
	{.board_arch = MINIX_BOARD_ARCH_X86 ,.arch = "i386"},
};

/* returns 0 if no board was found that match that id */
static int
get_board_id_by_short_name(const char *name)
{
	int x;
	for (x = 0; x < sizeof(shortname2id) / sizeof(shortname2id[0]); x++) {
		if (strncmp(name, shortname2id[x].name, 15) == 0) {
			return shortname2id[x].id;
		}
	}
	return 0;
}

/* returns 0 if no board was found that match that id */
static int
get_board_id_by_name(const char *name)
{
	int x;
	for (x = 0; x < sizeof(board_id2name) / sizeof(board_id2name[0]); x++) {
		if (strncmp(name, board_id2name[x].name, 40) == 0) {
			return board_id2name[x].id;
		}
	}
	return 0;
}

/* convert a board id to a board name to use later 
   returns NULL if no board was found that match that id */
static const char *
get_board_name(unsigned int id)
{
	int x;
	for (x = 0; x < sizeof(board_id2name) / sizeof(board_id2name[0]); x++) {
		if (board_id2name[x].id == id) {
			return board_id2name[x].name;
		}
	}
	return NULL;
}

/* convert a board id to a board name to use later 
   returns NULL if no board was found that match that id */
static const char *
get_board_arch_name(unsigned int id)
{
	int x;
	for (x = 0; x < sizeof(board_arch2arch) / sizeof(board_arch2arch[0]); x++) {
		if (board_arch2arch[x].board_arch == (id & MINIX_BOARD_ARCH_MASK) ) {
			return board_arch2arch[x].arch;
		}
	}
	return NULL;
}

#endif
