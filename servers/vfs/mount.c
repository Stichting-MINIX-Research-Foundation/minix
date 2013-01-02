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
static int have_root = 0;

/* Bitmap of in-use "none" pseudo devices. */
static bitchunk_t nonedev[BITMAP_CHUNKS(NR_NONEDEVS)] = { 0 };

#define alloc_nonedev(dev) SET_BIT(nonedev, minor(dev) - 1)
#define free_nonedev(dev) UNSET_BIT(nonedev, minor(dev) - 1)

static dev_t name_to_dev(int allow_mountpt, char path[PATH_MAX]);
static dev_t find_free_nonedev(void);
static void update_bspec(dev_t dev, endpoint_t fs_e, int send_drv_e);

/*===========================================================================*
 *				update_bspec				     *
 *===========================================================================*/
static void update_bspec(dev_t dev, endpoint_t fs_e, int send_drv_e)
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
				/* Can't update for out-of-range major */
				continue;
			}
			dp = &dmap[major(dev)];
			if (dp->dmap_driver == NONE) {
				/* Can't update for vanished driver */
				printf("VFS: can't send new driver label\n");
				continue;
			}

			if ((r = req_newdriver(fs_e, vp->v_sdev,
					dp->dmap_label)) != OK) {
				printf("VFS: Failed to send new driver label"
				       " for moved block special file to %d\n",
				       fs_e);
			}
		}
	}
}

/*===========================================================================*
 *                              do_fsready                                   *
 *===========================================================================*/
int do_fsready()
{
  /* deprecated */
  return(SUSPEND);
}

/*===========================================================================*
 *                              do_mount                                     *
 *===========================================================================*/
int do_mount()
{
/* Perform the mount(name, mfile, mount_flags) system call. */
  endpoint_t fs_e;
  int r, slot, rdonly, nodev;
  char mount_path[PATH_MAX], mount_dev[PATH_MAX];
  char mount_label[LABEL_MAX];
  dev_t dev;
  int mflags;
  vir_bytes label, vname1, vname2;
  size_t vname1_length, vname2_length;

  mflags = job_m_in.mount_flags;
  label = (vir_bytes) job_m_in.fs_label;
  vname1 = (vir_bytes) job_m_in.name1;
  vname1_length = (size_t) job_m_in.name1_length;
  vname2 = (vir_bytes) job_m_in.name2;
  vname2_length = (size_t) job_m_in.name2_length;

  /* Only the super-user may do MOUNT. */
  if (!super_user) return(EPERM);


  /* FS process' endpoint number */
  if (mflags & MS_LABEL16) {
	/* Get the label from the caller, and ask DS for the endpoint. */
	r = sys_datacopy(who_e, label, SELF, (vir_bytes) mount_label,
			 sizeof(mount_label));
	if (r != OK) return(r);

	mount_label[sizeof(mount_label)-1] = 0;

	r = ds_retrieve_label_endpt(mount_label, &fs_e);
	if (r != OK) return(r);
  } else {
	/* Legacy support: get the endpoint from the request itself. */
	fs_e = (endpoint_t) label;
	mount_label[0] = 0;
  }

  /* Sanity check on process number. */
  if (isokendpt(fs_e, &slot) != OK) return(EINVAL);

  /* Should the file system be mounted read-only? */
  rdonly = (mflags & MS_RDONLY);

  /* A null string for block special device means don't use a device at all. */
  nodev = (vname1_length == 0);
  if (!nodev) {
	/* If 'name' is not for a block special file, return error. */
	if (fetch_name(vname1, vname1_length, mount_dev) != OK)
		return(err_code);
	if ((dev = name_to_dev(FALSE /*allow_mountpt*/, mount_dev)) == NO_DEV)
		return(err_code);
  } else {
	/* Find a free pseudo-device as substitute for an actual device. */
	if ((dev = find_free_nonedev()) == NO_DEV)
		return(err_code);
	strlcpy(mount_dev, "none", sizeof(mount_dev));
  }

  /* Fetch the name of the mountpoint */
  if (fetch_name(vname2, vname2_length, mount_path) != OK) return(err_code);

  /* Do the actual job */
  return mount_fs(dev, mount_dev, mount_path, fs_e, rdonly, mount_label);
}


