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
FORWARD _PROTOTYPE( int fs_exit, (endpoint_t fs_e)                      );
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
  int r;
	
  /* FS process' endpoint number */ 
  fs_e = (unsigned long)m_in.m1_p3;

  /* Do the actual job */
  r = mount_fs(fs_e);

  /* If not OK and not suspended, bring down FS proc.. */
  if (r != OK && r != SUSPEND) {
      /* Ask RS to bring down FS */
      if (-1 == fs_exit(fs_e)) {
          printf("VFSmount: WARNING: couldn't stop FS endp: %d\n", fs_e);
      }
  }
  
  return r; 
}


/*===========================================================================*
 *                              mount                                        *
 *===========================================================================*/
PRIVATE int mount_fs(endpoint_t fs_e)
{
/* Perform the mount(name, mfile, rd_only) system call. */
  int rdir, mdir;               /* TRUE iff {root|mount} file is dir */
  int i, r, found;
  struct fproc *tfp;
  struct dmap *dp;
  dev_t dev;
  message m;
  struct vnode *vp, *root_node, *mounted_on, *bspec;
  struct vmnt *vmp, *vmp2;
  struct mountpoint_req mreq;
  struct node_details res;
  struct readsuper_req sreq;
  struct readsuper_res sres;
  struct lookup_req lookup_req;
  
  /* Only the super-user may do MOUNT. */
  if (!super_user) return(EPERM);

  /* If FS not yet logged in, save message and suspend mount */
  if (last_login_fs_e != fs_e) {
      mount_m_in = m_in; 
      return SUSPEND;
  }
  
  /* Mount request got after FS login or 
   * FS login arrived after a suspended mount */
  last_login_fs_e = 0;
  
  /* Clear endpoint field */
  mount_m_in.m1_p3 = 0;

  /* If 'name' is not for a block special file, return error. */
  if (fetch_name(m_in.name1, m_in.name1_length, M1) != OK) return(err_code);
  
  /* Get free vnode for the mountpoint */
  if ((mounted_on = get_free_vnode()) == NIL_VNODE) {
        printf("VFSmount: not free vnode available\n");
        return ENFILE;
  }
  
  /* Mark it as used so that we won't find the same for the root_node */
  mounted_on->v_count = 1;
  
  /* Convert name to device number */
  if ((dev = name_to_dev()) == NO_DEV) return(err_code);

  /* Check whether there is a block special file open which uses the 
   * same device (partition) */
  for (bspec = &vnode[0]; bspec < &vnode[NR_VNODES]; ++bspec) {
      if (bspec->v_count > 0 && bspec->v_sdev == dev) {
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
          /* Normally, m_mounted_on refers to the mount point. For a root
           * filesystem, m_mounted_on is equal to the root vnode. We assume
           * that the root of FS is always the real root. If the two
           * vnodes are different or if the root of FS is equal two the
           * root of the filesystem we found, we found a filesystem that
           * is in use. */
          mounted_on->v_count = 0;
          return EBUSY;   /* already mounted */
      }

      if (root_dev == vmp->m_dev)
          panic("fs", "inconsistency remounting old root", NO_NUM);

      /* Now get the inode of the file to be mounted on. */
      if (fetch_name(m_in.name2, m_in.name2_length, M1) != OK) {
          return(err_code);
      }

      /* Fill in lookup request fields */
      lookup_req.path = user_fullpath;
      lookup_req.lastc = NULL;
      lookup_req.flags = EAT_PATH;

      /* Request lookup */
      if ((r = lookup(&lookup_req, &res)) != OK) return r;

      /* Fill in request message fields.*/
      mreq.fs_e = res.fs_e;
      mreq.inode_nr = res.inode_nr;
      mreq.uid = fp->fp_effuid;
      mreq.gid = fp->fp_effgid;

      /* Issue request */
      if ((r = req_mountpoint(&mreq, &res)) != OK) return r;

      mounted_on->v_fs_e = res.fs_e;  
      mounted_on->v_inode_nr = res.inode_nr;
      mounted_on->v_mode = res.fmode;
      mounted_on->v_size = res.fsize;
      mounted_on->v_sdev = NO_DEV;
      mounted_on->v_count = 1;

      /* Find the vmnt for the vnode */
      if ( (vmp2 = find_vmnt(mounted_on->v_fs_e)) == NIL_VMNT)
          printf("VFS: vmnt not found by mount()\n");
      mounted_on->v_vmnt = vmp2; 
      mounted_on->v_dev = vmp2->m_dev;  

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

  /* We'll need a vnode for the root inode, check whether there is one */
  if ((root_node = get_free_vnode()) == NIL_VNODE) {
        printf("VFSmount: no free vnode available\n");
        return ENFILE;
  }
  
  /* Set it back to zero so that if st goes wrong it won't be kept in use */
  mounted_on->v_count = 0;
  
  /* Fetch the name of the mountpoint */
  if (fetch_name(m_in.name2, m_in.name2_length, M1) != OK) {
        return(err_code);
  }

  /* Get driver process' endpoint */  
  dp = &dmap[(dev >> MAJOR) & BYTE];
  if (dp->dmap_driver == NONE) {
        printf("VFSmount: no driver for dev %x\n", dev);
        return(EINVAL);
  }

  /* Open the device the file system lives on. */
  if (dev_open(dev, fs_e, m_in.rd_only ? R_BIT : (R_BIT|W_BIT)) != OK) {
        return(EINVAL);
  }

  /* Request for reading superblock and root inode */
  sreq.fs_e = fs_e;
  sreq.readonly = m_in.rd_only;
  sreq.boottime = boottime;
  sreq.driver_e = dp->dmap_driver;
  sreq.dev = dev;
  sreq.slink_storage = user_fullpath;
  
  if (!strcmp(user_fullpath, "/")) sreq.isroot = 1;
  else sreq.isroot = 0;
 
  /* Issue request */
  if ((r = req_readsuper(&sreq, &sres)) != OK) {
      printf("VFSmount: reading superblock error dev: %d\n", dev);          
      dev_close(dev);
      return r;
  }

  /* Fill in root node's fields */
  root_node->v_fs_e = sres.fs_e;
  root_node->v_inode_nr = sres.inode_nr;
  root_node->v_mode = sres.fmode;
  root_node->v_size = sres.fsize;
  root_node->v_sdev = NO_DEV;
  root_node->v_count = 1;

  /* Fill in max file size and blocksize for the vmnt */
  vmp->m_fs_e = sres.fs_e;
  vmp->m_dev = dev;
  vmp->m_block_size = sres.blocksize;
  vmp->m_max_file_size = sres.maxsize;
  vmp->m_flags = m_in.rd_only;
  vmp->m_driver_e = dp->dmap_driver;
  
  /* Root node is indeed on the partition */
  root_node->v_vmnt = vmp;
  root_node->v_dev = vmp->m_dev;
  
  if (strcmp(user_fullpath, "/") == 0 && allow_newroot) {
      printf("Replacing root\n");

      /* Superblock and root node already read. 
       * Nothing else can go wrong. Perform the mount. */
      vmp->m_root_node = root_node;
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

  /* Fill in lookup request fields */
  lookup_req.path = user_fullpath;
  lookup_req.lastc = NULL;
  lookup_req.flags = EAT_PATH;

  /* Request lookup */
  if ((r = lookup(&lookup_req, &res)) != OK) {
      put_vnode(root_node);
      return r;
  }

  /* Fill in request fields.*/
  mreq.fs_e = res.fs_e;
  mreq.inode_nr = res.inode_nr;
  mreq.uid = fp->fp_effuid;
  mreq.gid = fp->fp_effgid;

  /* Issue request */
  if ((r = req_mountpoint(&mreq, &res)) != OK) {
      put_vnode(root_node);
      return r;
  }
  
  /* Fill in vnode's fields */
  mounted_on->v_fs_e = res.fs_e;  
  mounted_on->v_inode_nr = res.inode_nr;
  mounted_on->v_mode = res.fmode;
  mounted_on->v_size = res.fsize;
  mounted_on->v_sdev = NO_DEV;
  mounted_on->v_count = 1;

  /* Find the vmnt for the vnode */
  if ( (vmp2 = find_vmnt(mounted_on->v_fs_e)) == NIL_VMNT)
      printf("VFS: vmnt not found by mount()");
  mounted_on->v_vmnt = vmp2; 
  mounted_on->v_dev = vmp2->m_dev;

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
      dev_close(dev);
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
  struct vmnt *vmp;
  struct dmap *dp;
  int count, r;
  int fs_e;

  /* Find vmnt */
  for (vmp = &vmnt[0]; vmp < &vmnt[NR_MNTS]; ++vmp) {
      if (vmp->m_dev == dev) break;
      else if (vmp == &vmnt[NR_MNTS])
          return EINVAL;
  }

  /* See if the mounted device is busy.  Only 1 vnode using it should be
   * open -- the root vnode -- and that inode only 1 time.
   */
  count = 0;
  for (vp = &vnode[0]; vp < &vnode[NR_VNODES]; vp++) {
      if (vp->v_count > 0 && vp->v_dev == dev) {
          count += vp->v_count;
      }
  }

  if (count > 1) {
      printf("VFSunmount: %d filesystem is busy count: %d\n", dev, count);
      return(EBUSY);    /* can't umount a busy file system */
  }

  /* Request FS the unmount */
  if ((r = req_unmount(vmp->m_fs_e)) != OK) return r;

  /* Close the device the file system lives on. */
  dev_close(dev);

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
          vp->v_blocksize = _MIN_BLOCK_SIZE;

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
      vmp->m_root_node->v_count = 0;
  }
  else {
      vmp->m_mounted_on->v_count--;
  }

  fs_e = vmp->m_fs_e;

  vmp->m_root_node = NIL_VNODE;
  vmp->m_mounted_on = NIL_VNODE;
  vmp->m_dev = NO_DEV;
  vmp->m_fs_e = 0;
  vmp->m_driver_e = 0;

  /* Ask RS to bring down FS */
  if (-1 == fs_exit(fs_e)) {
      printf("VFSunmount: WARNING: couldn't stop FS endp: %d\n", fs_e);
  }

  printf("VFSunmount: DEV: %d unmounted\n", dev);  
  return(OK);
}

/*===========================================================================*
 *                              name_to_dev                                  *
 *===========================================================================*/
PRIVATE dev_t name_to_dev()
{
/* Convert the block special file 'path' to a device number.  If 'path'
 * is not a block special file, return error code in 'err_code'. */
  struct lookup_req lookup_req;
  struct node_details res;
  int r;
  
  /* Fill in lookup request fields */
  lookup_req.path = user_fullpath;
  lookup_req.lastc = NULL;
  lookup_req.flags = EAT_PATH;

  /* Request lookup */
  if ((r = lookup(&lookup_req, &res)) != OK) return r;

  if ((res.fmode & I_TYPE) != I_BLOCK_SPECIAL) {
  	err_code = ENOTBLK;
	return NO_DEV;
  }
  
  return res.dev;
}


/*===========================================================================*
 *                              fs_exit                                      *
 *===========================================================================*/
PRIVATE int fs_exit(fs_e)
endpoint_t fs_e;
{
/* Build a message for stoping a FS server and ask RS to do it */
  message m;
  pid_t fs_pid;
  int r;

  /* Don't need to stop the one in the bootimage */
  if (fs_e == MFS_PROC_NR) return OK;
  
  /* Get pid for this endpoint */
  if (-1 == (fs_pid = getnpid(fs_e))) {
      printf("VFS: couldn't find pid for fs_e: %d\n", fs_e);
      return -1;
  }

  /* Ask RS to stop process */
  m.RS_PID = fs_pid;
  if (OK != (r = _taskcall(RS_PROC_NR, RS_DOWN, &m))) {
      printf("VFSfs_exit: couldn't bring FS down pid: %d\n", fs_pid);
      return -1;
  }

  return OK;
}


