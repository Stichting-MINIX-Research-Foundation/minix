/*	$NetBSD: flash.c,v 1.12 2014/07/25 08:10:36 dholland Exp $	*/

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

/*-
 * Framework for storage devices based on Flash technology
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: flash.c,v 1.12 2014/07/25 08:10:36 dholland Exp $");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/kmem.h>
#include <sys/uio.h>
#include <sys/kernel.h>

#include <sys/atomic.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/malloc.h>
#include <sys/reboot.h>

#include <sys/flashio.h>
#include "flash.h"

#ifdef FLASH_DEBUG
int flashdebug = FLASH_DEBUG;
#endif

extern struct cfdriver flash_cd;

dev_type_open(flashopen);
dev_type_close(flashclose);
dev_type_read(flashread);
dev_type_write(flashwrite);
dev_type_ioctl(flashioctl);
dev_type_strategy(flashstrategy);
dev_type_dump(flashdump);

int flash_print(void *aux, const char *pnp);

bool flash_shutdown(device_t dev, int how);
int flash_nsectors(struct buf *bp);
int flash_sector(struct buf *bp);

int flash_match(device_t parent, cfdata_t match, void *aux);
void flash_attach(device_t parent, device_t self, void *aux);
int flash_detach(device_t device, int flags);

CFATTACH_DECL_NEW(flash, sizeof(struct flash_softc),
    flash_match, flash_attach, flash_detach, NULL);

/**
 * Block device's operation
 */
const struct bdevsw flash_bdevsw = {
	.d_open = flashopen,
	.d_close = flashclose,
	.d_strategy = flashstrategy,
	.d_ioctl = flashioctl,
	.d_dump = flashdump,
	.d_psize = nosize,
	.d_discard = nodiscard,	/* XXX this driver probably wants a discard */
	.d_flag = D_DISK | D_MPSAFE
};

/**
 * Character device's operations
 */
const struct cdevsw flash_cdevsw = {
	.d_open = flashopen,
	.d_close = flashclose,
	.d_read = flashread,
	.d_write = flashwrite,
	.d_ioctl = flashioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_DISK | D_MPSAFE
};

/* ARGSUSED */
int
flash_match(device_t parent, cfdata_t match, void *aux)
{
	/* pseudo device, always attaches */
	return 1;
}

/* ARGSUSED */
void
flash_attach(device_t parent, device_t self, void *aux)
{
	struct flash_softc * const sc = device_private(self);
	struct flash_attach_args * const faa = aux;
	char pbuf[2][sizeof("9999 KB")];

	sc->sc_dev = self;
	sc->sc_parent_dev = parent;
	sc->flash_if = faa->flash_if;
	sc->sc_partinfo = faa->partinfo;
	sc->hw_softc = device_private(parent);

	format_bytes(pbuf[0], sizeof(pbuf[0]), sc->sc_partinfo.part_size);
	format_bytes(pbuf[1], sizeof(pbuf[1]), sc->flash_if->erasesize);

	aprint_naive("\n");

	switch (sc->flash_if->type) {
	case FLASH_TYPE_NOR:
		aprint_normal(": NOR flash partition size %s, offset %#jx",
			pbuf[0], (uintmax_t )sc->sc_partinfo.part_offset);
		break;

	case FLASH_TYPE_NAND:
		aprint_normal(": NAND flash partition size %s, offset %#jx",
			pbuf[0], (uintmax_t )sc->sc_partinfo.part_offset);
		break;

	default:
		aprint_normal(": %s unknown flash", pbuf[0]);
	}

	if (sc->sc_partinfo.part_flags & FLASH_PART_READONLY) {
		sc->sc_readonly = true;
		aprint_normal(", read only");
	} else {
		sc->sc_readonly = false;
	}

	aprint_normal("\n");

	if (sc->sc_partinfo.part_size == 0) {
		aprint_error_dev(self,
		    "partition size must be larger than 0\n");
		return;
	}

	switch (sc->flash_if->type) {
	case FLASH_TYPE_NOR:
		aprint_normal_dev(sc->sc_dev,
		    "erase size %s bytes, write size %d bytes\n",
		    pbuf[1], sc->flash_if->writesize);
		break;

	case FLASH_TYPE_NAND:
	default:
		aprint_normal_dev(sc->sc_dev,
		    "erase size %s, page size %d bytes, write size %d bytes\n",
		    pbuf[1], sc->flash_if->page_size,
		    sc->flash_if->writesize);
		break;
	}

	if (!pmf_device_register1(sc->sc_dev, NULL, NULL, flash_shutdown))
		aprint_error_dev(sc->sc_dev,
		    "couldn't establish power handler\n");
}

