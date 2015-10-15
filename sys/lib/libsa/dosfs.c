/*	$NetBSD: dosfs.c,v 1.20 2014/03/20 03:13:18 christos Exp $	*/

/*
 * Copyright (c) 1996, 1998 Robert Nordier
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Readonly filesystem for Microsoft FAT12/FAT16/FAT32 filesystems,
 * also supports VFAT.
 */

/*
 * XXX DOES NOT SUPPORT:
 *
 *	LIBSA_FS_SINGLECOMPONENT
 */

#include <sys/param.h>

#include <fs/msdosfs/bpb.h>
#include <fs/msdosfs/direntry.h>

#ifdef _STANDALONE
#include <lib/libkern/libkern.h>
#else
#include <string.h>
#include <stddef.h>
#endif

#include "stand.h"
#include "dosfs.h"

#define SECSIZ  512		/* sector size */
#define SSHIFT    9		/* SECSIZ shift */
#define DEPSEC   16		/* directory entries per sector */
#define DSHIFT    4		/* DEPSEC shift */
#define LOCLUS    2		/* lowest cluster number */

typedef union {
	struct direntry de;	/* standard directory entry */
	struct winentry xde;	/* extended directory entry */
} DOS_DIR;

typedef struct {
	struct open_file *fd;	/* file descriptor */
	u_char *buf;		/* buffer */
	u_int   bufsec;		/* buffered sector */
	u_int   links;		/* active links to structure */
	u_int   spc;		/* sectors per cluster */
	u_int   bsize;		/* cluster size in bytes */
	u_int   bshift;		/* cluster conversion shift */
	u_int   dirents;	/* root directory entries */
	u_int   spf;		/* sectors per fat */
	u_int   rdcl;		/* root directory start cluster */
	u_int   lsnfat;		/* start of fat */
	u_int   lsndir;		/* start of root dir */
	u_int   lsndta;		/* start of data area */
	u_int   fatsz;		/* FAT entry size */
	u_int   xclus;		/* maximum cluster number */
} DOS_FS;

typedef struct {
	DOS_FS *fs;		/* associated filesystem */
	struct direntry de;	/* directory entry */
	u_int   offset;		/* current offset */
	u_int   c;		/* last cluster read */
} DOS_FILE;

/* Initial portion of DOS boot sector */
typedef struct {
	u_char  jmp[3];		/* usually 80x86 'jmp' opcode */
	u_char  oem[8];		/* OEM name and version */
	struct byte_bpb710 bpb;	/* BPB */
} DOS_BS;

/* Supply missing "." and ".." root directory entries */
static const char *const dotstr[2] = {".", ".."};
static const struct direntry dot[2] = {
	{".       ", "   ", ATTR_DIRECTORY,
		0, 0, {0, 0}, {0, 0}, {0, 0}, {0, 0},
		{0, 0}, {0x21, 0}, {0, 0}, {0, 0, 0, 0}},

	{"..      ", "   ", ATTR_DIRECTORY,
		0, 0, {0, 0}, {0, 0}, {0, 0}, {0, 0},
		{0, 0}, {0x21, 0}, {0, 0}, {0, 0, 0, 0}}
};

/* The usual conversion macros to avoid multiplication and division */
#define bytsec(n)      ((n) >> SSHIFT)
#define secbyt(s)      ((s) << SSHIFT)
#define entsec(e)      ((e) >> DSHIFT)
#define bytblk(fs, n)  ((n) >> (fs)->bshift)
#define blkbyt(fs, b)  ((b) << (fs)->bshift)
#define secblk(fs, s)  ((s) >> ((fs)->bshift - SSHIFT))
#define blksec(fs, b)  ((b) << ((fs)->bshift - SSHIFT))

/* Convert cluster number to offset within filesystem */
#define blkoff(fs, b) (secbyt((fs)->lsndta) + blkbyt(fs, (b) - LOCLUS))

/* Convert cluster number to logical sector number */
#define blklsn(fs, b)  ((fs)->lsndta + blksec(fs, (b) - LOCLUS))

