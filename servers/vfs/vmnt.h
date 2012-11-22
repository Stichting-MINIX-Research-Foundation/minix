#ifndef __VFS_VMNT_H__
#define __VFS_VMNT_H__

#include "tll.h"
#include "comm.h"

EXTERN struct vmnt {
  int m_fs_e;			/* FS process' kernel endpoint */
  tll_t m_lock;
  comm_t m_comm;
  dev_t m_dev;			/* device number */
  unsigned int m_flags;		/* mount flags */
  struct vnode *m_mounted_on;	/* vnode on which the partition is mounted */
  struct vnode *m_root_node;	/* root vnode */
  char m_label[LABEL_MAX];	/* label of the file system process */
  char m_mount_path[PATH_MAX];	/* path on which vmnt is mounted */
  char m_mount_dev[PATH_MAX];	/* path on which vmnt is mounted */
} vmnt[NR_MNTS];

/* vmnt flags */
#define VMNT_READONLY		01	/* Device mounted readonly */
#define VMNT_CALLBACK		02	/* FS did back call */
#define VMNT_MOUNTING		04	/* Device is being mounted */
#define VMNT_FORCEROOTBSF	010	/* Force usage of none-device */

/* vmnt lock types mapping */
#define VMNT_READ TLL_READ
#define VMNT_WRITE TLL_READSER
#define VMNT_EXCL TLL_WRITE

#endif
