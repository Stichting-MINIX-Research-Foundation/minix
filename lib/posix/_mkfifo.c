#include <lib.h>
#define mkfifo	_mkfifo
#define mknod	_mknod
#include <sys/stat.h>
#include <unistd.h>

PUBLIC int mkfifo(name, mode)
_CONST char *name;
_mnx_Mode_t mode;
{
  return mknod(name, mode | S_IFIFO, (Dev_t) 0);
}
