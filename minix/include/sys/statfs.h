/* Data for fstatfs() call. */

#ifndef _STATFS_H
#define _STATFS_H

#include <sys/cdefs.h>
#include <sys/types.h>

struct statfs {
  int f_bsize;		/* file system block size */
};

int fstatfs(int fd, struct statfs *st);

#endif /* _STATFS_H */
