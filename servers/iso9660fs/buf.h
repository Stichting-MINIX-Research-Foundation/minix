#include <sys/dir.h>			/* need struct direct */
#include <dirent.h>

PUBLIC struct buf {
  union {   
    char b__data[_MAX_BLOCK_SIZE];		     /* ordinary user data */
    struct direct b__dir[NR_DIR_ENTRIES(_MAX_BLOCK_SIZE)];/* directory block */
  } b;

  block_t b_blocknr;		/* block number of its (minor) device */
  char b_count;			/* number of users of this buffer */
} buf[NR_BUFS];

/* A block is free if b_dev == NO_DEV. */


/* These defs make it possible to use to bp->b_data instead of bp->b.b__data */
#define b_data   b.b__data
#define b_dir    b.b__dir

#define INODE_BLOCK        0				 /* inode block */
#define DIRECTORY_BLOCK    1				 /* directory block */
