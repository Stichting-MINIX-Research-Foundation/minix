#include "fs.h"
#include "inode.h"
#include <string.h>
#include <sys/stat.h>


/*===========================================================================*
 *				stat_inode				     *
 *===========================================================================*/
static int stat_inode(
  register struct inode *rip,	/* pointer to inode to stat */
  endpoint_t who_e,		/* Caller endpoint */
  cp_grant_id_t gid		/* grant for the stat buf */
)
{
/* Common code for stat and fstat system calls. */
  mode_t type;
  struct stat statbuf;
  u32_t blocks; /* The unit of this is 512 */
  int r, s;

  type = rip->i_mode & I_TYPE;
  s = (type == I_CHAR_SPECIAL || type == I_BLOCK_SPECIAL);

  /* Update the atime, ctime, and mtime fields in the inode, if need be. */
  if (rip->i_update) update_times(rip);

  blocks = rip->i_size / S_BLKSIZE;
  if (rip->i_size % S_BLKSIZE != 0)
	blocks += 1;

  memset(&statbuf, 0, sizeof(struct stat));

  statbuf.st_dev = rip->i_dev;
  statbuf.st_ino = rip->i_num;
  statbuf.st_mode = rip->i_mode;
  statbuf.st_nlink = rip->i_nlinks;
  statbuf.st_uid = rip->i_uid;
  statbuf.st_gid = (short int) rip->i_gid;
  statbuf.st_rdev = (dev_t) (s ? rip->i_rdev : NO_DEV);
  statbuf.st_size = rip->i_size;
  if (!s)  statbuf.st_mode &= ~I_REGULAR;/* wipe out I_REGULAR bit for pipes */
  statbuf.st_atime = rip->i_atime;
  statbuf.st_mtime = rip->i_mtime;
  statbuf.st_ctime = rip->i_ctime;
  statbuf.st_blksize = PIPE_BUF;
  statbuf.st_blocks = blocks;

  /* Copy the struct to user space. */
  r = sys_safecopyto(who_e, gid, (vir_bytes) 0, (vir_bytes) &statbuf,
		(size_t) sizeof(statbuf));

  return(r);
}


/*===========================================================================*
 *                             fs_stat					     *
 *===========================================================================*/
int fs_stat(message *fs_m_in, message *fs_m_out)
{
  register int r;              /* return value */
  register struct inode *rip;  /* target inode */

  if( (rip = find_inode(fs_m_in->REQ_INODE_NR)) == NULL) return(EINVAL);
  get_inode(rip->i_dev, rip->i_num);	/* mark inode in use */
  r = stat_inode(rip, fs_m_in->m_source, (cp_grant_id_t) fs_m_in->REQ_GRANT);
  put_inode(rip);			/* release the inode */
  return(r);
}