/* Convert cluster number to offset within FAT */
#define fatoff(sz, c)  ((sz) == 12 ? (c) + ((c) >> 1) :  \
                        (sz) == 16 ? (c) << 1 :          \
                        (c) << 2)

/* Does cluster number reference a valid data cluster? */
#define okclus(fs, c)  ((c) >= LOCLUS && (c) <= (fs)->xclus)

/* Get start cluster from directory entry */
#define stclus(sz, de)  ((sz) != 32 ? (u_int)getushort((de)->deStartCluster) : \
                         ((u_int)getushort((de)->deHighClust) << 16) |  \
                         (u_int)getushort((de)->deStartCluster))

static int dosunmount(DOS_FS *);
static int parsebs(DOS_FS *, DOS_BS *);
static int namede(DOS_FS *, const char *, const struct direntry **);
static int lookup(DOS_FS *, u_int, const char *, const struct direntry **);
static void cp_xdnm(u_char *, struct winentry *);
static void cp_sfn(u_char *, struct direntry *);
static off_t fsize(DOS_FS *, struct direntry *);
static int fatcnt(DOS_FS *, u_int);
static int fatget(DOS_FS *, u_int *);
static int fatend(u_int, u_int);
static int ioread(DOS_FS *, u_int, void *, u_int);
static int iobuf(DOS_FS *, u_int);
static int ioget(struct open_file *, u_int, void *, u_int);

#define strcasecmp(s1, s2) dos_strcasecmp(s1, s2)
static int
strcasecmp(const char *s1, const char *s2)
{
	char c1, c2;
	#define TO_UPPER(c) ((c) >= 'a' && (c) <= 'z' ? (c) - ('a' - 'A') : (c))
	for (;;) {
		c1 = *s1++;
		c2 = *s2++;
		if (TO_UPPER(c1) != TO_UPPER(c2))
			return 1;
		if (c1 == 0)
			return 0;
	}
	#undef TO_UPPER
}

/*
 * Mount DOS filesystem
 */
static int
dos_mount(DOS_FS *fs, struct open_file *fd)
{
	int     err;

	(void)memset(fs, 0, sizeof(DOS_FS));
	fs->fd = fd;
	if ((err = !(fs->buf = alloc(SECSIZ)) ? errno : 0) ||
	    (err = ioget(fs->fd, 0, fs->buf, 1)) ||
	    (err = parsebs(fs, (DOS_BS *)fs->buf))) {
		(void) dosunmount(fs);
		return err;
	}
	return 0;
}

#ifndef LIBSA_NO_FS_CLOSE
/*
 * Unmount mounted filesystem
 */
static int
dos_unmount(DOS_FS *fs)
{
	int     err;

	if (fs->links)
		return EBUSY;
	if ((err = dosunmount(fs)))
		return err;
	return 0;
}
#endif

/*
 * Common code shared by dos_mount() and dos_unmount()
 */
static int
dosunmount(DOS_FS *fs)
{
	if (fs->buf)
		dealloc(fs->buf, SECSIZ);
	dealloc(fs, sizeof(DOS_FS));
	return 0;
}

/*
 * Open DOS file
 */
__compactcall int
dosfs_open(const char *path, struct open_file *fd)
{
	const struct direntry *de;
	DOS_FILE *f;
	DOS_FS *fs;
	u_int   size, clus;
	int     err = 0;

	/* Allocate mount structure, associate with open */
	fs = alloc(sizeof(DOS_FS));

	if ((err = dos_mount(fs, fd)))
		goto out;

	if ((err = namede(fs, path, &de)))
		goto out;

	clus = stclus(fs->fatsz, de);
	size = getulong(de->deFileSize);

	if ((!(de->deAttributes & ATTR_DIRECTORY) && (!clus != !size)) ||
	    ((de->deAttributes & ATTR_DIRECTORY) && size) ||
	    (clus && !okclus(fs, clus))) {
		err = EINVAL;
		goto out;
	}

	f = alloc(sizeof(DOS_FILE));
#ifdef BOOTXX
	/* due to __internal_memset_ causing all sorts of register spillage
	   (and being completely unoptimized for zeroing small amounts of
	   memory), if we hand-initialize the remaining members of f to zero,
	   the code size drops 68 bytes. This makes no sense, admittedly. */
	f->offset = 0;
	f->c = 0;
#else
	(void)memset(f, 0, sizeof(DOS_FILE));
#endif
	f->fs = fs;
	fs->links++;
	f->de = *de;
	fd->f_fsdata = (void *)f;
	fsmod = "msdos";

out:
	return err;
}

