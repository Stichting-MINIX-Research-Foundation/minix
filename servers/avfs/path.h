#ifndef __VFS_PATH_H__
#define __VFS_PATH_H__

struct lookup {
  char *l_path;			/* Path to lookup */
  int l_flags;			/* VFS/FS flags (see <minix/vfsif.h>) */
  tll_access_t l_vmnt_lock;	/* Lock to obtain on vmnt */
  tll_access_t l_vnode_lock;	/* Lock to obtain on vnode */
  struct vmnt **l_vmp;		/* vmnt object that was locked */
  struct vnode **l_vnode;	/* vnode object that was locked */
};

#endif
