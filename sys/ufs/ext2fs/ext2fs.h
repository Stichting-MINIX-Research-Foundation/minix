/*	$NetBSD: ext2fs.h,v 1.36 2013/06/23 07:28:37 dholland Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)fs.h	8.10 (Berkeley) 10/27/94
 *  Modified for ext2fs by Manuel Bouyer.
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
 *
 *	@(#)fs.h	8.10 (Berkeley) 10/27/94
 *  Modified for ext2fs by Manuel Bouyer.
 */

#ifndef _UFS_EXT2FS_EXT2FS_H_
#define _UFS_EXT2FS_EXT2FS_H_

#include <sys/bswap.h>

/*
 * Each disk drive contains some number of file systems.
 * A file system consists of a number of cylinder groups.
 * Each cylinder group has inodes and data.
 *
 * A file system is described by its super-block, which in turn
 * describes the cylinder groups.  The super-block is critical
 * data and is replicated in each cylinder group to protect against
 * catastrophic loss.  This is done at `newfs' time and the critical
 * super-block data does not change, so the copies need not be
 * referenced further unless disaster strikes.
 *
 * The first boot and super blocks are given in absolute disk addresses.
 * The byte-offset forms are preferred, as they don't imply a sector size.
 */
#define BBSIZE		1024
#define SBSIZE		1024
#define	BBOFF		((off_t)(0))
#define	SBOFF		((off_t)(BBOFF + BBSIZE))
#define	BBLOCK		((daddr_t)(0))
#define	SBLOCK		((daddr_t)(BBLOCK + BBSIZE / DEV_BSIZE))

/*
 * Addresses stored in inodes are capable of addressing blocks
 * XXX
 */

/*
 * MINBSIZE is the smallest allowable block size.
 * MINBSIZE must be big enough to hold a cylinder group block,
 * thus changes to (struct cg) must keep its size within MINBSIZE.
 * Note that super blocks are always of size SBSIZE,
 * and that both SBSIZE and MAXBSIZE must be >= MINBSIZE.
 */
#define LOG_MINBSIZE	10
#define MINBSIZE	(1 << LOG_MINBSIZE)

/*
 * The path name on which the file system is mounted is maintained
 * in fs_fsmnt. MAXMNTLEN defines the amount of space allocated in
 * the super block for this name.
 */
#define MAXMNTLEN	512

/*
 * MINFREE gives the minimum acceptable percentage of file system
 * blocks which may be free. If the freelist drops below this level
 * only the superuser may continue to allocate blocks. This may
 * be set to 0 if no reserve of free blocks is deemed necessary,
 * however throughput drops by fifty percent if the file system
 * is run at between 95% and 100% full; thus the minimum default
 * value of fs_minfree is 5%. However, to get good clustering
 * performance, 10% is a better choice. hence we use 10% as our
 * default value. With 10% free space, fragmentation is not a
 * problem, so we choose to optimize for time.
 */
#define MINFREE		5

/*
 * Super block for an ext2fs file system.
 */
struct ext2fs {
	uint32_t  e2fs_icount;		/* Inode count */
	uint32_t  e2fs_bcount;		/* blocks count */
	uint32_t  e2fs_rbcount;		/* reserved blocks count */
	uint32_t  e2fs_fbcount;		/* free blocks count */
	uint32_t  e2fs_ficount;		/* free inodes count */
	uint32_t  e2fs_first_dblock;	/* first data block */
	uint32_t  e2fs_log_bsize;	/* block size = 1024*(2^e2fs_log_bsize) */
	uint32_t  e2fs_fsize;		/* fragment size */
	uint32_t  e2fs_bpg;		/* blocks per group */
	uint32_t  e2fs_fpg;		/* frags per group */
	uint32_t  e2fs_ipg;		/* inodes per group */
	uint32_t  e2fs_mtime;		/* mount time */
	uint32_t  e2fs_wtime;		/* write time */
	uint16_t  e2fs_mnt_count;	/* mount count */
	uint16_t  e2fs_max_mnt_count;	/* max mount count */
	uint16_t  e2fs_magic;		/* magic number */
	uint16_t  e2fs_state;		/* file system state */
	uint16_t  e2fs_beh;		/* behavior on errors */
	uint16_t  e2fs_minrev;		/* minor revision level */
	uint32_t  e2fs_lastfsck;	/* time of last fsck */
	uint32_t  e2fs_fsckintv;	/* max time between fscks */
	uint32_t  e2fs_creator;		/* creator OS */
	uint32_t  e2fs_rev;		/* revision level */
	uint16_t  e2fs_ruid;		/* default uid for reserved blocks */
	uint16_t  e2fs_rgid;		/* default gid for reserved blocks */
	/* EXT2_DYNAMIC_REV superblocks */
	uint32_t  e2fs_first_ino;	/* first non-reserved inode */
	uint16_t  e2fs_inode_size;	/* size of inode structure */
	uint16_t  e2fs_block_group_nr;	/* block grp number of this sblk*/
	uint32_t  e2fs_features_compat;	/*  compatible feature set */
	uint32_t  e2fs_features_incompat; /* incompatible feature set */
	uint32_t  e2fs_features_rocompat; /* RO-compatible feature set */
	uint8_t   e2fs_uuid[16];	/* 128-bit uuid for volume */
	char      e2fs_vname[16];	/* volume name */
	char      e2fs_fsmnt[64];	/* name mounted on */
	uint32_t  e2fs_algo;		/* For compression */
	uint8_t   e2fs_prealloc;	/* # of blocks to preallocate */
	uint8_t   e2fs_dir_prealloc;	/* # of blocks to preallocate for dir */
	uint16_t  e2fs_reserved_ngdb;	/* # of reserved gd blocks for resize */
	uint32_t  reserved2[204];
};


