

#include "fs.h"
#include <sys/stat.h>
#include <sys/statfs.h>
#include <minix/com.h>
#include <string.h>
#include "buf.h"
#include "inode.h"
#include "super.h"

#include <minix/vfsif.h>


FORWARD _PROTOTYPE( int stat_inode, (struct inode *rip, int pipe_pos,
			int who_e, cp_grant_id_t gid)			);


/*===========================================================================*
 *				stat_inode				     *
 *===========================================================================*/
PRIVATE int stat_inode(rip, pipe_pos, who_e, gid)
register struct inode *rip;	/* pointer to inode to stat */
int pipe_pos;   		/* position in a pipe, supplied by fstat() */
int who_e;			/* Caller endpoint */
cp_grant_id_t gid;		/* grant for the stat buf */
{
/* Common code for stat and fstat system calls. */

  struct stat statbuf;
  mode_t mo;
  int r, s;

  /* Update the atime, ctime, and mtime fields in the inode, if need be. */
  if (rip->i_update) update_times(rip);

  /* Fill in the statbuf struct. */
  mo = rip->i_mode & I_TYPE;

  /* true iff special */
  s = (mo == I_CHAR_SPECIAL || mo == I_BLOCK_SPECIAL);

  statbuf.st_dev = rip->i_dev;
  statbuf.st_ino = rip->i_num;
  statbuf.st_mode = rip->i_mode;
  statbuf.st_nlink = rip->i_nlinks;
  statbuf.st_uid = rip->i_uid;
  statbuf.st_gid = rip->i_gid;
  statbuf.st_rdev = (dev_t) (s ? rip->i_zone[0] : NO_DEV);
  statbuf.st_size = rip->i_size;

  if (rip->i_pipe == I_PIPE) {
	statbuf.st_mode &= ~I_REGULAR;	/* wipe out I_REGULAR bit for pipes */
	statbuf.st_size -= pipe_pos;
  }

  statbuf.st_atime = rip->i_atime;
  statbuf.st_mtime = rip->i_mtime;
  statbuf.st_ctime = rip->i_ctime;

  /* Copy the struct to user space. */
  r = sys_safecopyto(who_e, gid, 0, (vir_bytes) &statbuf,
  		(phys_bytes) sizeof(statbuf), D);
  
  return(r);
}


/*===========================================================================*
 *				fs_fstatfs				     *
 *===========================================================================*/
PUBLIC int fs_fstatfs()
{
  struct statfs st;
  struct inode *rip;
  int r;

  if ((rip = find_inode(fs_dev, ROOT_INODE)) == NIL_INODE) {
      printf("mfs:fstatfs: couldn't find inode %d\n", ROOT_INODE);
      return EINVAL;
  }
  
  st.f_bsize = rip->i_sp->s_block_size;
  
  /* Copy the struct to user space. */
  r = sys_safecopyto(fs_m_in.m_source, fs_m_in.REQ_GRANT, 0,
	(vir_bytes) &st, (phys_bytes) sizeof(st), D);
  
  return(r);
}

/*===========================================================================*
 *                             fs_stat					     *
 *===========================================================================*/
PUBLIC int fs_stat()
{
  register int r;              /* return value */
  register struct inode *rip;  /* target inode */

  if ( (rip = get_inode(fs_dev, fs_m_in.REQ_INODE_NR)) == NIL_INODE) {
printf("MFS(%d) get_inode by fs_stat() failed\n", SELF_E);
        return(EINVAL);
  }
  
  r = stat_inode(rip, 0, fs_m_in.m_source, fs_m_in.REQ_GRANT);
  put_inode(rip);		/* release the inode */
  return(r);
}



