#include "fs.h"
#include "buf.h"
#include "inode.h"
#include "super.h"
#include <stdlib.h>
#include <minix/vfsif.h>
#include <minix/bdev.h>
#include <machine/param.h>
#include <machine/vmparam.h>

/*===========================================================================*
 *				fs_mount				     *
 *===========================================================================*/
int fs_mount(dev_t dev, unsigned int flags, struct fsdriver_node *root_node,
	unsigned int *res_flags)
{
/* Read the super block of the partition, get the root inode, and report the
 * details of both back to VFS.
 *
 * Phase A is read-only: the file system is never written, so the on-disk
 * "clean" state is left untouched regardless of how it was mounted.
 */
  struct inode *root_ip;
  struct fs *fs;
  int r, readonly;
  uint64_t freefrags;

  fs_dev = dev;
  readonly = (flags & REQ_RDONLY) ? 1 : 0;

  /* Open the device the file system lives on. */
  if (bdev_open(fs_dev, readonly ? BDEV_R_BIT : (BDEV_R_BIT|BDEV_W_BIT)) != OK)
	return(EINVAL);

  /* Allocate and fill in the super block. */
  if (!(superblock = malloc(sizeof(*superblock))))
	panic("Can't allocate memory for superblock.");

  superblock->s_dev = fs_dev;	/* read_super() needs to know the device */
  r = read_super(superblock);
  if (r != OK) {
	superblock->s_dev = NO_DEV;
	free_super(superblock);
	superblock = NULL;
	bdev_close(fs_dev);
	return(r);
  }

  fs = &superblock->s_fs;

  superblock->s_rd_only = readonly;

  /* The cache block size is the fragment size; report block usage in those
   * units so the VM second-level cache can be sized correctly.
   */
  /* Enable the VM (inode-keyed) cache: mmap support -- which the
   * dynamically linked userland depends on for exec -- requires
   * publishing cache pages to VM.  This must be set before
   * lmfs_set_blocksize(), which latches the decision (and itself
   * requires page-multiple cache blocks, so a file system with
   * sub-page fragments still runs without the VM cache).
   * Caveat: the write path's fragment reallocation moves data between
   * device blocks; until it explicitly forgets the affected VM pages,
   * writing to a file that is also mmapped can leave stale data in
   * the VM cache (TODO). */
  lmfs_may_use_vmcache(1);

  lmfs_set_blocksize(superblock->s_block_size);
  freefrags = (uint64_t) fs->fs_cstotal.cs_nbfree * fs->fs_frag +
	(uint64_t) fs->fs_cstotal.cs_nffree;
  lmfs_set_blockusage((uint64_t) fs->fs_size, (uint64_t) fs->fs_size - freefrags);

  /* Get the root inode of the mounted file system. */
  if ((root_ip = get_inode(fs_dev, ROOT_INODE)) == NULL) {
	printf("ffs: couldn't get root inode\n");
	superblock->s_dev = NO_DEV;
	free_super(superblock);
	superblock = NULL;
	bdev_close(fs_dev);
	return(EINVAL);
  }

  if (root_ip->i_din.di_mode == 0) {
	printf("ffs: zero mode for root inode?\n");
	put_inode(root_ip);
	superblock->s_dev = NO_DEV;
	free_super(superblock);
	superblock = NULL;
	bdev_close(fs_dev);
	return(EINVAL);
  }

  if ((root_ip->i_din.di_mode & I_TYPE) != I_DIRECTORY) {
	printf("ffs: root inode is not a directory\n");
	put_inode(root_ip);
	superblock->s_dev = NO_DEV;
	free_super(superblock);
	superblock = NULL;
	bdev_close(fs_dev);
	return(EINVAL);
  }

  /* On a read-write mount, mark the file system dirty until it is unmounted. */
  if (!readonly) {
	fs->fs_clean = 0;
	fs->fs_fmod = 1;
	write_super(superblock);
  }

  /* Root inode properties. */
  root_node->fn_ino_nr = root_ip->i_num;
  root_node->fn_mode = root_ip->i_din.di_mode;
  root_node->fn_size = root_ip->i_din.di_size;
  root_node->fn_uid = root_ip->i_din.di_uid;
  root_node->fn_gid = root_ip->i_din.di_gid;
  root_node->fn_dev = NO_DEV;

  *res_flags = RES_NOFLAGS;

  return(OK);
}

/*===========================================================================*
 *				fs_mountpt				     *
 *===========================================================================*/
int fs_mountpt(ino_t ino_nr)
{
/* Verify that the given inode can serve as a mount point. */
  struct inode *rip;
  int r = OK;
  mode_t bits;

  if ((rip = get_inode(fs_dev, ino_nr)) == NULL)
	return(EINVAL);

  if (rip->i_mountpoint)
	r = EBUSY;

  /* It may not be a special file. */
  bits = rip->i_din.di_mode & I_TYPE;
  if (bits == I_BLOCK_SPECIAL || bits == I_CHAR_SPECIAL)
	r = ENOTDIR;

  put_inode(rip);

  if (r == OK)
	rip->i_mountpoint = TRUE;

  return(r);
}

/*===========================================================================*
 *				fs_unmount				     *
 *===========================================================================*/
void fs_unmount(void)
{
/* Unmount the file system. */
  struct inode *rip, *root_ip;
  int count;

  /* See if the mounted device is busy.  Only the root inode should still be
   * open, exactly once.  This is an integrity check only: VFS expects the
   * unmount to succeed either way.
   */
  count = 0;
  for (rip = &inode[0]; rip < &inode[NR_INODES]; rip++)
	if (rip->i_count > 0 && rip->i_dev == fs_dev)
		count += rip->i_count;
  if (count != 1)
	printf("ffs: file system has %d in-use inodes!\n", count);

  if ((root_ip = find_inode(fs_dev, ROOT_INODE)) == NULL)
	panic("ffs: couldn't find root inode");

  put_inode(root_ip);

  /* Flush everything and mark the file system clean. */
  if (!superblock->s_rd_only) {
	fs_sync();
	superblock->s_fs.fs_clean = FS_ISCLEAN;
	superblock->s_fs.fs_fmod = 1;
	write_super(superblock);
  }

  /* Close the device the file system lives on. */
  bdev_close(fs_dev);

  /* Throw all blocks out of the VM cache, to prevent corruption later. */
  lmfs_invalidate(fs_dev);

  superblock->s_dev = NO_DEV;

  /* Release the in-core super block; the next mount allocates a fresh one. */
  free_super(superblock);
  superblock = NULL;
}
