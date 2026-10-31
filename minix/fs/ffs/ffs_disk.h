/* On-disk format of the Berkeley Fast File System (UFS1 / UFS2).
 *
 * This header is a trimmed, native-little-endian distillation of the NetBSD
 * on-disk definitions found in sys/ufs/ffs/fs.h, sys/ufs/ufs/dinode.h and
 * sys/ufs/ufs/dir.h.  Big-endian and byte-swapping support has been removed:
 * this server only handles native little-endian images (see CLAUDE.md
 * "Endianness").  Both UFS1 (FFSv1) and UFS2 (FFSv2) on-disk layouts are
 * described; the server keeps a widened UFS2 dinode in core and converts
 * to/from the narrower UFS1 dinode at the rw_inode boundary (see inode.c).
 *
 * The structures below describe on-disk layout and therefore use fixed-width
 * types and must match the NetBSD/amd64 (LP64) layout exactly.
 */

#ifndef FFS_DISK_H
#define FFS_DISK_H

#include <sys/param.h>		/* DEV_BSIZE, NBBY, howmany() */

/*
 * Inode numbers.
 */
#define UFS_ROOTINO	((ino_t)2)	/* root directory inode */
#define UFS_WINO	((ino_t)1)	/* whiteout placeholder inode */

/*
 * Number of block pointers in an inode.
 */
#define UFS_NXADDR	2		/* external attribute blocks */
#define UFS_NDADDR	12		/* direct blocks */
#define UFS_NIADDR	3		/* indirect blocks (single/double/triple) */

/*
 * UFS2 on-disk inode (256 bytes).
 */
struct ufs2_dinode {
	u_int16_t	di_mode;	/*   0: IFMT, permissions */
	int16_t		di_nlink;	/*   2: file link count */
	u_int32_t	di_uid;		/*   4: file owner */
	u_int32_t	di_gid;		/*   8: file group */
	u_int32_t	di_blksize;	/*  12: inode blocksize */
	u_int64_t	di_size;	/*  16: file byte count */
	u_int64_t	di_blocks;	/*  24: bytes actually held */
	int64_t		di_atime;	/*  32: last access time */
	int64_t		di_mtime;	/*  40: last modified time */
	int64_t		di_ctime;	/*  48: last inode change time */
	int64_t		di_birthtime;	/*  56: inode creation time */
	int32_t		di_mtimensec;	/*  64: last modified time */
	int32_t		di_atimensec;	/*  68: last access time */
	int32_t		di_ctimensec;	/*  72: last inode change time */
	int32_t		di_birthnsec;	/*  76: inode creation time */
	int32_t		di_gen;		/*  80: generation number */
	u_int32_t	di_kernflags;	/*  84: kernel flags */
	u_int32_t	di_flags;	/*  88: status flags (chflags) */
	int32_t		di_extsize;	/*  92: external attributes size */
	int64_t		di_extb[UFS_NXADDR];/* 96: external attributes block */
	int64_t		di_db[UFS_NDADDR];  /* 112: direct disk blocks */
	int64_t		di_ib[UFS_NIADDR];  /* 208: indirect disk blocks */
	u_int64_t	di_modrev;	/* 232: i_modrev for NFSv4 */
	int64_t		di_spare[2];	/* 240: reserved; currently unused */
};

/* The first direct block doubles as the device number for specials. */
#define di_rdev		di_db[0]

#define UFS2_MAXSYMLINKLEN	((UFS_NDADDR + UFS_NIADDR) * sizeof(int64_t))

#define DINODE2_SIZE	(sizeof(struct ufs2_dinode))

/*
 * UFS1 on-disk inode (128 bytes).  Block pointers (di_db/di_ib) and the time
 * fields are 32-bit, unlike UFS2.  The server never stores this struct in
 * core: rw_inode() widens it into a struct ufs2_dinode on read and narrows it
 * back on write (inode.c: ufs1_to_ufs2 / ufs2_to_ufs1).
 */