/* in-memory data for ext2fs */
struct m_ext2fs {
	struct ext2fs e2fs;
	u_char	e2fs_fsmnt[MAXMNTLEN];	/* name mounted on */
	int8_t	e2fs_ronly;	/* mounted read-only flag */
	int8_t	e2fs_fmod;	/* super block modified flag */
	int32_t	e2fs_bsize;	/* block size */
	int32_t e2fs_bshift;	/* ``lblkno'' calc of logical blkno */
	int32_t e2fs_bmask;	/* ``blkoff'' calc of blk offsets */
	int64_t e2fs_qbmask;	/* ~fs_bmask - for use with quad size */
	int32_t	e2fs_fsbtodb;	/* fsbtodb and dbtofsb shift constant */
	int32_t	e2fs_ncg;	/* number of cylinder groups */
	int32_t	e2fs_ngdb;	/* number of group descriptor block */
	int32_t	e2fs_ipb;	/* number of inodes per block */
	int32_t	e2fs_itpg;	/* number of inode table per group */
	struct	ext2_gd *e2fs_gd; /* group descripors */
};



/*
 * Filesystem identification
 */
#define	E2FS_MAGIC	0xef53	/* the ext2fs magic number */
#define E2FS_REV0	0	/* GOOD_OLD revision */
#define E2FS_REV1	1	/* Support compat/incompat features */

/* compatible/incompatible features */
#define EXT2F_COMPAT_PREALLOC		0x0001
#define EXT2F_COMPAT_AFS		0x0002
#define EXT2F_COMPAT_HASJOURNAL		0x0004
#define EXT2F_COMPAT_EXTATTR		0x0008
#define EXT2F_COMPAT_RESIZE		0x0010
#define EXT2F_COMPAT_DIRHASHINDEX	0x0020
#define	EXT2F_COMPAT_BITS \
	"\20" \
	"\06COMPAT_DIRHASHINDEX" \
	"\05COMPAT_RESIZE" \
	"\04COMPAT_EXTATTR" \
	"\03COMPAT_HASJOURNAL" \
	"\02COMPAT_AFS" \
	"\01COMPAT_PREALLOC"

#define EXT2F_ROCOMPAT_SPARSESUPER	0x0001
#define EXT2F_ROCOMPAT_LARGEFILE	0x0002
#define EXT2F_ROCOMPAT_BTREE_DIR	0x0004
#define EXT2F_ROCOMPAT_HUGE_FILE	0x0008
#define EXT2F_ROCOMPAT_GDT_CSUM		0x0010
#define EXT2F_ROCOMPAT_DIR_NLINK	0x0020
#define EXT2F_ROCOMPAT_EXTRA_ISIZE	0x0040
#define	EXT2F_ROCOMPAT_BITS \
	"\20" \
	"\07ROCOMPAT_EXTRA_ISIZE" \
	"\06ROCOMPAT_DIR_NLINK" \
	"\05ROCOMPAT_GDT_CSUM" \
	"\04ROCOMPAT_HUGE_FILE" \
	"\03ROCOMPAT_BTREE_DIR" \
	"\02ROCOMPAT_LARGEFILE" \
	"\01ROCOMPAT_SPARSESUPER"

