/*	$NetBSD: minixfs3.c,v 1.1 2012/01/16 18:44:13 christos Exp $	*/

/*-
 * Copyright (c) 2012
 *	Vrije Universiteit, Amsterdam, The Netherlands. All rights reserved.
 *
 * Author: Evgeniy Ivanov (based on libsa/ext2fs.c).
 *
 * This code is derived from src/sys/lib/libsa/ext2fs.c contributed to 
 * The NetBSD Foundation, see copyrights below.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1997 Manuel Bouyer.
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * The Mach Operating System project at Carnegie-Mellon University.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *
 * Copyright (c) 1990, 1991 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Author: David Golub
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 *	Stand-alone file reading package for MFS file system.
 */

#include <sys/param.h>
#include <sys/time.h>
#ifdef _STANDALONE
#include <lib/libkern/libkern.h>
#else
#include <string.h>
#endif

#include "stand.h"
#include "minixfs3.h"

#if defined(LIBSA_FS_SINGLECOMPONENT) && !defined(LIBSA_NO_FS_SYMLINK)
#define LIBSA_NO_FS_SYMLINK
#endif

#if defined(LIBSA_NO_TWIDDLE)
#define twiddle()
#endif

typedef uint32_t	ino32_t;
#ifndef FSBTODB
#define FSBTODB(fs, indp) fsbtodb(fs, indp)
#endif

/*
 * To avoid having a lot of filesystem-block sized buffers lurking (which
 * could be 32k) we only keep a few entries of the indirect block map.
 * With 8k blocks, 2^8 blocks is ~500k so we reread the indirect block
 * ~13 times pulling in a 6M kernel.
 * The cache size must be smaller than the smallest filesystem block,
 * so LN2_IND_CACHE_SZ <= 9 (UFS2 and 4k blocks).
 */
#define LN2_IND_CACHE_SZ	6
#define IND_CACHE_SZ		(1 << LN2_IND_CACHE_SZ)
#define IND_CACHE_MASK		(IND_CACHE_SZ - 1)

/*
 * In-core open file.
 */
struct file {
	off_t		f_seekp;	/* seek pointer */
	struct mfs_sblock  *f_fs;	/* pointer to super-block */
	struct mfs_dinode  f_di;	/* copy of on-disk inode */
	uint		f_nishift;	/* for blocks in indirect block */
	block_t		f_ind_cache_block;
	block_t		f_ind_cache[IND_CACHE_SZ];

	char		*f_buf;		/* buffer for data block */
	size_t		f_buf_size;	/* size of data block */
	daddr_t		f_buf_blkno;	/* block number of data block */
};

#if defined(LIBSA_ENABLE_LS_OP)

#define NELEM(x) (sizeof (x) / sizeof(*x))

typedef struct entry_t entry_t;
struct entry_t {
	entry_t	*e_next;
	ino32_t	e_ino;
	char	e_name[1];
};

#endif /* LIBSA_ENABLE_LS_OP */


static int read_inode(ino32_t, struct open_file *);
static int block_map(struct open_file *, block_t, block_t *);
static int buf_read_file(struct open_file *, void *, size_t *);
static int search_directory(const char *, int, struct open_file *, ino32_t *);
static int read_sblock(struct open_file *, struct mfs_sblock *);

/*
 * Read a new inode into a file structure.
 */
static int
read_inode(ino32_t inumber, struct open_file *f)
{
	struct file *fp = (struct file *)f->f_fsdata;
	struct mfs_sblock *fs = fp->f_fs;
	char *buf;
	size_t rsize;
	int rc;
	daddr_t inode_sector;
	struct mfs_dinode *dip;

	inode_sector = FSBTODB(fs, ino_to_fsba(fs, inumber));

	/*
	 * Read inode and save it.
	 */
	buf = fp->f_buf;
	twiddle();
	rc = DEV_STRATEGY(f->f_dev)(f->f_devdata, F_READ,
	    inode_sector, fs->mfs_block_size, buf, &rsize);
	if (rc)
		return rc;
	if (rsize != fs->mfs_block_size)
		return EIO;

	dip = (struct mfs_dinode *)(buf +
	    INODE_SIZE * ino_to_fsbo(fs, inumber));
	mfs_iload(dip, &fp->f_di);

	/*
	 * Clear out the old buffers
	 */
	fp->f_ind_cache_block = ~0;
	fp->f_buf_blkno = -1;
	return rc;
}

