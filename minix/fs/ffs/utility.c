#include "fs.h"
#include "buf.h"
#include "inode.h"
#include "super.h"

/*===========================================================================*
 *				get_block				     *
 *===========================================================================*/
struct buf *get_block(dev_t dev, block64_t block, int how)
{
/* Wrapper routine for lmfs_get_block().  Like the ext2 server, this code does
 * not deal gracefully with block read errors anywhere, so to prevent silent
 * corruption we panic on an I/O failure here.
 */
  struct buf *bp;
  int r;

  if ((r = lmfs_get_block(&bp, dev, block, how)) != OK && r != ENOENT)
	panic("ffs: error getting block (%llu,%llu): %d",
	    (unsigned long long)dev, (unsigned long long)block, r);

  assert(r == OK || how == PEEK);

  return (r == OK) ? bp : NULL;
}
