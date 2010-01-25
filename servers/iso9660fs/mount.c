#include "inc.h"
#include <string.h>
#include <minix/com.h>
#include <minix/vfsif.h>
#include <minix/ds.h>
#include "const.h"
#include "glo.h"


/*===========================================================================*
 *				fs_readsuper				     *
 *===========================================================================*/
PUBLIC int fs_readsuper() {

  cp_grant_id_t label_gid;
  size_t label_len;
  int r = OK;
  unsigned long tasknr;
  endpoint_t driver_e;
  int readonly, isroot;

  fs_dev    = fs_m_in.REQ_DEV;
  label_gid = fs_m_in.REQ_GRANT;
  label_len = fs_m_in.REQ_PATH_LEN;
  readonly  = 1;			/* Always mount devices read only. */
  isroot    = (fs_m_in.REQ_FLAGS & REQ_ISROOT) ? 1 : 0;

  if (label_len > sizeof(fs_dev_label)) 
	return(EINVAL);

  r = sys_safecopyfrom(fs_m_in.m_source, label_gid, 0, (vir_bytes)fs_dev_label,
		       label_len, D);
  if (r != OK) {
	printf("ISOFS %s:%d safecopyfrom failed: %d\n", __FILE__, __LINE__, r);
	return(EINVAL);
  }

  r = ds_retrieve_label_num(fs_dev_label, &tasknr);
  if (r != OK) {
	printf("ISOFS %s:%d ds_retrieve_label_num failed for '%s': %d\n",
		__FILE__, __LINE__, fs_dev_label, r);
	return(EINVAL);
  }

  driver_e = tasknr;

  /* Map the driver endpoint for this major */
  driver_endpoints[(fs_dev >> MAJOR) & BYTE].driver_e =  driver_e;

  /* Open the device the file system lives on */
  if (dev_open(driver_e, fs_dev, driver_e,
	readonly ? R_BIT : (R_BIT|W_BIT)) != OK) {
        return(EINVAL);
  }

  /* Read the superblock */
  r = read_vds(&v_pri, fs_dev);
  if (r != OK)
	return(r);

  /* Return some root inode properties */
  fs_m_out.RES_INODE_NR = ID_DIR_RECORD(v_pri.dir_rec_root);
  fs_m_out.RES_MODE = v_pri.dir_rec_root->d_mode;
  fs_m_out.RES_FILE_SIZE_LO = v_pri.dir_rec_root->d_file_size;
  fs_m_out.RES_UID = SYS_UID; /* Always root */
  fs_m_out.RES_GID = SYS_GID; /* operator */

  return(r);
}


/*===========================================================================*
 *				fs_mountpoint				     *
 *===========================================================================*/
PUBLIC int fs_mountpoint() {
/* This function looks up the mount point, it checks the condition whether
 * the partition can be mounted on the inode or not. 
 */

  register struct dir_record *rip;
  int r = OK;
  mode_t bits;
  
  /* Temporarily open the file. */
  if ((rip = get_dir_record(fs_m_in.REQ_INODE_NR)) == NULL)
	return(EINVAL);

  if (rip->d_mountpoint)
	r = EBUSY;

  /* If the inode is not a dir returns error */
  if ((rip->d_mode & I_TYPE) != I_DIRECTORY)
	r = ENOTDIR;
	
  release_dir_record(rip);

  if (r == OK)
	rip->d_mountpoint = TRUE;

  return(r);
}


/*===========================================================================*
 *				fs_unmount				     *
 *===========================================================================*/
PUBLIC int fs_unmount(void) {
  release_v_pri(&v_pri);	/* Release the super block */
  dev_close(driver_endpoints[(fs_dev >> MAJOR) & BYTE].driver_e, fs_dev);
  return(OK);
}

