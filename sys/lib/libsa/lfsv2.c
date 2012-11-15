/* $NetBSD: lfsv2.c,v 1.5 2012/05/21 21:34:16 dsl Exp $ */

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
#endif

#define	fs_bsize		lfs_bsize
#define	IFILE_Vx		IFILE

#ifdef LFS_IFILE_FRAG_ADDRESSING	/* XXX see sys/ufs/lfs/ -- not tested */
#define	INOPBx(fs) INOPF(fs)
#else
#define	INOPBx(fs) INOPB(fs)
#endif

#define	FSMOD			"lfs"
#define	FSMOD2			"ffs"

#include "lib/libsa/ufs.c"
