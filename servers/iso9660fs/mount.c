#include "inc.h"
#include <string.h>
#include <minix/com.h>
#include <minix/vfsif.h>
#include <minix/ds.h>

#include "const.h"
#include "glo.h"

/* Function of mounting and unmounting for ISO9660 */

/* Reads the super inode. This function is requested at the mounting of the
 * filesystem. */
/*===========================================================================*
 *				fs_readsuper				     *
 *===========================================================================*/
PUBLIC int fs_readsuper() {

  int r = OK;

  fs_dev = fs_m_in.REQ_DEV;

  driver_endpoints[(fs_dev >> MAJOR) & BYTE].driver_e =  fs_m_in.REQ_DRIVER_E;
  vfs_slink_storage = fs_m_in.REQ_SLINK_STORAGE;

  r = read_vds(&v_pri, fs_dev);	/* This function reads the super block on the 
				 * device and save it in v_pri */

  if (r != OK) return(r);

  /* Return some root inode properties */
  fs_m_out.RES_INODE_NR = ID_DIR_RECORD(v_pri.dir_rec_root);
  fs_m_out.RES_MODE = I_DIRECTORY;
  fs_m_out.RES_FILE_SIZE = v_pri.dir_rec_root->d_file_size;

  /* and some partition properties */
  fs_m_out.RES_MAXSIZE = v_pri.volume_space_size_l; /* Volume space */
  fs_m_out.RES_BLOCKSIZE = v_pri.logical_block_size_l; /* block size */

  return r;
}

/*===========================================================================*
 *				fs_readsuper_s				     *
 *===========================================================================*/
PUBLIC int fs_readsuper_s() {

  cp_grant_id_t label_gid;
  size_t label_len;
  int r = OK;
  unsigned long tasknr;
  endpoint_t driver_e;

  fs_dev = fs_m_in.REQ_DEV;

  label_gid= fs_m_in.REQ_GRANT2;
  label_len= fs_m_in.REQ_PATH_LEN;

  if (label_len > sizeof(fs_dev_label))  {
	printf("iso9660fs:fs_readsuper: label too long\n");
	return EINVAL;
  }

  r= sys_safecopyfrom(fs_m_in.m_source, label_gid, 0, (vir_bytes)fs_dev_label,
	label_len, D);
  if (r != OK) {
	printf("iso9660fs: fs_readsuper: safecopyfrom failed: %d\n", r);
	return EINVAL;
  }

  r= ds_retrieve_u32(fs_dev_label, &tasknr);
  if (r != OK) {
	printf("mfs:fs_readsuper: ds_retrieve_u32 failed for '%s': %d\n",
		fs_dev_label, r);
	return EINVAL;
  }

  driver_e= tasknr;

  /* Map the driver endpoint for this major */
  driver_endpoints[(fs_dev >> MAJOR) & BYTE].driver_e =  driver_e;

  if (dev_open(driver_e, fs_dev, driver_e,
	fs_m_in.REQ_READONLY ? R_BIT : (R_BIT|W_BIT)) != OK) {
        return(EINVAL);
  }

  r = read_vds(&v_pri, fs_dev);	/* This function reads the super block on the 
				 * device and save it in v_pri */

  if (r != OK)
    return(r);

  /* Return some root inode properties */
  fs_m_out.RES_INODE_NR = ID_DIR_RECORD(v_pri.dir_rec_root);
  fs_m_out.RES_MODE = v_pri.dir_rec_root->d_mode;
  fs_m_out.RES_FILE_SIZE = v_pri.dir_rec_root->d_file_size;
  fs_m_out.RES_UID = 0;		/* root */
  fs_m_out.RES_GID = 0;		/* operator */

  /* and some partition properties */
  fs_m_out.RES_MAXSIZE = v_pri.volume_space_size_l; /* Volume space */
  fs_m_out.RES_BLOCKSIZE = v_pri.logical_block_size_l; /* block size */

  return r;
}

/*===========================================================================*
 *				fs_mountpoint_s				     *
 *===========================================================================*/
PUBLIC int fs_mountpoint_s() {
/* This function looks up the mount point, it checks the condition whether
 * the partition can be mounted on the inode or not. 
 */

  register struct dir_record *rip;
  int r = OK;
  mode_t bits;
  
  /* Temporarily open the file. */
  if ((rip = get_dir_record(fs_m_in.REQ_INODE_NR)) == NULL) {
    printf("ISO9660FS(%d) get_inode by fs_mountpoint() failed\n", SELF_E);
    return(EINVAL);
  }

  if (rip->d_mountpoint)
	r= EBUSY;

  /* If the inode is not a dir returns error */
  if ((rip->d_mode & I_TYPE) != I_DIRECTORY)
    r = ENOTDIR;
	
  release_dir_record(rip);

  if (r == OK)
	rip->d_mountpoint = TRUE;

  return r;
}

/* Unmount the filesystem */
/*===========================================================================*
 *				fs_unmount				     *
 *===========================================================================*/
PUBLIC int fs_unmount(void) {
  release_v_pri(&v_pri);	/* Release the super block */

  dev_close(driver_endpoints[(fs_dev >> MAJOR) & BYTE].driver_e, fs_dev);

  return OK;
}
