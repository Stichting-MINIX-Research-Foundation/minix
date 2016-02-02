/* This file contains the table used to map system call numbers onto the
 * routines that perform them.
 *
 * Created (MFS based):
 *   February 2010 (Evgeniy Ivanov)
 */

#define _TABLE

#include "fs.h"

struct fsdriver puffs_table = {
	.fdr_mount	= fs_mount,
	.fdr_unmount	= fs_unmount,
	.fdr_lookup	= fs_lookup,
	.fdr_putnode	= fs_putnode,
	.fdr_read	= fs_read,
	.fdr_write	= fs_write,
	.fdr_getdents	= fs_getdents,
	.fdr_trunc	= fs_trunc,
	.fdr_create	= fs_create,
	.fdr_mkdir	= fs_mkdir,
	.fdr_mknod	= fs_mknod,
	.fdr_link	= fs_link,
	.fdr_unlink	= fs_unlink,
	.fdr_rmdir	= fs_unlink,
	.fdr_rename	= fs_rename,
	.fdr_slink	= fs_slink,
	.fdr_rdlink	= fs_rdlink,
	.fdr_stat	= fs_stat,
	.fdr_chown	= fs_chown,
	.fdr_chmod	= fs_chmod,
	.fdr_utime	= fs_utime,
	.fdr_mountpt	= fs_mountpt,
	.fdr_statvfs	= fs_statvfs,
	.fdr_sync	= fs_sync
};
