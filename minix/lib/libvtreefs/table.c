/* VTreeFS - table.c - by Alen Stojanov and David van Moolenbroek */

#define _TABLE
#include "inc.h"

struct fsdriver vtreefs_table = {
	.fdr_mount	= fs_mount,
	.fdr_unmount	= fs_unmount,
	.fdr_lookup	= fs_lookup,
	.fdr_putnode	= fs_putnode,
	.fdr_read	= fs_read,
	.fdr_getdents	= fs_getdents,
	.fdr_rdlink	= fs_rdlink,
	.fdr_stat	= fs_stat,
	.fdr_statvfs	= fs_statvfs,
	.fdr_other	= fs_other
};
