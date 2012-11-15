/*	$NetBSD: ext2fs.c,v 1.13 2012/05/21 21:34:16 dsl Exp $	*/

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
 *	Stand-alone file reading package for Ext2 file system.
 */

/* #define EXT2FS_DEBUG */

#include <sys/param.h>
#include <sys/time.h>
#include <ufs/ext2fs/ext2fs_dinode.h>
#include <ufs/ext2fs/ext2fs_dir.h>
#include <ufs/ext2fs/ext2fs.h>
#ifdef _STANDALONE
#include <lib/libkern/libkern.h>
#else
#include <string.h>
#endif

#include "stand.h"
#include "ext2fs.h"

#if defined(LIBSA_FS_SINGLECOMPONENT) && !defined(LIBSA_NO_FS_SYMLINK)
#define LIBSA_NO_FS_SYMLINK
#endif

#if defined(LIBSA_NO_TWIDDLE)
#define twiddle()
#endif

#ifndef indp_t
#define indp_t		int32_t
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
	struct m_ext2fs	*f_fs;		/* pointer to super-block */
	struct ext2fs_dinode	f_di;		/* copy of on-disk inode */
	uint		f_nishift;	/* for blocks in indirect block */
	indp_t		f_ind_cache_block;
	indp_t		f_ind_cache[IND_CACHE_SZ];

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
	uint8_t	e_type;
	char	e_name[1];
};

static const char    *const typestr[] = {
	"unknown",
	"REG",
	"DIR",
	"CHR",
	"BLK",
	"FIFO",
	"SOCK",
	"LNK"
};

#endif /* LIBSA_ENABLE_LS_OP */

static int read_inode(ino32_t, struct open_file *);
static int block_map(struct open_file *, indp_t, indp_t *);
static int buf_read_file(struct open_file *, char **, size_t *);
static int search_directory(const char *, int, struct open_file *, ino32_t *);
static int read_sblock(struct open_file *, struct m_ext2fs *);
static int read_gdblock(struct open_file *, struct m_ext2fs *);
#ifdef EXT2FS_DEBUG
static void dump_sblock(struct m_ext2fs *);
#endif

/*
 * Read a new inode into a file structure.
 */
static int
read_inode(ino32_t inumber, struct open_file *f)
{
	struct file *fp = (struct file *)f->f_fsdata;
	struct m_ext2fs *fs = fp->f_fs;
	char *buf;
	size_t rsize;
	int rc;
	daddr_t inode_sector;
	struct ext2fs_dinode *dip;

	inode_sector = FSBTODB(fs, ino_to_fsba(fs, inumber));

	/*
	 * Read inode and save it.
	 */
	buf = fp->f_buf;
	twiddle();
	rc = DEV_STRATEGY(f->f_dev)(f->f_devdata, F_READ,
	    inode_sector, fs->e2fs_bsize, buf, &rsize);
	if (rc)
		return rc;
	if (rsize != fs->e2fs_bsize)
		return EIO;

	dip = (struct ext2fs_dinode *)(buf +
	    EXT2_DINODE_SIZE(fs) * ino_to_fsbo(fs, inumber));
	e2fs_iload(dip, &fp->f_di);

	/*
	 * Clear out the old buffers
	 */
	fp->f_ind_cache_block = ~0;
	fp->f_buf_blkno = -1;
	return rc;
}

/*
 * Given an offset in a file, find the disk block number that
 * contains that block.
 */