/*
 * Read from file
 */
__compactcall int
dosfs_read(struct open_file *fd, void *vbuf, size_t nbyte, size_t *resid)
{
	off_t   size;
	u_int8_t *buf = vbuf;
	u_int   nb, off, clus, c, cnt, n;
	DOS_FILE *f = (DOS_FILE *)fd->f_fsdata;
	int     err = 0;

	nb = (u_int) nbyte;
	if ((size = fsize(f->fs, &f->de)) == -1)
		return EINVAL;
	if (nb > (n = size - f->offset))
		nb = n;
	off = f->offset;
	if ((clus = stclus(f->fs->fatsz, &f->de)))
		off &= f->fs->bsize - 1;
	c = f->c;
	cnt = nb;
	while (cnt) {
		n = 0;
		if (!c) {
			if ((c = clus))
				n = bytblk(f->fs, f->offset);
		} else if (!off) {
			n++;
		}
		while (n--) {
			if ((err = fatget(f->fs, &c)))
				goto out;
			if (!okclus(f->fs, c)) {
				err = EINVAL;
				goto out;
			}
		}
		if (!clus || (n = f->fs->bsize - off) > cnt)
			n = cnt;
		if ((err = ioread(f->fs, (c ? blkoff(f->fs, c) :
				secbyt(f->fs->lsndir)) + off,
			    buf, n)))
			goto out;
		f->offset += n;
		f->c = c;
		off = 0;
		buf += n;
		cnt -= n;
	}
out:
	if (resid)
		*resid = nbyte - nb + cnt;
	return err;
}

#ifndef LIBSA_NO_FS_WRITE
/*
 * Not implemented.
 */
__compactcall int
dosfs_write(struct open_file *fd, void *start, size_t size, size_t *resid)
{

	return EROFS;
}
#endif /* !LIBSA_NO_FS_WRITE */

#ifndef LIBSA_NO_FS_SEEK
/*
 * Reposition within file
 */
__compactcall off_t
dosfs_seek(struct open_file *fd, off_t offset, int whence)
{
	off_t   off;
	u_int   size;
	DOS_FILE *f = (DOS_FILE *)fd->f_fsdata;

	size = getulong(f->de.deFileSize);
	switch (whence) {
	case SEEK_SET:
		off = 0;
		break;
	case SEEK_CUR:
		off = f->offset;
		break;
	case SEEK_END:
		off = size;
		break;
	default:
		return -1;
	}
	off += offset;
	if (off < 0 || off > size)
		return -1;
	f->offset = (u_int) off;
	f->c = 0;
	return off;
}
#endif /* !LIBSA_NO_FS_SEEK */

#ifndef LIBSA_NO_FS_CLOSE
/*
 * Close open file
 */
__compactcall int
dosfs_close(struct open_file *fd)
{
	DOS_FILE *f = (DOS_FILE *)fd->f_fsdata;
	DOS_FS *fs = f->fs;

	f->fs->links--;
	dealloc(f, sizeof(DOS_FILE));
	dos_unmount(fs);
	return 0;
}
#endif /* !LIBSA_NO_FS_CLOSE */

/*
 * Return some stat information on a file.
 */
__compactcall int
dosfs_stat(struct open_file *fd, struct stat *sb)
{
	DOS_FILE *f = (DOS_FILE *)fd->f_fsdata;

	/* only important stuff */
	sb->st_mode = (f->de.deAttributes & ATTR_DIRECTORY) ?
	    (S_IFDIR | 0555) : (S_IFREG | 0444);
	sb->st_nlink = 1;
	sb->st_uid = 0;
	sb->st_gid = 0;
	if ((sb->st_size = fsize(f->fs, &f->de)) == -1)
		return EINVAL;
	return 0;
}

