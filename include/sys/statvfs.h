/* Statvfs implementation corresponding to:
 * http://www.opengroup.org/onlinepubs/009695399/basedefs/sys/statvfs.h.html
 */

#ifndef _STATVFS_H
#define _STATVFS_H

#include <minix/types.h>


struct statvfs {
	unsigned long f_bsize;  /* File system block size. */
	unsigned long f_frsize; /* Fundamental file system block size. */
	fsblkcnt_t    f_blocks; /* Total number of blocks on file system */
			     /* in units of f_frsize. */
	fsblkcnt_t    f_bfree;  /* Total number of free blocks. */
	fsblkcnt_t    f_bavail; /* Number of free blocks available to */
	                        /* non-privileged process. */
	fsfilcnt_t    f_files;  /* Total number of file serial numbers. */
	fsfilcnt_t    f_ffree;  /* Total number of free file serial numbers. */
	fsfilcnt_t    f_favail; /* Number of file serial numbers available */
	                             /* to non-privileged process. */
	unsigned long f_fsid;        /* File system ID. */
	unsigned long f_flag;        /* Bit mask of f_flag values. */
	unsigned long f_namemax;     /* Maximum filename length. */
	unsigned char __padding[32]; /* Padding for future compatibility */
};

_PROTOTYPE( int fstatvfs, (int fd, struct statvfs *st)		    );
_PROTOTYPE( int statvfs,  (const char *path, struct statvfs *st));

/* Possible values for statvfs->f_flag */
#define ST_RDONLY	0x001	/* Read-only file system */
#define ST_NOSUID	0x002	/* Does not support the semantics of the
				 * ST_ISUID and ST_ISGID file mode bits. */
#define ST_NOTRUNC	0x004	/* File system does not truncate file names
				 * longer than NAME_MAX */

#endif /* _STAVTFS_H */
