/* This file performs the MOUNT and UMOUNT system calls.
 *
 * The entry points into this file are
 *   do_fsready:	perform the FS_READY system call
 *   do_mount:		perform the MOUNT system call
 *   do_umount:		perform the UMOUNT system call
 *   unmount:		unmount a file system
 */

#include "fs.h"
#include <fcntl.h>
#include <string.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/keymap.h>
#include <minix/const.h>
#include <minix/endpoint.h>
#include <minix/syslib.h>
#include <minix/bitmap.h>
#include <minix/ds.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <dirent.h>
#include <assert.h>
#include "file.h"
#include "fproc.h"
#include "dmap.h"
#include <minix/vfsif.h>
#include "vnode.h"
#include "vmnt.h"
#include "path.h"
#include "param.h"

/* Allow the root to be replaced before the first 'real' mount. */
PRIVATE int have_root = 0;

/* Bitmap of in-use "none" pseudo devices. */
PRIVATE bitchunk_t nonedev[BITMAP_CHUNKS(NR_NONEDEVS)] = { 0 };

#define alloc_nonedev(dev) SET_BIT(nonedev, minor(dev) - 1)
#define free_nonedev(dev) UNSET_BIT(nonedev, minor(dev) - 1)

FORWARD _PROTOTYPE( dev_t name_to_dev, (int allow_mountpt,
					char path[PATH_MAX])		);
FORWARD _PROTOTYPE( dev_t find_free_nonedev, (void)			);
FORWARD _PROTOTYPE( void update_bspec, (dev_t dev, endpoint_t fs_e,
				      int send_drv_e)			);

/*===========================================================================*
 *				update_bspec				     *
 *===========================================================================*/
PRIVATE void update_bspec(dev_t dev, endpoint_t fs_e, int send_drv_e)
{
/* Update all block special files for a certain device, to use a new FS endpt
 * to route raw block I/O requests through.
 */
  struct vnode *vp;
  struct dmap *dp;
  int r, major;

  for (vp = &vnode[0]; vp < &vnode[NR_VNODES]; ++vp)
	if (vp->v_ref_count > 0 && S_ISBLK(vp->v_mode) && vp->v_sdev == dev) {
		vp->v_bfs_e = fs_e;
		if (send_drv_e) {
			major = major(dev);
			if (major < 0 || major >= NR_DEVICES) {
				/* Can't update driver endpoint for out of
				 * range major */
				continue;
			}
			dp = &dmap[major(dev)];
			if (dp->dmap_driver == NONE) {
				/* Can't send new driver endpoint for
				 * vanished driver */
				printf("VFS: can't send new driver endpt\n");
				continue;
			}

			if ((r = req_newdriver(fs_e, vp->v_sdev,
						dp->dmap_driver)) != OK) {
				printf("VFS: Failed to send new driver endpoint"
				       " for moved block special file\n");
			}
		}
	}
}

/*===========================================================================*
 *                              do_fsready                                   *
 *===========================================================================*/
PUBLIC int do_fsready()
{
  /* deprecated */
  return(SUSPEND);
}

/*===========================================================================*
 *                              do_mount                                     *
 *===========================================================================*/
