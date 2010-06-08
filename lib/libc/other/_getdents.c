#include <lib.h>
#define getdents _getdents
#include <dirent.h>

PUBLIC ssize_t getdents(fd, buffer, nbytes)
int fd;
struct dirent *buffer;
size_t nbytes;
{
  message m;

  m.m1_i1 = fd;
  m.m1_i2 = nbytes;
  m.m1_p1 = (char *) buffer;
  return _syscall(VFS_PROC_NR, GETDENTS, &m);
}
