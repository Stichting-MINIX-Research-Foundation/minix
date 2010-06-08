#include <lib.h>
#define umask	_umask
#include <sys/stat.h>

PUBLIC mode_t umask(mode_t complmode)
{
  message m;

  m.m1_i1 = complmode;
  return( (mode_t) _syscall(VFS_PROC_NR, UMASK, &m));
}
