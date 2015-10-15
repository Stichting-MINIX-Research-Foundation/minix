/* $NetBSD: ffsv1.c,v 1.6 2012/05/21 21:34:16 dsl Exp $ */

#define LIBSA_FFSv1

#define ufs_open	ffsv1_open
#define ufs_close	ffsv1_close
#define ufs_read	ffsv1_read
#define ufs_write	ffsv1_write
#define ufs_seek	ffsv1_seek
#define ufs_stat	ffsv1_stat
#if defined(LIBSA_ENABLE_LS_OP)
#define ufs_ls		ffsv1_ls
#if defined(__minix) && defined(LIBSA_ENABLE_LOAD_MODS_OP)
#define ufs_load_mods	ffsv1_load_mods
#endif /* defined(__minix) && defined(LIBSA_ENABLE_LOAD_MODS_OP) */
#endif

#define ufs_dinode	ufs1_dinode
#define indp_t		int32_t

#include "ufs.c"
