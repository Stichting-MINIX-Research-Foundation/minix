/*	$NetBSD: ufs.c,v 1.58 2012/05/21 21:34:16 dsl Exp $	*/

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
 *	Stand-alone file reading package for UFS and LFS filesystems.
 */

#include <sys/param.h>
#include <sys/time.h>
#include <ufs/ufs/dinode.h>
#include <ufs/ufs/dir.h>
#ifdef LIBSA_LFS
#include <sys/queue.h>
#include <sys/condvar.h>
#include <sys/mount.h>			/* XXX for MNAMELEN */
#include <ufs/lfs/lfs.h>
#else
#include <ufs/ffs/fs.h>
#endif
#ifdef _STANDALONE
#include <lib/libkern/libkern.h>
#else
#include <string.h>
#endif

#include "stand.h"
#ifdef LIBSA_LFS
#include "lfs.h"
#else
#include "ufs.h"
#endif

/* If this file is compiled by itself, build ufs (aka ffsv1) support */
#if !defined(LIBSA_FFSv2) && !defined(LIBSA_LFS)
#define LIBSA_FFSv1
#endif

#if defined(LIBSA_FS_SINGLECOMPONENT) && !defined(LIBSA_NO_FS_SYMLINK)
#define LIBSA_NO_FS_SYMLINK
#endif
#if defined(COMPAT_UFS) && defined(LIBSA_NO_COMPAT_UFS)
#undef COMPAT_UFS
#endif

#ifdef LIBSA_LFS
/*
 * In-core LFS superblock.  This exists only to placate the macros in lfs.h,
 */
struct fs {
	struct dlfs	lfs_dlfs;
};
#define fs_magic	lfs_magic
#define fs_maxsymlinklen lfs_maxsymlinklen

#define FS_MAGIC	LFS_MAGIC
#define SBLOCKSIZE	LFS_SBPAD
#define SBLOCKOFFSET	LFS_LABELPAD
#else
/* NB ufs2 doesn't use the common suberblock code... */
#define FS_MAGIC	FS_UFS1_MAGIC
#define SBLOCKOFFSET	SBLOCK_UFS1
#endif

#if defined(LIBSA_NO_TWIDDLE)
#define twiddle()
#endif

#undef cgstart
#if defined(LIBSA_FFSv2)
#define cgstart(fc, c) cgstart_ufs2((fs), (c))
#else
#define cgstart(fc, c) cgstart_ufs1((fs), (c))
#endif

#ifndef ufs_dinode
#define ufs_dinode	ufs1_dinode
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
	struct fs	*f_fs;		/* pointer to super-block */
	struct ufs_dinode	f_di;		/* copy of on-disk inode */
	uint		f_nishift;	/* for blocks in indirect block */
	indp_t		f_ind_cache_block;
	indp_t		f_ind_cache[IND_CACHE_SZ];

	char		*f_buf;		/* buffer for data block */
	size_t		f_buf_size;	/* size of data block */
	daddr_t		f_buf_blkno;	/* block number of data block */
};

static int read_inode(ino32_t, struct open_file *);
static int block_map(struct open_file *, indp_t, indp_t *);
static int buf_read_file(struct open_file *, char **, size_t *);
static int search_directory(const char *, int, struct open_file *, ino32_t *);
#ifdef LIBSA_FFSv1
static void ffs_oldfscompat(struct fs *);
#endif
#ifdef LIBSA_FFSv2
static int ffs_find_superblock(struct open_file *, struct fs *);
#endif

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
	"FIFO",
	"CHR",
	0,
	"DIR",
	0,
	"BLK",
	0,
	"REG",
	0,
	"LNK",
	0,
	"SOCK",
	0,
	"WHT"
};
#endif /* LIBSA_ENABLE_LS_OP */

#ifdef LIBSA_LFS
/*
 * Find an inode's block.  Look it up in the ifile.  Whee!
 */
static int
find_inode_sector(ino32_t inumber, struct open_file *f, daddr_t *isp)
{
	struct file *fp = (struct file *)f->f_fsdata;
	struct fs *fs = fp->f_fs;
	daddr_t ifileent_blkno;
	char *ent_in_buf;
	size_t buf_after_ent;
	int rc;

	rc = read_inode(fs->lfs_ifile, f);
	if (rc)
		return rc;

	ifileent_blkno =
	    (inumber / fs->lfs_ifpb) + fs->lfs_cleansz + fs->lfs_segtabsz;
	fp->f_seekp = (off_t)ifileent_blkno * fs->fs_bsize +
	    (inumber % fs->lfs_ifpb) * sizeof (IFILE_Vx);
	rc = buf_read_file(f, &ent_in_buf, &buf_after_ent);
	if (rc)
		return rc;
	/* make sure something's not badly wrong, but don't panic. */
	if (buf_after_ent < sizeof (IFILE_Vx))
		return EINVAL;

	*isp = FSBTODB(fs, ((IFILE_Vx *)ent_in_buf)->if_daddr);
	if (*isp == LFS_UNUSED_DADDR)	/* again, something badly wrong */
		return EINVAL;
	return 0;
}
#endif