PUBLIC int do_mount()
{
/* Perform the mount(name, mfile, mount_flags) system call. */
  endpoint_t fs_e;
  int r, slot, rdonly, nodev;
  char fullpath[PATH_MAX];
  char mount_label[LABEL_MAX];
  dev_t dev;

  /* Only the super-user may do MOUNT. */
  if (!super_user) return(EPERM);

  /* FS process' endpoint number */
  if (m_in.mount_flags & MS_LABEL16) {
	/* Get the label from the caller, and ask DS for the endpoint. */
	r = sys_datacopy(who_e, (vir_bytes) m_in.fs_label, SELF,
		(vir_bytes) mount_label, (phys_bytes) sizeof(mount_label));
	if (r != OK) return(r);

	mount_label[sizeof(mount_label)-1] = 0;

	r = ds_retrieve_label_endpt(mount_label, &fs_e);
	if (r != OK) return(r);
  } else {
	/* Legacy support: get the endpoint from the request itself. */
	fs_e = (endpoint_t) m_in.fs_label;
	mount_label[0] = 0;
  }

  /* Sanity check on process number. */
  if (isokendpt(fs_e, &slot) != OK) return(EINVAL);

  /* Should the file system be mounted read-only? */
  rdonly = (m_in.mount_flags & MS_RDONLY);

  /* A null string for block special device means don't use a device at all. */
  nodev = (m_in.name1_length == 0);
  if (!nodev) {
	/* If 'name' is not for a block special file, return error. */
	if (fetch_name(m_in.name1, m_in.name1_length, M1, fullpath) != OK)
		return(err_code);
	if ((dev = name_to_dev(FALSE /*allow_mountpt*/, fullpath)) == NO_DEV)
		return(err_code);
  } else {
	/* Find a free pseudo-device as substitute for an actual device. */
	if ((dev = find_free_nonedev()) == NO_DEV)
		return(err_code);
  }

  /* Fetch the name of the mountpoint */
  if (fetch_name(m_in.name2, m_in.name2_length, M1, fullpath) != OK)
	return(err_code);

  /* Do the actual job */
  return mount_fs(dev, fullpath, fs_e, rdonly, mount_label);
}


/*===========================================================================*
 *                              mount_fs				     *
 *===========================================================================*/
