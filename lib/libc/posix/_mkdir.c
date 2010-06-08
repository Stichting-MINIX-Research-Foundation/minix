#include <lib.h>
#define mkdir	_mkdir
#include <sys/stat.h>
#include <string.h>

PUBLIC int mkdir(const char *name, mode_t mode)
{
  message m;

  m.m1_i1 = strlen(name) + 1;
  m.m1_i2 = mode;
  m.m1_p1 = (char *) name;
  return(_syscall(VFS_PROC_NR, MKDIR, &m));
}
