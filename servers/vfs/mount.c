/* This file performs the MOUNT and UMOUNT system calls.
 *
 * The entry points into this file are
 *   do_fslogin:	perform the FSLOGIN system call
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
#include "file.h"
#include "fproc.h"
#include "param.h"
#include <minix/vfsif.h>
#include "vnode.h"
#include "vmnt.h"

/* Allow the root to be replaced before the first 'real' mount. */
PRIVATE int allow_newroot = 1;

/* Bitmap of in-use "none" pseudo devices. */
PRIVATE bitchunk_t nonedev[BITMAP_CHUNKS(NR_NONEDEVS)] = { 0 };

#define alloc_nonedev(dev) SET_BIT(nonedev, minor(dev) - 1)
#define free_nonedev(dev) UNSET_BIT(nonedev, minor(dev) - 1)

FORWARD _PROTOTYPE( dev_t name_to_dev, (int allow_mountpt)		);
FORWARD _PROTOTYPE( int mount_fs, (endpoint_t fs_e)			);
FORWARD _PROTOTYPE( int is_nonedev, (Dev_t dev)				);
FORWARD _PROTOTYPE( dev_t find_free_nonedev, (void)				);

/*===========================================================================*
 *                              do_fslogin                                   *
 *===========================================================================*/
PUBLIC int do_fslogin()
{
  int r;

  /* Login before mount request */
  if (mount_fs_e != who_e) {
      last_login_fs_e = who_e;
      r = SUSPEND;
  }
  /* Login after a suspended mount */
  else {
	/* Copy back original mount request message */
	m_in = mount_m_in;

	/* Set up last login FS */
	last_login_fs_e = who_e;

	/* Set up endpoint and call nr */
	who_e = m_in.m_source;
	who_p = _ENDPOINT_P(who_e);
	call_nr = m_in.m_type;
	fp = &fproc[who_p];       /* pointer to proc table struct */
	super_user = (fp->fp_effuid == SU_UID ? TRUE : FALSE);   /* su? */
      
	r = mount_fs(mount_fs_e);
  }
  return(r);
}


/*===========================================================================*
 *                              do_mount                                     *
 *===========================================================================*/
PUBLIC int do_mount()
{
  u32_t fs_e;
  int r, proc_nr;

  /* Only the super-user may do MOUNT. */
  if (!super_user) return(EPERM);
	
  /* FS process' endpoint number */ 
  if (m_in.mount_flags & MS_LABEL16) {
	/* Get the label from the caller, and ask DS for the endpoint. */
	r = sys_datacopy(who_e, (vir_bytes) m_in.fs_label, SELF,
		(vir_bytes) mount_label, (phys_bytes) sizeof(mount_label));
	if (r != OK) return(r);

	mount_label[sizeof(mount_label)-1] = 0;

	r = ds_retrieve_label_num(mount_label, &fs_e);
	if (r != OK) return(r);

	if (isokendpt(fs_e, &proc_nr) != OK) return(EINVAL);
  } else {
	/* Legacy support: get the endpoint from the request itself. */
	fs_e = (unsigned long) m_in.fs_label;
	mount_label[0] = 0;
  }

  /* Sanity check on process number. */
  if(fs_e <= 0) {
	printf("VFS: warning: got process number %d for mount call.\n", fs_e);
	return EINVAL;
  }

  /* Do the actual job */
  return mount_fs(fs_e);
}


/*===========================================================================*
 *                              mount                                        *
 *===========================================================================*/
