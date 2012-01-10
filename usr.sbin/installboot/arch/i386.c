/* $NetBSD: i386.c,v 1.37 2011/08/14 17:50:17 christos Exp $ */

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by David Laight.
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

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
#if !defined(__lint)
__RCSID("$NetBSD: i386.c,v 1.37 2011/08/14 17:50:17 christos Exp $");
#endif /* !__lint */

#include <sys/param.h>
#ifndef HAVE_NBTOOL_CONFIG_H
#include <sys/ioctl.h>
#include <sys/dkio.h>
#endif

#include <assert.h>
#include <errno.h>
#include <err.h>
#include <md5.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "installboot.h"

static const struct console_name {
	const char	*name;		/* Name of console selection */
	const int	dev;		/* value matching CONSDEV_* from sys/arch/i386/stand/lib/libi386.h */
} consoles[] = {
	{ "pc",		0 /* CONSDEV_PC */ },
	{ "com0",	1 /* CONSDEV_COM0 */ },
	{ "com1",	2 /* CONSDEV_COM1 */ },
	{ "com2",	3 /* CONSDEV_COM2 */ },
	{ "com3",	4 /* CONSDEV_COM3 */ },
	{ "com0kbd",	5 /* CONSDEV_COM0KBD */ },
	{ "com1kbd",	6 /* CONSDEV_COM1KBD */ },
	{ "com2kbd",	7 /* CONSDEV_COM2KBD */ },
	{ "com3kbd",	8 /* CONSDEV_COM3KBD */ },
	{ "auto",	-1 /* CONSDEV_AUTO */ },
};

static int i386_setboot(ib_params *);
static int i386_editboot(ib_params *);

struct ib_mach ib_mach_i386 =
	{ "i386", i386_setboot, no_clearboot, i386_editboot,
		IB_RESETVIDEO | IB_CONSOLE | IB_CONSPEED | IB_CONSADDR |
		IB_KEYMAP | IB_PASSWORD | IB_TIMEOUT |
		IB_MODULES | IB_BOOTCONF };

#ifdef __minix
struct ib_mach ib_mach_i686 =
	{ "i686", i386_setboot, no_clearboot, i386_editboot,
		IB_RESETVIDEO | IB_CONSOLE | IB_CONSPEED | IB_CONSADDR |
		IB_KEYMAP | IB_PASSWORD | IB_TIMEOUT |
		IB_MODULES | IB_BOOTCONF };
#endif

struct ib_mach ib_mach_amd64 =
	{ "amd64", i386_setboot, no_clearboot, i386_editboot,
		IB_RESETVIDEO | IB_CONSOLE | IB_CONSPEED | IB_CONSADDR |
		IB_KEYMAP | IB_PASSWORD | IB_TIMEOUT |
		IB_MODULES | IB_BOOTCONF };

/*
 * Attempting to write the 'labelsector' (or a sector near it - within 8k?)
 * using the non-raw disk device fails silently.  This can be detected (today)
 * by doing a fsync() and a read back.
 * This is very likely to affect installboot, indeed the code may need to
 * be written into the 'labelsector' itself - especially on non-512 byte media.
 * We do all writes with a read verify.
 * If EROFS is returned we also try to enable writes to the label sector.
 * (Maybe these functions should be in the generic part of installboot.)
 */
static int
pwrite_validate(int fd, const void *buf, size_t n_bytes, off_t offset)
{
	void *r_buf;
	ssize_t rv;

	r_buf = malloc(n_bytes);
	if (r_buf == NULL)
		return -1;
	rv = pwrite(fd, buf, n_bytes, offset);
	if (rv == -1) {
		free(r_buf);
		return -1;
	}
	fsync(fd);
	if (pread(fd, r_buf, rv, offset) == rv && memcmp(r_buf, buf, rv) == 0) {
		free(r_buf);
		return rv;
	}
	free(r_buf);
	errno = EROFS;
	return -1;
}

