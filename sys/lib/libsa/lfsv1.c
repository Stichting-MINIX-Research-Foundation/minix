/* $NetBSD: lfsv1.c,v 1.14 2015/08/12 18:28:01 dholland Exp $ */

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
#if defined(__minix) && defined(LIBSA_ENABLE_LOAD_MODS_OP)
#define ufs_load_mods		lfsv1_load_mods
#endif /* defined(__minix) && defined(LIBSA_ENABLE_LOAD_MODS_OP) */
#endif

#define ufs_dinode		lfs32_dinode

#define	fs_bsize		lfs_dlfs_u.u_32.dlfs_ibsize

#define	INOPBx(fs) LFS_INOPB(fs)

#define UFS_NINDIR		LFS_NINDIR
#define ufs_blkoff(a, b)	lfs_blkoff((a), (b))
#define ufs_lblkno(a, b)	lfs_lblkno((a), (b))
#define dblksize(a, b, c)	lfs_dblksize((a), (b), (c))
#define	FSBTODB(fs, daddr)	(daddr)		/* LFSv1 uses sectors for addresses */

#define	FSMOD			"lfs"

#include "lib/libsa/ufs.c"
