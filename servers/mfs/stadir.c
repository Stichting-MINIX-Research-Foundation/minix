

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
			char *user_addr, int who_e)			);


/*===========================================================================*
 *				fs_getdir       			     *
 *===========================================================================*/
PUBLIC int fs_getdir()
{
  register int r;
  register struct inode *rip;

  struct inodelist *rlp;

  caller_uid = fs_m_in.REQ_UID;
  caller_gid = fs_m_in.REQ_GID;
  
  /* Try to open the new directory. */
  if ( (rip = get_inode(fs_dev, fs_m_in.REQ_INODE_NR)) == NIL_INODE) {
printf("MFS(%d) get_inode by fs_getdir() failed\n", SELF_E);
        return(EINVAL);
  }
  
  r = forbidden(rip, X_BIT);	/* check if dir is searchable */

  /* If error, return inode. */
  if (r != OK) {
	put_inode(rip);
	return(r);
  }
  
  /* If OK send back inode details */
  fs_m_out.m_source = rip->i_dev; /* filled with FS endpoint by the system */
  fs_m_out.RES_INODE_NR = rip->i_num;
  fs_m_out.RES_MODE = rip->i_mode;
  fs_m_out.RES_FILE_SIZE = rip->i_size;

/*
printf("MFS(%d): ", SELF_E);
for (rlp = &hash_inodes[0]; rlp < &hash_inodes[INODE_HASH_SIZE]; ++rlp) {
    	int elements = 0;
	LIST_FOREACH(rip, rlp, i_hash) elements++;
	printf("%d ", elements);
}
printf("\n");
*/
	
  return OK;  
}

/*===========================================================================*
 *				fs_stat					     *
 *===========================================================================*/
PUBLIC int fs_stat()
{
  register struct inode *rip;
  register int r;
  
  /* Both stat() and fstat() use the same routine to do the real work.  That
   * routine expects an inode, so acquire it temporarily.
   */
  caller_uid = fs_m_in.REQ_UID;
  caller_gid = fs_m_in.REQ_GID;
  
  if ( (rip = get_inode(fs_dev, fs_m_in.REQ_INODE_NR)) == NIL_INODE) {
printf("MFS(%d) get_inode by fs_stat() failed\n", SELF_E);
        return(EINVAL);
  }
  
  r = stat_inode(rip, 0, fs_m_in.REQ_USER_ADDR, fs_m_in.REQ_WHO_E);
  put_inode(rip);		/* release the inode */
  return(r);
}


/*===========================================================================*
 *				fs_fstat				     *
 *===========================================================================*/
PUBLIC int fs_fstat()
{
  int r;   
  struct inode *rip;

  /* Both stat() and fstat() use the same routine to do the real work.  That
   * routine expects an inode, so acquire it temporarily.
   */
  if ((rip = find_inode(fs_dev, fs_m_in.REQ_FD_INODE_NR))
          == NIL_INODE) {
      printf("FSfstat: couldn't find inode %d\n", fs_m_in.REQ_FD_INODE_NR);
      return EINVAL;
  }
  
  r = stat_inode(rip, fs_m_in.REQ_FD_POS, fs_m_in.REQ_FD_USER_ADDR, 
          fs_m_in.REQ_FD_WHO_E);

  return r;
}

/*===========================================================================*
 *				stat_inode				     *
 *===========================================================================*/
PRIVATE int stat_inode(rip, pipe_pos, user_addr, who_e)
register struct inode *rip;	/* pointer to inode to stat */
int pipe_pos;   		/* position in a pipe, supplied by fstat() */
char *user_addr;		/* user space address where stat buf goes */
int who_e;                      /* kernel endpoint of the caller */
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
  r = sys_datacopy(SELF, (vir_bytes) &statbuf,
  		who_e, (vir_bytes) user_addr, (phys_bytes) sizeof(statbuf));
  
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
  
  if ((rip = find_inode(fs_dev, fs_m_in.REQ_FD_INODE_NR))
          == NIL_INODE) {
      printf("FSfstatfs: couldn't find inode %d\n", fs_m_in.REQ_FD_INODE_NR);
      return EINVAL;
  }
  
  st.f_bsize = rip->i_sp->s_block_size;
  
  /* Copy the struct to user space. */
  r = sys_datacopy(SELF, (vir_bytes) &st, fs_m_in.REQ_FD_WHO_E, 
          (vir_bytes) fs_m_in.REQ_FD_USER_ADDR, (phys_bytes) sizeof(st));
  
  return(r);
}

/*===========================================================================*
 *                             fs_lstat                                     *
 *===========================================================================*/
PUBLIC int fs_lstat()
{
  register int r;              /* return value */
  register struct inode *rip;  /* target inode */
  caller_uid = fs_m_in.REQ_UID;
  caller_gid = fs_m_in.REQ_GID;

  if ( (rip = get_inode(fs_dev, fs_m_in.REQ_INODE_NR)) == NIL_INODE) {
printf("MFS(%d) get_inode by fs_lstat() failed\n", SELF_E);
        return(EINVAL);
  }
  
  r = stat_inode(rip, 0, fs_m_in.REQ_USER_ADDR, fs_m_in.REQ_WHO_E);
  put_inode(rip);		/* release the inode */
  return(r);
}