PUBLIC int mount_fs(
dev_t dev,
char mountpoint[PATH_MAX],
endpoint_t fs_e,
int rdonly,
char mount_label[LABEL_MAX] )
{
  int rdir, mdir;               /* TRUE iff {root|mount} file is dir */
  int i, r = OK, found, isroot, mount_root, con_reqs;
  struct fproc *tfp;
  struct dmap *dp;
  struct vnode *root_node, *vp = NULL;
  struct vmnt *new_vmp, *parent_vmp;
  char *label;
  struct node_details res;
  struct lookup resolve;

  /* Look up block device driver label when dev is not a pseudo-device */
  label = "";
  if (!is_nonedev(dev)) {
	/* Get driver process' endpoint */
	dp = &dmap[major(dev)];
	if (dp->dmap_driver == NONE) {
		printf("VFS: no driver for dev %d\n", dev);
		return(EINVAL);
	}

	label = dp->dmap_label;
	assert(strlen(label) > 0);
  }

  /* Scan vmnt table to see if dev already mounted. If not, find a free slot.*/
  found = FALSE;
  for (i = 0; i < NR_MNTS; ++i) {
	if (vmnt[i].m_dev == dev) found = TRUE;
  }
  if (found) {
	return(EBUSY);
  } else if ((new_vmp = get_free_vmnt()) == NULL) {
	return(ENOMEM);
  }

  if ((r = lock_vmnt(new_vmp, VMNT_EXCL)) != OK) return(r);

  isroot = (strcmp(mountpoint, "/") == 0);
  mount_root = (isroot && have_root < 2); /* Root can be mounted twice:
					   * 1: ramdisk
					   * 2: boot disk (e.g., harddisk)
					   */

  if (!mount_root) {
	/* Get vnode of mountpoint */
	lookup_init(&resolve, mountpoint, PATH_NOFLAGS, &parent_vmp, &vp);
	resolve.l_vmnt_lock = VMNT_EXCL;
	resolve.l_vnode_lock = VNODE_WRITE;
	if ((vp = eat_path(&resolve, fp)) == NULL)
		r = err_code;
	else if (vp->v_ref_count == 1) {
		/*Tell FS on which vnode it is mounted (glue into mount tree)*/
		r = req_mountpoint(vp->v_fs_e, vp->v_inode_nr);
	} else
		r = EBUSY;

	if (vp != NULL)	{
		/* Quickly unlock to allow back calls (from e.g. FUSE) to
		 * relock */
		unlock_vmnt(parent_vmp);
	}

	if (r != OK) {
		if (vp != NULL) {
			unlock_vnode(vp);
			put_vnode(vp);
		}
		unlock_vmnt(new_vmp);
		return(r);
	}
  }

/* XXX: move this upwards before lookup after proper locking. */
  /* We'll need a vnode for the root inode */
  if ((root_node = get_free_vnode()) == NULL || dev == 266) {
	if (vp != NULL) {
		unlock_vnode(vp);
		put_vnode(vp);
	}
	unlock_vmnt(new_vmp);
	return(err_code);
  }

  lock_vnode(root_node, VNODE_OPCL);

  /* Store some essential vmnt data first */
  new_vmp->m_fs_e = fs_e;
  new_vmp->m_dev = dev;
  if (rdonly) new_vmp->m_flags |= VMNT_READONLY;
  else new_vmp->m_flags &= ~VMNT_READONLY;

  /* Tell FS which device to mount */
  r = req_readsuper(fs_e, label, dev, rdonly, isroot, &res, &con_reqs);
  if (r != OK) {
	if (vp != NULL) {
		unlock_vnode(vp);
		put_vnode(vp);
	}
	new_vmp->m_fs_e = NONE;
	new_vmp->m_dev = NO_DEV;
	unlock_vnode(root_node);
	unlock_vmnt(new_vmp);
	return(r);
  }

  /* Fill in root node's fields */
  root_node->v_fs_e = res.fs_e;
  root_node->v_inode_nr = res.inode_nr;
  root_node->v_mode = res.fmode;
  root_node->v_uid = res.uid;
  root_node->v_gid = res.gid;
  root_node->v_size = res.fsize;
  root_node->v_sdev = NO_DEV;
  root_node->v_fs_count = 1;
  root_node->v_ref_count = 1;

  /* Root node is indeed on the partition */
  root_node->v_vmnt = new_vmp;
  root_node->v_dev = new_vmp->m_dev;
  if (con_reqs == 0)
	new_vmp->m_comm.c_max_reqs = 1;	/* Default if FS doesn't tell us */
  else
	new_vmp->m_comm.c_max_reqs = con_reqs;
  new_vmp->m_comm.c_cur_reqs = 0;

  lock_bsf();

  if (mount_root) {
	/* Superblock and root node already read.
	 * Nothing else can go wrong. Perform the mount. */
	new_vmp->m_root_node = root_node;
	new_vmp->m_mounted_on = NULL;
	strcpy(new_vmp->m_label, mount_label);
	if (is_nonedev(dev)) alloc_nonedev(dev);
	update_bspec(dev, fs_e, 0 /* Don't send new driver endpoint */);

	ROOT_DEV = dev;
	ROOT_FS_E = fs_e;

	/* Replace all root and working directories */
	for (i = 0, tfp = fproc; i < NR_PROCS; i++, tfp++) {
		if (tfp->fp_pid == PID_FREE)
			continue;

#define		MAKEROOT(what) { 			\
			if (what) put_vnode(what);	\
			dup_vnode(root_node);		\
			what = root_node;		\
		}

		MAKEROOT(tfp->fp_rd);
		MAKEROOT(tfp->fp_wd);
	}

	unlock_vnode(root_node);
	unlock_vmnt(new_vmp);
	have_root++; /* We have a (new) root */
	unlock_bsf();
	return(OK);
  }

  /* File types may not conflict. */
  mdir = ((vp->v_mode & I_TYPE) == I_DIRECTORY); /*TRUE iff dir*/
  rdir = ((root_node->v_mode & I_TYPE) == I_DIRECTORY);
  if (!mdir && rdir) r = EISDIR;

  /* If error, return the super block and both inodes; release the vmnt. */
  if (r != OK) {
	unlock_vnode(vp);
	unlock_vnode(root_node);
	unlock_vmnt(new_vmp);
	put_vnode(vp);
	put_vnode(root_node);
	new_vmp->m_dev = NO_DEV;
	unlock_bsf();
	return(r);
  }

  /* Nothing else can go wrong.  Perform the mount. */
  new_vmp->m_mounted_on = vp;
  new_vmp->m_root_node = root_node;
  strcpy(new_vmp->m_label, mount_label);

  /* Allocate the pseudo device that was found, if not using a real device. */
  if (is_nonedev(dev)) alloc_nonedev(dev);

  /* The new FS will handle block I/O requests for its device now. */
  update_bspec(dev, fs_e, 0 /* Don't send new driver endpoint */);

  unlock_vnode(vp);
  unlock_vnode(root_node);
  unlock_vmnt(new_vmp);
  unlock_bsf();

  return(r);
}


