#include <lib.h>
#define umask	_umask
#include <sys/stat.h>

PUBLIC mode_t umask(complmode)
Mode_t complmode;
{
  message m;

  m.m1_i1 = complmode;
  return( (mode_t) _syscall(FS, UMASK, &m));
}
