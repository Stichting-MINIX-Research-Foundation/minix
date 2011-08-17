#ifndef __VFS_VMNT_H__
#define __VFS_VMNT_H__

EXTERN struct vmnt {
  int m_fs_e;			/* FS process' kernel endpoint */
  tll_t m_lock;
  comm_t m_comm;
  dev_t m_dev;			/* device number */
  unsigned int m_flags;		/* mount flags */
  struct vnode *m_mounted_on;	/* vnode on which the partition is mounted */
  struct vnode *m_root_node;	/* root vnode */
  char m_label[LABEL_MAX];	/* label of the file system process */
} vmnt[NR_MNTS];

/* vmnt flags */
#define VMNT_READONLY		01	/* Device mounted readonly */
#define VMNT_BACKCALL		02	/* FS did back call */

/* vmnt lock types mapping */
#define VMNT_READ TLL_READ
#define VMNT_WRITE TLL_READSER
#define VMNT_EXCL TLL_WRITE

#endif
