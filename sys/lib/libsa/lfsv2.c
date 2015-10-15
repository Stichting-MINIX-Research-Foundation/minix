/* $NetBSD: lfsv2.c,v 1.14 2015/08/12 18:28:01 dholland Exp $ */

#define	LIBSA_LFS
#define	REQUIRED_LFS_VERSION	2

#define	ufs_open		lfsv2_open
#define	ufs_close		lfsv2_close
#define	ufs_read		lfsv2_read
#define	ufs_write		lfsv2_write
#define	ufs_seek		lfsv2_seek
#define	ufs_stat		lfsv2_stat
#if defined(LIBSA_ENABLE_LS_OP)
#define	ufs_ls			lfsv2_ls
#if defined(__minix) && defined(LIBSA_ENABLE_LOAD_MODS_OP)
#define ufs_load_mods		lfsv2_load_mods
#endif /* defined(__minix) && defined(LIBSA_ENABLE_LOAD_MODS_OP) */
#endif

#define ufs_dinode		lfs32_dinode

#define	fs_bsize		lfs_dlfs_u.u_32.dlfs_bsize

#ifdef LFS_IFILE_FRAG_ADDRESSING	/* XXX see sys/ufs/lfs/ -- not tested */
#define	INOPBx(fs) LFS_INOPF(fs)
#else
#define	INOPBx(fs) LFS_INOPB(fs)
#endif

#define UFS_NINDIR		LFS_NINDIR
#define ufs_blkoff(a, b)	lfs_blkoff((a), (b))
#define ufs_lblkno(a, b)	lfs_lblkno((a), (b))
#define dblksize(a, b, c)	lfs_dblksize((a), (b), (c))
#define FSBTODB(a, b)		LFS_FSBTODB((a), (b))

#define	FSMOD			"lfs"

#include "lib/libsa/ufs.c"
