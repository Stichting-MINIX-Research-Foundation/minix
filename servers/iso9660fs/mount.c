#include "inc.h"
#include <minix/vfsif.h>
#include <minix/bdev.h>
#include "const.h"
#include "glo.h"


/*===========================================================================*
 *				fs_readsuper				     *
 *===========================================================================*/
int fs_readsuper() {

  cp_grant_id_t label_gid;
  size_t label_len;
  int r = OK;

  fs_dev    = fs_m_in.REQ_DEV;
  label_gid = fs_m_in.REQ_GRANT;
  label_len = fs_m_in.REQ_PATH_LEN;

  if (label_len > sizeof(fs_dev_label)) 
	return(EINVAL);

  r = sys_safecopyfrom(fs_m_in.m_source, label_gid, 0, (vir_bytes)fs_dev_label,
		       label_len);
  if (r != OK) {
	printf("ISOFS %s:%d safecopyfrom failed: %d\n", __FILE__, __LINE__, r);
	return(EINVAL);
  }

  /* Map the driver label for this major */
  bdev_driver(fs_dev, fs_dev_label);

  /* Open the device the file system lives on in read only mode */
  if (bdev_open(fs_dev, R_BIT) != OK) {
        return(EINVAL);
  }

  /* Read the superblock */
  r = read_vds(&v_pri, fs_dev);
  if (r != OK) {
	bdev_close(fs_dev);
	return(r);
  }

  lmfs_set_blocksize(v_pri.logical_block_size_l, major(fs_dev));

  /* Return some root inode properties */
  fs_m_out.RES_INODE_NR = ID_DIR_RECORD(v_pri.dir_rec_root);
  fs_m_out.RES_MODE = v_pri.dir_rec_root->d_mode;
  fs_m_out.RES_FILE_SIZE_LO = v_pri.dir_rec_root->d_file_size;
  fs_m_out.RES_UID = SYS_UID; /* Always root */
  fs_m_out.RES_GID = SYS_GID; /* operator */

  fs_m_out.RES_CONREQS = 1;	/* We can handle only 1 request at a time */

  return(r);
}


/*===========================================================================*
 *				fs_mountpoint				     *
 *===========================================================================*/
int fs_mountpoint()
{
/* This function looks up the mount point, it checks the condition whether
 * the partition can be mounted on the inode or not. 
 */

  register struct dir_record *rip;
  int r = OK;
  
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
int fs_unmount(void) {
  release_v_pri(&v_pri);	/* Release the super block */
  bdev_close(fs_dev);
  unmountdone = TRUE;
  return(OK);
}

