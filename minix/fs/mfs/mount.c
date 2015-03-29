#include "fs.h"
#include "inode.h"
#include "super.h"
#include <minix/vfsif.h>
#include <minix/bdev.h>

/*===========================================================================*
 *				fs_mount				     *
 *===========================================================================*/
int fs_mount(dev_t dev, unsigned int flags, struct fsdriver_node *root_node,
	unsigned int *res_flags)
{
/* This function reads the superblock of the partition, gets the root inode
 * and sends back the details of them.
 */
  struct inode *root_ip;
  int r, readonly;

  fs_dev = dev;
  readonly = (flags & REQ_RDONLY) ? 1 : 0;

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

  lmfs_set_blocksize(superblock.s_block_size);

  /* Compute the current number of used zones, and report it to libminixfs.
   * Note that libminixfs really wants numbers of *blocks*, but this MFS
   * implementation dropped support for differing zone/block sizes a while ago.
   */
  used_zones = superblock.s_zones - count_free_bits(&superblock, ZMAP);

  lmfs_set_blockusage(superblock.s_zones, used_zones);
  
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
  
  /* Root inode properties */
  root_node->fn_ino_nr = root_ip->i_num;
  root_node->fn_mode = root_ip->i_mode;
  root_node->fn_size = root_ip->i_size;
  root_node->fn_uid = root_ip->i_uid;
  root_node->fn_gid = root_ip->i_gid;
  root_node->fn_dev = NO_DEV;

  *res_flags = RES_NOFLAGS;

  /* Mark it dirty */
  if(!superblock.s_rd_only) {
	  superblock.s_flags &= ~MFSFLAG_CLEAN;
	  if(write_super(&superblock) != OK)
		panic("mounting: couldn't write dirty superblock");
  }

  return(r);
}


/*===========================================================================*
 *				fs_mountpt				     *
 *===========================================================================*/
int fs_mountpt(ino_t ino_nr)
{
/* This function looks up the mount point, it checks the condition whether
 * the partition can be mounted on the inode or not. 
 */
  register struct inode *rip;
  int r = OK;
  mode_t bits;
  
  /* Temporarily open the file. */
  if( (rip = get_inode(fs_dev, ino_nr)) == NULL)
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
void fs_unmount(void)
{
/* Unmount a file system. */
  int count;
  struct inode *rip, *root_ip;

  /* See if the mounted device is busy.  Only 1 inode using it should be
   * open --the root inode-- and that inode only 1 time.  This is an integrity
   * check only: VFS expects the unmount to succeed either way.
   */
  count = 0;
  for (rip = &inode[0]; rip < &inode[NR_INODES]; rip++) 
	  if (rip->i_count > 0 && rip->i_dev == fs_dev) count += rip->i_count;
  if (count != 1)
	printf("MFS: file system has %d in-use inodes!\n", count);

  if ((root_ip = find_inode(fs_dev, ROOT_INODE)) == NULL)
	panic("MFS: couldn't find root inode\n");
   
  put_inode(root_ip);

  /* force any cached blocks out of memory */
  fs_sync();

  /* Mark it clean if we're allowed to write _and_ it was clean originally. */
  if (!superblock.s_rd_only) {
	superblock.s_flags |= MFSFLAG_CLEAN;
	write_super(&superblock);
  }

  /* Close the device the file system lives on. */
  bdev_close(fs_dev);

  /* Throw out blocks out of the VM cache, to prevent corruption later. */
  lmfs_invalidate(fs_dev);

  /* Finish off the unmount. */
  superblock.s_dev = NO_DEV;
}

