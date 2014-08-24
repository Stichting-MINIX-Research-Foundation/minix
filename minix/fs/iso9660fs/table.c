
/*
 * This file contains the table used to map system call numbers onto the
 * routines that perform them.
 */

#define _TABLE

#include "inc.h"

struct fsdriver isofs_table = {
	.fdr_mount	= fs_mount,
	.fdr_unmount	= fs_unmount,
	.fdr_lookup	= fs_lookup,
	.fdr_putnode	= fs_putnode,
	.fdr_read	= fs_read,
#if 0 /* FIXME: isofs uses subpage block sizes */
	.fdr_peek	= fs_read,
#endif
	.fdr_getdents	= fs_getdents,
	.fdr_rdlink	= fs_rdlink,
	.fdr_stat	= fs_stat,
	.fdr_mountpt	= fs_mountpt,
	.fdr_statvfs	= fs_statvfs,
	.fdr_driver	= lmfs_driver,
	.fdr_bread	= lmfs_bio,
	.fdr_bwrite	= lmfs_bio,
#if 0 /* FIXME: isofs uses subpage block sizes */
	.fdr_bpeek	= lmfs_bio,
#endif
	.fdr_bflush	= lmfs_bflush
};