PRIVATE int mount_fs(endpoint_t fs_e)
{
/* Perform the mount(name, mfile, mount_flags) system call. */
  int rdir, mdir;               /* TRUE iff {root|mount} file is dir */
  int i, r, found, rdonly, nodev, isroot, replace_root;
  struct fproc *tfp;
  struct dmap *dp;
  dev_t dev;
  message m;
  struct vnode *root_node, *vp = NULL, *bspec;
  struct vmnt *vmp;
  char *label;
  struct node_details res;

  /* Only the super-user may do MOUNT. */
  if (!super_user) return(EPERM);

  /* If FS not yet logged in, save message and suspend mount */
  if (last_login_fs_e != fs_e) {
	mount_m_in = m_in;
	mount_fs_e = fs_e;
	/* mount_label is already saved */
	return(SUSPEND);
  }
  
  /* Mount request got after FS login or FS login arrived after a suspended
   * mount.
   */
  last_login_fs_e = NONE;
  
  /* Clear endpoint field */
  mount_fs_e = NONE;

  /* Should the file system be mounted read-only? */
  rdonly = (m_in.mount_flags & MS_RDONLY);

  /* A null string for block special device means don't use a device at all. */
  nodev = (m_in.name1_length == 0);

  if (!nodev) {
	/* If 'name' is not for a block special file, return error. */
	if (fetch_name(m_in.name1, m_in.name1_length, M1) != OK)
		return(err_code);
	if ((dev = name_to_dev(FALSE /*allow_mountpt*/)) == NO_DEV)
		return(err_code);
  } else {
	/* Find a free pseudo-device as substitute for an actual device. */
	if ((dev = find_free_nonedev()) == NO_DEV)
		return(err_code);
  }

  /* Check whether there is a block special file open which uses the 
   * same device (partition) */
  for (bspec = &vnode[0]; bspec < &vnode[NR_VNODES]; ++bspec) {
	if (bspec->v_ref_count > 0 && bspec->v_sdev == dev) {
		/* Found, sync the buffer cache */
		req_sync(bspec->v_fs_e);          
		break;
		/* Note: there are probably some blocks in the FS process'
		 * buffer cache which contain data on this minor, although
		 * they will be purged since the handling moves to the new
		 * FS process (if everything goes well with the mount...)
		 */ 
	}
  }
  /* Didn't find? */
  if (bspec == &vnode[NR_VNODES] && bspec->v_sdev != dev)
	bspec = NULL;
  
  /* Scan vmnt table to see if dev already mounted. If not, find a free slot.*/
  found = FALSE; 
  vmp = NIL_VMNT;
  for (i = 0; i < NR_MNTS; ++i) {
	  if (vmnt[i].m_dev == dev) {
		  vmp = &vmnt[i];
		  found = TRUE;
		  break;
	  } else if (!vmp && vmnt[i].m_dev == NO_DEV) {
		  vmp = &vmnt[i];
	  }
  }

  /* Partition was/is already mounted */
  if (found) {
	/* It is possible that we have an old root lying around that 
	 * needs to be remounted. This could for example be a boot
	 * ramdisk that has already been replaced by the real root.
	 */
	if(vmp->m_mounted_on || root_dev == vmp->m_dev) {
		return(EBUSY);   /* not a root or still mounted */
	}
  
	/* Now get the inode of the file to be mounted on. */
	if (fetch_name(m_in.name2, m_in.name2_length, M1)!=OK) return(err_code);
	if ((vp = eat_path(PATH_NOFLAGS)) == NIL_VNODE) return(err_code);
	if (vp->v_ref_count != 1) {
		put_vnode(vp);
		return(EBUSY);
	}

	/* Tell FS on which vnode it is mounted (glue into mount tree) */
	if ((r = req_mountpoint(vp->v_fs_e, vp->v_inode_nr)) == OK) {
		root_node = vmp->m_root_node;

		/* File types of 'vp' and 'root_node' may not conflict. */
		mdir = ((vp->v_mode & I_TYPE) == I_DIRECTORY);/* TRUE iff dir*/
		rdir = ((root_node->v_mode & I_TYPE) == I_DIRECTORY);
		if(!mdir && rdir) r = EISDIR;
	}

	if (r != OK) {
		put_vnode(vp);
		return(r);
	}

	/* Nothing else can go wrong.  Perform the mount. */
	vmp->m_mounted_on = vp;
	vmp->m_flags = rdonly;
	strcpy(vmp->m_label, mount_label);
	allow_newroot = 0;             	/* The root is now fixed */
	if (nodev) alloc_nonedev(dev);	/* Make the allocation final */

	return(OK);
  }

  /* Fetch the name of the mountpoint */
  if (fetch_name(m_in.name2, m_in.name2_length, M1) != OK) return(err_code);
  isroot = (strcmp(user_fullpath, "/") == 0);
  replace_root = (isroot && allow_newroot);

  if(!replace_root) {
  	/* Get vnode of mountpoint */
	if ((vp = eat_path(PATH_NOFLAGS)) == NIL_VNODE) return(err_code);

	/* Tell FS on which vnode it is mounted (glue into mount tree) */
	if ((r = req_mountpoint(vp->v_fs_e, vp->v_inode_nr)) != OK) {
		put_vnode(vp);
		return r;
	}
  }

  /* We'll need a vnode for the root inode, check whether there is one */
  if ((root_node = get_free_vnode()) == NIL_VNODE) {
	if (vp != NIL_VNODE) put_vnode(vp);
	return(ENFILE);
  }

  label = "";
  if (!nodev) {
	/* Get driver process' endpoint */
	dp = &dmap[(dev >> MAJOR) & BYTE];
	if (dp->dmap_driver == NONE) {
		printf("VFS: no driver for dev %x\n", dev);
		if (vp != NIL_VNODE) put_vnode(vp);
		return(EINVAL);
	}

	label = dp->dmap_label;
	if (strlen(label) == 0)
		panic(__FILE__, "VFS mount_fs: no label for major",
			dev >> MAJOR);
  }

  /* Tell FS which device to mount */
  if ((r = req_readsuper(fs_e, label, dev, rdonly, isroot, &res)) != OK) {
	if (vp != NIL_VNODE) put_vnode(vp);
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
  
  /* Fill in max file size and blocksize for the vmnt */
  vmp->m_fs_e = res.fs_e;
  vmp->m_dev = dev;
  vmp->m_flags = rdonly;
  
  /* Root node is indeed on the partition */
  root_node->v_vmnt = vmp;
  root_node->v_dev = vmp->m_dev;
  
  if(replace_root) {
	/* Superblock and root node already read. 
	 * Nothing else can go wrong. Perform the mount. */
	vmp->m_root_node = root_node;
	vmp->m_mounted_on = NULL;
	strcpy(vmp->m_label, mount_label);
	if (nodev) alloc_nonedev(dev);

	root_dev = dev;
	ROOT_FS_E = fs_e;

	/* Replace all root and working directories */
	for (i= 0, tfp= fproc; i<NR_PROCS; i++, tfp++) {
		if (tfp->fp_pid == PID_FREE)
			continue;

#define MAKEROOT(what) { 		\
		put_vnode(what);	\
		dup_vnode(root_node);	\
		what = root_node;	\
	}

		if(tfp->fp_rd) MAKEROOT(tfp->fp_rd);
		if(tfp->fp_wd) MAKEROOT(tfp->fp_wd);
	}

	return(OK);
  }
  
  /* File types may not conflict. */
  mdir = ((vp->v_mode & I_TYPE) == I_DIRECTORY); /*TRUE iff dir*/
  rdir = ((root_node->v_mode & I_TYPE) == I_DIRECTORY);
  if (!mdir && rdir) r = EISDIR;

  /* If error, return the super block and both inodes; release the vmnt. */
  if (r != OK) {
	put_vnode(vp);
	put_vnode(root_node);
	vmp->m_dev = NO_DEV;
	return(r);
  }

  /* Nothing else can go wrong.  Perform the mount. */
  vmp->m_mounted_on = vp;
  vmp->m_root_node = root_node;
  strcpy(vmp->m_label, mount_label);
  
  /* The root is now fixed */
  allow_newroot = 0;

  /* Allocate the pseudo device that was found, if not using a real device. */
  if (nodev) alloc_nonedev(dev);

  /* There was a block spec file open, and it should be handled by the 
   * new FS proc now */
  if (bspec) bspec->v_bfs_e = fs_e; 

  return(OK);
}


