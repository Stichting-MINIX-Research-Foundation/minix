/*	$NetBSD: cd9660.c,v 1.30 2014/03/20 03:13:18 christos Exp $	*/

/*
 * Copyright (C) 1996 Wolfgang Solfrank.
 * Copyright (C) 1996 TooLs GmbH.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Stand-alone ISO9660 file reading package.
 *
 * Note: This doesn't support Rock Ridge extensions, extended attributes,
 * blocksizes other than 2048 bytes, multi-extent files, etc.
 */
#include <sys/param.h>
#ifdef _STANDALONE
#include <lib/libkern/libkern.h>
#else
#include <string.h>
#endif
#include <fs/cd9660/iso.h>

#include "stand.h"
#include "cd9660.h"

/*
 * XXX Does not currently implement:
 * XXX
 * XXX LIBSA_NO_FS_SYMLINK (does this even make sense?)
 * XXX LIBSA_FS_SINGLECOMPONENT
 */

struct file {
	off_t off;			/* Current offset within file */
	daddr_t bno;			/* Starting block number  */
	off_t size;			/* Size of file */
};

struct ptable_ent {
	char namlen	[ISODCL( 1, 1)];	/* 711 */
	char extlen	[ISODCL( 2, 2)];	/* 711 */
	char block	[ISODCL( 3, 6)];	/* 732 */
	char parent	[ISODCL( 7, 8)];	/* 722 */
	char name	[1];
};
#define	PTFIXSZ		8
#define	PTSIZE(pp)	roundup(PTFIXSZ + isonum_711((pp)->namlen), 2)

#define	cdb2devb(bno)	((bno) * ISO_DEFAULT_BLOCK_SIZE / DEV_BSIZE)

static int	pnmatch(const char *, struct ptable_ent *);
static int	dirmatch(const char *, struct iso_directory_record *);

static int
pnmatch(const char *path, struct ptable_ent *pp)
{
	char *cp;
	int i;

	cp = pp->name;
	for (i = isonum_711(pp->namlen); --i >= 0; path++, cp++) {
		if (toupper(*path) == *cp)
			continue;
		return 0;
	}
	if (*path != '/')
		return 0;
	return 1;
}

static int
dirmatch(const char *path, struct iso_directory_record *dp)
{
	char *cp;
	int i;

	/* This needs to be a regular file */
	if (dp->flags[0] & 6)
		return 0;

	cp = dp->name;
	for (i = isonum_711(dp->name_len); --i >= 0; path++, cp++) {
		if (!*path)
			break;
		if (toupper(*path) == *cp)
			continue;
		return 0;
	}
	if (*path)
		return 0;
	/*
	 * Allow stripping of trailing dots and the version number.
	 * Note that this will find the first instead of the last version
	 * of a file.
	 */
	if (i >= 0 && (*cp == ';' || *cp == '.')) {
		/* This is to prevent matching of numeric extensions */
		if (*cp == '.' && cp[1] != ';')
			return 0;
		while (--i >= 0)
			if (*++cp != ';' && (*cp < '0' || *cp > '9'))
				return 0;
	}
	return 1;
}