/*
 * Given an offset in a file, find the disk block number (not zone!)
 * that contains that block.
 */
static int
block_map(struct open_file *f, block_t file_block, block_t *disk_block_p)
{
	struct file *fp = (struct file *)f->f_fsdata;
	struct mfs_sblock *fs = fp->f_fs;
	uint level;
	block_t ind_cache;
	block_t ind_block_num;
	zone_t zone;
	size_t rsize;
	int rc;
	int boff;
	int scale = fs->mfs_log_zone_size; /* for block-zone conversion */
	block_t *buf = (void *)fp->f_buf;

	/*
	 * Index structure of an inode:
	 *
	 * mdi_blocks[0..NR_DZONES-1]
	 *			hold zone numbers for zones
	 *			0..NR_DZONES-1
	 *
	 * mdi_blocks[NR_DZONES+0]
	 *			block NDADDR+0 is the single indirect block
	 *			holds zone numbers for zones
	 *			NR_DZONES .. NR_DZONES + NINDIR(fs)-1
	 *
	 * mdi_blocks[NR_DZONES+1]
	 *			block NDADDR+1 is the double indirect block
	 *			holds zone numbers for INDEX blocks for zones
	 *			NR_DZONES + NINDIR(fs) ..
	 *			NR_TZONES + NINDIR(fs) + NINDIR(fs)**2 - 1
	 */

	zone = file_block >> scale;
	boff = (int) (file_block - (zone << scale) ); /* relative blk in zone */

	if (zone < NR_DZONES) {
		/* Direct zone */
		zone_t z = fs2h32(fp->f_di.mdi_zone[zone]);
		if (z == NO_ZONE) {
			*disk_block_p = NO_BLOCK;
			return 0;
		}
		*disk_block_p = (block_t) ((z << scale) + boff);
		return 0;
	}

	zone -= NR_DZONES;

	ind_cache = zone >> LN2_IND_CACHE_SZ;
	if (ind_cache == fp->f_ind_cache_block) {
		*disk_block_p =
		    fs2h32(fp->f_ind_cache[zone & IND_CACHE_MASK]);
		return 0;
	}

	for (level = 0;;) {
		level += fp->f_nishift;

		if (zone < (block_t)1 << level)
			break;
		if (level > NIADDR * fp->f_nishift)
			/* Zone number too high */
			return EFBIG;
		zone -= (block_t)1 << level;
	}

	ind_block_num =
	    fs2h32(fp->f_di.mdi_zone[NR_DZONES + (level / fp->f_nishift - 1)]);

	for (;;) {
		level -= fp->f_nishift;
		if (ind_block_num == 0) {
			*disk_block_p = NO_BLOCK;	/* missing */
			return 0;
		}

		twiddle();
		/*
		 * If we were feeling brave, we could work out the number
		 * of the disk sector and read a single disk sector instead
		 * of a filesystem block.
		 * However we don't do this very often anyway...
		 */
		rc = DEV_STRATEGY(f->f_dev)(f->f_devdata, F_READ,
			FSBTODB(fs, ind_block_num), fs->mfs_block_size,
			buf, &rsize);
		if (rc)
			return rc;
		if (rsize != fs->mfs_block_size)
			return EIO;

		ind_block_num = fs2h32(buf[zone >> level]);
		if (level == 0)
			break;
		zone &= (1 << level) - 1;
	}

	/* Save the part of the block that contains this sector */
	memcpy(fp->f_ind_cache, &buf[zone & ~IND_CACHE_MASK],
	    IND_CACHE_SZ * sizeof fp->f_ind_cache[0]);
	fp->f_ind_cache_block = ind_cache;

	zone = (zone_t)ind_block_num;
	*disk_block_p = (block_t)((zone << scale) + boff);
	return 0;
}