static int
block_map(struct open_file *f, indp_t file_block, indp_t *disk_block_p)
{
	struct file *fp = (struct file *)f->f_fsdata;
	struct m_ext2fs *fs = fp->f_fs;
	uint level;
	indp_t ind_cache;
	indp_t ind_block_num;
	size_t rsize;
	int rc;
	indp_t *buf = (void *)fp->f_buf;

	/*
	 * Index structure of an inode:
	 *
	 * e2di_blocks[0..NDADDR-1]
	 *			hold block numbers for blocks
	 *			0..NDADDR-1
	 *
	 * e2di_blocks[NDADDR+0]
	 *			block NDADDR+0 is the single indirect block
	 *			holds block numbers for blocks
	 *			NDADDR .. NDADDR + NINDIR(fs)-1
	 *
	 * e2di_blocks[NDADDR+1]
	 *			block NDADDR+1 is the double indirect block
	 *			holds block numbers for INDEX blocks for blocks
	 *			NDADDR + NINDIR(fs) ..
	 *			NDADDR + NINDIR(fs) + NINDIR(fs)**2 - 1
	 *
	 * e2di_blocks[NDADDR+2]
	 *			block NDADDR+2 is the triple indirect block
	 *			holds block numbers for	double-indirect
	 *			blocks for blocks
	 *			NDADDR + NINDIR(fs) + NINDIR(fs)**2 ..
	 *			NDADDR + NINDIR(fs) + NINDIR(fs)**2
	 *				+ NINDIR(fs)**3 - 1
	 */

	if (file_block < NDADDR) {
		/* Direct block. */
		*disk_block_p = fs2h32(fp->f_di.e2di_blocks[file_block]);
		return 0;
	}

	file_block -= NDADDR;

	ind_cache = file_block >> LN2_IND_CACHE_SZ;
	if (ind_cache == fp->f_ind_cache_block) {
		*disk_block_p =
		    fs2h32(fp->f_ind_cache[file_block & IND_CACHE_MASK]);
		return 0;
	}

	for (level = 0;;) {
		level += fp->f_nishift;
		if (file_block < (indp_t)1 << level)
			break;
		if (level > NIADDR * fp->f_nishift)
			/* Block number too high */
			return EFBIG;
		file_block -= (indp_t)1 << level;
	}

	ind_block_num =
	    fs2h32(fp->f_di.e2di_blocks[NDADDR + (level / fp->f_nishift - 1)]);

	for (;;) {
		level -= fp->f_nishift;
		if (ind_block_num == 0) {
			*disk_block_p = 0;	/* missing */
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
			FSBTODB(fp->f_fs, ind_block_num), fs->e2fs_bsize,
			buf, &rsize);
		if (rc)
			return rc;
		if (rsize != fs->e2fs_bsize)
			return EIO;
		ind_block_num = fs2h32(buf[file_block >> level]);
		if (level == 0)
			break;
		file_block &= (1 << level) - 1;
	}

	/* Save the part of the block that contains this sector */
	memcpy(fp->f_ind_cache, &buf[file_block & ~IND_CACHE_MASK],
	    IND_CACHE_SZ * sizeof fp->f_ind_cache[0]);
	fp->f_ind_cache_block = ind_cache;

	*disk_block_p = ind_block_num;

	return 0;
}

/*
 * Read a portion of a file into an internal buffer.
 * Return the location in the buffer and the amount in the buffer.
 */
static int
buf_read_file(struct open_file *f, char **buf_p, size_t *size_p)
{
	struct file *fp = (struct file *)f->f_fsdata;
	struct m_ext2fs *fs = fp->f_fs;
	long off;
	indp_t file_block;
	indp_t disk_block;
	size_t block_size;
	int rc;

	off = blkoff(fs, fp->f_seekp);
	file_block = lblkno(fs, fp->f_seekp);
	block_size = fs->e2fs_bsize;	/* no fragment */

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
	/* XXX should handle LARGEFILE */
	if (*size_p > fp->f_di.e2di_size - fp->f_seekp)
		*size_p = fp->f_di.e2di_size - fp->f_seekp;

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
	struct ext2fs_direct *dp;
	struct ext2fs_direct *edp;
	char *buf;
	size_t buf_size;
	int namlen;
	int rc;

	fp->f_seekp = 0;
	/* XXX should handle LARGEFILE */
	while (fp->f_seekp < (off_t)fp->f_di.e2di_size) {
		rc = buf_read_file(f, &buf, &buf_size);
		if (rc)
			return rc;

		dp = (struct ext2fs_direct *)buf;
		edp = (struct ext2fs_direct *)(buf + buf_size);
		for (; dp < edp;
		    dp = (void *)((char *)dp + fs2h16(dp->e2d_reclen))) {
			if (fs2h16(dp->e2d_reclen) <= 0)
				break;
			if (fs2h32(dp->e2d_ino) == (ino32_t)0)
				continue;
			namlen = dp->e2d_namlen;
			if (namlen == length &&
			    !memcmp(name, dp->e2d_name, length)) {
				/* found entry */
				*inumber_p = fs2h32(dp->e2d_ino);
				return 0;
			}
		}
		fp->f_seekp += buf_size;
	}
	return ENOENT;
}

