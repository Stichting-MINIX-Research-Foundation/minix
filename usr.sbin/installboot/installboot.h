/*	$NetBSD: installboot.h,v 1.38 2011/06/14 05:35:08 matt Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Luke Mewburn of Wasabi Systems.
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

#ifndef	_INSTALLBOOT_H
#define	_INSTALLBOOT_H

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#include "../../sys/sys/bootblock.h"
#else
#include <sys/bootblock.h>
#include <sys/endian.h>
#endif

#include <sys/stat.h>
#include <stdint.h>

typedef enum {
				/* flags from global options */
	IB_VERBOSE =	1<<0,		/* verbose operation */
	IB_NOWRITE =	1<<1,		/* don't write */
	IB_CLEAR =	1<<2,		/* clear boot block */
	IB_EDIT =	1<<3,		/* edit boot parameters */
	IB_FORCE =	1<<4,		/* Ignore some consistency checks */

				/* flags from -o options */
	IB_ALPHASUM =	1<<8,		/* set Alpha checksum */
	IB_APPEND =	1<<9,		/* append stage 1 to EO(regular)F */
	IB_SUNSUM =	1<<10,		/* set Sun checksum */
	IB_STAGE1START=	1<<11,		/* start block for stage 1 provided */
	IB_STAGE2START=	1<<12,		/* start block for stage 2 provided */
	IB_COMMAND = 	1<<13,		/* Amiga commandline option */
	IB_RESETVIDEO =	1<<14,		/* i386 reset video */
	IB_CONSOLE =	1<<15,		/* i386 console */
	IB_CONSPEED =	1<<16,		/* i386 console baud rate */
	IB_TIMEOUT =	1<<17,		/* i386 boot timeout */
	IB_PASSWORD =	1<<18,		/* i386 boot password */
	IB_KEYMAP = 	1<<19,		/* i386 console keymap */
	IB_CONSADDR = 	1<<20,		/* i386 console io address */
	IB_MODULES =	1<<21,		/* i386: load modules */
	IB_BOOTCONF = 	1<<22,		/* i386: read boot.conf */
} ib_flags;

typedef struct {
	ib_flags	 flags;		/* flags (see above) */
	struct ib_mach	*machine;	/* machine details (see below) */
	struct ib_fs	*fstype;	/* file system details (see below) */
	const char	*filesystem;	/* name of target file system */
	int		 fsfd;		/*  open fd to filesystem */
	struct stat	 fsstat;	/*  fstat(2) of fsfd */
	const char	*stage1;	/* name of stage1 bootstrap */
	int		 s1fd;		/*  open fd to stage1 */
	struct stat	 s1stat;	/*  fstat(2) of s1fd */
	uint64_t	 s1start;	/*  start block of stage1 */
	const char	*stage2;	/* name of stage2 bootstrap */
	uint64_t	 s2start;	/*  start block of stage2 */
	uint32_t	 sectorsize;	/* sector size of target fs */
		/* parsed -o option=value data */
	const char	*command;	/* name of command string */
	const char	*console;	/* name of console */
	int		 conspeed;	/* console baud rate */
	int		 consaddr;	/* console io address */
	const char	*password;	/* boot password */
	int		 timeout;	/* interactive boot timeout */
	const char	*keymap;	/* keyboard translations */
} ib_params;

typedef struct {
	uint64_t	block;
	uint32_t	blocksize;
} ib_block;

struct ib_mach {
	const char	*name;
	int		(*setboot)	(ib_params *);
	int		(*clearboot)	(ib_params *);
	int		(*editboot)	(ib_params *);
	ib_flags	valid_flags;
};

struct ib_fs {
		/* compile time parameters */
	const char	*name;
	int		(*match)	(ib_params *);
	int		(*findstage2)	(ib_params *, uint32_t *, ib_block *);
		/* run time fs specific parameters */
	uint32_t	 blocksize;
	uint32_t	 needswap;
	off_t		sblockloc;	/* location of superblock */
	off_t		offset;		/* file system offset (e.g. RAID) */
};

typedef enum {
	BBINFO_BIG_ENDIAN =	0,
	BBINFO_LITTLE_ENDIAN =	1,
} bbinfo_endian;

struct bbinfo_params {
	const char	*magic;		/* magic string to look for */
	uint32_t	offset;		/* offset to write start of stage1 */
	uint32_t	blocksize;	/* blocksize of stage1 */
	uint32_t	maxsize;	/* max size of stage1 */
	uint32_t	headeroffset;	/*
					 * header offset (relative to offset)
					 * to read stage1 into
					 */
	bbinfo_endian	endian;
};

extern struct ib_mach	* const machines[];
extern struct ib_fs	fstypes[];

	/* installboot.c */
uint16_t	compute_sunsum(const uint16_t *);
int		set_sunsum(ib_params *, uint16_t *, uint16_t);
int		no_setboot(ib_params *);
int		no_clearboot(ib_params *);
int		no_editboot(ib_params *);

	/* bbinfo.c */
int		shared_bbinfo_clearboot(ib_params *, struct bbinfo_params *,
		    int (*)(ib_params *, struct bbinfo_params *, uint8_t *));
int		shared_bbinfo_setboot(ib_params *, struct bbinfo_params *,
		    int (*)(ib_params *, struct bbinfo_params *, uint8_t *));

	/* fstypes.c */
int		hardcode_stage2(ib_params *, uint32_t *, ib_block *);
int		ffs_match(ib_params *);
int		ffs_findstage2(ib_params *, uint32_t *, ib_block *);
int		raid_match(ib_params *);
int		raw_match(ib_params *);
int		raw_findstage2(ib_params *, uint32_t *, ib_block *);
int		ext2fs_match(ib_params *);
int		ext2fs_findstage2(ib_params *, uint32_t *, ib_block *);

	/* install_master.c */
void		install_master(const char *device, char *masterboot, char **guide);
int		isoption(const char *option, const char *test);

	/* minixfs3.c */
int minixfs3_is_minix_partition(ib_params *params);
int minixfs3_has_bootblock_space(ib_params *params);


	/* machines.c */
extern struct ib_mach ib_mach_alpha;
extern struct ib_mach ib_mach_amd64;
extern struct ib_mach ib_mach_amiga;
extern struct ib_mach ib_mach_ews4800mips;
extern struct ib_mach ib_mach_hp300;
extern struct ib_mach ib_mach_hp700;
extern struct ib_mach ib_mach_i386;
extern struct ib_mach ib_mach_landisk;
extern struct ib_mach ib_mach_macppc;
extern struct ib_mach ib_mach_news68k;
extern struct ib_mach ib_mach_newsmips;
extern struct ib_mach ib_mach_next68k;
extern struct ib_mach ib_mach_pmax;
extern struct ib_mach ib_mach_sparc;
extern struct ib_mach ib_mach_sparc64;
extern struct ib_mach ib_mach_sun2;
extern struct ib_mach ib_mach_sun3;
extern struct ib_mach ib_mach_vax;
extern struct ib_mach ib_mach_x68k;

#endif	/* _INSTALLBOOT_H */
