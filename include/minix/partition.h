/*	minix/partition.h				Author: Kees J. Bot
 *								7 Dec 1995
 * Place of a partition on disk and the disk geometry,
 * for use with the DIOCGETP and DIOCSETP ioctl's.
 */
#ifndef _MINIX__PARTITION_H
#define _MINIX__PARTITION_H

#include <sys/types.h>

#include <minix/u64.h>

int minix_sizeup(char *name, u64_t *bytes);

struct partition {
  u64_t base;		/* byte offset to the partition start */
  u64_t size;		/* number of bytes in the partition */
  unsigned cylinders;	/* disk geometry */
  unsigned heads;
  unsigned sectors;
};

#endif /* _MINIX__PARTITION_H */