/*
 * Read a portion of a file into an internal buffer.
 * Return the location in the buffer and the amount in the buffer.
 */
static int
buf_read_file(struct open_file *f, void *v, size_t *size_p)
{
	char **buf_p = v;
	struct file *fp = (struct file *)f->f_fsdata;
	struct mfs_sblock *fs = fp->f_fs;
	long off;
	block_t file_block;
	block_t disk_block;
	size_t block_size;
	int rc;

	off = blkoff(fs, fp->f_seekp);
	file_block = lblkno(fs, fp->f_seekp);
	block_size = fs->mfs_block_size;

	if (file_block != fp->f_buf_blkno) {
		rc = block_map(f, file_block, &disk_block);
		if (rc)
			return rc;

		if (disk_block == 0) {
			memset(fp->f_buf, 0, block_size);
			fp->f_buf_size = block_size;
		} else {
			twiddle();
			rc = DEV_STRATEGY(f->f_dev)(f->f_devdata, F_READ,
				FSBTODB(fs, disk_block),
				block_size, fp->f_buf, &fp->f_buf_size);
			if (rc)
				return rc;
		}

		fp->f_buf_blkno = file_block;
	}

	/*
	 * Return address of byte in buffer corresponding to
	 * offset, and size of remainder of buffer after that
	 * byte.
	 */
	*buf_p = fp->f_buf + off;
	*size_p = block_size - off;

	/*
	 * But truncate buffer at end of file.
	 */
	if (*size_p > fp->f_di.mdi_size - fp->f_seekp)
		*size_p = fp->f_di.mdi_size - fp->f_seekp;

	return 0;
}

/*
 * Search a directory for a name and return its
 * inode number.
 */
static int
search_directory(const char *name, int length, struct open_file *f,
	ino32_t *inumber_p)
{
	struct file *fp = (struct file *)f->f_fsdata;
	struct mfs_sblock *fs = fp->f_fs;
	struct mfs_direct *dp;
	struct mfs_direct *dbuf;
	size_t buf_size;
	int namlen;
	int rc;

	fp->f_seekp = 0;

	while (fp->f_seekp < (off_t)fp->f_di.mdi_size) {
		rc = buf_read_file(f, (void *)&dbuf, &buf_size);
		if (rc)
			return rc;
		if (buf_size == 0)
			return EIO;

		/* XXX we assume, that buf_read_file reads an fs block and
		 * doesn't truncate buffer. Currently i_size in MFS doesn't
		 * the same as size of allocated blocks, it makes buf_read_file
		 * to truncate buf_size.
		 */
		if (buf_size < fs->mfs_block_size)
			buf_size = fs->mfs_block_size;

		for (dp = dbuf; dp < &dbuf[NR_DIR_ENTRIES(fs)]; dp++) {
			char *cp;
			if (fs2h32(dp->mfsd_ino) == (ino32_t) 0)
				continue;
			/* Compute the length of the name */
			cp = memchr(dp->mfsd_name, '\0', sizeof(dp->mfsd_name));
			if (cp == NULL)
				namlen = sizeof(dp->mfsd_name);
			else
				namlen = cp - (dp->mfsd_name);

			if (namlen == length &&
			    !memcmp(name, dp->mfsd_name, length)) {
				/* found entry */
				*inumber_p = fs2h32(dp->mfsd_ino);
				return 0;
			}
		}
		fp->f_seekp += buf_size;
	}
	return ENOENT;
}

