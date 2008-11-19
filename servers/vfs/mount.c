/* This file performs the MOUNT and UMOUNT system calls.
 *
 * The entry points into this file are
 *   do_mount:  perform the MOUNT system call
 *   do_umount: perform the UMOUNT system call
 *
 * Changes for VFS:
 *   Jul 2006 (Balazs Gerofi)
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
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include "file.h"
#include "fproc.h"
#include "param.h"

#include <minix/vfsif.h>
#include "vnode.h"
#include "vmnt.h"

/* Allow the root to be replaced before the first 'real' mount. */
PRIVATE int allow_newroot = 1;

FORWARD _PROTOTYPE( dev_t name_to_dev, (void)                           );
FORWARD _PROTOTYPE( int mount_fs, (endpoint_t fs_e)                     );

/*===========================================================================*
 *                              do_fslogin                                   *
 *===========================================================================*/
PUBLIC int do_fslogin()
{
  /* Login before mount request */
  if ((unsigned long)mount_m_in.m1_p3 != who_e) {
      last_login_fs_e = who_e;
      return SUSPEND;
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
      
      
      return do_mount();
  }
}

/*===========================================================================*
 *                              do_mount                                     *
 *===========================================================================*/
PUBLIC int do_mount()
{
  endpoint_t fs_e; 

  /* Only the super-user may do MOUNT. */
  if (!super_user) return(EPERM);
	
  /* FS process' endpoint number */ 
  fs_e = (unsigned long)m_in.m1_p3;

  /* Sanity check on process number. */
  if(fs_e <= 0) {
	printf("vfs: warning: got process number %d for mount call.\n", fs_e);
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
/* Perform the mount(name, mfile, rd_only) system call. */
  int rdir, mdir;               /* TRUE iff {root|mount} file is dir */
  int i, r, found, isroot, replace_root;
  struct fproc *tfp;
  struct dmap *dp;
  dev_t dev;
  message m;
  struct vnode *vp, *root_node, *mounted_on, *bspec;
  struct vmnt *vmp, *vmp2;
  char *label;
  struct node_details res;
  
  /* Only the super-user may do MOUNT. */
  if (!super_user) return(EPERM);

  /* If FS not yet logged in, save message and suspend mount */
  if (last_login_fs_e != fs_e) {
      mount_m_in = m_in; 
      return SUSPEND;
  }
  
  /* Mount request got after FS login or 
   * FS login arrived after a suspended mount */
  last_login_fs_e = NONE;
  
  /* Clear endpoint field */
  mount_m_in.m1_p3 = (char *) NONE;

  /* If 'name' is not for a block special file, return error. */
  if (fetch_name(m_in.name1, m_in.name1_length, M1) != OK) return(err_code);
  
  /* Convert name to device number */
  if ((dev = name_to_dev()) == NO_DEV) return(err_code);

  /* Check whether there is a block special file open which uses the 
   * same device (partition) */
  for (bspec = &vnode[0]; bspec < &vnode[NR_VNODES]; ++bspec) {
      if (bspec->v_ref_count > 0 && bspec->v_sdev == dev) {
          /* Found, sync the buffer cache */
          req_sync(bspec->v_fs_e);          
          break;
          /* Note: there are probably some blocks in the FS process' buffer
           * cache which contain data on this minor, although they will be
           * purged since the handling moves to the new FS process (if
           * everything goes well with the mount...)
           */ 
      }
  }
  /* Didn't find? */
  if (bspec == &vnode[NR_VNODES] && bspec->v_sdev != dev)
      bspec = NULL;
  
  /* Scan vmnt table to see if dev already mounted, if not, 
   * find a free slot.*/
  found = FALSE; 
  vmp = NIL_VMNT;
  for (i = 0; i < NR_MNTS; ++i) {
        if (vmnt[i].m_dev == dev) {
            vmp = &vmnt[i];
            found = TRUE;
            break;
        }
        else if (!vmp && vmnt[i].m_dev == NO_DEV) {
            vmp = &vmnt[i];
        }
  }

  /* Partition was/is already mounted */
  if (found) {
	/* It is possible that we have an old root lying around that 
	 * needs to be remounted. */
	if (vmp->m_mounted_on != vmp->m_root_node ||
		vmp->m_mounted_on == fproc[FS_PROC_NR].fp_rd) {
		/* Normally, m_mounted_on refers to the mount point. For a
		 * root filesystem, m_mounted_on is equal to the root vnode.
		 * We assume that the root of FS is always the real root. If
		 * the two vnodes are different or if the root of FS is equal
		 * to the root of the filesystem we found, we found a
		 * filesystem that is in use.
		 */
		return EBUSY;   /* already mounted */
	}

	if (root_dev == vmp->m_dev)
		panic("fs", "inconsistency remounting old root", NO_NUM);

	/* Now get the inode of the file to be mounted on. */
	if (fetch_name(m_in.name2, m_in.name2_length, M1) != OK) {
		return(err_code);
	}

	/* Request lookup */
	r = lookup_vp(0 /*flags*/, 0 /*!use_realuid*/, &mounted_on);
	if (r != OK) return r;

	if (vp->v_ref_count != 1)
	{
		put_vnode(vp);
		printf("vfs:mount_fs: mount point is busy\n");
		return EBUSY;
	}

	/* Issue mountpoint request */
	r = req_mountpoint(mounted_on->v_fs_e, mounted_on->v_inode_nr);
	if (r != OK)
	{
		put_vnode(mounted_on);
		printf("vfs:mount_fs: req_mountpoint_s failed with %d\n", r);
		return r;
	}

	/* Get the root inode of the mounted file system. */
	root_node = vmp->m_root_node;

	/* File types may not conflict. */
	if (r == OK) {
		mdir = ((mounted_on->v_mode & I_TYPE) == I_DIRECTORY); 
		/* TRUE iff dir */
		rdir = ((root_node->v_mode & I_TYPE) == I_DIRECTORY);
		if (!mdir && rdir) r = EISDIR;
	}

	/* If error, return the mount point. */
	if (r != OK) {
		put_vnode(mounted_on);

		return(r);
	}

	/* Nothing else can go wrong.  Perform the mount. */
	put_vnode(vmp->m_mounted_on);
	vmp->m_mounted_on = mounted_on;
	vmp->m_flags = m_in.rd_only;
	allow_newroot = 0;              /* The root is now fixed */

	return(OK);
  }

  /* Fetch the name of the mountpoint */
  if (fetch_name(m_in.name2, m_in.name2_length, M1) != OK) {
	return(err_code);
  }

  isroot= (strcmp(user_fullpath, "/") == 0);
  replace_root= (isroot && allow_newroot);

  if (!replace_root)
  {
	/* Get mount point and inform the FS it is on. */
#if 0
	printf("vfs:mount_fs: mount point at '%s'\n", user_fullpath);
#endif

	r = lookup_vp(0 /*flags*/, 0 /*!use_realuid*/, &mounted_on);
	if (r != OK)
		return r;

	/* Issue mountpoint request */
	r = req_mountpoint(mounted_on->v_fs_e, mounted_on->v_inode_nr);
	if (r != OK) {
		put_vnode(mounted_on);
		printf("vfs:mount_fs: req_mountpoint_s failed with %d\n", r);
		return r;
	}
  }

  /* We'll need a vnode for the root inode, check whether there is one */
  if ((root_node = get_free_vnode(__FILE__, __LINE__)) == NIL_VNODE) {
        printf("VFSmount: no free vnode available\n");
        return ENFILE;
  }
  

  /* Get driver process' endpoint */  
  dp = &dmap[(dev >> MAJOR) & BYTE];
  if (dp->dmap_driver == NONE) {
        printf("VFSmount: no driver for dev %x\n", dev);
        return(EINVAL);
  }
  label= dp->dmap_label;
  if (strlen(label) == 0)
  {
	panic(__FILE__, "vfs:mount_fs: no label for major", dev >> MAJOR);
  }
#if 0
  printf("vfs:mount_fs: label = '%s'\n", label);
#endif

  /* Issue request */
  r = req_readsuper(fs_e, label, dev, m_in.rd_only, isroot, &res);
  if (r != OK) {
	put_vnode(mounted_on);
	printf("vfs:mount_fs: req_readsuper failed with %d\n", r);
	return r;
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
  vmp->m_flags = m_in.rd_only;
  vmp->m_driver_e = dp->dmap_driver;
  
  /* Root node is indeed on the partition */
  root_node->v_vmnt = vmp;
  root_node->v_dev = vmp->m_dev;
  
  if (replace_root) {
      /* Superblock and root node already read. 
       * Nothing else can go wrong. Perform the mount. */
      vmp->m_root_node = root_node;
      root_node->v_ref_count++;
      vmp->m_mounted_on = root_node;
      root_dev = dev;
      ROOT_FS_E = fs_e;

      /* Replace all root and working directories */
      for (i= 0, tfp= fproc; i<NR_PROCS; i++, tfp++) {
          if (tfp->fp_pid == PID_FREE)
              continue;

          if (tfp->fp_rd == NULL)
              panic("fs", "do_mount: null rootdir", i);
          put_vnode(tfp->fp_rd);
          dup_vnode(root_node);
          tfp->fp_rd = root_node;

          if (tfp->fp_wd == NULL)
              panic("fs", "do_mount: null workdir", i);
          put_vnode(tfp->fp_wd);
          dup_vnode(root_node);
          tfp->fp_wd = root_node;
      }

      return(OK);
  }

  /* File types may not conflict. */
  if (r == OK) {
      mdir = ((mounted_on->v_mode & I_TYPE) == I_DIRECTORY);/* TRUE iff dir */
      rdir = ((root_node->v_mode & I_TYPE) == I_DIRECTORY);
      if (!mdir && rdir) r = EISDIR;
  }

  /* If error, return the super block and both inodes; release the vmnt. */
  if (r != OK) {
      put_vnode(mounted_on);
      put_vnode(root_node);

      vmp->m_dev = NO_DEV;
      return(r);
  }

  /* Nothing else can go wrong.  Perform the mount. */
  vmp->m_mounted_on = mounted_on;
  vmp->m_root_node = root_node;

  /* The root is now fixed */
  allow_newroot = 0;            

  /* There was a block spec file open, and it should be handled by the 
   * new FS proc now */
  if (bspec) {
      printf("VFSmount: moving opened block spec to new FS_e: %d...\n", fs_e);
      bspec->v_bfs_e = fs_e; 
  }
  return(OK);
}

/*===========================================================================*
 *                              do_umount                                    *
 *===========================================================================*/
PUBLIC int do_umount()
{
/* Perform the umount(name) system call. */
  dev_t dev;

  /* Only the super-user may do UMOUNT. */
  if (!super_user) return(EPERM);

  /* If 'name' is not for a block special file, return error. */
  if (fetch_name(m_in.name, m_in.name_length, M3) != OK) return(err_code);
  if ( (dev = name_to_dev()) == NO_DEV) return(err_code);

  return(unmount(dev));
}


/*===========================================================================*
 *                              unmount                                      *
 *===========================================================================*/
PUBLIC int unmount(dev)
Dev_t dev;
{
  struct vnode *vp;
  struct vmnt *vmp_i = NULL, *vmp = NULL;
  struct dmap *dp;
  int count, r;
  int fs_e;

  /* Find vmnt */
  for (vmp_i = &vmnt[0]; vmp_i < &vmnt[NR_MNTS]; ++vmp_i) {
      if (vmp_i->m_dev == dev) {
	if(vmp) panic(__FILE__, "device mounted more than once", dev);
	vmp = vmp_i;
      }
  }

  /* Device mounted? */
  if(!vmp)
	return EINVAL;

  /* See if the mounted device is busy.  Only 1 vnode using it should be
   * open -- the root vnode -- and that inode only 1 time.
   */
  count = 0;
  for (vp = &vnode[0]; vp < &vnode[NR_VNODES]; vp++) {
      if (vp->v_ref_count > 0 && vp->v_dev == dev) {
          count += vp->v_ref_count;
      }
  }

  if (count > 1) {
      return(EBUSY);    /* can't umount a busy file system */
  }

  vnode_clean_refs(vmp->m_root_node);

  /* Request FS the unmount */
  if(vmp->m_fs_e <= 0)
	panic(__FILE__, "unmount: strange fs endpoint", vmp->m_fs_e);
  if ((r = req_unmount(vmp->m_fs_e)) != OK) return r;

  /* Is there a block special file that was handled by that partition? */
  for (vp = &vnode[0]; vp < &vnode[NR_VNODES]; vp++) {
      if ((vp->v_mode & I_TYPE) == I_BLOCK_SPECIAL && 
              vp->v_bfs_e == vmp->m_fs_e) {

          /* Get the driver endpoint of the block spec device */
          dp = &dmap[(dev >> MAJOR) & BYTE];
          if (dp->dmap_driver == NONE) {
              printf("VFSblock_spec_open: driver not found for device %d\n", 
                      dev);
              /* What should be done, panic??? */
              continue;
          }

          printf("VFSunmount: moving block spec %d to root FS\n", dev);
          vp->v_bfs_e = ROOT_FS_E;

          /* Send the driver endpoint (even if it is known already...) */
          if ((r = req_newdriver(vp->v_bfs_e, vp->v_sdev, dp->dmap_driver))
                  != OK) {
              printf("VFSunmount: error sending driver endpoint for block spec\n");
          }
      }
  }

  /* Root device is mounted on itself */
  if (vmp->m_root_node != vmp->m_mounted_on) {
      put_vnode(vmp->m_mounted_on);
      vmp->m_root_node->v_ref_count = 0;
  }
  else {
      vmp->m_mounted_on->v_fs_count--;
  }

  fs_e = vmp->m_fs_e;

  vmp->m_root_node = NIL_VNODE;
  vmp->m_mounted_on = NIL_VNODE;
  vmp->m_dev = NO_DEV;
  vmp->m_fs_e = NONE;
  vmp->m_driver_e = NONE;

  return(OK);
}

/*===========================================================================*
 *                              name_to_dev                                  *
 *===========================================================================*/
PRIVATE dev_t name_to_dev()
{
/* Convert the block special file 'path' to a device number.  If 'path'
 * is not a block special file, return error code in 'err_code'. */
  int r;
  dev_t dev;
  struct vnode *vp;
  
  /* Request lookup */
  if ((r = lookup_vp(0 /*flags*/, 0 /*!use_realuid*/, &vp)) != OK) return r;

  if ((vp->v_mode & I_TYPE) != I_BLOCK_SPECIAL) {
  	err_code = ENOTBLK;
	dev= NO_DEV;
  }
  else
	dev= vp->v_sdev;

  put_vnode(vp);
  
  return dev;
}