/*===========================================================================*
 *				mount_pfs				     *
 *===========================================================================*/
PUBLIC void mount_pfs(void)
{
/* Mount the Pipe File Server. It's not really mounted onto the file system,
   but it's necessary it has a vmnt entry to make locking easier */

  dev_t dev;
  struct vmnt *vmp;

  if ((dev = find_free_nonedev()) == NO_DEV)
	panic("VFS: no nonedev to initialize PFS");

  if ((vmp = get_free_vmnt()) == NULL)
	panic("VFS: no vmnt to initialize PFS");

  alloc_nonedev(dev);

  vmp->m_dev = dev;
  vmp->m_fs_e = PFS_PROC_NR;
  strcpy(vmp->m_label, "pfs");
}

/*===========================================================================*
 *                              do_umount                                    *
 *===========================================================================*/
PUBLIC int do_umount(void)
{
/* Perform the umount(name) system call. */
  char label[LABEL_MAX];
  dev_t dev;
  int r;
  char fullpath[PATH_MAX];

  /* Only the super-user may do umount. */
  if (!super_user) return(EPERM);

  /* If 'name' is not for a block special file or mountpoint, return error. */
  if (fetch_name(m_in.name, m_in.name_length, M3, fullpath) != OK)
	return(err_code);
  if ((dev = name_to_dev(TRUE /*allow_mountpt*/, fullpath)) == NO_DEV)
	return(err_code);

  if ((r = unmount(dev, label)) != OK) return(r);

  /* Return the label of the mounted file system, so that the caller
   * can shut down the corresponding server process.
   */
  if (strlen(label) >= M3_LONG_STRING)	/* should never evaluate to true */
	label[M3_LONG_STRING-1] = 0;
  strcpy(m_out.umount_label, label);
  return(OK);
}


/*===========================================================================*
 *                              unmount                                      *
 *===========================================================================*/
PUBLIC int unmount(
  dev_t dev,			/* block-special device */
  char *label			/* buffer to retrieve label, or NULL */
)
{
  struct vnode *vp;
  struct vmnt *vmp_i = NULL, *vmp = NULL;
  int count, locks, r;

  /* Find vmnt that is to be unmounted */
  for (vmp_i = &vmnt[0]; vmp_i < &vmnt[NR_MNTS]; ++vmp_i) {
	  if (vmp_i->m_dev == dev) {
		  if(vmp) panic("device mounted more than once: %d", dev);
		  vmp = vmp_i;
	  }
  }

  /* Did we find the vmnt (i.e., was dev a mounted device)? */
  if(!vmp) return(EINVAL);

  if ((r = lock_vmnt(vmp, VMNT_EXCL)) != OK) return(r);

  /* See if the mounted device is busy.  Only 1 vnode using it should be
   * open -- the root vnode -- and that inode only 1 time. */
  locks = count = 0;
  for (vp = &vnode[0]; vp < &vnode[NR_VNODES]; vp++)
	  if (vp->v_ref_count > 0 && vp->v_dev == dev) {
		count += vp->v_ref_count;
		if (is_vnode_locked(vp)) locks++;
	  }

  if (count > 1 || locks > 1 || tll_haspendinglock(&vmp->m_lock)) {
	unlock_vmnt(vmp);
	return(EBUSY);    /* can't umount a busy file system */
  }

  /* Tell FS to drop all inode references for root inode except 1. */
  vnode_clean_refs(vmp->m_root_node);

  if (vmp->m_mounted_on) {
	put_vnode(vmp->m_mounted_on);
	vmp->m_mounted_on = NULL;
  }

  vmp->m_comm.c_max_reqs = 1;	/* Force max concurrent reqs to just one, so
				 * we won't send any messages after the
				 * unmount request */

  /* Tell FS to unmount */
  if ((r = req_unmount(vmp->m_fs_e)) != OK)              /* Not recoverable. */
	printf("VFS: ignoring failed umount attempt FS endpoint: %d (%d)\n",
	       vmp->m_fs_e, r);

  if (is_nonedev(vmp->m_dev)) free_nonedev(vmp->m_dev);

  if (label != NULL) strcpy(label, vmp->m_label);

  if (vmp->m_root_node) {	/* PFS lacks a root node */
	vmp->m_root_node->v_ref_count = 0;
	vmp->m_root_node->v_fs_count = 0;
	vmp->m_root_node->v_sdev = NO_DEV;
	vmp->m_root_node = NULL;
  }
  vmp->m_dev = NO_DEV;
  vmp->m_fs_e = NONE;

  unlock_vmnt(vmp);

  /* The root FS will handle block I/O requests for this device now. */
  lock_bsf();
  update_bspec(dev, ROOT_FS_E, 1 /* send new driver endpoint */);
  unlock_bsf();

  return(OK);
}


