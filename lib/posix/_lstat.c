#include <lib.h>
#define lstat	_lstat
#define stat	_stat
#include <sys/stat.h>
#include <string.h>

PUBLIC int lstat(name, buffer)
_CONST char *name;
struct stat *buffer;
{
  message m;
  int r;

  m.m1_i1 = strlen(name) + 1;
  m.m1_p1 = (char *) name;
  m.m1_p2 = (char *) buffer;
  if((r = _syscall(FS, LSTAT, &m)) >= 0 || errno != ENOSYS)
     return r;
  return _stat(name, buffer);
}