static int
write_boot_area(ib_params *params, uint8_t *buf, size_t len)
{
	int rv, i;

	/*
	 * Writing the 'label' sector (likely to be bytes 512-1023) could
	 * fail, so we try to avoid writing that area.
	 * Unfortunately, if we are accessing the raw disk, and the sector
	 * size is larger than 512 bytes that is also doomed.
	 * See how we get on....
	 *
	 * NB: Even if the physical sector size is not 512, the space for
	 * the label is 512 bytes from the start of the disk.
	 * So all the '512' constants in these functions are correct.
	 */

	/* Write out first 512 bytes - the pbr code */
	rv = pwrite_validate(params->fsfd, buf, 512, 0);
	if (rv == 512) {
		/* That worked, do the rest */
		if (len == 512)
			return 1;
		len -= 512 * 2;
		rv = pwrite_validate(params->fsfd, buf + 512 * 2, len, 512 * 2);
		if (rv != (ssize_t)len)
			goto bad_write;
		return 1;
	}
	if (rv != -1 || (errno != EINVAL && errno != EROFS))
		goto bad_write;

	if (errno == EINVAL) {
		/* Assume the failure was due to to the sector size > 512 */
		rv = pwrite_validate(params->fsfd, buf, len, 0);
		if (rv == (ssize_t)len)
			return 1;
		if (rv != -1 || (errno != EROFS))
			goto bad_write;
	}

#ifdef DIOCWLABEL
	/* Pesky label is protected, try to unprotect it */
	i = 1;
	rv = ioctl(params->fsfd, DIOCWLABEL, &i);
	if (rv != 0) {
		warn("Cannot enable writes to the label sector");
		return 0;
	}
	/* Try again with label write-enabled */
	rv = pwrite_validate(params->fsfd, buf, len, 0);

	/* Reset write-protext */
	i = 0;
	ioctl(params->fsfd, DIOCWLABEL, &i);
	if (rv == (ssize_t)len)
		return 1;
#endif

  bad_write:
	if (rv == -1)
		warn("Writing `%s'", params->filesystem);
	else 
		warnx("Writing `%s': short write, %u bytes",
			params->filesystem, rv);
	return 0;
}

static void
show_i386_boot_params(struct x86_boot_params  *bpp)
{
	size_t i;

	printf("Boot options:        ");
	printf("timeout %d, ", le32toh(bpp->bp_timeout));
	printf("flags %x, ", le32toh(bpp->bp_flags));
	printf("speed %d, ", le32toh(bpp->bp_conspeed));
	printf("ioaddr %x, ", le32toh(bpp->bp_consaddr));
	for (i = 0; i < __arraycount(consoles); i++) {
		if (consoles[i].dev == (int)le32toh(bpp->bp_consdev))
			break;
	}
	if (i == __arraycount(consoles))
		printf("console %d\n", le32toh(bpp->bp_consdev));
	else
		printf("console %s\n", consoles[i].name);
	if (bpp->bp_keymap[0])
		printf("                     keymap %s\n", bpp->bp_keymap);
}

static int
is_zero(const uint8_t *p, unsigned int len)
{
	return len == 0 || (p[0] == 0 && memcmp(p, p + 1, len - 1) == 0);
}

