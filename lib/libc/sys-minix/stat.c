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

  m.m1_i1 = strlen(name) + 1;
  m.m1_p1 = (char *) __UNCONST(name);
  m.m1_p2 = (char *) buffer;

  return _syscall(VFS_PROC_NR, STAT, &m);
}

int _fstat(int fd, struct stat *buffer) { return fstat(fd, buffer); }

int fstat(int fd, struct stat *buffer)
{
  message m;

  m.m1_i1 = fd;
  m.m1_p1 = (char *) buffer;

  return _syscall(VFS_PROC_NR, FSTAT, &m);
}

int _lstat(const char *name, struct stat *buffer) { return lstat(name, buffer); }

int lstat(const char *name, struct stat *buffer)
{
  message m;

  m.m1_i1 = strlen(name) + 1;
  m.m1_p1 = (char *) __UNCONST(name);
  m.m1_p2 = (char *) buffer;

  return _syscall(VFS_PROC_NR, LSTAT, &m);
}
