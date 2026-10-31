/* In-core super block.  A single file system is served by each instance of
 * this process, so there is exactly one in-core super block.
 *
 * The on-disk superblock (struct fs) is embedded directly; since this server
 * is native-little-endian, no field-by-field byte swapping is needed (see
 * CLAUDE.md "Endianness").  UFS1's legacy "fs_old_*" fields are normalized
 * into the wide UFS2 fields on read and back on write (super.c).
 */

#ifndef FFS_SUPER_H
#define FFS_SUPER_H

EXTERN struct super_block {
  struct fs s_fs;		/* the on-disk superblock, read natively */

  /* The following items are only used when the super_block is in memory. */
  struct csum *s_csp;		/* cylinder-group summary array (fs_csaddr) */
  size_t  s_csp_size;		/* mmap()ed size of s_csp, for munmap() */
  off_t   s_sboff;		/* byte offset the superblock was found at */
  dev_t   s_dev;		/* whose super block is this?  NO_DEV if free */
  int     s_rd_only;		/* nonzero if file system mounted read only */
  unsigned int s_block_size;	/* cache block size == fs_fsize (fragment) */
  off_t   s_max_size;		/* maximum file size on this device */
} *superblock;

#endif /* FFS_SUPER_H */
