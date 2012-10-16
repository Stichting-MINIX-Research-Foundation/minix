#include "fs.h"
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include "inode.h"
#include "super.h"
#include <minix/vfsif.h>

/*===========================================================================*
 *				estimate_blocks				     *
 *===========================================================================*/
static blkcnt_t estimate_blocks(struct inode *rip)
{
/* Return the number of 512-byte blocks used by this file. This includes space
 * used by data zones and indirect blocks (actually also zones). Reading in all
 * indirect blocks is too costly for a stat call, so we disregard holes and
 * return a conservative estimation.
 */
  blkcnt_t zones, sindirs, dindirs, nr_indirs, sq_indirs;
  unsigned int zone_size;

  /* Compute the number of zones used by the file. */
  zone_size = rip->i_sp->s_block_size << rip->i_sp->s_log_zone_size;

  zones = (blkcnt_t) ((rip->i_size + zone_size - 1) / zone_size);

  /* Compute the number of indirect blocks needed for that zone count. */
  nr_indirs = (blkcnt_t) rip->i_nindirs;
  sq_indirs = nr_indirs * nr_indirs;

  sindirs = (zones - (blkcnt_t) rip->i_ndzones + nr_indirs - 1) / nr_indirs;
  dindirs = (sindirs - 1 + sq_indirs - 1) / sq_indirs;

  /* Return the number of 512-byte blocks corresponding to the number of data
   * zones and indirect blocks.
   */
  return (zones + sindirs + dindirs) * (blkcnt_t) (zone_size / 512);
}

/*===========================================================================*
 *				stat_inode				     *
 *===========================================================================*/
static int stat_inode(
  register struct inode *rip,	/* pointer to inode to stat */
  endpoint_t who_e,		/* Caller endpoint */
  cp_grant_id_t gid		/* grant for the stat buf */
)
{
/* Common code for stat and fstat system calls. */

  struct stat statbuf;
  mode_t mo;
  int r, s;

  /* Update the atime, ctime, and mtime fields in the inode, if need be. */
  if (rip->i_update) update_times(rip);

  /* Fill in the statbuf struct. */
  mo = rip->i_mode & I_TYPE;

  /* true iff special */
  s = (mo == I_CHAR_SPECIAL || mo == I_BLOCK_SPECIAL);

  memset(&statbuf, 0, sizeof(struct stat));

  statbuf.st_dev = rip->i_dev;
  statbuf.st_ino = rip->i_num;
  statbuf.st_mode = rip->i_mode;
  statbuf.st_nlink = rip->i_nlinks;
  statbuf.st_uid = rip->i_uid;
  statbuf.st_gid = rip->i_gid;
  statbuf.st_rdev = (s ? (dev_t) rip->i_zone[0] : NO_DEV);
  statbuf.st_size = rip->i_size;
  statbuf.st_atime = rip->i_atime;
  statbuf.st_mtime = rip->i_mtime;
  statbuf.st_ctime = rip->i_ctime;
  statbuf.st_blksize = lmfs_fs_block_size();
  statbuf.st_blocks = estimate_blocks(rip);

  /* Copy the struct to user space. */
  r = sys_safecopyto(who_e, gid, (vir_bytes) 0, (vir_bytes) &statbuf,
  		(size_t) sizeof(statbuf));

  return(r);
}

/*===========================================================================*
 *				fs_fstatfs				     *
 *===========================================================================*/
int fs_fstatfs()
{
  struct statfs st;
  struct inode *rip;
  int r;

  if((rip = find_inode(fs_dev, ROOT_INODE)) == NULL)
	  return(EINVAL);
   
  st.f_bsize = rip->i_sp->s_block_size;
  
  /* Copy the struct to user space. */
  r = sys_safecopyto(fs_m_in.m_source, (cp_grant_id_t) fs_m_in.REQ_GRANT,
  		     (vir_bytes) 0, (vir_bytes) &st, (size_t) sizeof(st));
  
  return(r);
}


/*===========================================================================*
 *				fs_statvfs				     *
 *===========================================================================*/
int fs_statvfs()
{
  struct statvfs st;
  struct super_block *sp;
  int r, scale;
  u32_t used;

  sp = get_super(fs_dev);

  scale = sp->s_log_zone_size;

  fs_blockstats((u32_t *) &st.f_blocks, (u32_t *) &st.f_bfree, &used);
  st.f_bavail = st.f_bfree;

  st.f_bsize =  sp->s_block_size << scale;
  st.f_frsize = sp->s_block_size;
  st.f_files = sp->s_ninodes;
  st.f_ffree = count_free_bits(sp, IMAP);
  st.f_favail = st.f_ffree;
  st.f_fsid = fs_dev;
  st.f_flag = (sp->s_rd_only == 1 ? ST_RDONLY : 0);
  st.f_namemax = MFS_DIRSIZ;

  /* Copy the struct to user space. */
  r = sys_safecopyto(fs_m_in.m_source, fs_m_in.REQ_GRANT, 0, (vir_bytes) &st,
		     (phys_bytes) sizeof(st));
  
  return(r);
}

/*===========================================================================*
 *                             fs_stat					     *
 *===========================================================================*/
int fs_stat()
{
  register int r;              /* return value */
  register struct inode *rip;  /* target inode */

  if ((rip = get_inode(fs_dev, (ino_t) fs_m_in.REQ_INODE_NR)) == NULL)
	return(EINVAL);
  
  r = stat_inode(rip, fs_m_in.m_source, (cp_grant_id_t) fs_m_in.REQ_GRANT);
  put_inode(rip);		/* release the inode */
  return(r);
}

