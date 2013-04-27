#ifndef _MKFS_MFSDIR_H
#define _MKFS_MFSDIR_H

/* Minix MFS V1/V2 on-disk directory filename. */
#define MFS_DIRSIZ	14

struct direct {
  uint16_t d_ino;
  char d_name[MFS_DIRSIZ];
} __packed;

#endif /* _MKFS_MFSDIR_H */
