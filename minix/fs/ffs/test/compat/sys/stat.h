#ifndef SHIM_SYS_STAT_H
#define SHIM_SYS_STAT_H
#include <minixshim.h>
struct stat {
	mode_t st_mode; nlink_t st_nlink; uid_t st_uid; gid_t st_gid;
	dev_t st_rdev; off_t st_size; long st_blksize; long long st_blocks;
	long st_atime; long st_atimensec; long st_mtime; long st_mtimensec;
	long st_ctime; long st_ctimensec; long st_birthtime; long st_birthtimensec;
};
#define S_IFMT  0170000
#define S_IFIFO 0010000
#define S_IFCHR 0020000
#define S_IFDIR 0040000
#define S_IFBLK 0060000
#define S_IFREG 0100000
#define S_IFLNK 0120000
#define S_IFSOCK 0140000
#define S_ISDIR(m)  (((m)&S_IFMT)==S_IFDIR)
#define S_ISCHR(m)  (((m)&S_IFMT)==S_IFCHR)
#define S_ISBLK(m)  (((m)&S_IFMT)==S_IFBLK)
#define S_ISREG(m)  (((m)&S_IFMT)==S_IFREG)
#define S_ISFIFO(m) (((m)&S_IFMT)==S_IFIFO)
#define S_ISLNK(m)  (((m)&S_IFMT)==S_IFLNK)
#define S_ISSOCK(m) (((m)&S_IFMT)==S_IFSOCK)
#endif