static int
update_i386_boot_params(ib_params *params, struct x86_boot_params  *bpp)
{
	struct x86_boot_params bp;
	uint32_t bplen;
	size_t i;

	bplen = le32toh(bpp->bp_length);
	if (bplen > sizeof bp)
		/* Ignore pad space in bootxx */
		bplen = sizeof bp;

	/* Take (and update) local copy so we handle size mismatches */
	memset(&bp, 0, sizeof bp);
	memcpy(&bp, bpp, bplen);

	if (params->flags & IB_TIMEOUT)
		bp.bp_timeout = htole32(params->timeout);
	if (params->flags & IB_RESETVIDEO)
		bp.bp_flags ^= htole32(X86_BP_FLAGS_RESET_VIDEO);
	if (params->flags & IB_CONSPEED)
		bp.bp_conspeed = htole32(params->conspeed);
	if (params->flags & IB_CONSADDR)
		bp.bp_consaddr = htole32(params->consaddr);
	if (params->flags & IB_CONSOLE) {
		for (i = 0; i < __arraycount(consoles); i++)
			if (strcmp(consoles[i].name, params->console) == 0)
				break;

		if (i == __arraycount(consoles)) {
			warnx("invalid console name, valid names are:");
			(void)fprintf(stderr, "\t%s", consoles[0].name);
			for (i = 1; consoles[i].name != NULL; i++)
				(void)fprintf(stderr, ", %s", consoles[i].name);
			(void)fprintf(stderr, "\n");
			return 1;
		}
		bp.bp_consdev = htole32(consoles[i].dev);
	}
	if (params->flags & IB_PASSWORD) {
		if (params->password[0]) {
			MD5_CTX md5ctx;
			MD5Init(&md5ctx);
			MD5Update(&md5ctx, params->password,
			    strlen(params->password));
			MD5Final(bp.bp_password, &md5ctx);
			bp.bp_flags |= htole32(X86_BP_FLAGS_PASSWORD);
		} else {
			memset(&bp.bp_password, 0, sizeof bp.bp_password);
			bp.bp_flags &= ~htole32(X86_BP_FLAGS_PASSWORD);
		}
	}
	if (params->flags & IB_KEYMAP)
		strlcpy(bp.bp_keymap, params->keymap, sizeof bp.bp_keymap);
	if (params->flags & IB_MODULES)
		bp.bp_flags ^= htole32(X86_BP_FLAGS_NOMODULES);
	if (params->flags & IB_BOOTCONF)
		bp.bp_flags ^= htole32(X86_BP_FLAGS_NOBOOTCONF);

	if (params->flags & (IB_NOWRITE | IB_VERBOSE))
		show_i386_boot_params(&bp);

	/* Check we aren't trying to set anything we can't save */
	if (!is_zero((char *)&bp + bplen, sizeof bp - bplen)) {
		warnx("Patch area in stage1 bootstrap is too small");
		return 1;
	}
	memcpy(bpp, &bp, bplen);
	return 0;
}