int
flash_detach(device_t device, int flags)
{
	struct flash_softc * const sc = device_private(device);

	pmf_device_deregister(sc->sc_dev);

	/* freeing flash_if is our responsibility */
	kmem_free(sc->flash_if, sizeof(*sc->flash_if));

	return 0;
}

int
flash_print(void *aux, const char *pnp)
{
	struct flash_attach_args *arg;
	const char *type;

	if (pnp != NULL) {
		arg = aux;
		switch (arg->flash_if->type) {
		case FLASH_TYPE_NOR:
			type = "NOR";
			break;
		case FLASH_TYPE_NAND:
			type = "NAND";
			break;
		default:
			panic("flash_print: unknown type %d",
			    arg->flash_if->type);
		}
		aprint_normal("%s flash at %s", type, pnp);
	}
	return UNCONF;
}

device_t
flash_attach_mi(struct flash_interface * const flash_if, device_t device)
{
	struct flash_attach_args arg;

#ifdef DIAGNOSTIC
	if (flash_if == NULL) {
		aprint_error("flash_attach_mi: NULL\n");
		return 0;
	}
#endif
	arg.flash_if = flash_if;

	return config_found_ia(device, "flashbus", &arg, flash_print);
}

/**
 * flash_open - open the character device
 * Checks if there is a driver registered to the minor number of the open
 * request.
 */
int
flashopen(dev_t dev, int flags, int fmt, lwp_t *l)
{
	int unit = minor(dev);
	struct flash_softc *sc;

	FLDPRINTFN(1, ("flash: opening device unit %d\n", unit));

	if ((sc = device_lookup_private(&flash_cd, unit)) == NULL)
		return ENXIO;

	/* TODO return eperm if want to open for writing a read only dev */

	/* reset buffer length */
//	sc->sc_cache->fc_len = 0;

	return 0;
}

/**
 * flash_close - close device
 * We don't have to release any resources, so just return 0.
 */
int
flashclose(dev_t dev, int flags, int fmt, lwp_t *l)
{
	int unit = minor(dev);
	struct flash_softc *sc;
	int err;

	FLDPRINTFN(1, ("flash: closing flash device unit %d\n", unit));

	if ((sc = device_lookup_private(&flash_cd, unit)) == NULL)
		return ENXIO;

	if (!sc->sc_readonly) {
		err = flash_sync(sc->sc_dev);
		if (err)
			return err;
	}

	return 0;
}

/**
 * flash_read - read from character device
 * This function uses the registered driver's read function to read the
 * requested length to * a buffer and then moves this buffer to userspace.
 */
int
flashread(dev_t dev, struct uio * const uio, int flag)
{
	return physio(flashstrategy, NULL, dev, B_READ, minphys, uio);
}

/**
 * flash_write - write to character device
 * This function moves the data into a buffer from userspace to kernel space,
 * then uses the registered driver's write function to write out the data to
 * the media.
 */
int
flashwrite(dev_t dev, struct uio * const uio, int flag)
{
	return physio(flashstrategy, NULL, dev, B_WRITE, minphys, uio);
}

