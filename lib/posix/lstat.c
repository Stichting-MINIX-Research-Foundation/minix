#include <lib.h>
#define lstat	_lstat
#include <sys/stat.h>
#include <string.h>

PUBLIC int lstat(name, buffer)
_CONST char *name;
struct stat *buffer;
{
  message m;

  m.m1_i1 = strlen(name) + 1;
  m.m1_p1 = (char *) name;
  m.m1_p2 = (char *) buffer;
  return(_syscall(FS, LSTAT, &m));
}
