/* This file contains the file system call table.
 *
 * Created:
 *   April 2009 (D.C. van Moolenbroek)
 */

#define _TABLE
#include "inc.h"

struct fsdriver sffs_dtable = {
	.fdr_mount	= do_mount,
	.fdr_unmount	= do_unmount,
	.fdr_lookup	= do_lookup,
	.fdr_putnode	= do_putnode,
	.fdr_read	= do_read,
	.fdr_write	= do_write,
	.fdr_getdents	= do_getdents,
	.fdr_trunc	= do_trunc,
	.fdr_create	= do_create,
	.fdr_mkdir	= do_mkdir,
	.fdr_unlink	= do_unlink,
	.fdr_rmdir	= do_rmdir,
	.fdr_rename	= do_rename,
	.fdr_stat	= do_stat,
	.fdr_chmod	= do_chmod,
	.fdr_utime	= do_utime,
	.fdr_statvfs	= do_statvfs
};