static int
i386_setboot(ib_params *params)
{
	unsigned int	u;
	ssize_t		rv;
	uint32_t	*magic, expected_magic;
	union {
	    struct mbr_sector	mbr;
	    uint8_t		b[8192];
	} disk_buf, bootstrap;

	assert(params != NULL);
	assert(params->fsfd != -1);
	assert(params->filesystem != NULL);
	assert(params->s1fd != -1);
	assert(params->stage1 != NULL);

	/*
	 * There is only 8k of space in a FFSv1 partition (and ustarfs)
	 * so ensure we don't splat over anything important.
	 */
	if (params->s1stat.st_size > (off_t)(sizeof bootstrap)) {
		warnx("stage1 bootstrap `%s' (%u bytes) is larger than 8192 bytes",
			params->stage1, (unsigned int)params->s1stat.st_size);
		return 0;
	}
	if (params->s1stat.st_size < 3 * 512 && params->s1stat.st_size != 512) {
		warnx("stage1 bootstrap `%s' (%u bytes) is too small",
			params->stage1, (unsigned int)params->s1stat.st_size);
		return 0;
	}

	/* Read in the existing disk header and boot code */
	rv = pread(params->fsfd, &disk_buf, sizeof (disk_buf), 0);
	if (rv != sizeof(disk_buf)) {
		if (rv == -1)
			warn("Reading `%s'", params->filesystem);
		else
			warnx("Reading `%s': short read, %ld bytes"
			    " (should be %ld)", params->filesystem, (long)rv,
			    (long)sizeof(disk_buf));
		return 0;
	}

	if (disk_buf.mbr.mbr_magic != le16toh(MBR_MAGIC)) {
		if (params->flags & IB_VERBOSE) {
			printf(
		    "Ignoring PBR with invalid magic in sector 0 of `%s'\n",
			    params->filesystem);
		}
		memset(&disk_buf, 0, 512);
	}

	/* Read the new bootstrap code. */
	rv = pread(params->s1fd, &bootstrap, params->s1stat.st_size, 0);
	if (rv != params->s1stat.st_size) {
		if (rv == -1)
			warn("Reading `%s'", params->stage1);
		else
			warnx("Reading `%s': short read, %ld bytes"
			    " (should be %ld)", params->stage1, (long)rv,
			    (long)params->s1stat.st_size);
		return 0;
	}

	/*
	 * The bootstrap code is either 512 bytes for booting FAT16, or best
	 * part of 8k (with bytes 512-1023 all zeros).
	 */
	if (params->s1stat.st_size == 512) {
		/* Magic number is at end of pbr code */
		magic = (void *)(bootstrap.b + 512 - 16 + 4);
		expected_magic = htole32(X86_BOOT_MAGIC_FAT);
	} else {
		/* Magic number is at start of sector following label */
		magic = (void *)(bootstrap.b + 512 * 2 + 4);
		expected_magic = htole32(X86_BOOT_MAGIC_1);
		/*
		 * For a variety of reasons we restrict our 'normal' partition
		 * boot code to a size which enable it to be used as mbr code.
		 * IMHO this is bugus (dsl).
		 */
		if (!is_zero(bootstrap.b + 512-2-64, 64)) {
			warnx("Data in mbr partition table of new bootstrap");
			return 0;
		}
		if (!is_zero(bootstrap.b + 512, 512)) {
			warnx("Data in label part of new bootstrap");
			return 0;
		}
		/* Copy mbr table and label from existing disk buffer */
		memcpy(bootstrap.b + 512-2-64, disk_buf.b + 512-2-64, 64);
		memcpy(bootstrap.b + 512, disk_buf.b + 512, 512);
	}

	/* Validate the 'magic number' that marks the parameter block */
	if (*magic != expected_magic) {
		warnx("Invalid magic in stage1 bootstrap %x != %x",
				*magic, expected_magic);
		return 0;
	}

	/*
	 * If the partition has a FAT (or NTFS) filesystem, then we must
	 * preserve the BIOS Parameter Block (BPB).
	 * It is also very likely that there isn't 8k of space available
	 * for (say) bootxx_msdos, and that blindly installing it will trash
	 * the FAT filesystem.
	 * To avoid this we check the number of 'reserved' sectors to ensure
	 * there there is enough space.
	 * Unfortunately newfs(8) doesn't (yet) splat the BPB (which is
	 * effectively the FAT superblock) when a filesystem is initailised
	 * so this code tends to complain rather too often,
	 * Specifying 'installboot -f' will delete the old BPB info.
	 */
	if (!(params->flags & IB_FORCE)) {
		#define USE_F ", use -f (may invalidate filesystem)"
		/*
		 * For FAT compatibility, the pbr code starts 'jmp xx; nop'
		 * followed by the BIOS Parameter Block (BPB).
		 * The 2nd byte (jump offset) is the size of the nop + BPB.
		 */
		if (bootstrap.b[0] != 0xeb || bootstrap.b[2] != 0x90) {
			warnx("No BPB in new bootstrap %02x:%02x:%02x" USE_F,
				bootstrap.b[0], bootstrap.b[1], bootstrap.b[2]);
			return 0;
		}

		/* Find size of old BPB, and copy into new bootcode */
		if (!is_zero(disk_buf.b + 3 + 8, disk_buf.b[1] - 1 - 8)) {
			struct mbr_bpbFAT16 *bpb = (void *)(disk_buf.b + 3 + 8);
			/* Check enough space before the FAT for the bootcode */
			u = le16toh(bpb->bpbBytesPerSec)
			    * le16toh(bpb->bpbResSectors);
			if (u != 0 && u < params->s1stat.st_size) {
				warnx("Insufficient reserved space before FAT "
					"(%u bytes available)" USE_F, u);
				return 0;
			}
			/* Check we have enough space for the old bpb */
			if (disk_buf.b[1] > bootstrap.b[1]) {
				/* old BPB is larger, allow if extra zeros */
				if (!is_zero(disk_buf.b + 2 + bootstrap.b[1],
				    disk_buf.b[1] - bootstrap.b[1])) {
					warnx("Old BPB too big" USE_F);
					    return 0;
				}
				u = bootstrap.b[1];
			} else {
				/* Old BPB is shorter, leave zero filled */
				u = disk_buf.b[1];
			}
			memcpy(bootstrap.b + 2, disk_buf.b + 2, u);
		}
		#undef USE_F
	}

	/*
	 * Fill in any user-specified options into the
	 *      struct x86_boot_params
	 * that follows the magic number.
	 * See sys/arch/i386/stand/bootxx/bootxx.S for more information.
	 */
	if (update_i386_boot_params(params, (void *)(magic + 1)))
		return 0;

	if (params->flags & IB_NOWRITE) {
		return 1;
	}

	/* Copy new bootstrap data into disk buffer, ignoring label area */
	memcpy(&disk_buf, &bootstrap, 512);
	if (params->s1stat.st_size > 512 * 2) {
		memcpy(disk_buf.b + 2 * 512, bootstrap.b + 2 * 512,
		    params->s1stat.st_size - 2 * 512);
		/* Zero pad to 512 byte sector boundary */
		memset(disk_buf.b + params->s1stat.st_size, 0,
			(8192 - params->s1stat.st_size) & 511);
	}

	return write_boot_area(params, disk_buf.b, sizeof disk_buf.b);
}

