#include <lib.h>
#define fstat	_fstat
#include <sys/stat.h>
#include <string.h>

PUBLIC int fstat(fd, buffer)
int fd;
struct stat *buffer;
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
  COPY_PREV_STAT_TO_NEW(buffer, &old_sb);

  return r;
}
