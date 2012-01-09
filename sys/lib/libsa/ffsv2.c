/* $NetBSD: ffsv2.c,v 1.5 2011/12/25 06:09:08 tsutsui Exp $ */

#define LIBSA_FFSv2

#define ufs_open	ffsv2_open
#define ufs_close	ffsv2_close
#define ufs_read	ffsv2_read
#define ufs_write	ffsv2_write
#define ufs_seek	ffsv2_seek
#define ufs_stat	ffsv2_stat
#if defined(LIBSA_ENABLE_LS_OP)
#define ufs_ls		ffsv2_ls
#endif

#define ufs_dinode	ufs2_dinode
#define indp_t		int64_t

#define	FSMOD		"ffs"

#include "ufs.c"