/*===========================================================================*
 *				unmount_all				     *
 *===========================================================================*/
PUBLIC void unmount_all(void)
{
/* Unmount all filesystems.  File systems are mounted on other file systems,
 * so you have to pull off the loose bits repeatedly to get it all undone.
 */

  int i;
  struct vmnt *vmp;

  /* Now unmount the rest */
  for (i = 0; i < NR_MNTS; i++) {
	/* Unmount at least one. */
	for (vmp = &vmnt[0]; vmp < &vmnt[NR_MNTS]; vmp++) {
		if (vmp->m_dev != NO_DEV)
			unmount(vmp->m_dev, NULL);
	}
  }
  check_vnode_locks();
  check_vmnt_locks();
  check_filp_locks();
  check_bsf_lock();
}

/*===========================================================================*
 *                              name_to_dev                                  *
 *===========================================================================*/
PRIVATE dev_t name_to_dev(int allow_mountpt, char path[PATH_MAX])
{
/* Convert the block special file in 'user_fullpath' to a device number.
 * If the given path is not a block special file, but 'allow_mountpt' is set
 * and the path is the root node of a mounted file system, return that device
 * number. In all other cases, return NO_DEV and an error code in 'err_code'.
 */
  dev_t dev;
  struct vnode *vp;
  struct vmnt *vmp;
  struct lookup resolve;

  lookup_init(&resolve, path, PATH_NOFLAGS, &vmp, &vp);
  resolve.l_vmnt_lock = VMNT_READ;
  resolve.l_vnode_lock = VNODE_READ;

  /* Request lookup */
  if ((vp = eat_path(&resolve, fp)) == NULL) return(NO_DEV);

  if ((vp->v_mode & I_TYPE) == I_BLOCK_SPECIAL) {
	dev = vp->v_sdev;
  } else if (allow_mountpt && vp->v_vmnt->m_root_node == vp) {
	dev = vp->v_dev;
  } else {
	err_code = ENOTBLK;
	dev = NO_DEV;
  }

  unlock_vnode(vp);
  unlock_vmnt(vmp);
  put_vnode(vp);
  return(dev);
}


/*===========================================================================*
 *                              is_nonedev				     *
 *===========================================================================*/
PUBLIC int is_nonedev(dev_t dev)
{
/* Return whether the given device is a "none" pseudo device.
 */

  return (major(dev) == NONE_MAJOR &&
	minor(dev) > 0 && minor(dev) <= NR_NONEDEVS);
}


/*===========================================================================*
 *                              find_free_nonedev			     *
 *===========================================================================*/
PRIVATE dev_t find_free_nonedev(void)
{
/* Find a free "none" pseudo device. Do not allocate it yet.
 */
  int i;

  for (i = 0; i < NR_NONEDEVS; i++)
	if (!GET_BIT(nonedev, i))
		return makedev(NONE_MAJOR, i + 1);

  err_code = EMFILE;
  return NO_DEV;
}