__compactcall int
cd9660_open(const char *path, struct open_file *f)
{
	struct file *fp = 0;
	void *buf;
	struct iso_primary_descriptor *vd;
	size_t buf_size, nread, psize, dsize;
	daddr_t bno;
	int parent, ent;
	struct ptable_ent *pp;
	struct iso_directory_record *dp = 0;
	int rc;

	/* First find the volume descriptor */
	buf_size = ISO_DEFAULT_BLOCK_SIZE;
	buf = alloc(buf_size);
	vd = buf;
	for (bno = 16;; bno++) {
#if !defined(LIBSA_NO_TWIDDLE)
		twiddle();
#endif
		rc = DEV_STRATEGY(f->f_dev)(f->f_devdata, F_READ, cdb2devb(bno),
					   ISO_DEFAULT_BLOCK_SIZE, buf, &nread);
		if (rc)
			goto out;
		if (nread != ISO_DEFAULT_BLOCK_SIZE) {
			rc = EIO;
			goto out;
		}
		rc = EINVAL;
		if (memcmp(vd->id, ISO_STANDARD_ID, sizeof vd->id) != 0)
			goto out;
		if (isonum_711(vd->type) == ISO_VD_END)
			goto out;
		if (isonum_711(vd->type) == ISO_VD_PRIMARY)
			break;
	}
	if (isonum_723(vd->logical_block_size) != ISO_DEFAULT_BLOCK_SIZE)
		goto out;

	/* Now get the path table and lookup the directory of the file */
	bno = isonum_732(vd->type_m_path_table);
	psize = isonum_733(vd->path_table_size);

	if (psize > ISO_DEFAULT_BLOCK_SIZE) {
		dealloc(buf, ISO_DEFAULT_BLOCK_SIZE);
		buf = alloc(buf_size = roundup(psize, ISO_DEFAULT_BLOCK_SIZE));
	}

#if !defined(LIBSA_NO_TWIDDLE)
	twiddle();
#endif
	rc = DEV_STRATEGY(f->f_dev)(f->f_devdata, F_READ, cdb2devb(bno),
	                           buf_size, buf, &nread);
	if (rc)
		goto out;
	if (nread != buf_size) {
		rc = EIO;
		goto out;
	}

	parent = 1;
	pp = (struct ptable_ent *)buf;
	ent = 1;
	bno = isonum_732(pp->block) + isonum_711(pp->extlen);

	rc = ENOENT;

	while (*path) {
		/*
		 * Remove extra separators
		 */
		while (*path == '/')
			path++;

		if ((char *)pp >= (char *)buf + psize)
			break;
		if (isonum_722(pp->parent) != parent)
			break;
		if (!pnmatch(path, pp)) {
			pp = (struct ptable_ent *)((char *)pp + PTSIZE(pp));
			ent++;
			continue;
		}
		path += isonum_711(pp->namlen) + 1;
		parent = ent;
		bno = isonum_732(pp->block) + isonum_711(pp->extlen);
		while ((char *)pp < (char *)buf + psize) {
			if (isonum_722(pp->parent) == parent)
				break;
			pp = (struct ptable_ent *)((char *)pp + PTSIZE(pp));
			ent++;
		}
	}

	/*
	 * Now bno has the start of the directory that supposedly
	 * contains the file
	 */
	bno--;
	dsize = 1;		/* Something stupid, but > 0 XXX */
	for (psize = 0; psize < dsize;) {
		if (!(psize % ISO_DEFAULT_BLOCK_SIZE)) {
			bno++;
#if !defined(LIBSA_NO_TWIDDLE)
			twiddle();
#endif
			rc = DEV_STRATEGY(f->f_dev)(f->f_devdata, F_READ,
			                           cdb2devb(bno),
			                           ISO_DEFAULT_BLOCK_SIZE,
			                           buf, &nread);
			if (rc)
				goto out;
			if (nread != ISO_DEFAULT_BLOCK_SIZE) {
				rc = EIO;
				goto out;
			}
			dp = (struct iso_directory_record *)buf;
		}
		if (!isonum_711(dp->length)) {
			if ((void *)dp == buf)
				psize += ISO_DEFAULT_BLOCK_SIZE;
			else
				psize = roundup(psize, ISO_DEFAULT_BLOCK_SIZE);
			continue;
		}
		if (dsize == 1)
			dsize = isonum_733(dp->size);
		if (dirmatch(path, dp))
			break;
		psize += isonum_711(dp->length);
		dp = (struct iso_directory_record *)
			((char *)dp + isonum_711(dp->length));
	}

	if (psize >= dsize) {
		rc = ENOENT;
		goto out;
	}

	/* allocate file system specific data structure */
	fp = alloc(sizeof(struct file));
	memset(fp, 0, sizeof(struct file));
	f->f_fsdata = (void *)fp;

	fp->off = 0;
	fp->bno = isonum_733(dp->extent);
	fp->size = isonum_733(dp->size);
	dealloc(buf, buf_size);
	fsmod = "cd9660";

	return 0;

out:
	if (fp)
		dealloc(fp, sizeof(struct file));
	dealloc(buf, buf_size);

	return rc;
}