int
read_sblock(struct open_file *f, struct mfs_sblock *fs)
{
	static uint8_t sbbuf[MINBSIZE];
	size_t buf_size;
	int rc;

	/* We must read amount multiple of sector size, hence we can't
	 * read SBSIZE and read MINBSIZE.
	 */
	if (SBSIZE > MINBSIZE)
		return EINVAL;

	rc = DEV_STRATEGY(f->f_dev)(f->f_devdata, F_READ,
	    SUPER_BLOCK_OFF / DEV_BSIZE, MINBSIZE, sbbuf, &buf_size);
	if (rc)
		return rc;

	if (buf_size != MINBSIZE)
		return EIO;

	mfs_sbload((void *)sbbuf, fs);

	if (fs->mfs_magic != SUPER_MAGIC)
		return EINVAL;
	if (fs->mfs_block_size < MINBSIZE)
		return EINVAL;
	if ((fs->mfs_block_size % 512) != 0)
		return EINVAL;
	if (SBSIZE > fs->mfs_block_size)
		return EINVAL;
	if ((fs->mfs_block_size % INODE_SIZE) != 0)
		return EINVAL;

	/* For even larger disks, a similar problem occurs with s_firstdatazone.
	 * If the on-disk field contains zero, we assume that the value was too
	 * large to fit, and compute it on the fly.
	 */
	if (fs->mfs_firstdatazone_old == 0) {
		block_t offset;
		offset = START_BLOCK + fs->mfs_imap_blocks + fs->mfs_zmap_blocks;
		offset += (fs->mfs_ninodes + fs->mfs_inodes_per_block - 1) /
				fs->mfs_inodes_per_block;

		fs->mfs_firstdatazone =
			(offset + (1 << fs->mfs_log_zone_size) - 1) >>
				fs->mfs_log_zone_size;
	} else {
		fs->mfs_firstdatazone = (zone_t) fs->mfs_firstdatazone_old;
	}

	if (fs->mfs_imap_blocks < 1 || fs->mfs_zmap_blocks < 1
			|| fs->mfs_ninodes < 1 || fs->mfs_zones < 1
			|| fs->mfs_firstdatazone <= 4
			|| fs->mfs_firstdatazone >= fs->mfs_zones
			|| (unsigned) fs->mfs_log_zone_size > 4)
		return EINVAL;

	/* compute in-memory mfs_sblock values */
	fs->mfs_inodes_per_block = fs->mfs_block_size / INODE_SIZE;


	{
		int32_t mult = fs->mfs_block_size >> LOG_MINBSIZE;
		int ln2 = LOG_MINBSIZE;

		for (; mult != 1; ln2++)
			mult >>= 1;

		fs->mfs_bshift = ln2;
		/* XXX assume hw bsize = 512 */
		fs->mfs_fsbtodb = ln2 - LOG_MINBSIZE + 1;
	}

	fs->mfs_qbmask = fs->mfs_block_size - 1;
	fs->mfs_bmask = ~fs->mfs_qbmask;

	return 0;
}

/*
 * Open a file.
 */
