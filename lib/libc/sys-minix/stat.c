#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <sys/stat.h>
#include <string.h>

/* XXX until that st_Xtime macroses used, we have to undefine them,
 * because of minix_prev_stat
 */
#undef st_atime
#undef st_ctime
#undef st_mtime

static void prev_stat2new_stat(struct stat *new, struct minix_prev_stat *prev)
{
  /* Copy field by field because of st_gid type mismath and
   * difference in order after atime.
   */
  new->st_dev = prev->st_dev;
  new->st_ino = prev->st_ino;
  new->st_mode = prev->st_mode;
  new->st_nlink = prev->st_nlink;
  new->st_uid = prev->st_uid;
  new->st_gid = prev->st_gid;
  new->st_rdev = prev->st_rdev;
  new->st_size = prev->st_size;
  new->st_atimespec.tv_sec = prev->st_atime;
  new->st_mtimespec.tv_sec = prev->st_mtime;
  new->st_ctimespec.tv_sec = prev->st_ctime;
}

int _stat(const char *name, struct stat *buffer);
int _lstat(const char *name, struct stat *buffer);
int _fstat(int fd, struct stat *buffer);

#ifdef __weak_alias
__weak_alias(_stat, __stat50);
__weak_alias(_lstat, __lstat50);
__weak_alias(_fstat, __fstat50);

__weak_alias(stat, __stat50);
__weak_alias(lstat, __lstat50);
__weak_alias(fstat, __fstat50);
#endif

int stat(const char *name, struct stat *buffer)
{
  message m;
  int r;
  struct minix_prev_stat old_sb;

  m.m1_i1 = strlen(name) + 1;
  m.m1_p1 = (char *) __UNCONST(name);
  m.m1_p2 = (char *) buffer;

  if((r = _syscall(VFS_PROC_NR, STAT, &m)) >= 0 || errno != ENOSYS)
	return r;

  errno = 0;

  /* ENOSYS: new binary and old VFS, fallback to PREV_STAT.
   * User has struct stat (buffer), VFS still fills minix_prev_stat.
   */
  m.m1_i1 = strlen(name) + 1;
  m.m1_p1 = (char *) __UNCONST(name);
  m.m1_p2 = (char *) &old_sb;

  if((r = _syscall(VFS_PROC_NR, PREV_STAT, &m)) < 0)
	return r;

  memset(buffer, 0, sizeof(struct stat));
  prev_stat2new_stat(buffer, &old_sb);

  return r;
}

int _fstat(int fd, struct stat *buffer) { return fstat(fd, buffer); }

int fstat(int fd, struct stat *buffer)
{
  message m;
  int r;
  struct minix_prev_stat old_sb;

  m.m1_i1 = fd;
  m.m1_p1 = (char *) buffer;

  if((r = _syscall(VFS_PROC_NR, FSTAT, &m)) >= 0 || errno != ENOSYS)
	return r;

  errno = 0;

  /* ENOSYS: new binary and old VFS, fallback to PREV_STAT.
   * User has struct stat (buffer), VFS still fills minix_prev_stat.
   */
  m.m1_i1 = fd;
  m.m1_p1 = (char *) &old_sb;

  if((r = _syscall(VFS_PROC_NR, PREV_FSTAT, &m)) < 0)
	return r;

  memset(buffer, 0, sizeof(struct stat));
  prev_stat2new_stat(buffer, &old_sb);

  return r;
}

int _lstat(const char *name, struct stat *buffer) { return lstat(name, buffer); }

int lstat(const char *name, struct stat *buffer)
{
  message m;
  int r;
  struct minix_prev_stat old_sb;

  m.m1_i1 = strlen(name) + 1;
  m.m1_p1 = (char *) __UNCONST(name);
  m.m1_p2 = (char *) buffer;

  if((r = _syscall(VFS_PROC_NR, LSTAT, &m)) >= 0 || errno != ENOSYS)
	return r;

  errno = 0;

  /* ENOSYS: new binary and old VFS, fallback to PREV_STAT.
   * User has struct stat (buffer), VFS still fills minix_prev_stat.
   */
  m.m1_i1 = strlen(name) + 1;
  m.m1_p1 = (char *) __UNCONST(name);
  m.m1_p2 = (char *) &old_sb;

  if((r = _syscall(VFS_PROC_NR, PREV_LSTAT, &m)) < 0)
	return r;

  memset(buffer, 0, sizeof(struct stat));
  prev_stat2new_stat(buffer, &old_sb);

  return r;
}
