#include <lib.h>
#define mkfifo	_mkfifo
#define mknod	_mknod
#include <sys/stat.h>
#include <unistd.h>

PUBLIC int mkfifo(const char *name, mode_t mode)
{
  return mknod(name, mode | S_IFIFO, (dev_t) 0);
}