struct ufs1_dinode {
	u_int16_t	di_mode;	/*   0: IFMT, permissions */
	int16_t		di_nlink;	/*   2: file link count */
	u_int16_t	di_oldids[2];	/*   4: obsolete (was uid/gid) */
	u_int64_t	di_size;	/*   8: file byte count */
	int32_t		di_atime;	/*  16: last access time */
	int32_t		di_atimensec;	/*  20: last access time */
	int32_t		di_mtime;	/*  24: last modified time */
	int32_t		di_mtimensec;	/*  28: last modified time */
	int32_t		di_ctime;	/*  32: last inode change time */
	int32_t		di_ctimensec;	/*  36: last inode change time */
	int32_t		di_db[UFS_NDADDR];  /* 40: direct disk blocks */
	int32_t		di_ib[UFS_NIADDR];  /* 88: indirect disk blocks */
	u_int32_t	di_flags;	/* 100: status flags (chflags) */
	u_int32_t	di_blocks;	/* 104: blocks actually held */
	int32_t		di_gen;		/* 108: generation number */
	u_int32_t	di_uid;		/* 112: file owner */
	u_int32_t	di_gid;		/* 116: file group */
	u_int64_t	di_modrev;	/* 120: i_modrev for NFSv4 */
};

#define UFS1_MAXSYMLINKLEN	((UFS_NDADDR + UFS_NIADDR) * sizeof(int32_t))

#define DINODE1_SIZE	(sizeof(struct ufs1_dinode))

/* fs_old_inodefmt: file systems at FS_44INODEFMT or newer use the modern
 * cylinder-group (cg_*off) and directory (d_type) layouts.  newfs and makefs
 * always write this format, for both UFS1 and UFS2. */
#define FS_42INODEFMT	(-1)
#define FS_44INODEFMT	2

/* File type bits in di_mode (identical to MINIX's I_* values). */
#define IFMT		0170000
#define IFIFO		0010000
#define IFCHR		0020000
#define IFDIR		0040000
#define IFBLK		0060000
#define IFREG		0100000
#define IFLNK		0120000
#define IFSOCK		0140000

/*
 * Superblock location.  UFS2 places the standard superblock 64 KB from the
 * start of the partition.  SBLOCKSIZE bytes are reserved for it on disk,
 * although the meaningful part is only sizeof(struct fs).
 */
#define SBLOCK_FLOPPY	0
#define SBLOCK_UFS1	8192
#define SBLOCK_UFS2	65536
#define SBLOCK_PIGGY	262144
#define SBLOCKSIZE	8192

/* Order in which to search for the superblock (UFS2 first, then the legacy
 * UFS1 location used by makefs, then the tiny-media and piggy locations). */
#define SBLOCKSEARCH \
	{ SBLOCK_UFS2, SBLOCK_UFS1, SBLOCK_FLOPPY, SBLOCK_PIGGY, -1 }

#define MAXMNTLEN	468
#define MAXVOLLEN	32
#define FSMAXSNAP	20
#define NOCSPTRS	((128 / sizeof(void *)) - 4)
#define MAXFRAG		8

/*
 * Per-cylinder-group summary information (global, kept at fs_csaddr).
 */
struct csum {
	int32_t	cs_ndir;	/* number of directories */
	int32_t	cs_nbfree;	/* number of free blocks */
	int32_t	cs_nifree;	/* number of free inodes */
	int32_t	cs_nffree;	/* number of free frags */
};

struct csum_total {
	int64_t	cs_ndir;	/* number of directories */
	int64_t	cs_nbfree;	/* number of free blocks */
	int64_t	cs_nifree;	/* number of free inodes */
	int64_t	cs_nffree;	/* number of free frags */
	int64_t	cs_spare[4];	/* future expansion */
};