int
read_sblock(struct open_file *f, struct m_ext2fs *fs)
{
	static uint8_t sbbuf[SBSIZE];
	struct ext2fs ext2fs;
	size_t buf_size;
	int rc;

	rc = DEV_STRATEGY(f->f_dev)(f->f_devdata, F_READ,
	    SBOFF / DEV_BSIZE, SBSIZE, sbbuf, &buf_size);
	if (rc)
		return rc;

	if (buf_size != SBSIZE)
		return EIO;

	e2fs_sbload((void *)sbbuf, &ext2fs);
	if (ext2fs.e2fs_magic != E2FS_MAGIC)
		return EINVAL;
	if (ext2fs.e2fs_rev > E2FS_REV1 ||
	    (ext2fs.e2fs_rev == E2FS_REV1 &&
	     (ext2fs.e2fs_first_ino != EXT2_FIRSTINO ||
	     (ext2fs.e2fs_inode_size != 128 && ext2fs.e2fs_inode_size != 256) ||
	      ext2fs.e2fs_features_incompat & ~EXT2F_INCOMPAT_SUPP))) {
		return ENODEV;
	}

	e2fs_sbload((void *)sbbuf, &fs->e2fs);
	/* compute in-memory m_ext2fs values */
	fs->e2fs_ncg =
	    howmany(fs->e2fs.e2fs_bcount - fs->e2fs.e2fs_first_dblock,
	    fs->e2fs.e2fs_bpg);
	/* XXX assume hw bsize = 512 */
	fs->e2fs_fsbtodb = fs->e2fs.e2fs_log_bsize + 1;
	fs->e2fs_bsize = MINBSIZE << fs->e2fs.e2fs_log_bsize;
	fs->e2fs_bshift = LOG_MINBSIZE + fs->e2fs.e2fs_log_bsize;
	fs->e2fs_qbmask = fs->e2fs_bsize - 1;
	fs->e2fs_bmask = ~fs->e2fs_qbmask;
	fs->e2fs_ngdb =
	    howmany(fs->e2fs_ncg, fs->e2fs_bsize / sizeof(struct ext2_gd));
	fs->e2fs_ipb = fs->e2fs_bsize / ext2fs.e2fs_inode_size;
	fs->e2fs_itpg = fs->e2fs.e2fs_ipg / fs->e2fs_ipb;

	return 0;
}

int
read_gdblock(struct open_file *f, struct m_ext2fs *fs)
{
	struct file *fp = (struct file *)f->f_fsdata;
	size_t rsize;
	uint gdpb;
	int i, rc;

	gdpb = fs->e2fs_bsize / sizeof(struct ext2_gd);

	for (i = 0; i < fs->e2fs_ngdb; i++) {
		rc = DEV_STRATEGY(f->f_dev)(f->f_devdata, F_READ,
		    FSBTODB(fs, fs->e2fs.e2fs_first_dblock +
		    1 /* superblock */ + i),
		    fs->e2fs_bsize, fp->f_buf, &rsize);
		if (rc)
			return rc;
		if (rsize != fs->e2fs_bsize)
			return EIO;

		e2fs_cgload((struct ext2_gd *)fp->f_buf,
		    &fs->e2fs_gd[i * gdpb],
		    (i == (fs->e2fs_ngdb - 1)) ?
		    (fs->e2fs_ncg - gdpb * i) * sizeof(struct ext2_gd):
		    fs->e2fs_bsize);
	}

	return 0;
}


/*
 * Open a file.
 */