#define EXT2F_INCOMPAT_COMP		0x0001
#define EXT2F_INCOMPAT_FTYPE		0x0002
#define	EXT2F_INCOMPAT_REPLAY_JOURNAL	0x0004
#define	EXT2F_INCOMPAT_USES_JOURNAL	0x0008
#define EXT2F_INCOMPAT_META_BG		0x0010
#define EXT2F_INCOMPAT_EXTENTS		0x0040
#define EXT2F_INCOMPAT_64BIT		0x0080
#define EXT2F_INCOMPAT_MMP		0x0100
#define EXT2F_INCOMPAT_FLEX_BG		0x0200
#define	EXT2F_INCOMPAT_BITS \
	"\20" \
	"\012INCOMPAT_FLEX_BG" \
	"\011INCOMPAT_MMP" \
	"\010INCOMPAT_64BIT" \
	"\07INCOMPAT_EXTENTS" \
	"\05INCOMPAT_META_BG" \
	"\04INCOMPAT_USES_JOURNAL" \
	"\03INCOMPAT_REPLAY_JOURNAL" \
	"\02INCOMPAT_FTYPE" \
	"\01INCOMPAT_COMP"

/*
 * Features supported in this implementation
 *
 * We support the following REV1 features:
 * - EXT2F_ROCOMPAT_SPARSESUPER
 *    superblock backups stored only in cg_has_sb(bno) groups
 * - EXT2F_ROCOMPAT_LARGEFILE
 *    use e2di_dacl in struct ext2fs_dinode to store 
 *    upper 32bit of size for >2GB files
 * - EXT2F_INCOMPAT_FTYPE
 *    store file type to e2d_type in struct ext2fs_direct
 *    (on REV0 e2d_namlen is uint16_t and no e2d_type, like ffs)
 */
#define EXT2F_COMPAT_SUPP		0x0000
#define EXT2F_ROCOMPAT_SUPP		(EXT2F_ROCOMPAT_SPARSESUPER \
					 | EXT2F_ROCOMPAT_LARGEFILE \
					 | EXT2F_ROCOMPAT_HUGE_FILE)
#define EXT2F_INCOMPAT_SUPP		EXT2F_INCOMPAT_FTYPE

/*
 * Definitions of behavior on errors
 */
#define E2FS_BEH_CONTINUE	1	/* continue operation */
#define E2FS_BEH_READONLY	2	/* remount fs read only */
#define E2FS_BEH_PANIC		3	/* cause panic */
#define E2FS_BEH_DEFAULT	E2FS_BEH_CONTINUE

/*
 * OS identification
 */
#define E2FS_OS_LINUX	0
#define E2FS_OS_HURD	1
#define E2FS_OS_MASIX	2
#define E2FS_OS_FREEBSD	3
#define E2FS_OS_LITES	4

/*
 * Filesystem clean flags
 */
#define	E2FS_ISCLEAN	0x01
#define	E2FS_ERRORS	0x02

/* ext2 file system block group descriptor */

struct ext2_gd {
	uint32_t ext2bgd_b_bitmap;	/* blocks bitmap block */
	uint32_t ext2bgd_i_bitmap;	/* inodes bitmap block */
	uint32_t ext2bgd_i_tables;	/* inodes table block  */
	uint16_t ext2bgd_nbfree;	/* number of free blocks */
	uint16_t ext2bgd_nifree;	/* number of free inodes */
	uint16_t ext2bgd_ndirs;		/* number of directories */
	uint16_t reserved;
	uint32_t reserved2[3];
};


/*
 * If the EXT2F_ROCOMPAT_SPARSESUPER flag is set, the cylinder group has a
 * copy of the super and cylinder group descriptors blocks only if it's
 * 1, a power of 3, 5 or 7
 */

static __inline int cg_has_sb(int) __unused;
static __inline int
cg_has_sb(int i)
{
	int a3, a5, a7;

	if (i == 0 || i == 1)
		return 1;
	for (a3 = 3, a5 = 5, a7 = 7;
	    a3 <= i || a5 <= i || a7 <= i;
	    a3 *= 3, a5 *= 5, a7 *= 7)
		if (i == a3 || i == a5 || i == a7)
			return 1;
	return 0;
}

/* EXT2FS metadatas are stored in little-endian byte order. These macros
 * helps reading theses metadatas
 */

