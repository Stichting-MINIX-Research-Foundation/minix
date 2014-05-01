#include "fs.h"
#include "inode.h"
#include "super.h"
#include <minix/vfsif.h>
#include <minix/bdev.h>

static int cleanmount = 1;

/*===========================================================================*
 *				fs_readsuper				     *
 *===========================================================================*/
int fs_readsuper()
{
/* This function reads the superblock of the partition, gets the root inode
 * and sends back the details of them. Note, that the FS process does not
 * know the index of the vmnt object which refers to it, whenever the pathname 
 * lookup leaves a partition an ELEAVEMOUNT error is transferred back 
 * so that the VFS knows that it has to find the vnode on which this FS 
 * process' partition is mounted on.
 */
  struct inode *root_ip;
  cp_grant_id_t label_gid;
  size_t label_len;
  int r;
  int readonly, isroot;

  fs_dev    = fs_m_in.m_vfs_fs_readsuper.device;
  label_gid = fs_m_in.m_vfs_fs_readsuper.grant;
  label_len = fs_m_in.m_vfs_fs_readsuper.path_len;
  readonly  = (fs_m_in.m_vfs_fs_readsuper.flags & REQ_RDONLY) ? 1 : 0;
  isroot    = (fs_m_in.m_vfs_fs_readsuper.flags & REQ_ISROOT) ? 1 : 0;

  if (label_len > sizeof(fs_dev_label))
	return(EINVAL);

  r = sys_safecopyfrom(fs_m_in.m_source, label_gid, (vir_bytes) 0,
		       (vir_bytes) fs_dev_label, label_len);
  if (r != OK) {
	printf("MFS %s:%d safecopyfrom failed: %d\n", __FILE__, __LINE__, r);
	return(EINVAL);
  }

  /* Map the driver label for this major. */
  bdev_driver(fs_dev, fs_dev_label);

  /* Open the device the file system lives on. */
  if (bdev_open(fs_dev, readonly ? BDEV_R_BIT : (BDEV_R_BIT|BDEV_W_BIT) ) !=
		OK) {
        return(EINVAL);
  }
  
  /* Fill in the super block. */
  superblock.s_dev = fs_dev;	/* read_super() needs to know which dev */
  r = read_super(&superblock);

  /* Is it recognized as a Minix filesystem? */
  if (r != OK) {
	superblock.s_dev = NO_DEV;
	bdev_close(fs_dev);
	return(r);
  }

  /* Remember whether we were mounted cleanly so we know what to
   * do at unmount time
   */
  if(superblock.s_flags & MFSFLAG_CLEAN)
	cleanmount = 1;

  /* clean check: if rw and not clean, switch to readonly */
  if(!(superblock.s_flags & MFSFLAG_CLEAN) && !readonly) {
	if(bdev_close(fs_dev) != OK)
		panic("couldn't bdev_close after found unclean FS");
	readonly = 1;

	if (bdev_open(fs_dev, BDEV_R_BIT) != OK) {
		panic("couldn't bdev_open after found unclean FS");
		return(EINVAL);
  	}
	printf("MFS: WARNING: FS 0x%llx unclean, mounting readonly\n", fs_dev);
  }
  
  lmfs_set_blocksize(superblock.s_block_size, major(fs_dev));
  
  /* Get the root inode of the mounted file system. */
  if( (root_ip = get_inode(fs_dev, ROOT_INODE)) == NULL)  {
	printf("MFS: couldn't get root inode\n");
	superblock.s_dev = NO_DEV;
	bdev_close(fs_dev);
	return(EINVAL);
  }
  
  if(root_ip->i_mode == 0) {
	printf("%s:%d zero mode for root inode?\n", __FILE__, __LINE__);
	put_inode(root_ip);
	superblock.s_dev = NO_DEV;
	bdev_close(fs_dev);
	return(EINVAL);
  }

  superblock.s_rd_only = readonly;
  superblock.s_is_root = isroot;
  
  /* Root inode properties */
  fs_m_out.m_fs_vfs_readsuper.inode = root_ip->i_num;
  fs_m_out.m_fs_vfs_readsuper.mode = root_ip->i_mode;
  fs_m_out.m_fs_vfs_readsuper.file_size = root_ip->i_size;
  fs_m_out.m_fs_vfs_readsuper.uid = root_ip->i_uid;
  fs_m_out.m_fs_vfs_readsuper.gid = root_ip->i_gid;
  fs_m_out.m_fs_vfs_readsuper.flags = RES_HASPEEK;

  /* Mark it dirty */
  if(!superblock.s_rd_only) {
	  superblock.s_flags &= ~MFSFLAG_CLEAN;
	  if(write_super(&superblock) != OK)
		panic("mounting: couldn't write dirty superblock");
  }

  return(r);
}


/*===========================================================================*
 *				fs_mountpoint				     *
 *===========================================================================*/
int fs_mountpoint()
{
/* This function looks up the mount point, it checks the condition whether
 * the partition can be mounted on the inode or not. 
 */
  register struct inode *rip;
  int r = OK;
  mode_t bits;
  
  /* Temporarily open the file. */
  if( (rip = get_inode(fs_dev, fs_m_in.m_vfs_fs_mountpoint.inode)) == NULL)
	  return(EINVAL);
  
  
  if(rip->i_mountpoint) r = EBUSY;

  /* It may not be special. */
  bits = rip->i_mode & I_TYPE;
  if (bits == I_BLOCK_SPECIAL || bits == I_CHAR_SPECIAL) r = ENOTDIR;

  put_inode(rip);

  if(r == OK) rip->i_mountpoint = TRUE;

  return(r);
}


/*===========================================================================*
 *				fs_unmount				     *
 *===========================================================================*/
int fs_unmount()
{
/* Unmount a file system by device number. */
  int count;
  struct inode *rip, *root_ip;

  if(superblock.s_dev != fs_dev) return(EINVAL);
  
  /* See if the mounted device is busy.  Only 1 inode using it should be
   * open --the root inode-- and that inode only 1 time. */
  count = 0;
  for (rip = &inode[0]; rip < &inode[NR_INODES]; rip++) 
	  if (rip->i_count > 0 && rip->i_dev == fs_dev) count += rip->i_count;

  if ((root_ip = find_inode(fs_dev, ROOT_INODE)) == NULL) {
  	panic("MFS: couldn't find root inode\n");
  	return(EINVAL);
  }
   
  if (count > 1) return(EBUSY);	/* can't umount a busy file system */
  put_inode(root_ip);

  /* force any cached blocks out of memory */
  (void) fs_sync();

  /* Mark it clean if we're allowed to write _and_ it was clean originally. */
  if(cleanmount && !superblock.s_rd_only) {
	superblock.s_flags |= MFSFLAG_CLEAN;
	write_super(&superblock);
  }

  /* Close the device the file system lives on. */
  bdev_close(fs_dev);

  /* Throw out blocks out of the VM cache, to prevent corruption later. */
  lmfs_invalidate(fs_dev);

  /* Finish off the unmount. */
  superblock.s_dev = NO_DEV;
  unmountdone = TRUE;

  return(OK);
}

