/*	$NetBSD: minixfs3.h,v 1.5 2013/06/23 07:28:36 dholland Exp $ */

/*-
 * Copyright (c) 2012
 *	Vrije Universiteit, Amsterdam, The Netherlands. All rights reserved.
 *
 * Author: Evgeniy Ivanov
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

#ifndef MINIX_FS_3_H
#define MINIX_FS_3_H

FS_DEF(minixfs3);

#if !defined(__minix)
typedef uint32_t zone_t;
typedef uint16_t zone1_t;
typedef uint32_t block_t;
#endif /* !defined(__minix) */

#define NR_DZONES	7	/* # direct zone numbers in an inode */
#define NR_TZONES	10	/* total # zone numbers in an inode */
#define NIADDR		2       /* Indirect addresses in inode */

struct mfs_dinode {
	uint16_t  mdi_mode;	/* file type, protection, etc. */
	uint16_t  mdi_nlinks;	/* how many links to this file */
	int16_t   mdi_uid;	/* user id of the file's owner */
	uint16_t  mdi_gid;	/* group number */
	uint32_t  mdi_size;	/* current file size in bytes */
	uint32_t  mdi_atime;	/* time of last access */
	uint32_t  mdi_mtime;	/* when was file data last changed */
	uint32_t  mdi_ctime;	/* when was inode itself changed */
	zone_t    mdi_zone[NR_TZONES]; /* zone numbers for direct, ind, and
						dbl ind */
};

/* Maximum Minix MFS on-disk directory filename.
 * MFS uses 'struct direct' to write and parse
 * directory entries, so this can't be changed
 * without breaking filesystems.
 */
#define MFS_DIRSIZ	60

struct mfs_direct {
	uint32_t  mfsd_ino;
	char      mfsd_name[MFS_DIRSIZ];
} __packed;

struct mfs_sblock {
	uint32_t  mfs_ninodes;		/* # usable inodes on the minor device */
	zone1_t   mfs_nzones;		/* total device size, including bit maps etc */
	int16_t   mfs_imap_blocks;	/* # of blocks used by inode bit map */
	int16_t   mfs_zmap_blocks;	/* # of blocks used by zone bit map */
	zone1_t   mfs_firstdatazone_old;/* number of first data zone (small) */
	int16_t   mfs_log_zone_size;	/* log2 of blocks/zone */
	int16_t   mfs_pad;		/* try to avoid compiler-dependent padding */
	int32_t   mfs_max_size;		/* maximum file size on this device */
	zone_t    mfs_zones;		/* number of zones (replaces s_nzones in V2) */
	int16_t   mfs_magic;		/* magic number to recognize super-blocks */
	int16_t   mfs_pad2;		/* try to avoid compiler-dependent padding */
	uint16_t  mfs_block_size;	/* block size in bytes. */
	char      mfs_disk_version;	/* filesystem format sub-version */

  /* The following items are only used when the super_block is in memory,
   * mfs_inodes_per_block must be the firs one (see SBSIZE)
   */
	unsigned mfs_inodes_per_block;	/* precalculated from magic number */
	zone_t   mfs_firstdatazone;	/* number of first data zone (big) */
	int32_t  mfs_bshift;		/* ``lblkno'' calc of logical blkno */
	int32_t  mfs_bmask;		/* ``blkoff'' calc of blk offsets */
	int64_t  mfs_qbmask;		/* ~fs_bmask - for use with quad size */
	int32_t  mfs_fsbtodb;		/* fsbtodb and dbtofsb shift constant */
};

#define LOG_MINBSIZE	10
#define MINBSIZE	(1 << LOG_MINBSIZE)

#define SUPER_MAGIC	0x4d5a	/* magic # for MFSv3 file systems */

#define ROOT_INODE	((uint32_t) 1)	/* inode number for root directory */
#define SUPER_BLOCK_OFF (1024)		/* bytes offset */
#define START_BLOCK	((block_t) 2)	/* first fs block (not counting SB) */

