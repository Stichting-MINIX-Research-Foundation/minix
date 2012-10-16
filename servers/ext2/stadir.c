/* Created (MFS based):
 *   February 2010 (Evgeniy Ivanov)
 */

#include "fs.h"
#include <string.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include "inode.h"
#include "super.h"
#include <minix/vfsif.h>


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
  statbuf.st_nlink = rip->i_links_count;
  statbuf.st_uid = rip->i_uid;
  statbuf.st_gid = rip->i_gid;
  statbuf.st_rdev = (s ? rip->i_block[0] : NO_DEV);
  statbuf.st_size = rip->i_size;
  statbuf.st_atime = rip->i_atime;
  statbuf.st_mtime = rip->i_mtime;
  statbuf.st_ctime = rip->i_ctime;
  statbuf.st_blksize = rip->i_sp->s_block_size;
  statbuf.st_blocks = rip->i_blocks;

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

/*===========================================================================*
 *                             fs_statvfs                                    *
 *===========================================================================*/
int fs_statvfs()
{
  struct statvfs st;
  struct super_block *sp;
  int r;

  sp = get_super(fs_dev);

  st.f_bsize =  sp->s_block_size;
  st.f_frsize = sp->s_block_size;
  st.f_blocks = sp->s_blocks_count;
  st.f_bfree = sp->s_free_blocks_count;
  st.f_bavail = sp->s_free_blocks_count - sp->s_r_blocks_count;
  st.f_files = sp->s_inodes_count;
  st.f_ffree = sp->s_free_inodes_count;
  st.f_favail = sp->s_free_inodes_count;
  st.f_fsid = fs_dev;
  st.f_flag = (sp->s_rd_only == 1 ? ST_RDONLY : 0);
  st.f_flag |= ST_NOTRUNC;
  st.f_namemax = NAME_MAX;

  /* Copy the struct to user space. */
  r = sys_safecopyto(fs_m_in.m_source, fs_m_in.REQ_GRANT, 0, (vir_bytes) &st,
                    (phys_bytes) sizeof(st));

  return(r);
}

/*===========================================================================*
 *                              blockstats                                   *
  *===========================================================================*/
void fs_blockstats(u32_t *blocks, u32_t *free, u32_t *used)
{
        struct super_block *sp = get_super(fs_dev);

	*blocks = sp->s_blocks_count;
	*free = sp->s_free_blocks_count;
	*used = *blocks - *free;
}