void
flashstrategy(struct buf * const bp)
{
	struct flash_softc *sc;
	const struct flash_interface *flash_if;
	const struct flash_partition *part;
	int unit, device_blks;

	unit = minor(bp->b_dev);
	sc = device_lookup_private(&flash_cd, unit);
	if (sc == NULL) {
		bp->b_error = ENXIO;
		goto done;
	}

	flash_if = sc->flash_if;
	part = &sc->sc_partinfo;

	/* divider */
	KASSERT(flash_if->writesize != 0);

	aprint_debug_dev(sc->sc_dev, "flash_strategy()\n");

	if (!(bp->b_flags & B_READ) && sc->sc_readonly) {
		bp->b_error = EACCES;
		goto done;
	}

	/* check if length is not negative */
	if (bp->b_blkno < 0) {
		bp->b_error = EINVAL;
		goto done;
	}

	/* zero lenght i/o */
	if (bp->b_bcount == 0) {
		goto done;
	}

	device_blks = sc->sc_partinfo.part_size / DEV_BSIZE;
	KASSERT(part->part_offset % DEV_BSIZE == 0);
	bp->b_rawblkno = bp->b_blkno + (part->part_offset / DEV_BSIZE);

	if (bounds_check_with_mediasize(bp, DEV_BSIZE, device_blks) <= 0) {
		goto done;
	}

	bp->b_resid = bp->b_bcount;
	flash_if->submit(sc->sc_parent_dev, bp);

	return;
done:
	bp->b_resid = bp->b_bcount;
	biodone(bp);
}

/*
 * Handle the ioctl for the device
 */
int
flashioctl(dev_t dev, u_long command, void * const data, int flags, lwp_t *l)
{
	struct flash_erase_params *ep;
	struct flash_info_params *ip;
	struct flash_dump_params *dp;
	struct flash_badblock_params *bbp;
	struct flash_erase_instruction ei;
	struct flash_softc *sc;
	int unit, err;
	size_t retlen;
	flash_off_t offset;
	bool bad;

	unit = minor(dev);
	if ((sc = device_lookup_private(&flash_cd, unit)) == NULL)
		return ENXIO;

	err = 0;
	switch (command) {
	case FLASH_ERASE_BLOCK:
		/**
		 * Set up an erase instruction then call the registered
		 * driver's erase operation.
		 */
		ep = data;

		if (sc->sc_readonly) {
			return EACCES;
		}

		ei.ei_addr = ep->ep_addr;
		ei.ei_len = ep->ep_len;
		ei.ei_callback = NULL;

		err = flash_erase(sc->sc_dev, &ei);
		if (err) {
			return err;
		}

		break;
	case FLASH_BLOCK_ISBAD:
		/**
		 * Set up an erase instruction then call the registered
		 * driver's erase operation.
		 */
		bbp = data;

		err = flash_block_isbad(sc->sc_dev, bbp->bbp_addr, &bad);
		if (err) {
			return err;
		}
		bbp->bbp_isbad = bad;

		break;
	case FLASH_BLOCK_MARKBAD:
		bbp = data;

		err = flash_block_markbad(sc->sc_dev, bbp->bbp_addr);

		break;
	case FLASH_DUMP:
		dp = data;
		offset = dp->dp_block * sc->flash_if->erasesize;
		FLDPRINTF(("Reading from block: %jd len: %jd\n",
			(intmax_t )dp->dp_block, (intmax_t )dp->dp_len));
		err = flash_read(sc->sc_parent_dev, offset, dp->dp_len,
		    &retlen, dp->dp_buf);
		if (err)
			return err;
		if (retlen != dp->dp_len) {
			dp->dp_len = -1;
			dp->dp_buf = NULL;
		}

		break;
	case FLASH_GET_INFO:
		ip = data;

		ip->ip_page_size = sc->flash_if->page_size;
		ip->ip_erase_size = sc->flash_if->erasesize;
		ip->ip_flash_type = sc->flash_if->type;
		ip->ip_flash_size = sc->sc_partinfo.part_size;
		break;
	default:
		err = ENODEV;
	}

	return err;
}

int
flashdump(dev_t dev, daddr_t blkno, void *va, size_t size)
{
	return EACCES;
}

bool
flash_shutdown(device_t self, int how)
{
	struct flash_softc * const sc = device_private(self);

	if ((how & RB_NOSYNC) == 0 && !sc->sc_readonly)
		flash_sync(self);

	return true;
}

const struct flash_interface *
flash_get_interface(dev_t dev)
{
	struct flash_softc *sc;
	int unit;

	unit = minor(dev);
	if ((sc = device_lookup_private(&flash_cd, unit)) == NULL)
		return NULL;

	return sc->flash_if;
}

const struct flash_softc *
flash_get_softc(dev_t dev)
{
	struct flash_softc *sc;
	int unit;

	unit = minor(dev);
	sc = device_lookup_private(&flash_cd, unit);

	return sc;
}

