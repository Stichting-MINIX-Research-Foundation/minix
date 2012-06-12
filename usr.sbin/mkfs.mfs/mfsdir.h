#ifndef _MFSDIR_H
#define _MFSDIR_H

/* Maximum Minix MFS on-disk directory filename.
 * MFS uses 'struct direct' to write and parse 
 * directory entries, so this can't be changed
 * without breaking filesystems.
 */

#define MFS_DIRSIZ	60

struct direct {
  uint32_t mfs_d_ino;
  char mfs_d_name[MFS_DIRSIZ];
#ifdef __NBSD_LIBC 
} __packed;
#else
};
#endif

#endif /* _MFSDIR_H */
