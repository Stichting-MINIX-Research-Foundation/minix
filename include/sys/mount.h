#ifndef _SYS_MOUNT_H
#define _SYS_MOUNT_H_

/*
 * File system types.
 */
#define MOUNT_FFS       "ffs"           /* UNIX "Fast" Filesystem */
#define MOUNT_UFS       MOUNT_FFS       /* for compatibility */
#define MOUNT_NFS       "nfs"           /* Network Filesystem */
#define MOUNT_MFS       "mfs"           /* Memory Filesystem */
#define MOUNT_MSDOS     "msdos"         /* MSDOS Filesystem */
#define MOUNT_LFS       "lfs"           /* Log-based Filesystem */
#define MOUNT_FDESC     "fdesc"         /* File Descriptor Filesystem */
#define MOUNT_NULL      "null"          /* Minimal Filesystem Layer */
#define MOUNT_OVERLAY   "overlay"       /* Minimal Overlay Filesystem Layer */
#define MOUNT_UMAP      "umap"  /* User/Group Identifier Remapping Filesystem */
#define MOUNT_KERNFS    "kernfs"        /* Kernel Information Filesystem */
#define MOUNT_PROCFS    "procfs"        /* /proc Filesystem */
#define MOUNT_AFS       "afs"           /* Andrew Filesystem */
#define MOUNT_CD9660    "cd9660"        /* ISO9660 (aka CDROM) Filesystem */
#define MOUNT_UNION     "union"         /* Union (translucent) Filesystem */
#define MOUNT_ADOSFS    "adosfs"        /* AmigaDOS Filesystem */
#define MOUNT_EXT2FS    "ext2fs"        /* Second Extended Filesystem */
#define MOUNT_CFS       "coda"          /* Coda Filesystem */
#define MOUNT_CODA      MOUNT_CFS       /* Coda Filesystem */
#define MOUNT_FILECORE  "filecore"      /* Acorn Filecore Filesystem */
#define MOUNT_NTFS      "ntfs"          /* Windows/NT Filesystem */
#define MOUNT_SMBFS     "smbfs"         /* CIFS (SMB) */
#define MOUNT_PTYFS     "ptyfs"         /* Pseudo tty filesystem */
#define MOUNT_TMPFS     "tmpfs"         /* Efficient memory file-system */
#define MOUNT_UDF       "udf"           /* UDF CD/DVD filesystem */
#define MOUNT_SYSVBFS   "sysvbfs"       /* System V Boot Filesystem */
#define MOUNT_PUFFS     "puffs"         /* Pass-to-Userspace filesystem */
#define MOUNT_HFS       "hfs"           /* Apple HFS+ Filesystem */
#define MOUNT_EFS       "efs"           /* SGI's Extent Filesystem */
#define MOUNT_ZFS       "zfs"           /* Sun ZFS */
#define MOUNT_NILFS     "nilfs"         /* NTT's NiLFS(2) logging file system */
#define MOUNT_RUMPFS    "rumpfs"        /* rump virtual file system */
#define MOUNT_V7FS      "v7fs"          /* 7th Edition of Unix Filesystem */

#include <sys/statvfs.h>
#include <minix/mount.h>

#endif /* !_SYS_MOUNT_H_ */
