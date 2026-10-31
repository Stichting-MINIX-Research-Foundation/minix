#ifndef FFS_TYPE_H
#define FFS_TYPE_H

#include <minix/libminixfs.h>

#include "ffs_disk.h"		/* on-disk UFS2 layout (struct fs, dinode, ...) */

/* Structure with options affecting global behaviour. */
struct opt {
  int unused;		/* no tunables yet; reserved for future use */
};

#endif /* FFS_TYPE_H */
