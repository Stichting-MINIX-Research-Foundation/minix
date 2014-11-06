/* VTreeFS - table.c - file system driver callback table */

#define _TABLE
#include "inc.h"

struct fsdriver vtreefs_table = {
	.fdr_mount	= fs_mount,
	.fdr_unmount	= fs_unmount,
	.fdr_lookup	= fs_lookup,
	.fdr_putnode	= fs_putnode,
	.fdr_read	= fs_read,
	.fdr_write	= fs_write,
	.fdr_getdents	= fs_getdents,
	.fdr_trunc	= fs_trunc,
	.fdr_mknod	= fs_mknod,
	.fdr_unlink	= fs_unlink,
	.fdr_slink	= fs_slink,
	.fdr_rdlink	= fs_rdlink,
	.fdr_stat	= fs_stat,
	.fdr_chmod	= fs_chmod,
	.fdr_chown	= fs_chown,
	.fdr_statvfs	= fs_statvfs,
	.fdr_other	= fs_other
};