/*
 * Read a new inode into a file structure.
 */
static int
read_inode(ino32_t inumber, struct open_file *f)
{
	struct file *fp = (struct file *)f->f_fsdata;
	struct fs *fs = fp->f_fs;
	char *buf;
	size_t rsize;
	int rc;
	daddr_t inode_sector;
#ifdef LIBSA_LFS
	struct ufs_dinode *dip;
	int cnt;
#endif

#ifdef LIBSA_LFS
	if (inumber == fs->lfs_ifile)
		inode_sector = FSBTODB(fs, fs->lfs_idaddr);
	else if ((rc = find_inode_sector(inumber, f, &inode_sector)) != 0)
		return rc;
#else
	inode_sector = FSBTODB(fs, ino_to_fsba(fs, inumber));
#endif

	/*
	 * Read inode and save it.
	 */
	buf = fp->f_buf;
	twiddle();
	rc = DEV_STRATEGY(f->f_dev)(f->f_devdata, F_READ,
	    inode_sector, fs->fs_bsize, buf, &rsize);
	if (rc)
		return rc;
	if (rsize != fs->fs_bsize)
		return EIO;

#ifdef LIBSA_LFS
	cnt = INOPBx(fs);
	dip = (struct ufs_dinode *)buf + (cnt - 1);
	for (; dip->di_inumber != inumber; --dip) {
		/* kernel code panics, but boot blocks which panic are Bad. */
		if (--cnt == 0)
			return EINVAL;
	}
	fp->f_di = *dip;
#else
	fp->f_di = ((struct ufs_dinode *)buf)[ino_to_fsbo(fs, inumber)];
#endif

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
	struct fs *fs = fp->f_fs;
	uint level;
	indp_t ind_cache;
	indp_t ind_block_num;
	size_t rsize;
	int rc;
	indp_t *buf = (void *)fp->f_buf;

	/*
	 * Index structure of an inode:
	 *
	 * di_db[0..NDADDR-1]	hold block numbers for blocks
	 *			0..NDADDR-1
	 *
	 * di_ib[0]		index block 0 is the single indirect block
	 *			holds block numbers for blocks
	 *			NDADDR .. NDADDR + NINDIR(fs)-1
	 *
	 * di_ib[1]		index block 1 is the double indirect block
	 *			holds block numbers for INDEX blocks for blocks
	 *			NDADDR + NINDIR(fs) ..
	 *			NDADDR + NINDIR(fs) + NINDIR(fs)**2 - 1
	 *
	 * di_ib[2]		index block 2 is the triple indirect block
	 *			holds block numbers for double-indirect
	 *			blocks for blocks
	 *			NDADDR + NINDIR(fs) + NINDIR(fs)**2 ..
	 *			NDADDR + NINDIR(fs) + NINDIR(fs)**2
	 *				+ NINDIR(fs)**3 - 1
	 */

	if (file_block < NDADDR) {
		/* Direct block. */
		*disk_block_p = fp->f_di.di_db[file_block];
		return 0;
	}

	file_block -= NDADDR;

	ind_cache = file_block >> LN2_IND_CACHE_SZ;
	if (ind_cache == fp->f_ind_cache_block) {
		*disk_block_p = fp->f_ind_cache[file_block & IND_CACHE_MASK];
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

	ind_block_num = fp->f_di.di_ib[level / fp->f_nishift - 1];

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
			FSBTODB(fp->f_fs, ind_block_num), fs->fs_bsize,
			buf, &rsize);
		if (rc)
			return rc;
		if (rsize != fs->fs_bsize)
			return EIO;
		ind_block_num = buf[file_block >> level];
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
	struct fs *fs = fp->f_fs;
	long off;
	indp_t file_block;
	indp_t disk_block;
	size_t block_size;
	int rc;

	off = blkoff(fs, fp->f_seekp);
	file_block = lblkno(fs, fp->f_seekp);
#ifdef LIBSA_LFS
	block_size = dblksize(fs, &fp->f_di, file_block);
#else
	block_size = sblksize(fs, (int64_t)fp->f_di.di_size, file_block);
#endif

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
	if (*size_p > fp->f_di.di_size - fp->f_seekp)
		*size_p = fp->f_di.di_size - fp->f_seekp;

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
	struct direct *dp;
	struct direct *edp;
	char *buf;
	size_t buf_size;
	int namlen;
	int rc;

	fp->f_seekp = 0;
	while (fp->f_seekp < (off_t)fp->f_di.di_size) {
		rc = buf_read_file(f, &buf, &buf_size);
		if (rc)
			return rc;

		dp = (struct direct *)buf;
		edp = (struct direct *)(buf + buf_size);
		for (;dp < edp; dp = (void *)((char *)dp + dp->d_reclen)) {
			if (dp->d_reclen <= 0)
				break;
			if (dp->d_ino == (ino32_t)0)
				continue;
#if BYTE_ORDER == LITTLE_ENDIAN
			if (fp->f_fs->fs_maxsymlinklen <= 0)
				namlen = dp->d_type;
			else
#endif
				namlen = dp->d_namlen;
			if (namlen == length &&
			    !memcmp(name, dp->d_name, length)) {
				/* found entry */
				*inumber_p = dp->d_ino;
				return 0;
			}
		}
		fp->f_seekp += buf_size;
	}
	return ENOENT;
}

