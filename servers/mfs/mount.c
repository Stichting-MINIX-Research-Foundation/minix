

#include "fs.h"
#include <fcntl.h>
#include <string.h>
#include <minix/com.h>
#include <sys/stat.h>
#include "buf.h"
#include "inode.h"
#include "super.h"
#include "drivers.h"
#include <minix/vfsif.h>





/*===========================================================================*
 *				fs_readsuper				     *
 *===========================================================================*/
PUBLIC int fs_readsuper()
{
/* This function reads the superblock of the partition, gets the root inode
 * and sends back the details of them. Note, that the FS process does not
 * know the index of the vmnt object which refers to it, whenever the pathname 
 * lookup leaves a partition an ELEAVEMOUNT error is transferred back 
 * so that the VFS knows that it has to find the vnode on which this FS 
 * process' partition is mounted on.
 */
  struct super_block *xp, *sp;
  struct inode *root_ip;
  int r = OK;

  fs_dev = fs_m_in.REQ_DEV;

  /* Map the driver endpoint for this major */
  driver_endpoints[(fs_dev >> MAJOR) & BYTE].driver_e =  fs_m_in.REQ_DRIVER_E;
  boottime = fs_m_in.REQ_BOOTTIME;
  vfs_slink_storage = fs_m_in.REQ_SLINK_STORAGE;

  sp = &super_block[0];
  
  /* Fill in the super block. */
  sp->s_dev = fs_dev;		/* read_super() needs to know which dev */
  r = read_super(sp);

  /* Is it recognized as a Minix filesystem? */
  if (r != OK) {
	sp->s_dev = NO_DEV;
	return(r);
  }
  
  /* Get the root inode of the mounted file system. */
  root_ip = NIL_INODE;		/* if 'r' not OK, make sure this is defined */
  if (r == OK) {
	if ( (root_ip = get_inode(fs_dev, ROOT_INODE)) == NIL_INODE) 
		r = err_code;
  }
  
  if (root_ip != NIL_INODE && root_ip->i_mode == 0) {
        put_inode(root_ip);
  	r = EINVAL;
  }

  if (r != OK) return r;
  sp->s_rd_only = fs_m_in.REQ_READONLY;
  sp->s_is_root = fs_m_in.REQ_ISROOT;
  
  /* Root inode properties */
  fs_m_out.RES_INODE_NR = root_ip->i_num;
  fs_m_out.RES_MODE = root_ip->i_mode;
  fs_m_out.RES_FILE_SIZE = root_ip->i_size;

  /* Partition properties */
  fs_m_out.RES_MAXSIZE = sp->s_max_size;
  fs_m_out.RES_BLOCKSIZE = sp->s_block_size;
  
  return r;
}


/*===========================================================================*
 *				fs_mountpoint				     *
 *===========================================================================*/
PUBLIC int fs_mountpoint()
{
/* This function looks up the mount point, it checks the condition whether
 * the partition can be mounted on the inode or not. If ok, it gets the
 * mountpoint inode's details and stores the mounted vmnt's index (in the
 * vmnt table) so that it can be transferred back when the pathname lookup
 * encounters a mountpoint.
 */
  register struct inode *rip;
  int r = OK;
  mode_t bits;
  
  /* Get inode */
  caller_uid = fs_m_in.REQ_UID;
  caller_gid = fs_m_in.REQ_GID;
  
  /* Temporarily open the file. */
  if ( (rip = get_inode(fs_dev, fs_m_in.REQ_INODE_NR)) == NIL_INODE) {
printf("MFS(%d) get_inode by fs_mountpoint() failed\n", SELF_E);
        return(EINVAL);
  }

  /* It may not be busy. */
  if (rip->i_count > 1) r = EBUSY;

  /* It may not be special. */
  bits = rip->i_mode & I_TYPE;
  if (bits == I_BLOCK_SPECIAL || bits == I_CHAR_SPECIAL) r = ENOTDIR;
	
  if ((rip->i_mode & I_TYPE) != I_DIRECTORY) r = ENOTDIR;
      
  if (r != OK) {
      put_inode(rip);
      return r;
  }
  
  rip->i_mount = I_MOUNT;
  rip->i_vmnt_ind = fs_m_in.REQ_VMNT_IND;

  fs_m_out.m_source = rip->i_dev;/* Filled with the FS endp by the system */
  fs_m_out.RES_INODE_NR = rip->i_num;
  fs_m_out.RES_FILE_SIZE = rip->i_size;
  fs_m_out.RES_MODE = rip->i_mode;

  return r;
}


/*===========================================================================*
 *				fs_unmount				     *
 *===========================================================================*/
PUBLIC int fs_unmount()
{
/* Unmount a file system by device number. */
  struct super_block *sp, *sp1;
  int count;
  register struct inode *rip;

  /* !!!!!!!!!!!!! REMOVE THIS LATER !!!!!!!!!!!!!!!!!!!!!!! */
  /* Find the super block. */
  sp = NIL_SUPER;
  for (sp1 = &super_block[0]; sp1 < &super_block[NR_SUPERS]; sp1++) {
	if (sp1->s_dev == fs_dev) {
		sp = sp1;
		break;
	}
  }
  if (sp == NIL_SUPER) {
  	return(EINVAL);
  }
  /* !!!!!!!!!!!!! REMOVE THIS LATER !!!!!!!!!!!!!!!!!!!!!!! */
  
  /* See if the mounted device is busy.  Only 1 inode using it should be
   * open -- the root inode -- and that inode only 1 time.
   */
  count = 0;
  for (rip = &inode[0]; rip < &inode[NR_INODES]; rip++) {
	if (rip->i_count > 0 && rip->i_dev == fs_dev) {
/*printf("FSunmount DEV: %d inode: %d count: %d iaddr: %d\n", 
		rip->i_dev, rip->i_num, rip->i_count, rip);*/	
		count += rip->i_count;
	}
  }
  
  if (count > 1) {
      printf("MFS(%d) unmount: filesystem is busy %d\n", SELF_E, count);
      return(EBUSY);	/* can't umount a busy file system */
  }

  /* Put the root inode */
  rip = get_inode(fs_dev, ROOT_INODE);
  put_inode(rip);
  put_inode(rip);

  /* Sync the disk, and invalidate cache. */
  (void) fs_sync();		/* force any cached blocks out of memory */
  /*invalidate(fs_dev);*/	/* invalidate cache entries for this dev */

  /* Finish off the unmount. */
  sp->s_dev = NO_DEV;
  

  return OK;
}



