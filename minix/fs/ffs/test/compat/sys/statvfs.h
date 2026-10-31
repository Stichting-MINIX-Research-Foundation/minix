#ifndef SHIM_SYS_STATVFS_H
#define SHIM_SYS_STATVFS_H
#include <minixshim.h>
#define ST_NOTRUNC 0
struct statvfs {
	unsigned long f_flag, f_bsize, f_frsize, f_iosize;
	uint64_t f_blocks, f_bfree, f_bavail, f_files, f_ffree, f_favail;
	unsigned long f_namemax;
};
#endif