__compactcall int
minixfs3_open(const char *path, struct open_file *f)
{
#ifndef LIBSA_FS_SINGLECOMPONENT
	const char *cp, *ncp;
	int c;
#endif
	ino32_t inumber;
	struct file *fp;
	struct mfs_sblock *fs;
	int rc;
#ifndef LIBSA_NO_FS_SYMLINK
	ino32_t parent_inumber;
	int nlinks = 0;
	char namebuf[MAXPATHLEN+1];
	char *buf;
#endif

	/* allocate file system specific data structure */
	fp = alloc(sizeof(struct file));
	memset(fp, 0, sizeof(struct file));
	f->f_fsdata = (void *)fp;

	/* allocate space and read super block */
	fs = alloc(sizeof(*fs));
	memset(fs, 0, sizeof(*fs));
	fp->f_fs = fs;
	twiddle();

	rc = read_sblock(f, fs);
	if (rc)
		goto out;

	/* alloc a block sized buffer used for all fs transfers */
	fp->f_buf = alloc(fs->mfs_block_size);

	/*
	 * Calculate indirect block levels.
	 */
	{
		int32_t mult;
		int ln2;

		/*
		 * We note that the number of indirect blocks is always
		 * a power of 2.  This lets us use shifts and masks instead
		 * of divide and remainder and avoinds pulling in the
		 * 64bit division routine into the boot code.
		 */
		mult = NINDIR(fs);
#ifdef DEBUG
		if (!powerof2(mult)) {
			/* Hummm was't a power of 2 */
			rc = EINVAL;
			goto out;
		}
#endif
		for (ln2 = 0; mult != 1; ln2++)
			mult >>= 1;

		fp->f_nishift = ln2;
	}

	inumber = ROOT_INODE;
	if ((rc = read_inode(inumber, f)) != 0)
		goto out;

#ifndef LIBSA_FS_SINGLECOMPONENT
	cp = path;
	while (*cp) {

		/*
		 * Remove extra separators
		 */
		while (*cp == '/')
			cp++;
		if (*cp == '\0')
			break;

		/*
		 * Check that current node is a directory.
		 */
		if ((fp->f_di.mdi_mode & I_TYPE) != I_DIRECTORY) {
			rc = ENOTDIR;
			goto out;
		}

		/*
		 * Get next component of path name.
		 */
		ncp = cp;
		while ((c = *cp) != '\0' && c != '/')
			cp++;

		/*
		 * Look up component in current directory.
		 * Save directory inumber in case we find a
		 * symbolic link.
		 */
#ifndef LIBSA_NO_FS_SYMLINK
		parent_inumber = inumber;
#endif
		rc = search_directory(ncp, cp - ncp, f, &inumber);
		if (rc)
			goto out;

		/*
		 * Open next component.
		 */
		if ((rc = read_inode(inumber, f)) != 0)
			goto out;

#ifndef LIBSA_NO_FS_SYMLINK
		/*
		 * Check for symbolic link.
		 */
		if ((fp->f_di.mdi_mode & I_TYPE) == I_SYMBOLIC_LINK) {
			int link_len = fp->f_di.mdi_size;
			int len;
			size_t buf_size;
			block_t	disk_block;

			len = strlen(cp);

			if (link_len + len > MAXPATHLEN ||
			    ++nlinks > MAXSYMLINKS) {
				rc = ENOENT;
				goto out;
			}

			memmove(&namebuf[link_len], cp, len + 1);

			/*
			 * Read file for symbolic link
			 */
			buf = fp->f_buf;
			rc = block_map(f, (block_t)0, &disk_block);
			if (rc)
				goto out;

			twiddle();
			rc = DEV_STRATEGY(f->f_dev)(f->f_devdata,
					F_READ, FSBTODB(fs, disk_block),
					fs->mfs_block_size, buf, &buf_size);
			if (rc)
				goto out;

			memcpy(namebuf, buf, link_len);

			/*
			 * If relative pathname, restart at parent directory.
			 * If absolute pathname, restart at root.
			 */
			cp = namebuf;
			if (*cp != '/')
				inumber = parent_inumber;
			else
				inumber = (ino32_t) ROOT_INODE;

			if ((rc = read_inode(inumber, f)) != 0)
				goto out;
		}
#endif	/* !LIBSA_NO_FS_SYMLINK */
	}

	/*
	 * Found terminal component.
	 */
	rc = 0;

#else /* !LIBSA_FS_SINGLECOMPONENT */

	/* look up component in the current (root) directory */
	rc = search_directory(path, strlen(path), f, &inumber);
	if (rc)
		goto out;

	/* open it */
	rc = read_inode(inumber, f);

#endif /* !LIBSA_FS_SINGLECOMPONENT */

	fp->f_seekp = 0;		/* reset seek pointer */

out:
	if (rc)
		minixfs3_close(f);

	return rc;
}