#if defined(LIBSA_ENABLE_LS_OP)
#include "ls.h"
__compactcall void
dosfs_ls(struct open_file *f, const char *pattern)
{
	lsunsup("dosfs");
}

#if defined(__minix) && defined(LIBSA_ENABLE_LOAD_MODS_OP)
__compactcall void
dosfs_load_mods(struct open_file *f, const char *pattern,
	void (*funcp)(char *), char *path)
{
	load_modsunsup("dosfs");
}
#endif /* defined(__minix) && defined(LIBSA_ENABLE_LOAD_MODS_OP) */
#endif

/*
 * Parse DOS boot sector
 */
static int
parsebs(DOS_FS *fs, DOS_BS *bs)
{
	u_int   sc;

	if ((bs->jmp[0] != 0x69 &&
		bs->jmp[0] != 0xe9 &&
		(bs->jmp[0] != 0xeb || bs->jmp[2] != 0x90)) ||
	    bs->bpb.bpbMedia < 0xf0)
		return EINVAL;
	if (getushort(bs->bpb.bpbBytesPerSec) != SECSIZ)
		return EINVAL;
	if (!(fs->spc = bs->bpb.bpbSecPerClust) || fs->spc & (fs->spc - 1))
		return EINVAL;
	fs->bsize = secbyt(fs->spc);
	fs->bshift = ffs(fs->bsize) - 1;
	if ((fs->spf = getushort(bs->bpb.bpbFATsecs))) {
		if (bs->bpb.bpbFATs != 2)
			return EINVAL;
		if (!(fs->dirents = getushort(bs->bpb.bpbRootDirEnts)))
			return EINVAL;
	} else {
		if (!(fs->spf = getulong(bs->bpb.bpbBigFATsecs)))
			return EINVAL;
		if (!bs->bpb.bpbFATs || bs->bpb.bpbFATs > 16)
			return EINVAL;
		if ((fs->rdcl = getulong(bs->bpb.bpbRootClust)) < LOCLUS)
			return EINVAL;
	}
	if (!(fs->lsnfat = getushort(bs->bpb.bpbResSectors)))
		return EINVAL;
	fs->lsndir = fs->lsnfat + fs->spf * bs->bpb.bpbFATs;
	fs->lsndta = fs->lsndir + entsec(fs->dirents);
	if (!(sc = getushort(bs->bpb.bpbSectors)) &&
	    !(sc = getulong(bs->bpb.bpbHugeSectors)))
		return EINVAL;
	if (fs->lsndta > sc)
		return EINVAL;
	if ((fs->xclus = secblk(fs, sc - fs->lsndta) + 1) < LOCLUS)
		return EINVAL;
	fs->fatsz = fs->dirents ? fs->xclus < 0xff6 ? 12 : 16 : 32;
	sc = (secbyt(fs->spf) << 1) / (fs->fatsz >> 2) - 1;
	if (fs->xclus > sc)
		fs->xclus = sc;
	return 0;
}

/*
 * Return directory entry from path
 */
static int
namede(DOS_FS *fs, const char *path, const struct direntry **dep)
{
	char    name[256];
	const struct direntry *de;
	char   *s;
	size_t  n;
	int     err;

	err = 0;
	de = dot;
	if (*path == '/')
		path++;
	while (*path) {
		if (!(s = strchr(path, '/')))
			s = strchr(path, 0);
		if ((n = s - path) > 255)
			return ENAMETOOLONG;
		memcpy(name, path, n);
		name[n] = 0;
		path = s;
		if (!(de->deAttributes & ATTR_DIRECTORY))
			return ENOTDIR;
		if ((err = lookup(fs, stclus(fs->fatsz, de), name, &de)))
			return err;
		if (*path == '/')
			path++;
	}
	*dep = de;
	return 0;
}

/*
 * Lookup path segment
 */
