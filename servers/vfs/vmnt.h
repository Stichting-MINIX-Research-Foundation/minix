

EXTERN struct vmnt {
  int m_fs_e;                   /* FS process' kernel endpoint */
  dev_t m_dev;                  /* device number */
  int m_driver_e;               /* device driver process' kernel endpoint */
  int m_flags;                  /* mount flags */
  struct vnode *m_mounted_on;   /* the vnode on which the partition is mounted */
  struct vnode *m_root_node;    /* root vnode */
} vmnt[NR_MNTS];

#define NIL_VMNT (struct vmnt *) 0