__compactcall int
minixfs3_close(struct open_file *f)
{
	struct file *fp = (struct file *)f->f_fsdata;

	f->f_fsdata = NULL;
	if (fp == NULL)
		return 0;

	if (fp->f_buf)
		dealloc(fp->f_buf, fp->f_fs->mfs_block_size);
	dealloc(fp->f_fs, sizeof(*fp->f_fs));
	dealloc(fp, sizeof(struct file));
	return 0;
}

/*
 * Copy a portion of a file into kernel memory.
 * Cross block boundaries when necessary.
 */
__compactcall int
minixfs3_read(struct open_file *f, void *start, size_t size, size_t *resid)
{
	struct file *fp = (struct file *)f->f_fsdata;
	size_t csize;
	char *buf;
	size_t buf_size;
	int rc = 0;
	char *addr = start;

	while (size != 0) {
		if (fp->f_seekp >= (off_t)fp->f_di.mdi_size)
			break;

		rc = buf_read_file(f, &buf, &buf_size);
		if (rc)
			break;

		csize = size;
		if (csize > buf_size)
			csize = buf_size;

		memcpy(addr, buf, csize);

		fp->f_seekp += csize;
		addr += csize;
		size -= csize;
	}

	if (resid)
		*resid = size;
	return rc;
}

/*
 * Not implemented.
 */
#ifndef LIBSA_NO_FS_WRITE
__compactcall int
minixfs3_write(struct open_file *f, void *start, size_t size, size_t *resid)
{

	return EROFS;
}
#endif /* !LIBSA_NO_FS_WRITE */

#ifndef LIBSA_NO_FS_SEEK
__compactcall off_t
minixfs3_seek(struct open_file *f, off_t offset, int where)
{
	struct file *fp = (struct file *)f->f_fsdata;

	switch (where) {
	case SEEK_SET:
		fp->f_seekp = offset;
		break;
	case SEEK_CUR:
		fp->f_seekp += offset;
		break;
	case SEEK_END:
		fp->f_seekp = fp->f_di.mdi_size - offset;
		break;
	default:
		return -1;
	}
	return fp->f_seekp;
}
#endif /* !LIBSA_NO_FS_SEEK */

__compactcall int
minixfs3_stat(struct open_file *f, struct stat *sb)
{
	struct file *fp = (struct file *)f->f_fsdata;

	/* only important stuff */
	memset(sb, 0, sizeof *sb);
	sb->st_mode = fp->f_di.mdi_mode;
	sb->st_uid = fp->f_di.mdi_uid;
	sb->st_gid = fp->f_di.mdi_gid;
	sb->st_size = fp->f_di.mdi_size;
	return 0;
}

