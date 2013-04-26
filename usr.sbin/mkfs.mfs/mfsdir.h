#ifndef _MKFS_MFSDIR_H
#define _MKFS_MFSDIR_H

/* Maximum Minix MFS on-disk directory filename.
 * MFS uses 'struct direct' to write and parse 
 * directory entries, so this can't be changed
 * without breaking filesystems.
 */
#define MFS_DIRSIZ	60

struct direct {
  uint32_t d_ino;
  char d_name[MFS_DIRSIZ];
} __packed;

#endif /* _MKFS_MFSDIR_H */
