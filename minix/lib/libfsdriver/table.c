
#include "fsdriver.h"

#define CALL(n) [((n) - FS_BASE)]

int (*fsdriver_callvec[NREQS])(const struct fsdriver * __restrict,
    const message * __restrict, message * __restrict) = {
	CALL(REQ_PUTNODE)	= fsdriver_putnode,
	CALL(REQ_SLINK)		= fsdriver_slink,
	CALL(REQ_FTRUNC)	= fsdriver_trunc,
	CALL(REQ_CHOWN)		= fsdriver_chown,
	CALL(REQ_CHMOD)		= fsdriver_chmod,
	CALL(REQ_INHIBREAD)	= fsdriver_inhibread,
	CALL(REQ_STAT)		= fsdriver_stat,
	CALL(REQ_UTIME)		= fsdriver_utime,
	CALL(REQ_STATVFS)	= fsdriver_statvfs,
	CALL(REQ_BREAD)		= fsdriver_bread,
	CALL(REQ_BWRITE)	= fsdriver_bwrite,
	CALL(REQ_UNLINK)	= fsdriver_unlink,
	CALL(REQ_RMDIR)		= fsdriver_rmdir,
	CALL(REQ_UNMOUNT)	= fsdriver_unmount,
	CALL(REQ_SYNC)		= fsdriver_sync,
	CALL(REQ_NEW_DRIVER)	= fsdriver_newdriver,
	CALL(REQ_FLUSH)		= fsdriver_flush,
	CALL(REQ_READ)		= fsdriver_read,
	CALL(REQ_WRITE)		= fsdriver_write,
	CALL(REQ_MKNOD)		= fsdriver_mknod,
	CALL(REQ_MKDIR)		= fsdriver_mkdir,
	CALL(REQ_CREATE)	= fsdriver_create,
	CALL(REQ_LINK)		= fsdriver_link,
	CALL(REQ_RENAME)	= fsdriver_rename,
	CALL(REQ_LOOKUP)	= fsdriver_lookup,
	CALL(REQ_MOUNTPOINT)	= fsdriver_mountpoint,
	CALL(REQ_READSUPER)	= fsdriver_readsuper,
	CALL(REQ_NEWNODE)	= fsdriver_newnode,
	CALL(REQ_RDLINK)	= fsdriver_rdlink,
	CALL(REQ_GETDENTS)	= fsdriver_getdents,
	CALL(REQ_PEEK)		= fsdriver_peek,
	CALL(REQ_BPEEK)		= fsdriver_bpeek
};
