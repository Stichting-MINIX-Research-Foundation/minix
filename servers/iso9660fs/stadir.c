#include "inc.h"
#include <assert.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <minix/com.h>
#include <string.h>
#include <time.h>

#include <minix/vfsif.h>


/*===========================================================================*
 *				stat_dir_record				     *
 *===========================================================================*/
static int stat_dir_record(
  register struct dir_record *dir,	/* pointer to dir record to stat */
  int pipe_pos,   		/* position in a pipe, supplied by fstat() */
  endpoint_t who_e,		/* Caller endpoint */
  cp_grant_id_t gid		/* grant for the stat buf */
)
{
/* This function returns all the info about a particular inode. It's missing
 * the recording date because of a bug in the standard functions stdtime.
 * Once the bug is fixed the function can be called inside this function to
 * return the date. */

/* Common code for stat and fstat system calls. */
  struct stat statbuf;
  int r;
  struct tm ltime;
  time_t time1;
  u32_t blocks;

  blocks = v_pri.volume_space_size_l;
  /* The unit of blocks should be 512 */
  assert(v_pri.logical_block_size_l >= 512);
  blocks = blocks * (v_pri.logical_block_size_l >> 9);

  memset(&statbuf, 0, sizeof(struct stat));

  statbuf.st_dev = fs_dev;	/* the device of the file */
  statbuf.st_ino = ID_DIR_RECORD(dir); /* the id of the dir record */
  statbuf.st_mode = dir->d_mode; /* flags of the file */
  statbuf.st_nlink = dir->d_count; /* times this file is used */
  statbuf.st_uid = 0;		/* user root */
  statbuf.st_gid = 0;		/* group operator */
  statbuf.st_rdev = NO_DEV;
  statbuf.st_size = dir->d_file_size;	/* size of the file */
  statbuf.st_blksize = v_pri.logical_block_size_l;
  statbuf.st_blocks = blocks;

  ltime.tm_year = dir->rec_date[0];
  ltime.tm_mon = dir->rec_date[1] - 1;
  ltime.tm_mday = dir->rec_date[2];
  ltime.tm_hour = dir->rec_date[3];
  ltime.tm_min = dir->rec_date[4];
  ltime.tm_sec = dir->rec_date[5];
  ltime.tm_isdst = 0;

  if (dir->rec_date[6] != 0)
	ltime.tm_hour += dir->rec_date[6] / 4;

  time1 = mktime(&ltime);

  statbuf.st_atime = time1;
  statbuf.st_mtime = time1;
  statbuf.st_ctime = time1;

  /* Copy the struct to user space. */
  r = sys_safecopyto(who_e, gid, 0, (vir_bytes) &statbuf,
		     (phys_bytes) sizeof(statbuf));
  
  return(r);
}


/*===========================================================================*
 *                             fs_stat					     *
 *===========================================================================*/
int fs_stat()
{
  register int r;              /* return value */
  struct dir_record *dir;
  r = EINVAL;

  if ((dir = get_dir_record(fs_m_in.m_vfs_fs_stat.inode)) != NULL) {
	r = stat_dir_record(dir, 0, fs_m_in.m_source, fs_m_in.m_vfs_fs_stat.grant);
	release_dir_record(dir);
  } 

  return(r);
}


/*===========================================================================*
 *				fs_statvfs				     *
 *===========================================================================*/
int fs_statvfs()
{
  struct statvfs st;
  int r;

  memset(&st, 0, sizeof(st));

  st.f_bsize =  v_pri.logical_block_size_l;
  st.f_frsize = st.f_bsize;
  st.f_iosize = st.f_bsize;
  st.f_blocks = v_pri.volume_space_size_l;
  st.f_namemax = NAME_MAX;

  /* Copy the struct to user space. */
  r = sys_safecopyto(fs_m_in.m_source, fs_m_in.m_vfs_fs_statvfs.grant, 0,
			 (vir_bytes) &st, (phys_bytes) sizeof(st));
  
  return(r);
}

/*===========================================================================*
 *                              blockstats                                   *
  *===========================================================================*/
void fs_blockstats(u64_t *blocks, u64_t *free, u64_t *used)
{
        *used = *blocks = v_pri.volume_space_size_l;
        *free = 0;
}

