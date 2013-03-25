#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <sys/stat.h>
#include <string.h>

int _stat(const char *name, struct stat *buffer);
int _lstat(const char *name, struct stat *buffer);
int _fstat(int fd, struct stat *buffer);

#ifdef __weak_alias
__weak_alias(_stat, __stat50);
__weak_alias(_lstat, __lstat50);
__weak_alias(_fstat, __fstat50);

__weak_alias(stat, __stat50);
__weak_alias(lstat, __lstat50);
__weak_alias(fstat, __fstat50);
#endif

int stat(const char *name, struct stat *buffer)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.VFS_STAT_LEN = strlen(name) + 1;
  m.VFS_STAT_NAME = (char *) __UNCONST(name);
  m.VFS_STAT_BUF = (char *) buffer;

  return _syscall(VFS_PROC_NR, VFS_STAT, &m);
}

int _fstat(int fd, struct stat *buffer) { return fstat(fd, buffer); }

int fstat(int fd, struct stat *buffer)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.VFS_FSTAT_FD = fd;
  m.VFS_FSTAT_BUF = (char *) buffer;

  return _syscall(VFS_PROC_NR, VFS_FSTAT, &m);
}

int _lstat(const char *name, struct stat *buffer) { return lstat(name, buffer); }

int lstat(const char *name, struct stat *buffer)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.VFS_STAT_LEN = strlen(name) + 1;
  m.VFS_STAT_NAME = (char *) __UNCONST(name);
  m.VFS_STAT_BUF = (char *) buffer;

  return _syscall(VFS_PROC_NR, VFS_LSTAT, &m);
}
