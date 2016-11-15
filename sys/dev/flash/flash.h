/*	$NetBSD: flash.h,v 1.7 2011/07/29 20:48:33 ahoka Exp $	*/

/*-
 * Copyright (c) 2011 Department of Software Engineering,
 *		      University of Szeged, Hungary
 * Copyright (c) 2011 Adam Hoka <ahoka@NetBSD.org>
 * Copyright (c) 2010 David Tengeri <dtengeri@inf.u-szeged.hu>
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

#ifndef _FLASH_H_
#define _FLASH_H_

#include <sys/param.h>
#include <sys/device.h>
#include <sys/module.h>
#include <sys/buf.h>
#include <sys/flashio.h>

#ifdef FLASH_DEBUG
#define FLDPRINTF(x)	if (flashdebug) printf x
#define FLDPRINTFN(n,x)	if (flashdebug>(n)) printf x
#else
#define FLDPRINTF(x)
#define FLDPRINTFN(n,x)
#endif

struct flash_partition {
	flash_off_t part_offset;
	flash_size_t part_size;
	int part_flags;
};

/**
 *  flash_softc - private structure for flash layer driver
 */

struct flash_softc {
	device_t sc_dev;
	device_t sc_parent_dev;		/* Hardware (parent) device */
	void *hw_softc;			/* Hardware device private softc */
	struct flash_interface *flash_if;	/* Hardware interface */
	struct flash_partition sc_partinfo;	/* partition information */

	bool sc_readonly;		/* read only flash device */
};

struct flash_attach_args {
	struct flash_interface *flash_if;	/* Hardware interface */
	struct flash_partition partinfo;
};

/**
 * struct erase_instruction - instructions to erase a flash eraseblock
 */
struct flash_erase_instruction {
	flash_off_t ei_addr;
	flash_off_t ei_len;
	void (*ei_callback)(struct flash_erase_instruction *);
	u_long ei_priv;
	u_char ei_state;
};

enum {
	FLASH_PART_READONLY	= (1<<0),
	FLASH_PART_FILESYSTEM	= (1<<2)
};

/**
 * struct flash_interface - interface for flash operations
 */
struct flash_interface {
	int (*erase)(device_t, struct flash_erase_instruction *);
	int (*read)(device_t, flash_off_t, size_t, size_t *, uint8_t *);
	int (*write)(device_t, flash_off_t, size_t, size_t *, const uint8_t *);
	int (*block_markbad)(device_t, flash_off_t);
	int (*block_isbad)(device_t, flash_off_t, bool *);
	int (*sync)(device_t);

	int (*submit)(device_t, struct buf *);

	uint32_t page_size;
	uint32_t erasesize;
	uint32_t writesize;
	uint32_t minor;
	uint8_t	type;
};

/**
 * struct cache - for caching writes on block device
 */
struct flash_cache {
	size_t fc_len;
	flash_off_t fc_block;
	uint8_t *fc_data;
};

device_t flash_attach_mi(struct flash_interface *, device_t);
const struct flash_interface *flash_get_interface(dev_t);
const struct flash_softc *flash_get_softc(dev_t);
device_t flash_get_device(dev_t);
flash_size_t flash_get_size(dev_t);

/* flash operations should be used through these */
int flash_erase(device_t, struct flash_erase_instruction *);
int flash_read(device_t, flash_off_t, size_t, size_t *, uint8_t *);
int flash_write(device_t, flash_off_t, size_t, size_t *, const uint8_t *);
int flash_block_markbad(device_t, flash_off_t);
int flash_block_isbad(device_t, flash_off_t, bool *);
int flash_sync(device_t);

/*
 * check_pattern - checks the buffer only contains the byte pattern
 *
 * This functions checks if the buffer only contains a specified byte pattern.
 * Returns %0 if found something else, %1 otherwise.
 */
static inline int
check_pattern(const void *buf, uint8_t patt, size_t offset, size_t size)
{
	size_t i;
	for (i = offset; i < size; i++) {
		if (((const uint8_t *)buf)[i] != patt)
			return 0;
	}
	return 1;
}

#endif /* _FLASH_H_ */