static int
lookup(DOS_FS *fs, u_int clus, const char *name, const struct direntry **dep)
{
	static DOS_DIR *dir = NULL;
	u_char  lfn[261];
	u_char  sfn[13];
	u_int   nsec, lsec, xdn, chk, sec, ent, x;
	int     err = 0, ok, i;

	if (!clus)
		for (ent = 0; ent < 2; ent++)
			if (!strcasecmp(name, dotstr[ent])) {
				*dep = dot + ent;
				return 0;
			}

	if (dir == NULL) {
		dir = alloc(sizeof(DOS_DIR) * DEPSEC);
		if (dir == NULL)
			return ENOMEM;
	}

	if (!clus && fs->fatsz == 32)
		clus = fs->rdcl;
	nsec = !clus ? entsec(fs->dirents) : fs->spc;
	lsec = 0;
	xdn = chk = 0;
	for (;;) {
		if (!clus && !lsec)
			lsec = fs->lsndir;
		else if (okclus(fs, clus))
			lsec = blklsn(fs, clus);
		else {
			err = EINVAL;
			goto out;
		}
		for (sec = 0; sec < nsec; sec++) {
			if ((err = ioget(fs->fd, lsec + sec, dir, 1)))
				goto out;
			for (ent = 0; ent < DEPSEC; ent++) {
				if (!*dir[ent].de.deName) {
					err = ENOENT;
					goto out;
				}
				if (*dir[ent].de.deName != 0xe5) {
					if (dir[ent].de.deAttributes ==
					    ATTR_WIN95) {
						x = dir[ent].xde.weCnt;
						if (x & WIN_LAST ||
						    (x + 1 == xdn &&
						     dir[ent].xde.weChksum ==
						     chk)) {
							if (x & WIN_LAST) {
								chk = dir[ent].xde.weChksum;
								x &= WIN_CNT;
							}
							if (x >= 1 && x <= 20) {
								cp_xdnm(lfn, &dir[ent].xde);
								xdn = x;
								continue;
							}
						}
					} else if (!(dir[ent].de.deAttributes &
						     ATTR_VOLUME)) {
						if ((ok = xdn == 1)) {
							for (x = 0, i = 0;
							     i < 11; i++)
								x = ((((x & 1) << 7) | (x >> 1)) +
								    msdos_dirchar(&dir[ent].de,i)) & 0xff;
							ok = chk == x &&
							    !strcasecmp(name, (const char *)lfn);
						}
						if (!ok) {
							cp_sfn(sfn, &dir[ent].de);
							ok = !strcasecmp(name, (const char *)sfn);
						}
						if (ok) {
							*dep = &dir[ent].de;
							goto out2;
						}
					}
				}
				xdn = 0;
			}
		}
		if (!clus)
			break;
		if ((err = fatget(fs, &clus)))
			goto out;
		if (fatend(fs->fatsz, clus))
			break;
	}
	err = ENOENT;
 out:
	dealloc(dir, sizeof(DOS_DIR) * DEPSEC);
	dir = NULL;
 out2:
	return err;
}

/*
 * Copy name from extended directory entry
 */
static void
cp_xdnm(u_char *lfn, struct winentry *xde)
{
	static const struct {
		u_int   off;
		u_int   dim;
	} ix[3] = {
		{ offsetof(struct winentry, wePart1),
		    sizeof(xde->wePart1) / 2 },
		{ offsetof(struct winentry, wePart2),
		    sizeof(xde->wePart2) / 2 },
		{ offsetof(struct winentry, wePart3),
		    sizeof(xde->wePart3) / 2 }
	};
	u_char *p;
	u_int   n, x, c;

	lfn += 13 * ((xde->weCnt & WIN_CNT) - 1);
	for (n = 0; n < 3; n++)
		for (p = (u_char *)xde + ix[n].off, x = ix[n].dim; x;
		    p += 2, x--) {
			if ((c = getushort(p)) && (c < 32 || c > 127))
				c = '?';
			if (!(*lfn++ = c))
				return;
		}
	if (xde->weCnt & WIN_LAST)
		*lfn = 0;
}

/*
 * Copy short filename
 */
