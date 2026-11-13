#include "fs.h"
#include "inode.h"
#include "super.h"
#include <sys/time.h>
#include <sys/stat.h>

/*===========================================================================*
 *				fs_utime				     *
 *===========================================================================*/
int fs_utime(ino_t ino_nr, struct timespec *atime, struct timespec *mtime)
{
  struct inode *rip;

  if ((rip = get_inode(fs_dev, ino_nr)) == NULL)
	return(EINVAL);

  if (rip->i_sp->s_rd_only) {
	put_inode(rip);
	return(EROFS);
  }

  rip->i_update = CTIME;		/* discard any stale ATIME/MTIME flags */

  switch (atime->tv_nsec) {
  case UTIME_NOW:
	rip->i_update |= ATIME;
	break;
  case UTIME_OMIT:			/* do not touch */
	break;
  default:
	rip->i_din.di_atime = atime->tv_sec;
	rip->i_din.di_atimensec = atime->tv_nsec;
	break;
  }

  switch (mtime->tv_nsec) {
  case UTIME_NOW:
	rip->i_update |= MTIME;
	break;
  case UTIME_OMIT:			/* do not touch */
	break;
  default:
	rip->i_din.di_mtime = mtime->tv_sec;
	rip->i_din.di_mtimensec = mtime->tv_nsec;
	break;
  }

  rip->i_dirt = IN_DIRTY;

  put_inode(rip);
  return(OK);
}
