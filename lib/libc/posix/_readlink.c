#include <lib.h>
#define readlink _readlink
#include <unistd.h>
#include <string.h>

PUBLIC int readlink(name, buffer, bufsiz)
_CONST char *name;
char *buffer;
size_t bufsiz;
{
  message m;

  m.m1_i1 = strlen(name) + 1;
  m.m1_i2 = bufsiz;
  m.m1_p1 = (char *) name;
  m.m1_p2 = (char *) buffer;

  return(_syscall(VFS_PROC_NR, RDLNK, &m));
}
