/* Created (MFS based):
 *   February 2010 (Evgeniy Ivanov)
 */

#include "fs.h"
#include "buf.h"
#include "inode.h"
#include "super.h"
#include <stdlib.h>
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
  u32_t mask;

  fs_dev = dev;
  readonly = (flags & REQ_RDONLY) ? 1 : 0;

  /* Open the device the file system lives on. */
  if (bdev_open(fs_dev, readonly ? BDEV_R_BIT : (BDEV_R_BIT|BDEV_W_BIT)) !=
		OK) {
        return(EINVAL);
  }

  /* Fill in the super block. */
  if(!(superblock = malloc(sizeof(*superblock))))
	panic("Can't allocate memory for superblock.");

  superblock->s_dev = fs_dev;	/* read_super() needs to know which dev */
  r = read_super(superblock);

  /* Is it recognized as a Minix filesystem? */
  if (r != OK) {
	superblock->s_dev = NO_DEV;
	bdev_close(fs_dev);
	return(r);
  }

  if (superblock->s_rev_level != EXT2_GOOD_OLD_REV) {
	struct super_block *sp = superblock; /* just shorter name */
	mask = ~SUPPORTED_INCOMPAT_FEATURES;
	if (HAS_INCOMPAT_FEATURE(sp, mask)) {
		if (HAS_INCOMPAT_FEATURE(sp, INCOMPAT_COMPRESSION & mask))
			printf("ext2: fs compression is not supported by server\n");
		if (HAS_INCOMPAT_FEATURE(sp, INCOMPAT_FILETYPE & mask))
			printf("ext2: fs in dir filetype is not supported by server\n");
		if (HAS_INCOMPAT_FEATURE(sp, INCOMPAT_RECOVER & mask))
			printf("ext2: fs recovery is not supported by server\n");
		if (HAS_INCOMPAT_FEATURE(sp, INCOMPAT_JOURNAL_DEV & mask))
			printf("ext2: fs journal dev is not supported by server\n");
		if (HAS_INCOMPAT_FEATURE(sp, INCOMPAT_META_BG & mask))
			printf("ext2: fs meta bg is not supported by server\n");
		return(EINVAL);
	}
	mask = ~SUPPORTED_RO_COMPAT_FEATURES;
	if (HAS_RO_COMPAT_FEATURE(sp, mask)) {
		if (HAS_RO_COMPAT_FEATURE(sp, RO_COMPAT_SPARSE_SUPER & mask)) {
			printf("ext2: sparse super is not supported by server, \
				remount read-only\n");
		}
		if (HAS_RO_COMPAT_FEATURE(sp, RO_COMPAT_LARGE_FILE & mask)) {
			printf("ext2: large files are not supported by server, \
				remount read-only\n");
		}
		if (HAS_RO_COMPAT_FEATURE(sp, RO_COMPAT_BTREE_DIR & mask)) {
			printf("ext2: dir's btree is not supported by server, \
				remount read-only\n");
		}
		return(EINVAL);
	}
  }

  if (superblock->s_state == EXT2_ERROR_FS) {
	printf("ext2: filesystem wasn't cleanly unmounted last time\n");
        superblock->s_dev = NO_DEV;
	bdev_close(fs_dev);
	return(EINVAL);
  }

  lmfs_set_blocksize(superblock->s_block_size);
  lmfs_set_blockusage(superblock->s_blocks_count,
	superblock->s_blocks_count - superblock->s_free_blocks_count);

  /* Get the root inode of the mounted file system. */
  if ( (root_ip = get_inode(fs_dev, ROOT_INODE)) == NULL)  {
	printf("ext2: couldn't get root inode\n");
	superblock->s_dev = NO_DEV;
	bdev_close(fs_dev);
	return(EINVAL);
  }

  if (root_ip != NULL && root_ip->i_mode == 0) {
	printf("%s:%d zero mode for root inode?\n", __FILE__, __LINE__);
	put_inode(root_ip);
	superblock->s_dev = NO_DEV;
	bdev_close(fs_dev);
	return(EINVAL);
  }

  if (root_ip != NULL && (root_ip->i_mode & I_TYPE) != I_DIRECTORY) {
	printf("%s:%d root inode has wrong type, it's not a DIR\n",
		 __FILE__, __LINE__);
	put_inode(root_ip);
	superblock->s_dev = NO_DEV;
	bdev_close(fs_dev);
	return(EINVAL);
  }

  superblock->s_rd_only = readonly;

  if (!readonly) {
	superblock->s_state = EXT2_ERROR_FS;
	superblock->s_mnt_count++;
	superblock->s_mtime = clock_time(NULL);
	write_super(superblock); /* Commit info, we just set above */
  }

  /* Root inode properties */
  root_node->fn_ino_nr = root_ip->i_num;
  root_node->fn_mode = root_ip->i_mode;
  root_node->fn_size = root_ip->i_size;
  root_node->fn_uid = root_ip->i_uid;
  root_node->fn_gid = root_ip->i_gid;
  root_node->fn_dev = NO_DEV;

  *res_flags = RES_NOFLAGS;

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
/* Unmount a file system by device number. */
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
	printf("ext2: file system has %d in-use inodes!\n", count);

  if ((root_ip = find_inode(fs_dev, ROOT_INODE)) == NULL)
	panic("ext2: couldn't find root inode");

  /* Sync fs data before checking count. In some cases VFS can force unmounting
   * and it will damage unsynced FS. We don't sync before checking root_ip since
   * if it is missing then something strange happened with FS, so it's better
   * to not use possibly corrupted data for syncing.
   */
  if (!superblock->s_rd_only) {
	/* force any cached blocks out of memory */
	fs_sync();
  }

  put_inode(root_ip);

  if (!superblock->s_rd_only) {
	superblock->s_wtime = clock_time(NULL);
	superblock->s_state = EXT2_VALID_FS;
	write_super(superblock); /* Commit info, we just set above */
  }

  /* Close the device the file system lives on. */
  bdev_close(fs_dev);

  /* Throw all blocks out of the VM cache, to prevent corruption later. */
  lmfs_invalidate(fs_dev);

  /* Finish off the unmount. */
  superblock->s_dev = NO_DEV;
}