#if BYTE_ORDER == LITTLE_ENDIAN
#	define h2fs16(x) (x)
#	define h2fs32(x) (x)
#	define h2fs64(x) (x)
#	define fs2h16(x) (x)
#	define fs2h32(x) (x)
#	define fs2h64(x) (x)
#	define e2fs_sbload(old, new) memcpy((new), (old), SBSIZE);
#	define e2fs_cgload(old, new, size) memcpy((new), (old), (size));
#	define e2fs_sbsave(old, new) memcpy((new), (old), SBSIZE);
#	define e2fs_cgsave(old, new, size) memcpy((new), (old), (size));
#else
void e2fs_sb_bswap(struct ext2fs *, struct ext2fs *);
void e2fs_cg_bswap(struct ext2_gd *, struct ext2_gd *, int);
#	define h2fs16(x) bswap16(x)
#	define h2fs32(x) bswap32(x)
#	define h2fs64(x) bswap64(x)
#	define fs2h16(x) bswap16(x)
#	define fs2h32(x) bswap32(x)
#	define fs2h64(x) bswap64(x)
#	define e2fs_sbload(old, new) e2fs_sb_bswap((old), (new))
#	define e2fs_cgload(old, new, size) e2fs_cg_bswap((old), (new), (size));
#	define e2fs_sbsave(old, new) e2fs_sb_bswap((old), (new))
#	define e2fs_cgsave(old, new, size) e2fs_cg_bswap((old), (new), (size));
#endif

/*
 * Turn file system block numbers into disk block addresses.
 * This maps file system blocks to device size blocks.
 */
#define EXT2_FSBTODB(fs, b)	((b) << (fs)->e2fs_fsbtodb)
#define EXT2_DBTOFSB(fs, b)	((b) >> (fs)->e2fs_fsbtodb)

/*
 * Macros for handling inode numbers:
 *	 inode number to file system block offset.
 *	 inode number to cylinder group number.
 *	 inode number to file system block address.
 */
#define	ino_to_cg(fs, x)	(((x) - 1) / (fs)->e2fs.e2fs_ipg)
#define	ino_to_fsba(fs, x)						\
	((fs)->e2fs_gd[ino_to_cg((fs), (x))].ext2bgd_i_tables +		\
	(((x) - 1) % (fs)->e2fs.e2fs_ipg) / (fs)->e2fs_ipb)
#define	ino_to_fsbo(fs, x)	(((x) - 1) % (fs)->e2fs_ipb)

/*
 * Give cylinder group number for a file system block.
 * Give cylinder group block number for a file system block.
 */
#define	dtog(fs, d) (((d) - (fs)->e2fs.e2fs_first_dblock) / (fs)->e2fs.e2fs_fpg)
#define	dtogd(fs, d) \
	(((d) - (fs)->e2fs.e2fs_first_dblock) % (fs)->e2fs.e2fs_fpg)

/*
 * The following macros optimize certain frequently calculated
 * quantities by using shifts and masks in place of divisions
 * modulos and multiplications.
 */
#define ext2_blkoff(fs, loc)	/* calculates (loc % fs->e2fs_bsize) */ \
	((loc) & (fs)->e2fs_qbmask)
#define ext2_lblktosize(fs, blk) /* calculates (blk * fs->e2fs_bsize) */ \
	((blk) << (fs)->e2fs_bshift)
#define ext2_lblkno(fs, loc)	/* calculates (loc / fs->e2fs_bsize) */ \
	((loc) >> (fs)->e2fs_bshift)
#define ext2_blkroundup(fs, size) /* calculates roundup(size, fs->e2fs_bsize) */ \
	(((size) + (fs)->e2fs_qbmask) & (fs)->e2fs_bmask)
#define ext2_fragroundup(fs, size) /* calculates roundup(size, fs->e2fs_bsize) */ \
	(((size) + (fs)->e2fs_qbmask) & (fs)->e2fs_bmask)
/*
 * Determine the number of available frags given a
 * percentage to hold in reserve.
 */
#define freespace(fs) \
   ((fs)->e2fs.e2fs_fbcount - (fs)->e2fs.e2fs_rbcount)

/*
 * Number of indirects in a file system block.
 */
#define	EXT2_NINDIR(fs)	((fs)->e2fs_bsize / sizeof(uint32_t))

#endif /* !_UFS_EXT2FS_EXT2FS_H_ */