#if !defined(LIBSA_NO_FS_CLOSE)
__compactcall int
cd9660_close(struct open_file *f)
{
	struct file *fp = (struct file *)f->f_fsdata;

	f->f_fsdata = 0;
	dealloc(fp, sizeof *fp);

	return 0;
}
#endif /* !defined(LIBSA_NO_FS_CLOSE) */

__compactcall int
cd9660_read(struct open_file *f, void *start, size_t size, size_t *resid)
{
	struct file *fp = (struct file *)f->f_fsdata;
	int rc = 0;
	daddr_t bno;
	char buf[ISO_DEFAULT_BLOCK_SIZE];
	char *dp;
	size_t nread, off;

	while (size) {
		if (fp->off < 0 || fp->off >= fp->size)
			break;
		bno = fp->off / ISO_DEFAULT_BLOCK_SIZE + fp->bno;
		if (fp->off & (ISO_DEFAULT_BLOCK_SIZE - 1)
		    || (fp->off + ISO_DEFAULT_BLOCK_SIZE) > fp->size
		    || size < ISO_DEFAULT_BLOCK_SIZE)
			dp = buf;
		else
			dp = start;
#if !defined(LIBSA_NO_TWIDDLE)
		twiddle();
#endif
		rc = DEV_STRATEGY(f->f_dev)(f->f_devdata, F_READ, cdb2devb(bno),
		                            ISO_DEFAULT_BLOCK_SIZE, dp, &nread);
		if (rc)
			return rc;
		if (nread != ISO_DEFAULT_BLOCK_SIZE)
			return EIO;
		if (dp == buf) {
			off = fp->off & (ISO_DEFAULT_BLOCK_SIZE - 1);
			if (nread > off + size)
				nread = off + size;
			nread -= off;
			if (nread > fp->size - fp->off)
				nread = fp->size - fp->off;
			memcpy(start, buf + off, nread);
			start = (char *)start + nread;
			fp->off += nread;
			size -= nread;
		} else {
			start = (char *)start + ISO_DEFAULT_BLOCK_SIZE;
			fp->off += ISO_DEFAULT_BLOCK_SIZE;
			size -= ISO_DEFAULT_BLOCK_SIZE;
		}
	}
	if(fp->off > fp->size)
		size += fp->off - fp->size;
	if (resid)
		*resid = size;
	return rc;
}

#if !defined(LIBSA_NO_FS_WRITE)
__compactcall int
cd9660_write(struct open_file *f, void *start, size_t size, size_t *resid)
{

	return EROFS;
}
#endif /* !defined(LIBSA_NO_FS_WRITE) */

#if !defined(LIBSA_NO_FS_SEEK)
__compactcall off_t
cd9660_seek(struct open_file *f, off_t offset, int where)
{
	struct file *fp = (struct file *)f->f_fsdata;

	switch (where) {
	case SEEK_SET:
		fp->off = offset;
		break;
	case SEEK_CUR:
		fp->off += offset;
		break;
	case SEEK_END:
		fp->off = fp->size - offset;
		break;
	default:
		return -1;
	}
	return fp->off;
}
#endif /* !defined(LIBSA_NO_FS_SEEK) */

__compactcall int
cd9660_stat(struct open_file *f, struct stat *sb)
{
	struct file *fp = (struct file *)f->f_fsdata;

	/* only importatn stuff */
	sb->st_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
	sb->st_uid = sb->st_gid = 0;
	sb->st_size = fp->size;
	return 0;
}

#if defined(LIBSA_ENABLE_LS_OP)
#include "ls.h"
__compactcall void
cd9660_ls(struct open_file *f, const char *pattern)
{
	lsunsup("cd9660");
}

#if defined(__minix) && defined(LIBSA_ENABLE_LOAD_MODS_OP)
__compactcall void
cd9660_load_mods(struct open_file *f, const char *pattern,
	void (*funcp)(char *), char *path)
{
	load_modsunsup("cd9660");
}
#endif /* defined(__minix) && defined(LIBSA_ENABLE_LOAD_MODS_OP) */
#endif