/*
 * On-disk superblock.  Layout copied verbatim from sys/ufs/ffs/fs.h so it
 * matches the image byte-for-byte; fields prefixed fs_old_* are UFS1 legacy
 * and unused here.
 */
struct fs {
	int32_t	 fs_firstfield;		/* historic linked list */
	int32_t	 fs_unused_1;		/* used for incore super blocks */
	int32_t  fs_sblkno;		/* addr of super-block in filesys */
	int32_t  fs_cblkno;		/* offset of cyl-block in filesys */
	int32_t  fs_iblkno;		/* offset of inode-blocks in filesys */
	int32_t  fs_dblkno;		/* offset of first data after cg */
	int32_t	 fs_old_cgoffset;
	int32_t	 fs_old_cgmask;
	int32_t	 fs_old_time;
	int32_t	 fs_old_size;
	int32_t	 fs_old_dsize;
	u_int32_t fs_ncg;		/* number of cylinder groups */
	int32_t	 fs_bsize;		/* size of basic blocks in fs */
	int32_t	 fs_fsize;		/* size of frag blocks in fs */
	int32_t	 fs_frag;		/* number of frags in a block in fs */
	int32_t	 fs_minfree;		/* minimum percentage of free blocks */
	int32_t	 fs_old_rotdelay;
	int32_t	 fs_old_rps;
	int32_t	 fs_bmask;		/* ``blkoff'' calc of blk offsets */
	int32_t	 fs_fmask;		/* ``fragoff'' calc of frag offsets */
	int32_t	 fs_bshift;		/* ``lblkno'' calc of logical blkno */
	int32_t	 fs_fshift;		/* ``numfrags'' calc number of frags */
	int32_t	 fs_maxcontig;		/* max number of contiguous blks */
	int32_t	 fs_maxbpg;		/* max number of blks per cyl group */
	int32_t	 fs_fragshift;		/* block to frag shift */
	int32_t	 fs_fsbtodb;		/* fsbtodb and dbtofsb shift constant */
	int32_t	 fs_sbsize;		/* actual size of super block */
	int32_t	 fs_spare1[2];
	int32_t	 fs_nindir;		/* value of FFS_NINDIR */
	u_int32_t fs_inopb;		/* value of FFS_INOPB */
	int32_t	 fs_old_nspf;
	int32_t	 fs_optim;		/* optimization preference */
	int32_t	 fs_old_npsect;
	int32_t	 fs_old_interleave;
	int32_t	 fs_old_trackskew;
	int32_t	 fs_id[2];		/* unique file system id */
	int32_t  fs_old_csaddr;
	int32_t	 fs_cssize;		/* size of cyl grp summary area */
	int32_t	 fs_cgsize;		/* cylinder group size */
	int32_t	 fs_spare2;
	int32_t	 fs_old_nsect;
	int32_t	 fs_old_spc;
	int32_t	 fs_old_ncyl;
	int32_t	 fs_old_cpg;
	u_int32_t fs_ipg;		/* inodes per group */
	int32_t	 fs_fpg;		/* blocks per group * fs_frag */
	struct	csum fs_old_cstotal;
	int8_t	 fs_fmod;		/* super block modified flag */
	uint8_t	 fs_clean;		/* file system is clean flag */
	int8_t	 fs_ronly;		/* mounted read-only flag */
	uint8_t	 fs_old_flags;
	u_char	 fs_fsmnt[MAXMNTLEN];	/* name mounted on */
	u_char   fs_volname[MAXVOLLEN];	/* volume name */
	uint64_t fs_swuid;		/* system-wide uid */
	int32_t	 fs_pad;
	int32_t	 fs_cgrotor;
	void 	*fs_ocsp[NOCSPTRS];	/* padding; in-core only */
	u_int8_t *fs_contigdirs;	/* in-core only */
	struct csum *fs_csp;		/* in-core only */
	int32_t	*fs_maxcluster;		/* in-core only */
	u_char	*fs_active;		/* in-core only */
	int32_t	 fs_old_cpc;
	int32_t	 fs_maxbsize;		/* maximum blocking factor permitted */
	uint8_t	 fs_journal_version;
	uint8_t	 fs_journal_location;
	uint8_t	 fs_journal_reserved[2];
	uint32_t fs_journal_flags;
	uint64_t fs_journallocs[4];
	uint32_t fs_quota_magic;
	uint8_t  fs_quota_flags;
	uint8_t  fs_quota_reserved[3];
	uint64_t fs_quotafile[2];
	int64_t	 fs_sparecon64[9];
	int64_t	 fs_sblockloc;		/* byte offset of standard superblock */
	struct	csum_total fs_cstotal;	/* cylinder summary information */
	int64_t  fs_time;		/* last time written */
	int64_t	 fs_size;		/* number of blocks in fs */
	int64_t	 fs_dsize;		/* number of data blocks in fs */
	int64_t  fs_csaddr;		/* blk addr of cyl grp summary area */
	int64_t	 fs_pendingblocks;
	u_int32_t fs_pendinginodes;
	uint32_t fs_snapinum[FSMAXSNAP];
	u_int32_t fs_avgfilesize;
	u_int32_t fs_avgfpdir;
	int32_t	 fs_save_cgsize;
	int32_t	 fs_sparecon32[26];
	uint32_t fs_flags;		/* see FS_ flags below */
	int32_t	 fs_contigsumsize;	/* size of cluster summary array */
	int32_t	 fs_maxsymlinklen;	/* max length of an internal symlink */
	int32_t	 fs_old_inodefmt;
	u_int64_t fs_maxfilesize;	/* maximum representable file size */
	int64_t	 fs_qbmask;		/* ~fs_bmask for use with 64-bit size */
	int64_t	 fs_qfmask;		/* ~fs_fmask for use with 64-bit size */
	int32_t	 fs_state;
	int32_t	 fs_old_postblformat;
	int32_t	 fs_old_nrpos;
	int32_t  fs_spare5[2];
	int32_t	 fs_magic;		/* magic number */
};

