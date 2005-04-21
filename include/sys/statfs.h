/* Data for fstatfs() call. */

#ifndef _STATFS_H
#define _STATFS_H

#ifndef _TYPES_H
#include <sys/types.h>
#endif

struct statfs {
  off_t f_bsize;		/* file system block size */
};

_PROTOTYPE( int fstatfs, (int fd, struct statfs *st)		);

#endif /* _STATFS_H */
