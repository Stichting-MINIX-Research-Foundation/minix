#include <lib.h>
#define lstat	_lstat
#include <sys/stat.h>
#include <string.h>

PUBLIC int lstat(name, buffer)
_CONST char *name;
struct stat *buffer;
{
  message m;
  int r;
  struct minix_prev_stat old_sb;

  m.m1_i1 = strlen(name) + 1;
  m.m1_p1 = (char *) name;
  m.m1_p2 = (char *) buffer;

  if((r = _syscall(VFS_PROC_NR, LSTAT, &m)) >= 0 || errno != ENOSYS)
	return r;

  errno = 0;

  /* ENOSYS: new binary and old VFS, fallback to PREV_STAT.
   * User has struct stat (buffer), VFS still fills minix_prev_stat.
   */
  m.m1_i1 = strlen(name) + 1;
  m.m1_p1 = (char *) name;
  m.m1_p2 = (char *) &old_sb;

  if((r = _syscall(VFS_PROC_NR, PREV_LSTAT, &m)) < 0)
	return r;

  memset(buffer, 0, sizeof(struct stat));
  COPY_PREV_STAT_TO_NEW(buffer, &old_sb);

  return r;
}
