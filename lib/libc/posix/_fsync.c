#include <lib.h>
#define sync	_sync
#include <unistd.h>

PUBLIC int fsync(int fd)
{
  message m;

  m.m1_i1 = fd;

  return(_syscall(VFS_PROC_NR, FSYNC, &m));
}
