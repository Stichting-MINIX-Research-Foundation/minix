#include <dirent.h>

PUBLIC struct buf {
  char b_data[_MAX_BLOCK_SIZE];		     /* ordinary user data */
  block_t b_blocknr;		/* block number of its (minor) device */
  char b_count;			/* number of users of this buffer */
} buf[NR_BUFS];

/* A block is free if b_dev == NO_DEV. */

#define INODE_BLOCK        0				 /* inode block */
#define DIRECTORY_BLOCK    1				 /* directory block */
