
/* This file contains the table used to map VFS/FS call numbers onto the
 * routines that perform them.
 */

#define _TABLE

#include "fs.h"

/* File System Handlers (pfs) */
struct fsdriver pfs_table = {
	.fdr_mount	= fs_mount,
	.fdr_unmount	= fs_unmount,
	.fdr_newnode	= fs_newnode,
	.fdr_putnode	= fs_putnode,
	.fdr_read	= fs_read,
	.fdr_write	= fs_write,
	.fdr_trunc	= fs_trunc,
	.fdr_stat	= fs_stat,
	.fdr_chmod	= fs_chmod
};