#ifdef LIBSA_FFSv2

daddr_t sblock_try[] = SBLOCKSEARCH;

static int
ffs_find_superblock(struct open_file *f, struct fs *fs)
{
	int i, rc;
	size_t buf_size;

	for (i = 0; sblock_try[i] != -1; i++) {
		rc = DEV_STRATEGY(f->f_dev)(f->f_devdata, F_READ,
		    sblock_try[i] / DEV_BSIZE, SBLOCKSIZE, fs, &buf_size);
		if (rc != 0 || buf_size != SBLOCKSIZE)
			return rc;
		if (fs->fs_sblockloc != sblock_try[i])
			/* an alternate superblock - try again */
			continue;
		if (fs->fs_magic == FS_UFS2_MAGIC) {
			return 0;
		}
	}
	return EINVAL;
}

#endif

/*
 * Open a file.
 */
__compactcall int
ufs_open(const char *path, struct open_file *f)
{
#ifndef LIBSA_FS_SINGLECOMPONENT
	const char *cp, *ncp;
	int c;
#endif
	ino32_t inumber;
	struct file *fp;
	struct fs *fs;
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
	fs = alloc(SBLOCKSIZE);
	fp->f_fs = fs;
	twiddle();

#ifdef LIBSA_FFSv2
	rc = ffs_find_superblock(f, fs);
	if (rc)
		goto out;
#else
	{
		size_t buf_size;
		rc = DEV_STRATEGY(f->f_dev)(f->f_devdata, F_READ,
			SBLOCKOFFSET / DEV_BSIZE, SBLOCKSIZE, fs, &buf_size);
		if (rc)
			goto out;
		if (buf_size != SBLOCKSIZE ||
#ifdef LIBSA_FFS
		    fs->lfs_version != REQUIRED_LFS_VERSION ||
#endif
		    fs->fs_magic != FS_MAGIC) {
			rc = EINVAL;
			goto out;
		}
	}
#if defined(LIBSA_LFS) && REQUIRED_LFS_VERSION == 2
	/*
	 * XXX	We should check the second superblock and use the eldest
	 *	of the two.  See comments near the top of lfs_mountfs()
	 *	in sys/ufs/lfs/lfs_vfsops.c.
	 *      This may need a LIBSA_LFS_SMALL check as well.
	 */
#endif
#endif

#ifdef LIBSA_FFSv1
	ffs_oldfscompat(fs);
#endif

	if (fs->fs_bsize > MAXBSIZE ||
	    (size_t)fs->fs_bsize < sizeof(struct fs)) {
		rc = EINVAL;
		goto out;
	}

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
		if (mult & (mult - 1)) {
			/* Hummm was't a power of 2 */
			rc = EINVAL;
			goto out;
		}
#endif
		for (ln2 = 0; mult != 1; ln2++)
			mult >>= 1;

		fp->f_nishift = ln2;
	}

	/* alloc a block sized buffer used for all fs transfers */
	fp->f_buf = alloc(fs->fs_bsize);
	inumber = ROOTINO;
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
		if ((fp->f_di.di_mode & IFMT) != IFDIR) {
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
		if ((fp->f_di.di_mode & IFMT) == IFLNK) {
			int link_len = fp->f_di.di_size;
			int len;

			len = strlen(cp);

			if (link_len + len > MAXPATHLEN ||
			    ++nlinks > MAXSYMLINKS) {
				rc = ENOENT;
				goto out;
			}

			memmove(&namebuf[link_len], cp, len + 1);

			if (link_len < fs->fs_maxsymlinklen) {
				memcpy(namebuf, fp->f_di.di_db, link_len);
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
					fs->fs_bsize, buf, &buf_size);
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
				inumber = (ino32_t)ROOTINO;

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
		ufs_close(f);
	else {
#ifdef FSMOD
		fsmod = FSMOD;
#endif
#ifdef FSMOD2
		fsmod2 = FSMOD2;
#endif
	}
	return rc;
}

__compactcall int
ufs_close(struct open_file *f)
{
	struct file *fp = (struct file *)f->f_fsdata;

	f->f_fsdata = NULL;
	if (fp == NULL)
		return 0;

	if (fp->f_buf)
		dealloc(fp->f_buf, fp->f_fs->fs_bsize);
	dealloc(fp->f_fs, SBLOCKSIZE);
	dealloc(fp, sizeof(struct file));
	return 0;
}

/*
 * Copy a portion of a file into kernel memory.
 * Cross block boundaries when necessary.
 */
__compactcall int
ufs_read(struct open_file *f, void *start, size_t size, size_t *resid)
{
	struct file *fp = (struct file *)f->f_fsdata;
	size_t csize;
	char *buf;
	size_t buf_size;
	int rc = 0;
	char *addr = start;

	while (size != 0) {
		if (fp->f_seekp >= (off_t)fp->f_di.di_size)
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
ufs_write(struct open_file *f, void *start, size_t size, size_t *resid)
{

	return EROFS;
}
#endif /* !LIBSA_NO_FS_WRITE */

#ifndef LIBSA_NO_FS_SEEK
__compactcall off_t
ufs_seek(struct open_file *f, off_t offset, int where)
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
		fp->f_seekp = fp->f_di.di_size - offset;
		break;
	default:
		return -1;
	}
	return fp->f_seekp;
}
#endif /* !LIBSA_NO_FS_SEEK */

__compactcall int
ufs_stat(struct open_file *f, struct stat *sb)
{
	struct file *fp = (struct file *)f->f_fsdata;

	/* only important stuff */
	memset(sb, 0, sizeof *sb);
	sb->st_mode = fp->f_di.di_mode;
	sb->st_uid = fp->f_di.di_uid;
	sb->st_gid = fp->f_di.di_gid;
	sb->st_size = fp->f_di.di_size;
	return 0;
}

#if defined(LIBSA_ENABLE_LS_OP)
__compactcall void
ufs_ls(struct open_file *f, const char *pattern,
	void (*funcp)(char* arg), char* path)
{
	struct file *fp = (struct file *)f->f_fsdata;
	char *buf;
	size_t buf_size;
	entry_t	*names = 0, *n, **np;

	fp->f_seekp = 0;
	while (fp->f_seekp < (off_t)fp->f_di.di_size) {
		struct direct  *dp, *edp;
		int rc = buf_read_file(f, &buf, &buf_size);
		if (rc)
			goto out;
		/* some firmware might use block size larger than DEV_BSIZE */
		if (buf_size < DIRBLKSIZ)
			goto out;

		dp = (struct direct *)buf;
		edp = (struct direct *)(buf + buf_size);

		for (; dp < edp; dp = (void *)((char *)dp + dp->d_reclen)) {
			const char *t;
			if (dp->d_ino ==  0)
				continue;

			if (dp->d_type >= NELEM(typestr) ||
			    !(t = typestr[dp->d_type])) {
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
			if (pattern && !fnmatch(dp->d_name, pattern))
				continue;
			n = alloc(sizeof *n + strlen(dp->d_name));
			if (!n) {
				printf("%d: %s (%s)\n",
					dp->d_ino, dp->d_name, t);
				continue;
			}
			n->e_ino = dp->d_ino;
			n->e_type = dp->d_type;
			strcpy(n->e_name, dp->d_name);
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
}
#endif /* LIBSA_ENABLE_LS_OP */

#ifdef LIBSA_FFSv1
/*
 * Sanity checks for old file systems.
 *
 * XXX - goes away some day.
 * Stripped of stuff libsa doesn't need.....
 */
static void
ffs_oldfscompat(struct fs *fs)
{

#ifdef COMPAT_UFS
	/*
	 * Newer Solaris versions have a slightly incompatible
	 * superblock - so always calculate this values on the fly, which
	 * is good enough for libsa purposes
	 */
	if (fs->fs_magic == FS_UFS1_MAGIC
#ifndef COMPAT_SOLARIS_UFS
	    && fs->fs_old_inodefmt < FS_44INODEFMT
#endif
	    ) {
		fs->fs_qbmask = ~fs->fs_bmask;
		fs->fs_qfmask = ~fs->fs_fmask;
	}
#endif
}
#endif
