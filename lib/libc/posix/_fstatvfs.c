#include <lib.h>
#define fstatvfs	_fstatvfs
#include <sys/statvfs.h>
#include <minix/com.h>

PUBLIC int fstatvfs(int fd, struct statvfs *buffer)
{
  message m;

  m.FSTATVFS_FD = fd;
  m.FSTATVFS_BUF = (char *) buffer;
  return(_syscall(VFS_PROC_NR, FSTATVFS, &m));
}
