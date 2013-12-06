/* $NetBSD: lfsv1.c,v 1.9 2013/06/23 07:28:36 dholland Exp $ */

#define	LIBSA_LFS
#define	REQUIRED_LFS_VERSION	1

#define	ufs_open		lfsv1_open
#define	ufs_close		lfsv1_close
#define	ufs_read		lfsv1_read
#define	ufs_write		lfsv1_write
#define	ufs_seek		lfsv1_seek
#define	ufs_stat		lfsv1_stat
#if defined(LIBSA_ENABLE_LS_OP)
#define	ufs_ls			lfsv1_ls
#endif

#define ufs_dinode		ulfs1_dinode

#define	fs_bsize		lfs_ibsize
#define	IFILE_Vx		IFILE_V1

#define	INOPBx(fs) LFS_INOPB(fs)

#define UFS_NINDIR		LFS_NINDIR
#define ufs_blkoff(a, b)	lfs_blkoff((a), (b))
#define ufs_lblkno(a, b)	lfs_lblkno((a), (b))
#define dblksize(a, b, c)	lfs_dblksize((a), (b), (c))
#define	FSBTODB(fs, daddr)	(daddr)		/* LFSv1 uses sectors for addresses */

#define	FSMOD			"lfs"
#define	FSMOD2			"ffs"

#include "lib/libsa/ufs.c"
