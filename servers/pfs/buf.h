/* Buffer (block) cache.  
 */

struct buf {
  /* Data portion of the buffer. */
  char b_data[PIPE_BUF];     /* ordinary user data */

  /* Header portion of the buffer. */
  struct buf *b_next;           /* used to link all free bufs in a chain */
  struct buf *b_prev;           /* used to link all free bufs the other way */
  ino_t b_num;			/* inode number on minor device */
  dev_t b_dev;                  /* major | minor device where block resides */
  int b_bytes;                  /* Number of bytes allocated in bp */
  int b_count;			/* Number of users of this buffer */
};

/* A block is free if b_dev == NO_DEV. */

#define NIL_BUF ((struct buf *) 0)	/* indicates absence of a buffer */

#define BUFHASH(b) ((b) % NR_BUFS)

EXTERN struct buf *front;	/* points to least recently used free block */
EXTERN struct buf *rear;	/* points to most recently used free block */
EXTERN int bufs_in_use;		/* # bufs currently in use (not on free list)*/