/*
 * File system identification.
 */
#define FS_UFS1_MAGIC	0x011954
#define FS_UFS2_MAGIC	0x19540119
#define FS_UFS2EA_MAGIC	0x19012038

/* File system flags (in fs_flags) that we refuse to mount read-write. */
#define FS_DOSOFTDEP	0x002
#define FS_POSIX1EACLS	0x010
#define FS_DOWAPBL	0x100
#define FS_DOQUOTA2	0x200
#define FS_NFS4ACLS	0x800

/* fs_clean values. */
#define FS_ISCLEAN	0x01
#define FS_WASCLEAN	0x02

#define MINBSIZE	4096

/*
 * Cylinder group on-disk structure.  Maps are reached through the
 * cg_*off offsets; cg_inosused()/cg_blksfree() below resolve them.
 */
#define CG_MAGIC	0x090255
struct cg {
	int32_t	 cg_firstfield;
	int32_t	 cg_magic;		/* magic number */
	int32_t	 cg_old_time;
	u_int32_t cg_cgx;		/* we are the cgx'th cylinder group */
	int16_t	 cg_old_ncyl;
	int16_t	 cg_old_niblk;
	u_int32_t cg_ndblk;		/* number of data blocks this cg */
	struct	 csum cg_cs;		/* cylinder summary information */
	u_int32_t cg_rotor;		/* position of last used block */
	u_int32_t cg_frotor;		/* position of last used frag */
	u_int32_t cg_irotor;		/* position of last used inode */
	u_int32_t cg_frsum[MAXFRAG];	/* counts of available frags */
	int32_t	 cg_old_btotoff;
	int32_t	 cg_old_boff;
	u_int32_t cg_iusedoff;		/* (u_int8) used inode map */
	u_int32_t cg_freeoff;		/* (u_int8) free block map */
	u_int32_t cg_nextfreeoff;
	u_int32_t cg_clustersumoff;
	u_int32_t cg_clusteroff;
	u_int32_t cg_nclusterblks;
	u_int32_t cg_niblk;		/* number of inode blocks this cg */
	u_int32_t cg_initediblk;	/* last initialized inode */
	int32_t	 cg_sparecon32[3];
	int64_t  cg_time;		/* time last written */
	int64_t  cg_sparecon64[3];
	u_int8_t cg_space[1];		/* space for cylinder group maps */
	/* actually longer */
};

