#ifndef	_SYS_STATVFS_H_
#define	_SYS_STATVFS_H_

#include <sys/cdefs.h>
#include <sys/featuretest.h>
#include <sys/stdint.h>
#include <machine/ansi.h>
#include <sys/ansi.h>

#define	_VFS_NAMELEN	32
#define	_VFS_MNAMELEN	1024

#ifndef	fsblkcnt_t
typedef	__fsblkcnt_t	fsblkcnt_t;	/* fs block count (statvfs) */
#define	fsblkcnt_t	__fsblkcnt_t
#endif

#ifndef	fsfilcnt_t
typedef	__fsfilcnt_t	fsfilcnt_t;	/* fs file count */
#define	fsfilcnt_t	__fsfilcnt_t
#endif

#ifndef	uid_t
typedef	__uid_t		uid_t;		/* user id */
#define	uid_t		__uid_t
#endif

#ifdef	_BSD_SIZE_T_
typedef	_BSD_SIZE_T_		size_t;
#define	_SIZE_T
#undef	_BSD_SIZE_T_
#endif

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

/* Possible values for statvfs->f_flag */
#define ST_RDONLY 0x1
#define ST_NOSUID 0x2
#ifdef __minix
#define ST_NOTRUNC 0x4
#endif /* !__minix*/

__BEGIN_DECLS
int	statvfs(const char *__restrict, struct statvfs *__restrict);
int	fstatvfs(int, struct statvfs *);
__END_DECLS

#endif /* _SYS_STATVFS_H_ */