device_t
flash_get_device(dev_t dev)
{
	struct flash_softc *sc;
	int unit;

	unit = minor(dev);
	sc = device_lookup_private(&flash_cd, unit);

	return sc->sc_dev;
}

flash_size_t
flash_get_size(dev_t dev)
{
	const struct flash_softc *sc;

	sc = flash_get_softc(dev);

	return sc->sc_partinfo.part_size;
}

int
flash_erase(device_t self, struct flash_erase_instruction * const ei)
{
	struct flash_softc * const sc = device_private(self);
	KASSERT(ei != NULL);
	struct flash_erase_instruction e = *ei;

	if (sc->sc_readonly)
		return EACCES;

	/* adjust for flash partition */
	e.ei_addr += sc->sc_partinfo.part_offset;

	/* bounds check for flash partition */
	if (e.ei_addr + e.ei_len > sc->sc_partinfo.part_size +
	    sc->sc_partinfo.part_offset)
		return EINVAL;

	return sc->flash_if->erase(device_parent(self), &e);
}

int
flash_read(device_t self, flash_off_t offset, size_t len, size_t * const retlen,
    uint8_t * const buf)
{
	struct flash_softc * const sc = device_private(self);

	offset += sc->sc_partinfo.part_offset;

	if (offset + len > sc->sc_partinfo.part_size +
	    sc->sc_partinfo.part_offset)
		return EINVAL;

	return sc->flash_if->read(device_parent(self),
	    offset, len, retlen, buf);
}

int
flash_write(device_t self, flash_off_t offset, size_t len,
    size_t * const retlen, const uint8_t * const buf)
{
	struct flash_softc * const sc = device_private(self);

	if (sc->sc_readonly)
		return EACCES;

	offset += sc->sc_partinfo.part_offset;

	if (offset + len > sc->sc_partinfo.part_size +
	    sc->sc_partinfo.part_offset)
		return EINVAL;

	return sc->flash_if->write(device_parent(self),
	    offset, len, retlen, buf);
}

int
flash_block_markbad(device_t self, flash_off_t offset)
{
	struct flash_softc * const sc = device_private(self);

	if (sc->sc_readonly)
		return EACCES;

	offset += sc->sc_partinfo.part_offset;

	if (offset + sc->flash_if->erasesize >=
	    sc->sc_partinfo.part_size +
	    sc->sc_partinfo.part_offset)
		return EINVAL;

	return sc->flash_if->block_markbad(device_parent(self), offset);
}

int
flash_block_isbad(device_t self, flash_off_t offset, bool * const bad)
{
	struct flash_softc * const sc = device_private(self);

	offset += sc->sc_partinfo.part_offset;

	if (offset + sc->flash_if->erasesize >
	    sc->sc_partinfo.part_size +
	    sc->sc_partinfo.part_offset)
		return EINVAL;

	return sc->flash_if->block_isbad(device_parent(self), offset, bad);
}

int
flash_sync(device_t self)
{
	struct flash_softc * const sc = device_private(self);

	if (sc->sc_readonly)
		return EACCES;

	/* noop now TODO: implement */
	return 0;
}

MODULE(MODULE_CLASS_DRIVER, flash, NULL);

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
flash_modcmd(modcmd_t cmd, void *opaque)
{
	int error = 0;
#ifdef _MODULE
	int bmaj = -1, cmaj = -1;
#endif

	switch (cmd) {
	case MODULE_CMD_INIT:
#ifdef _MODULE
		error = config_init_component(cfdriver_ioconf_flash,
		    cfattach_ioconf_flash, cfdata_ioconf_flash);
		if (error)
			return error;
		error = devsw_attach("flash", &flash_bdevsw, &bmaj,
		    &flash_cdevsw, &cmaj);
		if (error)
			config_fini_component(cfdriver_ioconf_flash,
			    cfattach_ioconf_flash, cfdata_ioconf_flash);
#endif
		return error;
	case MODULE_CMD_FINI:
#ifdef _MODULE
		devsw_detach(&flash_bdevsw, &flash_cdevsw);
		error = config_fini_component(cfdriver_ioconf_flash,
		    cfattach_ioconf_flash, cfdata_ioconf_flash);
#endif
		return error;
	default:
		return ENOTTY;
	}
}