/* UFS2 cylinder groups always use the new (cg_*off) layout. */
#define cg_inosused(cgp) \
	((u_int8_t *)((u_int8_t *)(cgp) + (cgp)->cg_iusedoff))
#define cg_blksfree(cgp) \
	((u_int8_t *)((u_int8_t *)(cgp) + (cgp)->cg_freeoff))
#define cg_chkmagic(cgp) ((cgp)->cg_magic == CG_MAGIC)

/*
 * Convert a file-system frag number into a device (DEV_BSIZE) block number.
 * The non-_KERNEL form (using fs_fsbtodb) is used: this server compiles with
 * _SYSTEM, not _KERNEL.
 */
#define FFS_FSBTODB(fs, b)	((b) << (fs)->fs_fsbtodb)
#define FFS_DBTOFSB(fs, b)	((b) >> (fs)->fs_fsbtodb)

/*
 * Cylinder-group locating macros (UFS2: cgstart == cgbase).
 */
#define cgbase(fs, c)	(((daddr_t)(fs)->fs_fpg) * (c))
#define cgstart(fs, c)	cgbase((fs), (c))
#define cgdmin(fs, c)	(cgstart(fs, c) + (fs)->fs_dblkno)	/* 1st data */
#define cgimin(fs, c)	(cgstart(fs, c) + (fs)->fs_iblkno)	/* inode blk */
#define cgsblock(fs, c)	(cgstart(fs, c) + (fs)->fs_sblkno)	/* super blk */
#define cgtod(fs, c)	(cgstart(fs, c) + (fs)->fs_cblkno)	/* cg block */

/*
 * Inode number -> cylinder group / frag address / offset within block.
 */
#define ino_to_cg(fs, x)	(((ino_t)(x)) / (fs)->fs_ipg)
#define ino_to_fsba(fs, x)						\
	((daddr_t)(cgimin(fs, ino_to_cg(fs, (ino_t)(x))) +		\
	    (ffs_blkstofrags((fs), ((((ino_t)(x)) % (fs)->fs_ipg) /	\
	    FFS_INOPB(fs))))))
#define ino_to_fsbo(fs, x)	(((ino_t)(x)) % FFS_INOPB(fs))

/*
 * Frag -> cylinder group, and frag offset within a cylinder group.
 */
#define dtog(fs, d)	((d) / (fs)->fs_fpg)
#define dtogd(fs, d)	((d) % (fs)->fs_fpg)

/*
 * Block / fragment arithmetic (64-bit safe forms).
 */
#define ffs_blkoff(fs, loc)	((loc) & (fs)->fs_qbmask)
#define ffs_fragoff(fs, loc)	((loc) & (fs)->fs_qfmask)
#define ffs_lblktosize(fs, blk)	((uint64_t)(((off_t)(blk)) << (fs)->fs_bshift))
#define ffs_lblkno(fs, loc)	((loc) >> (fs)->fs_bshift)
#define ffs_numfrags(fs, loc)	((loc) >> (fs)->fs_fshift)
#define ffs_blkroundup(fs, size) (((size) + (fs)->fs_qbmask) & (fs)->fs_bmask)
#define ffs_fragroundup(fs, size) (((size) + (fs)->fs_qfmask) & (fs)->fs_fmask)
#define ffs_fragstoblks(fs, frags) ((frags) >> (fs)->fs_fragshift)
#define ffs_blkstofrags(fs, blks) ((blks) << (fs)->fs_fragshift)
#define ffs_fragnum(fs, fsb)	((fsb) & ((fs)->fs_frag - 1))
#define ffs_blknum(fs, fsb)	((fsb) &~ ((fs)->fs_frag - 1))