/* # bytes/dir entry */
#define DIR_ENTRY_SIZE		sizeof(struct mfs_direct)
/* # dir entries/blk */
#define NR_DIR_ENTRIES(fs)	((fs)->mfs_block_size/DIR_ENTRY_SIZE)
/* mfs_sblock on-disk part size */
#define SBSIZE			offsetof(struct mfs_sblock, mfs_inodes_per_block)

#define ZONE_NUM_SIZE		sizeof(zone_t) /* # bytes in zone  */
#define INODE_SIZE		sizeof(struct mfs_dinode) /* bytes in dsk ino */
/* # zones/indir block */
#define MFS_NINDIR(fs)		((fs)->mfs_block_size/ZONE_NUM_SIZE)

#define NO_ZONE			((zone_t) 0)	/* absence of a zone number */
#define NO_BLOCK		((block_t) 0)	/* absence of a block number */

/* Turn file system block numbers into disk block addresses */
#define MFS_FSBTODB(fs, b)	((b) << (fs)->mfs_fsbtodb)

#define	ino_to_fsba(fs, x)						\
	(((x) - 1) / (fs)->mfs_inodes_per_block +			\
	START_BLOCK + (fs)->mfs_imap_blocks + (fs)->mfs_zmap_blocks)
#define	ino_to_fsbo(fs, x)	(((x) - 1) % (fs)->mfs_inodes_per_block)

/*
 * MFS metadatas are stored in little-endian byte order. These macros
 * helps reading theses metadatas.
 */
#if BYTE_ORDER == LITTLE_ENDIAN
#	define fs2h16(x) (x)
#	define fs2h32(x) (x)
#	define mfs_sbload(old, new)	\
		memcpy((new), (old), SBSIZE);
#	define mfs_iload(old, new)	\
		memcpy((new),(old),sizeof(struct mfs_dinode))
#else
void minixfs3_sb_bswap(struct mfs_sblock *, struct mfs_sblock *);
void minixfs3_i_bswap(struct mfs_dinode *, struct mfs_dinode *);
#	define fs2h16(x) bswap16(x)
#	define fs2h32(x) bswap32(x)
#	define mfs_sbload(old, new) minixfs3_sb_bswap((old), (new))
#	define mfs_iload(old, new) minixfs3_i_bswap((old), (new))
#endif /* BYTE_ORDER == LITTLE_ENDIAN */

/*
 * The following macros optimize certain frequently calculated
 * quantities by using shifts and masks in place of divisions
 * modulos and multiplications.
 */
#define mfs_blkoff(fs, loc)	/* calculates (loc % fs->mfs_bsize) */ \
	((loc) & (fs)->mfs_qbmask)
#define mfs_lblkno(fs, loc)	/* calculates (loc / fs->mfs_bsize) */ \
	((loc) >> (fs)->mfs_bshift)

/* Flag bits for i_mode in the inode. */
#define I_TYPE          0170000 /* this field gives inode type */
#define I_UNIX_SOCKET   0140000 /* unix domain socket */
#define I_SYMBOLIC_LINK 0120000 /* file is a symbolic link */
#define I_REGULAR       0100000 /* regular file, not dir or special */
#define I_BLOCK_SPECIAL 0060000 /* block special file */
#define I_DIRECTORY     0040000 /* file is a directory */
#define I_CHAR_SPECIAL  0020000 /* character special file */
#define I_NAMED_PIPE    0010000 /* named pipe (FIFO) */
#define I_SET_UID_BIT   0004000 /* set effective uid_t on exec */
#define I_SET_GID_BIT   0002000 /* set effective gid_t on exec */
#define I_SET_STCKY_BIT 0001000 /* sticky bit */
#define ALL_MODES       0007777 /* all bits for user, group and others */
#define RWX_MODES       0000777 /* mode bits for RWX only */
#define R_BIT           0000004 /* Rwx protection bit */
#define W_BIT           0000002 /* rWx protection bit */
#define X_BIT           0000001 /* rwX protection bit */
#define I_NOT_ALLOC     0000000 /* this inode is free */

#endif /* MINIX_FS_3_H */