/*===========================================================================*
 *                              do_umount                                    *
 *===========================================================================*/
PUBLIC int do_umount()
{
/* Perform the umount(name) system call. */
  char label[LABEL_MAX];
  dev_t dev;
  int r;
	
  /* Only the super-user may do umount. */
  if (!super_user) return(EPERM);
	
  /* If 'name' is not for a block special file or mountpoint, return error. */
  if(fetch_name(m_in.name, m_in.name_length, M3) != OK) return(err_code);
  if((dev = name_to_dev(TRUE /*allow_mountpt*/)) == NO_DEV) return(err_code);

  if((r = unmount(dev, label)) != OK) return(r);

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
PUBLIC int unmount(dev, label)
Dev_t dev;				/* block-special device */
char *label;				/* buffer to retrieve label, or NULL */
{
  struct vnode *vp, *vi;
  struct vmnt *vmp_i = NULL, *vmp = NULL;
  struct dmap *dp;
  int count, r;
  int fs_e;
  
  /* Find vmnt that is to be unmounted */
  for(vmp_i = &vmnt[0]; vmp_i < &vmnt[NR_MNTS]; ++vmp_i) {
	  if (vmp_i->m_dev == dev) {
		  if(vmp) panic(__FILE__,"device mounted more than once", dev);
		  vmp = vmp_i;
	  }
  }

  /* Did we find the vmnt (i.e., was dev a mounted device)? */
  if(!vmp) return(EINVAL);
  
  /* See if the mounted device is busy.  Only 1 vnode using it should be
   * open -- the root vnode -- and that inode only 1 time. */
  count = 0;
  for(vp = &vnode[0]; vp < &vnode[NR_VNODES]; vp++)
	  if(vp->v_ref_count > 0 && vp->v_dev == dev) count += vp->v_ref_count;

  if(count > 1) return(EBUSY);    /* can't umount a busy file system */
  
  /* Tell FS to drop all inode references for root inode except 1. */
  vnode_clean_refs(vmp->m_root_node);

  if (vmp->m_mounted_on) {
	put_vnode(vmp->m_mounted_on);
	vmp->m_mounted_on = NIL_VNODE;
  }

  /* Tell FS to unmount */
  if(vmp->m_fs_e <= 0 || vmp->m_fs_e == NONE)
	panic(__FILE__, "unmount: strange fs endpoint", vmp->m_fs_e);

  if ((r = req_unmount(vmp->m_fs_e)) != OK)              /* Not recoverable. */
	printf("VFS: ignoring failed umount attempt (%d)\n", r);

  if (is_nonedev(vmp->m_dev))
	free_nonedev(vmp->m_dev);

  if (label != NULL)
	strcpy(label, vmp->m_label);
 
  vmp->m_root_node->v_ref_count = 0;
  vmp->m_root_node->v_fs_count = 0;
  vmp->m_root_node->v_sdev = NO_DEV;
  vmp->m_root_node = NIL_VNODE;
  vmp->m_dev = NO_DEV;
  vmp->m_fs_e = NONE;
	
  /* Is there a block special file that was handled by that partition? */
  for (vp = &vnode[0]; vp < &vnode[NR_VNODES]; vp++) {
	if((vp->v_mode & I_TYPE)==I_BLOCK_SPECIAL && vp->v_bfs_e==vmp->m_fs_e){
		  
		/* Get the driver endpoint of the block spec device */
		dp = &dmap[(dev >> MAJOR) & BYTE];
		if (dp->dmap_driver == NONE) {
			printf("VFS: driver not found for device %d\n", dev);
			continue;
		}

		printf("VFS: umount moving block spec %d to root FS\n", dev);
		vp->v_bfs_e = ROOT_FS_E;
			
		  /* Send the (potentially new) driver endpoint */
		r = req_newdriver(vp->v_bfs_e, vp->v_sdev, dp->dmap_driver);
		if (r != OK) 
			printf("VFS: error sending driver endpoint for"
				" moved block spec\n");
		  
	}
  }
  
  return(OK);
}


/*===========================================================================*
 *                              name_to_dev                                  *
 *===========================================================================*/
PRIVATE dev_t name_to_dev(allow_mountpt)
int allow_mountpt;
{
/* Convert the block special file in 'user_fullpath' to a device number.
 * If the given path is not a block special file, but 'allow_mountpt' is set
 * and the path is the root node of a mounted file system, return that device
 * number. In all other cases, return NO_DEV and an error code in 'err_code'.
 */
  int r;
  dev_t dev;
  struct vnode *vp;
  
  /* Request lookup */
  if ((vp = eat_path(PATH_NOFLAGS)) == NIL_VNODE) {
	return(NO_DEV);
  }

  if ((vp->v_mode & I_TYPE) == I_BLOCK_SPECIAL) {
	dev = vp->v_sdev;
  } else if (allow_mountpt && vp->v_vmnt->m_root_node == vp) {
	dev = vp->v_dev;
  } else {
  	err_code = ENOTBLK;
	dev = NO_DEV;
  }

  put_vnode(vp);
  return(dev);
}


/*===========================================================================*
 *                              is_nonedev				     *
 *===========================================================================*/
PRIVATE int is_nonedev(dev)
{
/* Return whether the given device is a "none" pseudo device.
 */

  return (major(dev) == NONE_MAJOR &&
	minor(dev) > 0 && minor(dev) <= NR_NONEDEVS);
}


/*===========================================================================*
 *                              find_free_nonedev			     *
 *===========================================================================*/
PRIVATE dev_t find_free_nonedev()
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