#if defined(LIBSA_ENABLE_LS_OP)
__compactcall void
minixfs3_ls(struct open_file *f, const char *pattern,
		void (*funcp)(char* arg), char* path)
{
	struct file *fp = (struct file *)f->f_fsdata;
	struct mfs_sblock *fs = fp->f_fs;
	struct mfs_direct *dp;
	struct mfs_direct *dbuf;
	size_t buf_size;
	entry_t	*names = 0, *n, **np;

	fp->f_seekp = 0;
	while (fp->f_seekp < (off_t)fp->f_di.mdi_size) {
		int rc = buf_read_file(f, &dbuf, &buf_size);
		if (rc)
			goto out;

		/* XXX we assume, that buf_read_file reads an fs block and
		 * doesn't truncate buffer. Currently i_size in MFS doesn't
		 * the same as size of allocated blocks, it makes buf_read_file
		 * to truncate buf_size.
		 */
		if (buf_size < fs->mfs_block_size)
			buf_size = fs->mfs_block_size;

		for (dp = dbuf; dp < &dbuf[NR_DIR_ENTRIES(fs)]; dp++) {
			char *cp;
			int namlen;

			if (fs2h32(dp->mfsd_ino) == 0)
				continue;

			if (pattern && !fnmatch(dp->mfsd_name, pattern))
				continue;

			/* Compute the length of the name,
			 * We don't use strlen and strcpy, because original MFS
			 * code doesn't.
			 */
			cp = memchr(dp->mfsd_name, '\0', sizeof(dp->mfsd_name));
			if (cp == NULL)
				namlen = sizeof(dp->mfsd_name);
			else
				namlen = cp - (dp->mfsd_name);

			n = alloc(sizeof *n + namlen);
			if (!n) {
				printf("%d: %s\n",
					fs2h32(dp->mfsd_ino), dp->mfsd_name);
				continue;
			}
			n->e_ino = fs2h32(dp->mfsd_ino);
			strncpy(n->e_name, dp->mfsd_name, namlen);
			n->e_name[namlen] = '\0';
			for (np = &names; *np; np = &(*np)->e_next) {
				if (strcmp(n->e_name, (*np)->e_name) < 0)
					break;
			}
			n->e_next = *np;
			*np = n;
		}
		fp->f_seekp += buf_size;
	}

	if (names) {
		entry_t *p_names = names;
		do {
			n = p_names;
			if (funcp) {
				/* Call handler for each file instead of
				 * printing. Used by load_mods command.
				 */
				char namebuf[MAXPATHLEN+1];
				namebuf[0] = '\0';
				if (path != pattern) {
					strcpy(namebuf, path);
					namebuf[strlen(path)] = '/';
					namebuf[strlen(path) + 1] = '\0';
				}
				strcat(namebuf, n->e_name);

				funcp(namebuf);
			} else {
				printf("%d: %s\n",
					n->e_ino, n->e_name);
			}
			p_names = n->e_next;
		} while (p_names);
	} else {
		printf("not found\n");
	}
out:
	if (names) {
		do {
			n = names;
			names = n->e_next;
			dealloc(n, 0);
		} while (names);
	}
	return;
}
#endif

/*
 * byte swap functions for big endian machines
 * (mfs is always little endian)
 */

/* These functions are only needed if native byte order is not big endian */
#if BYTE_ORDER == BIG_ENDIAN
void
minixfs3_sb_bswap(struct mfs_sblock *old, struct mfs_sblock *new)
{
	new->mfs_ninodes	=	bswap32(old->mfs_ninodes);
	new->mfs_nzones		=	bswap16(old->mfs_nzones);
	new->mfs_imap_blocks	=	bswap16(old->mfs_imap_blocks);
	new->mfs_zmap_blocks	=	bswap16(old->mfs_zmap_blocks);
	new->mfs_firstdatazone_old =	bswap16(old->mfs_firstdatazone_old);
	new->mfs_log_zone_size	=	bswap16(old->mfs_log_zone_size);
	new->mfs_max_size	=	bswap32(old->mfs_max_size);
	new->mfs_zones		=	bswap32(old->mfs_zones);
	new->mfs_magic		=	bswap16(old->mfs_magic);
	new->mfs_block_size	=	bswap16(old->mfs_block_size);
	new->mfs_disk_version	=	old->mfs_disk_version;
}

void minixfs3_i_bswap(struct mfs_dinode *old, struct mfs_dinode *new)
{
	int i;

	new->mdi_mode		=	bswap16(old->mdi_mode);
	new->mdi_nlinks		=	bswap16(old->mdi_nlinks);
	new->mdi_uid		=	bswap16(old->mdi_uid);
	new->mdi_gid		=	bswap16(old->mdi_gid);
	new->mdi_size		=	bswap32(old->mdi_size);
	new->mdi_atime		=	bswap32(old->mdi_atime);
	new->mdi_mtime		=	bswap32(old->mdi_mtime);
	new->mdi_ctime		=	bswap32(old->mdi_ctime);

	/* We don't swap here, because indirects must be swapped later
	 * anyway, hence everything is done by block_map().
	 */
	for (i = 0; i < NR_TZONES; i++)
		new->mdi_zone[i] = old->mdi_zone[i];
}
#endif