/*===========================================================================*
 *                              mount_fs				     *
 *===========================================================================*/
int mount_fs(
dev_t dev,
char mount_dev[PATH_MAX],
char mount_path[PATH_MAX],
endpoint_t fs_e,
int rdonly,
char mount_label[LABEL_MAX] )
{
  int i, r = OK, found, isroot, mount_root, con_reqs, slot;
  struct fproc *tfp, *rfp;
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

  strlcpy(new_vmp->m_mount_path, mount_path, PATH_MAX);
  strlcpy(new_vmp->m_mount_dev, mount_dev, PATH_MAX);
  isroot = (strcmp(mount_path, "/") == 0);
  mount_root = (isroot && have_root < 2); /* Root can be mounted twice:
					   * 1: ramdisk
					   * 2: boot disk (e.g., harddisk)
					   */

  if (!mount_root) {
	/* Get vnode of mountpoint */
	lookup_init(&resolve, mount_path, PATH_NOFLAGS, &parent_vmp, &vp);
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

  /* We'll need a vnode for the root inode */
  if ((root_node = get_free_vnode()) == NULL) {
	if (vp != NULL) {
		unlock_vnode(vp);
		put_vnode(vp);
	}
	unlock_vmnt(new_vmp);
	return(err_code);
  }
  lock_vnode(root_node, VNODE_OPCL);

  /* Record process as a system process */
  if (isokendpt(fs_e, &slot) != OK) {
	if (vp != NULL) {
		unlock_vnode(vp);
		put_vnode(vp);
	}
	unlock_vnode(root_node);
	unlock_vmnt(new_vmp);
	return(EINVAL);
  }
  rfp = &fproc[slot];
  rfp->fp_flags |= FP_SRV_PROC;	/* File Servers are also services */

  /* Store some essential vmnt data first */
  new_vmp->m_fs_e = fs_e;
  new_vmp->m_dev = dev;
  if (rdonly) new_vmp->m_flags |= VMNT_READONLY;
  else new_vmp->m_flags &= ~VMNT_READONLY;

  /* Tell FS which device to mount */
  new_vmp->m_flags |= VMNT_MOUNTING;
  r = req_readsuper(fs_e, label, dev, rdonly, isroot, &res, &con_reqs);
  new_vmp->m_flags &= ~VMNT_MOUNTING;

  if (r != OK) {
	mark_vmnt_free(new_vmp);
	unlock_vnode(root_node);
	if (vp != NULL) {
		unlock_vnode(vp);
		put_vnode(vp);
	}
	unlock_vmnt(new_vmp);
	return(r);
  }

  lock_bsf();

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

  if (mount_root) {
	/* Superblock and root node already read.
	 * Nothing else can go wrong. Perform the mount. */
	new_vmp->m_root_node = root_node;
	new_vmp->m_mounted_on = NULL;
	strlcpy(new_vmp->m_label, mount_label, LABEL_MAX);
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
  if (!S_ISDIR(vp->v_mode) && S_ISDIR(root_node->v_mode)) r = EISDIR;

  /* If error, return the super block and both inodes; release the vmnt. */
  if (r != OK) {
	unlock_vnode(vp);
	unlock_vnode(root_node);
	mark_vmnt_free(new_vmp);
	unlock_vmnt(new_vmp);
	put_vnode(vp);
	put_vnode(root_node);
	unlock_bsf();
	return(r);
  }

  /* Nothing else can go wrong.  Perform the mount. */
  new_vmp->m_mounted_on = vp;
  new_vmp->m_root_node = root_node;
  strlcpy(new_vmp->m_label, mount_label, LABEL_MAX);

  /* Allocate the pseudo device that was found, if not using a real device. */
  if (is_nonedev(dev)) alloc_nonedev(dev);

  /* The new FS will handle block I/O requests for its device now. */
  if (!(new_vmp->m_flags & VMNT_FORCEROOTBSF))
	update_bspec(dev, fs_e, 0 /* Don't send new driver endpoint */);

  unlock_vnode(vp);
  unlock_vnode(root_node);
  unlock_vmnt(new_vmp);
  unlock_bsf();

  return(OK);
}


/*===========================================================================*
 *				mount_pfs				     *
 *===========================================================================*/
void mount_pfs(void)
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
  strlcpy(vmp->m_label, "pfs", LABEL_MAX);
  strlcpy(vmp->m_mount_path, "pipe", PATH_MAX);
  strlcpy(vmp->m_mount_dev, "none", PATH_MAX);
}

/*===========================================================================*
 *                              do_umount                                    *
 *===========================================================================*/
int do_umount(void)
{
/* Perform the umount(name) system call.
 * syscall might provide 'name' embedded in the message.
 */
  char label[LABEL_MAX];
  dev_t dev;
  int r;
  char fullpath[PATH_MAX];
  vir_bytes vname;
  size_t vname_length;

  vname = (vir_bytes) job_m_in.name;
  vname_length = (size_t) job_m_in.name_length;

  /* Only the super-user may do umount. */
  if (!super_user) return(EPERM);

  /* If 'name' is not for a block special file or mountpoint, return error. */
  if (copy_name(vname_length, fullpath) != OK) {
	/* Direct copy failed, try fetching from user space */
	if (fetch_name(vname, vname_length, fullpath) != OK)
		return(err_code);
  }
  if ((dev = name_to_dev(TRUE /*allow_mountpt*/, fullpath)) == NO_DEV)
	return(err_code);

  if ((r = unmount(dev, label)) != OK) return(r);

  /* Return the label of the mounted file system, so that the caller
   * can shut down the corresponding server process.
   */
  if (strlen(label) >= M3_LONG_STRING)	/* should never evaluate to true */
	label[M3_LONG_STRING-1] = 0;
  strlcpy(m_out.umount_label, label, M3_LONG_STRING);
  return(OK);
}


/*===========================================================================*
 *                              unmount					     *
 *===========================================================================*/
int unmount(
  dev_t dev,			/* block-special device */
  char label[LABEL_MAX]		/* buffer to retrieve label, or NULL */
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

  if (label != NULL) strlcpy(label, vmp->m_label, LABEL_MAX);

  if (vmp->m_root_node) {	/* PFS lacks a root node */
	vmp->m_root_node->v_ref_count = 0;
	vmp->m_root_node->v_fs_count = 0;
	vmp->m_root_node->v_sdev = NO_DEV;
	vmp->m_root_node = NULL;
  }
  mark_vmnt_free(vmp);

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
void unmount_all(int force)
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

  if (!force) return;

  /* Verify nothing is locked anymore */
  check_vnode_locks();
  check_vmnt_locks();
  check_filp_locks();
  check_bsf_lock();

  /* Verify we succesfully unmounted all file systems */
  for (vmp = &vmnt[0]; vmp < &vmnt[NR_MNTS]; vmp++) {
	if (vmp->m_dev != NO_DEV) {
		panic("vmp still mounted: %s %d %d\n", vmp->m_label,
			vmp->m_fs_e, vmp->m_dev);
	}
  }
}

/*===========================================================================*
 *                              name_to_dev                                  *
 *===========================================================================*/
static dev_t name_to_dev(int allow_mountpt, char path[PATH_MAX])
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

  if (S_ISBLK(vp->v_mode)) {
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
int is_nonedev(dev_t dev)
{
/* Return whether the given device is a "none" pseudo device.
 */

  return (major(dev) == NONE_MAJOR &&
	minor(dev) > 0 && minor(dev) <= NR_NONEDEVS);
}


/*===========================================================================*
 *                              find_free_nonedev			     *
 *===========================================================================*/
static dev_t find_free_nonedev(void)
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