static void
cp_sfn(u_char *sfn, struct direntry *de)
{
	u_char *p;
	int     j, i;

	p = sfn;
	if (*de->deName != ' ') {
		for (j = 7; de->deName[j] == ' '; j--);
		for (i = 0; i <= j; i++)
			*p++ = de->deName[i];
		if (*de->deExtension != ' ') {
			*p++ = '.';
			for (j = 2; de->deExtension[j] == ' '; j--);
			for (i = 0; i <= j; i++)
				*p++ = de->deExtension[i];
		}
	}
	*p = 0;
	if (*sfn == 5)
		*sfn = 0xe5;
}

/*
 * Return size of file in bytes
 */
static  off_t
fsize(DOS_FS *fs, struct direntry *de)
{
	u_long  size;
	u_int   c;
	int     n;

	if (!(size = getulong(de->deFileSize)) &&
	    de->deAttributes & ATTR_DIRECTORY) {
		if (!(c = getushort(de->deStartCluster))) {
			size = fs->dirents * sizeof(struct direntry);
		} else {
			if ((n = fatcnt(fs, c)) == -1)
				return n;
			size = blkbyt(fs, n);
		}
	}
	return size;
}

/*
 * Count number of clusters in chain
 */
static int
fatcnt(DOS_FS *fs, u_int c)
{
	int     n;

	for (n = 0; okclus(fs, c); n++)
		if (fatget(fs, &c))
			return -1;
	return fatend(fs->fatsz, c) ? n : -1;
}

/*
 * Get next cluster in cluster chain
 */
static int
fatget(DOS_FS *fs, u_int *c)
{
	u_char  buf[4];
	u_int   x;
	int     err;

	err = ioread(fs, secbyt(fs->lsnfat) + fatoff(fs->fatsz, *c), buf,
	    fs->fatsz != 32 ? 2 : 4);
	if (err)
		return err;
	x = fs->fatsz != 32 ? getushort(buf) : getulong(buf);
	*c = fs->fatsz == 12 ? *c & 1 ? x >> 4 : x & 0xfff : x;
	return 0;
}

/*
 * Is cluster an end-of-chain marker?
 */
static int
fatend(u_int sz, u_int c)
{
	return c > (sz == 12 ? 0xff7U : sz == 16 ? 0xfff7U : 0xffffff7);
}

/*
 * Offset-based I/O primitive
 */
static int
ioread(DOS_FS *fs, u_int offset, void *buf, u_int nbyte)
{
	char   *s;
	u_int   off, n;
	int     err;

	s = buf;
	if ((off = offset & (SECSIZ - 1))) {
		offset -= off;
		if ((err = iobuf(fs, bytsec(offset))))
			return err;
		offset += SECSIZ;
		if ((n = SECSIZ - off) > nbyte)
			n = nbyte;
		memcpy(s, fs->buf + off, n);
		s += n;
		nbyte -= n;
	}
	n = nbyte & (SECSIZ - 1);
	if (nbyte -= n) {
		if ((err = ioget(fs->fd, bytsec(offset), s, bytsec(nbyte))))
			return err;
		offset += nbyte;
		s += nbyte;
	}
	if (n) {
		if ((err = iobuf(fs, bytsec(offset))))
			return err;
		memcpy(s, fs->buf, n);
	}
	return 0;
}

/*
 * Buffered sector-based I/O primitive
 */
static int
iobuf(DOS_FS *fs, u_int lsec)
{
	int     err;

	if (fs->bufsec != lsec) {
		if ((err = ioget(fs->fd, lsec, fs->buf, 1)))
			return err;
		fs->bufsec = lsec;
	}
	return 0;
}

/*
 * Sector-based I/O primitive
 */
static int
ioget(struct open_file *fd, u_int lsec, void *buf, u_int nsec)
{
	size_t rsize;
	int err;

#ifndef LIBSA_NO_TWIDDLE
	twiddle();
#endif
	err = DEV_STRATEGY(fd->f_dev)(fd->f_devdata, F_READ, lsec,
	    secbyt(nsec), buf, &rsize);
	return err;
}