__compactcall int
ext2fs_open(const char *path, struct open_file *f)
{
#ifndef LIBSA_FS_SINGLECOMPONENT
	const char *cp, *ncp;
	int c;
#endif
	ino32_t inumber;
	struct file *fp;
	struct m_ext2fs *fs;
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

#ifdef EXT2FS_DEBUG
	dump_sblock(fs);
#endif

	/* alloc a block sized buffer used for all fs transfers */
	fp->f_buf = alloc(fs->e2fs_bsize);

	/* read group descriptor blocks */
	fs->e2fs_gd = alloc(sizeof(struct ext2_gd) * fs->e2fs_ncg);
	rc = read_gdblock(f, fs);
	if (rc)
		goto out;

	/*
	 * Calculate indirect block levels.
	 */
	{
		indp_t mult;
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

	inumber = EXT2_ROOTINO;
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
		if ((fp->f_di.e2di_mode & EXT2_IFMT) != EXT2_IFDIR) {
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
		if ((fp->f_di.e2di_mode & EXT2_IFMT) == EXT2_IFLNK) {
			/* XXX should handle LARGEFILE */
			int link_len = fp->f_di.e2di_size;
			int len;

			len = strlen(cp);

			if (link_len + len > MAXPATHLEN ||
			    ++nlinks > MAXSYMLINKS) {
				rc = ENOENT;
				goto out;
			}

			memmove(&namebuf[link_len], cp, len + 1);

			if (link_len < EXT2_MAXSYMLINKLEN) {
				memcpy(namebuf, fp->f_di.e2di_blocks, link_len);
			} else {
				/*
				 * Read file for symbolic link
				 */
				size_t buf_size;
				indp_t	disk_block;

				buf = fp->f_buf;
				rc = block_map(f, (indp_t)0, &disk_block);
				if (rc)
					goto out;

				twiddle();
				rc = DEV_STRATEGY(f->f_dev)(f->f_devdata,
					F_READ, FSBTODB(fs, disk_block),
					fs->e2fs_bsize, buf, &buf_size);
				if (rc)
					goto out;

				memcpy(namebuf, buf, link_len);
			}

			/*
			 * If relative pathname, restart at parent directory.
			 * If absolute pathname, restart at root.
			 */
			cp = namebuf;
			if (*cp != '/')
				inumber = parent_inumber;
			else
				inumber = (ino32_t)EXT2_ROOTINO;

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
		ext2fs_close(f);
	else {
		fsmod = "ext2fs";
		fsmod2 = "ffs";
	}
	return rc;
}

__compactcall int
ext2fs_close(struct open_file *f)
{
	struct file *fp = (struct file *)f->f_fsdata;

	f->f_fsdata = NULL;
	if (fp == NULL)
		return 0;

	if (fp->f_fs->e2fs_gd)
		dealloc(fp->f_fs->e2fs_gd,
		    sizeof(struct ext2_gd) * fp->f_fs->e2fs_ncg);
	if (fp->f_buf)
		dealloc(fp->f_buf, fp->f_fs->e2fs_bsize);
	dealloc(fp->f_fs, sizeof(*fp->f_fs));
	dealloc(fp, sizeof(struct file));
	return 0;
}

/*
 * Copy a portion of a file into kernel memory.
 * Cross block boundaries when necessary.
 */
__compactcall int
ext2fs_read(struct open_file *f, void *start, size_t size, size_t *resid)
{
	struct file *fp = (struct file *)f->f_fsdata;
	size_t csize;
	char *buf;
	size_t buf_size;
	int rc = 0;
	char *addr = start;

	while (size != 0) {
		/* XXX should handle LARGEFILE */
		if (fp->f_seekp >= (off_t)fp->f_di.e2di_size)
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
ext2fs_write(struct open_file *f, void *start, size_t size, size_t *resid)
{

	return EROFS;
}
#endif /* !LIBSA_NO_FS_WRITE */

#ifndef LIBSA_NO_FS_SEEK
__compactcall off_t
ext2fs_seek(struct open_file *f, off_t offset, int where)
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
		/* XXX should handle LARGEFILE */
		fp->f_seekp = fp->f_di.e2di_size - offset;
		break;
	default:
		return -1;
	}
	return fp->f_seekp;
}
#endif /* !LIBSA_NO_FS_SEEK */

__compactcall int
ext2fs_stat(struct open_file *f, struct stat *sb)
{
	struct file *fp = (struct file *)f->f_fsdata;

	/* only important stuff */
	memset(sb, 0, sizeof *sb);
	sb->st_mode = fp->f_di.e2di_mode;
	sb->st_uid = fp->f_di.e2di_uid;
	sb->st_gid = fp->f_di.e2di_gid;
	/* XXX should handle LARGEFILE */
	sb->st_size = fp->f_di.e2di_size;
	return 0;
}

#if defined(LIBSA_ENABLE_LS_OP)
__compactcall void
ext2fs_ls(struct open_file *f, const char *pattern,
		void (*funcp)(char* arg), char* path)
{
	struct file *fp = (struct file *)f->f_fsdata;
	size_t block_size = fp->f_fs->e2fs_bsize;
	char *buf;
	size_t buf_size;
	entry_t	*names = 0, *n, **np;

	fp->f_seekp = 0;
	while (fp->f_seekp < (off_t)fp->f_di.e2di_size) {
		struct ext2fs_direct  *dp, *edp;
		int rc = buf_read_file(f, &buf, &buf_size);
		if (rc)
			goto out;
		if (buf_size != block_size || buf_size == 0)
			goto out;

		dp = (struct ext2fs_direct *)buf;
		edp = (struct ext2fs_direct *)(buf + buf_size);

		for (; dp < edp;
		     dp = (void *)((char *)dp + fs2h16(dp->e2d_reclen))) {
			const char *t;

			if (fs2h16(dp->e2d_reclen) <= 0)
				goto out;

			if (fs2h32(dp->e2d_ino) == 0)
				continue;

			if (dp->e2d_type >= NELEM(typestr) ||
			    !(t = typestr[dp->e2d_type])) {
				/*
				 * This does not handle "old"
				 * filesystems properly. On little
				 * endian machines, we get a bogus
				 * type name if the namlen matches a
				 * valid type identifier. We could
				 * check if we read namlen "0" and
				 * handle this case specially, if
				 * there were a pressing need...
				 */
				printf("bad dir entry\n");
				goto out;
			}
			if (pattern && !fnmatch(dp->e2d_name, pattern))
				continue;
			n = alloc(sizeof *n + strlen(dp->e2d_name));
			if (!n) {
				printf("%d: %s (%s)\n",
					fs2h32(dp->e2d_ino), dp->e2d_name, t);
				continue;
			}
			n->e_ino = fs2h32(dp->e2d_ino);
			n->e_type = dp->e2d_type;
			strcpy(n->e_name, dp->e2d_name);
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
			printf("%d: %s (%s)\n",
				n->e_ino, n->e_name, typestr[n->e_type]);
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
 * (ext2fs is always little endian)
 *
 * XXX: We should use src/sys/ufs/ext2fs/ext2fs_bswap.c
 */

/* These functions are only needed if native byte order is not big endian */
#if BYTE_ORDER == BIG_ENDIAN
void
e2fs_sb_bswap(struct ext2fs *old, struct ext2fs *new)
{

	/* preserve unused fields */
	memcpy(new, old, sizeof(struct ext2fs));
	new->e2fs_icount	=	bswap32(old->e2fs_icount);
	new->e2fs_bcount	=	bswap32(old->e2fs_bcount);
	new->e2fs_rbcount	=	bswap32(old->e2fs_rbcount);
	new->e2fs_fbcount	=	bswap32(old->e2fs_fbcount);
	new->e2fs_ficount	=	bswap32(old->e2fs_ficount);
	new->e2fs_first_dblock	=	bswap32(old->e2fs_first_dblock);
	new->e2fs_log_bsize	=	bswap32(old->e2fs_log_bsize);
	new->e2fs_fsize		=	bswap32(old->e2fs_fsize);
	new->e2fs_bpg		=	bswap32(old->e2fs_bpg);
	new->e2fs_fpg		=	bswap32(old->e2fs_fpg);
	new->e2fs_ipg		=	bswap32(old->e2fs_ipg);
	new->e2fs_mtime		=	bswap32(old->e2fs_mtime);
	new->e2fs_wtime		=	bswap32(old->e2fs_wtime);
	new->e2fs_mnt_count	=	bswap16(old->e2fs_mnt_count);
	new->e2fs_max_mnt_count	=	bswap16(old->e2fs_max_mnt_count);
	new->e2fs_magic		=	bswap16(old->e2fs_magic);
	new->e2fs_state		=	bswap16(old->e2fs_state);
	new->e2fs_beh		=	bswap16(old->e2fs_beh);
	new->e2fs_minrev	=	bswap16(old->e2fs_minrev);
	new->e2fs_lastfsck	=	bswap32(old->e2fs_lastfsck);
	new->e2fs_fsckintv	=	bswap32(old->e2fs_fsckintv);
	new->e2fs_creator	=	bswap32(old->e2fs_creator);
	new->e2fs_rev		=	bswap32(old->e2fs_rev);
	new->e2fs_ruid		=	bswap16(old->e2fs_ruid);
	new->e2fs_rgid		=	bswap16(old->e2fs_rgid);
	new->e2fs_first_ino	=	bswap32(old->e2fs_first_ino);
	new->e2fs_inode_size	=	bswap16(old->e2fs_inode_size);
	new->e2fs_block_group_nr =	bswap16(old->e2fs_block_group_nr);
	new->e2fs_features_compat =	bswap32(old->e2fs_features_compat);
	new->e2fs_features_incompat =	bswap32(old->e2fs_features_incompat);
	new->e2fs_features_rocompat =	bswap32(old->e2fs_features_rocompat);
	new->e2fs_algo		=	bswap32(old->e2fs_algo);
	new->e2fs_reserved_ngdb	=	bswap16(old->e2fs_reserved_ngdb);
}

void e2fs_cg_bswap(struct ext2_gd *old, struct ext2_gd *new, int size)
{
	int i;

	for (i = 0; i < (size / sizeof(struct ext2_gd)); i++) {
		new[i].ext2bgd_b_bitmap	= bswap32(old[i].ext2bgd_b_bitmap);
		new[i].ext2bgd_i_bitmap	= bswap32(old[i].ext2bgd_i_bitmap);
		new[i].ext2bgd_i_tables	= bswap32(old[i].ext2bgd_i_tables);
		new[i].ext2bgd_nbfree	= bswap16(old[i].ext2bgd_nbfree);
		new[i].ext2bgd_nifree	= bswap16(old[i].ext2bgd_nifree);
		new[i].ext2bgd_ndirs	= bswap16(old[i].ext2bgd_ndirs);
	}
}

void e2fs_i_bswap(struct ext2fs_dinode *old, struct ext2fs_dinode *new)
{

	new->e2di_mode		=	bswap16(old->e2di_mode);
	new->e2di_uid		=	bswap16(old->e2di_uid);
	new->e2di_gid		=	bswap16(old->e2di_gid);
	new->e2di_nlink		=	bswap16(old->e2di_nlink);
	new->e2di_size		=	bswap32(old->e2di_size);
	new->e2di_atime		=	bswap32(old->e2di_atime);
	new->e2di_ctime		=	bswap32(old->e2di_ctime);
	new->e2di_mtime		=	bswap32(old->e2di_mtime);
	new->e2di_dtime		=	bswap32(old->e2di_dtime);
	new->e2di_nblock	=	bswap32(old->e2di_nblock);
	new->e2di_flags		=	bswap32(old->e2di_flags);
	new->e2di_gen		=	bswap32(old->e2di_gen);
	new->e2di_facl		=	bswap32(old->e2di_facl);
	new->e2di_dacl		=	bswap32(old->e2di_dacl);
	new->e2di_faddr		=	bswap32(old->e2di_faddr);
	memcpy(&new->e2di_blocks[0], &old->e2di_blocks[0],
	    (NDADDR + NIADDR) * sizeof(uint32_t));
}
#endif

#ifdef EXT2FS_DEBUG
void
dump_sblock(struct m_ext2fs *fs)
{

	printf("fs->e2fs.e2fs_bcount = %u\n", fs->e2fs.e2fs_bcount);
	printf("fs->e2fs.e2fs_first_dblock = %u\n", fs->e2fs.e2fs_first_dblock);
	printf("fs->e2fs.e2fs_log_bsize = %u\n", fs->e2fs.e2fs_log_bsize);
	printf("fs->e2fs.e2fs_bpg = %u\n", fs->e2fs.e2fs_bpg);
	printf("fs->e2fs.e2fs_ipg = %u\n", fs->e2fs.e2fs_ipg);
	printf("fs->e2fs.e2fs_magic = 0x%x\n", fs->e2fs.e2fs_magic);
	printf("fs->e2fs.e2fs_rev = %u\n", fs->e2fs.e2fs_rev);

	if (fs->e2fs.e2fs_rev == E2FS_REV1) {
		printf("fs->e2fs.e2fs_first_ino = %u\n",
		    fs->e2fs.e2fs_first_ino);
		printf("fs->e2fs.e2fs_inode_size = %u\n",
		    fs->e2fs.e2fs_inode_size);
		printf("fs->e2fs.e2fs_features_compat = %u\n",
		    fs->e2fs.e2fs_features_compat);
		printf("fs->e2fs.e2fs_features_incompat = %u\n",
		    fs->e2fs.e2fs_features_incompat);
		printf("fs->e2fs.e2fs_features_rocompat = %u\n",
		    fs->e2fs.e2fs_features_rocompat);
		printf("fs->e2fs.e2fs_reserved_ngdb = %u\n",
		    fs->e2fs.e2fs_reserved_ngdb);
	}

	printf("fs->e2fs_bsize = %u\n", fs->e2fs_bsize);
	printf("fs->e2fs_fsbtodb = %u\n", fs->e2fs_fsbtodb);
	printf("fs->e2fs_ncg = %u\n", fs->e2fs_ncg);
	printf("fs->e2fs_ngdb = %u\n", fs->e2fs_ngdb);
	printf("fs->e2fs_ipb = %u\n", fs->e2fs_ipb);
	printf("fs->e2fs_itpg = %u\n", fs->e2fs_itpg);
}
#endif
