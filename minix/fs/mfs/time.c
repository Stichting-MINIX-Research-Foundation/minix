#include "fs.h"
#include "inode.h"
#include <sys/time.h>
#include <sys/stat.h>


/*===========================================================================*
 *				fs_utime				     *
 *===========================================================================*/
int fs_utime(ino_t ino_nr, struct timespec *atime, struct timespec *mtime)
{
  register struct inode *rip;

  /* Temporarily open the file. */
  if( (rip = get_inode(fs_dev, ino_nr)) == NULL)
        return(EINVAL);

  rip->i_update = CTIME; /* discard any stale ATIME and MTIME flags */

  switch (atime->tv_nsec) {
  case UTIME_NOW:
	rip->i_update |= ATIME;
	break;
  case UTIME_OMIT: /* do not touch */
	break;
  default:
	/* MFS does not support subsecond resolution, so we round down. */
	rip->i_atime = atime->tv_sec;
	break;
  }

  switch (mtime->tv_nsec) {
  case UTIME_NOW:
	rip->i_update |= MTIME;
	break;
  case UTIME_OMIT: /* do not touch */
	break;
  default:
	/* MFS does not support subsecond resolution, so we round down. */
	rip->i_mtime = mtime->tv_sec;
	break;
  }

  IN_MARKDIRTY(rip);

  put_inode(rip);
  return(OK);
}

