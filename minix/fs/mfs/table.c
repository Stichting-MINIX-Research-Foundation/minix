
/* This file contains the table used to map file system calls onto the
 * routines that perform them.
 */

#define _TABLE

#include "fs.h"
#include "inode.h"
#include "buf.h"
#include "super.h"

struct fsdriver mfs_table = {
	.fdr_mount	= fs_mount,
	.fdr_unmount	= fs_unmount,
	.fdr_lookup	= fs_lookup,
	.fdr_putnode	= fs_putnode,
	.fdr_read	= fs_readwrite,
	.fdr_write	= fs_readwrite,
	.fdr_peek	= fs_readwrite,
	.fdr_getdents	= fs_getdents,
	.fdr_trunc	= fs_trunc,
	.fdr_seek	= fs_seek,
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
	.fdr_sync	= fs_sync,
	.fdr_driver	= lmfs_driver,
	.fdr_bread	= lmfs_bio,
	.fdr_bwrite	= lmfs_bio,
	.fdr_bpeek	= lmfs_bio,
	.fdr_bflush	= lmfs_bflush
};
