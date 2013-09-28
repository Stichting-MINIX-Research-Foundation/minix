#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>

#ifdef __weak_alias
__weak_alias(fstatfs, _fstatfs)
#endif

int fstatfs(int fd, struct statfs *buffer)
{
  struct statvfs svbuffer;
  int r;

  if ((r = fstatvfs(fd, &svbuffer)) != 0)
	return r;

  buffer->f_bsize = svbuffer.f_bsize;

  return 0;
}