static int
i386_editboot(ib_params *params)
{
	int		retval;
	uint8_t		buf[512];
	ssize_t		rv;
	uint32_t	magic;
	uint32_t	offset;
	struct x86_boot_params	*bpp;

	assert(params != NULL);
	assert(params->fsfd != -1);
	assert(params->filesystem != NULL);

	retval = 0;

	/*
	 * Read in the existing bootstrap.
	 * Look in any of the first 4 sectors.
	 */

	bpp = NULL;
	for (offset = 0; offset < 4 * 512; offset += 512) {
		rv = pread(params->fsfd, &buf, sizeof buf, offset);
		if (rv == -1) {
			warn("Reading `%s'", params->filesystem);
			goto done;
		} else if (rv != sizeof buf) {
			warnx("Reading `%s': short read", params->filesystem);
			goto done;
		}

		/* Magic number is 4 bytes in (to allow for a jmps) */
		/* Also allow any of the magic numbers. */
		magic = le32toh(*(uint32_t *)(buf + 4)) | 0xf;
		if (magic != (X86_BOOT_MAGIC_1 | 0xf))
			continue;

		/* The parameters are just after the magic number */
		bpp = (void *)(buf + 8);
		break;
	}
	if (bpp == NULL) {
		warnx("Invalid magic in existing bootstrap");
		goto done;
	}

	/*
	 * Fill in any user-specified options into the
	 *      struct x86_boot_params
	 * that's 8 bytes in from the start of the third sector.
	 * See sys/arch/i386/stand/bootxx/bootxx.S for more information.
	 */
	if (update_i386_boot_params(params, bpp))
		goto done;

	if (params->flags & IB_NOWRITE) {
		retval = 1;
		goto done;
	}

	/*
	 * Write boot code back
	 */
	rv = pwrite(params->fsfd, buf, sizeof buf, offset);
	if (rv == -1) {
		warn("Writing `%s'", params->filesystem);
		goto done;
	} else if (rv != sizeof buf) {
		warnx("Writing `%s': short write, %zd bytes (should be %zu)",
		    params->filesystem, rv, sizeof(buf));
		goto done;
	}

	retval = 1;

 done:
	return retval;
}
