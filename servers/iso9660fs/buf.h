#include <dirent.h>

#define b_data(bp) ((char *) (bp->data))

/* A block is free if b_dev == NO_DEV. */

#define INODE_BLOCK        0				 /* inode block */
#define DIRECTORY_BLOCK    1				 /* directory block */
