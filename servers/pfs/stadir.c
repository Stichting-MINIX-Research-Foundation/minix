#include "fs.h"
#include "inode.h"
#include <sys/stat.h>

FORWARD _PROTOTYPE( int stat_inode, (struct inode *rip, int who_e,
				     cp_grant_id_t gid)			);


/*===========================================================================*
 *				stat_inode				     *
 *===========================================================================*/
PRIVATE int stat_inode(rip, who_e, gid)
register struct inode *rip;	/* pointer to inode to stat */
int who_e;			/* Caller endpoint */
cp_grant_id_t gid;		/* grant for the stat buf */
{
/* Common code for stat and fstat system calls. */

  struct stat statbuf;
  int r, s;

  /* Update the atime, ctime, and mtime fields in the inode, if need be. */
  if (rip->i_update) update_times(rip);

  statbuf.st_dev = rip->i_dev;
  statbuf.st_ino = rip->i_num;
  statbuf.st_mode = rip->i_mode;
  statbuf.st_nlink = rip->i_nlinks;
  statbuf.st_uid = rip->i_uid;
  statbuf.st_gid = rip->i_gid;
  statbuf.st_rdev = (dev_t) 0;
  statbuf.st_size = rip->i_size;
  statbuf.st_mode &= ~I_REGULAR;	/* wipe out I_REGULAR bit for pipes */
  statbuf.st_atime = rip->i_atime;
  statbuf.st_mtime = rip->i_mtime;
  statbuf.st_ctime = rip->i_ctime;

  /* Copy the struct to user space. */
  r = sys_safecopyto(who_e, gid, 0, (vir_bytes) &statbuf,
  		(phys_bytes) sizeof(statbuf), D);
  
  return(r);
}


/*===========================================================================*
 *                             fs_stat					     *
 *===========================================================================*/
PUBLIC int fs_stat()
{
  register int r;              /* return value */
  register struct inode *rip;  /* target inode */

  if( (rip = find_inode(fs_m_in.REQ_INODE_NR)) == NIL_INODE) return(EINVAL);
  get_inode(rip->i_dev, rip->i_num);	/* mark inode in use */  
  r = stat_inode(rip, fs_m_in.m_source, fs_m_in.REQ_GRANT);
  put_inode(rip);			/* release the inode */
  return(r);
}

