#include <lib.h>
#define statvfs	_statvfs
#include <sys/statvfs.h>
#include <string.h>
#include <minix/com.h>

PUBLIC int statvfs(name, buffer)
_CONST char *name;
struct statvfs *buffer;
{
  message m;

  m.STATVFS_LEN = strlen(name) + 1;
  m.STATVFS_NAME = (char *) name;
  m.STATVFS_BUF = (char *) buffer;
  return(_syscall(VFS_PROC_NR, STATVFS, &m));
}
