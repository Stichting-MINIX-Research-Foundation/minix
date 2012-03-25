#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <fcntl.h>
#include <unistd.h>

#ifdef __weak_alias
__weak_alias(dup, _dup)
#endif

int dup(fd)
int fd;
{
  return(fcntl(fd, F_DUPFD, 0));
}