/* Number of inodes / indirects in a file-system block. */
#define FFS_INOPB(fs)	((fs)->fs_inopb)
#define FFS_NINDIR(fs)	((fs)->fs_nindir)

/*
 * On-disk block-pointer / indirect-entry width.  UFS1 stores 32-bit block
 * addresses in di_ib[] targets and indirect blocks; UFS2 stores 64-bit ones.
 * These helpers read and write a single entry from a buffer at native width so
 * the shared read/write/truncate code stays format-agnostic.  Block numbers
 * are non-negative frag numbers (NO_BLOCK == 0), so the widening is a plain
 * sign-extend of a positive value.
 */
#define FFS_DADDRSIZE(fs) \
	(((fs)->fs_magic == FS_UFS1_MAGIC) ? sizeof(int32_t) : sizeof(int64_t))

static __inline int64_t
ffs_getdaddr(const struct fs *fs, const void *p)
{
	if (fs->fs_magic == FS_UFS1_MAGIC)
		return (int64_t) *(const int32_t *) p;
	return *(const int64_t *) p;
}

static __inline void
ffs_putdaddr(const struct fs *fs, void *p, int64_t v)
{
	if (fs->fs_magic == FS_UFS1_MAGIC)
		*(int32_t *) p = (int32_t) v;
	else
		*(int64_t *) p = v;
}

/* Convert a byte count to a number of DEV_BSIZE (512-byte) blocks; di_blocks
 * is maintained in these units. */
#ifndef btodb
#define btodb(bytes)	((bytes) >> DEV_BSHIFT)
#endif

/* Extract the (fs_frag-wide) bit pattern for the block at bit offset loc. */
#define blkmap(fs, map, loc) \
	(((map)[(loc) / NBBY] >> ((loc) % NBBY)) & (0xff >> (NBBY - (fs)->fs_frag)))

/*
 * Size of the file block holding logical block lbn (the last block of a file
 * may be a fragment).
 */
#define ffs_blksize(fs, dsize, lbn) \
	(((lbn) >= UFS_NDADDR || (uint64_t)(dsize) >= ffs_lblktosize(fs, (lbn) + 1)) \
	    ? (uint64_t)(fs)->fs_bsize \
	    : ((uint64_t)ffs_fragroundup(fs, ffs_blkoff(fs, (uint64_t)(dsize)))))

/*
 * Directory layout (sys/ufs/ufs/dir.h).  UFS2 always uses the new format
 * with a valid d_type field.
 */
#define UFS_DIRBLKSIZ	DEV_BSIZE
#define FFS_MAXNAMLEN	255

#define d_ino d_fileno
struct direct {
	u_int32_t d_fileno;		/* inode number of entry */
	u_int16_t d_reclen;		/* length of this record */
	u_int8_t  d_type;		/* file type, see DT_* */
	u_int8_t  d_namlen;		/* length of string in d_name */
	char	  d_name[FFS_MAXNAMLEN + 1];
};

/* Minimum record length needed to hold a name of the given length. */
#define UFS_DIRECTSIZ(namlen) \
	((sizeof(struct direct) - (FFS_MAXNAMLEN+1)) + (((namlen)+1 + 3) &~ 3))
#define UFS_DIRSIZ(dp)	UFS_DIRECTSIZ((dp)->d_namlen)

/* Convert between an inode mode and a directory-entry type (d_type). */
#define FFS_IFTODT(mode)	(((mode) & 0170000) >> 12)

#endif /* FFS_DISK_H */
