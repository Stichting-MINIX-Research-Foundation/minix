#include "fs.h"
#include "buf.h"
#include "inode.h"
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>

static struct buf *new_block(dev_t dev, ino_t inum);

/*===========================================================================*
 *                              buf_pool                                     *
 *===========================================================================*/
void buf_pool(void)
{
/* Initialize the buffer pool. */

  front = NULL;
  rear = NULL;
}



/*===========================================================================*
 *				get_block				     *
 *===========================================================================*/
struct buf *get_block(dev_t dev, ino_t inum)
{
  struct buf *bp = front;

  while(bp != NULL) {
	if (bp->b_dev == dev && bp->b_num == inum) {
		bp->b_count++;
		return(bp);
	}
	bp = bp->b_next;
  }

  /* Buffer was not found. Try to allocate a new one */
  return new_block(dev, inum);
}


/*===========================================================================*
 *				new_block				     *
 *===========================================================================*/
static struct buf *new_block(dev_t dev, ino_t inum)
{
/* Allocate a new buffer and add it to the double linked buffer list */
  struct buf *bp;

  bp = malloc(sizeof(struct buf));
  if (bp == NULL) {
	err_code = ENOSPC;
	return(NULL);
  }
  bp->b_num = inum;
  bp->b_dev = dev;
  bp->b_bytes = 0;
  bp->b_count = 1;
  memset(bp->b_data, 0 , PIPE_BUF);

  /* Add at the end of the buffer */
  if (front == NULL) {	/* Empty list? */
	front = bp;
	bp->b_prev = NULL;
  } else {
	rear->b_next = bp;
	bp->b_prev = rear;
  }
  bp->b_next = NULL;
  rear = bp;

  return(bp);
}


/*===========================================================================*
 *				put_block				     *
 *===========================================================================*/
void put_block(dev_t dev, ino_t inum)
{
  struct buf *bp;

  bp = get_block(dev, inum);
  if (bp == NULL) return; /* We didn't find the block. Nothing to put. */

  bp->b_count--;	/* Compensate for above 'get_block'. */
  if (--bp->b_count > 0) return;

  /* Cut bp out of the loop */
  if (bp->b_prev == NULL)
	front = bp->b_next;
  else
	bp->b_prev->b_next = bp->b_next;

  if (bp->b_next == NULL)
	rear = bp->b_prev;
  else
	bp->b_next->b_prev = bp->b_prev;

  /* Buffer administration is done. Now it's safe to free up bp. */
  free(bp);
}
